#include "query_internal.h"
#include <stdlib.h>
#include <string.h>

static bool nonzero(const uint8_t* bytes, size_t length)
{
    size_t index;
    for (index = 0U; index < length; ++index) if (bytes[index] != 0U) return true;
    return false;
}

static bool valid_trace(const AetherRequestContext* context)
{
    return context != NULL && context->trace.valid &&
        nonzero(context->trace.trace_id.bytes, sizeof(context->trace.trace_id.bytes)) &&
        nonzero(context->trace.span_id.bytes, sizeof(context->trace.span_id.bytes));
}

AetherStatus aether_query_error(
    const AetherRequestContext* context, AetherStatus status,
    AetherQueryErrorCode code, AetherQueryError* out_error
)
{
    const char* message;
    if (out_error == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_error = (AetherQueryError){0};
    out_error->status = status;
    if (status == AETHER_STATUS_OK) return status;
    if (code == AETHER_QUERY_ERROR_NONE) {
        switch (status) {
        case AETHER_STATUS_INVALID_ARGUMENT: code = AETHER_QUERY_ERROR_INPUT; break;
        case AETHER_STATUS_PERMISSION_DENIED: code = AETHER_QUERY_ERROR_FORBIDDEN; break;
        case AETHER_STATUS_NOT_FOUND: code = AETHER_QUERY_ERROR_MISSING; break;
        case AETHER_STATUS_OUT_OF_MEMORY: code = AETHER_QUERY_ERROR_CAPACITY; break;
        case AETHER_STATUS_INTERNAL_ERROR: code = AETHER_QUERY_ERROR_INTERNAL; break;
        default: code = AETHER_QUERY_ERROR_STATE; break;
        }
    }
    switch (code) {
    case AETHER_QUERY_ERROR_INPUT: message = "Invalid query input."; break;
    case AETHER_QUERY_ERROR_VERSION: message = "Unsupported API version."; break;
    case AETHER_QUERY_ERROR_FORBIDDEN: message = "Access to this object is denied."; break;
    case AETHER_QUERY_ERROR_MISSING: message = "The requested object was not found."; break;
    case AETHER_QUERY_ERROR_CAPACITY: message = "Query response capacity exceeded."; break;
    case AETHER_QUERY_ERROR_STATE: message = "Query is unavailable in the current state."; break;
    case AETHER_QUERY_ERROR_REVISION_CONFLICT:
        message = "Object changed; reload before editing.";
        break;
    default: message = "Query failed."; break;
    }
    out_error->code = code;
    (void)memcpy(out_error->message, message, strlen(message) + 1U);
    if (valid_trace(context)) {
        out_error->diagnostic_available = true;
        out_error->diagnostic_id = context->trace.trace_id;
    }
    return status;
}

AetherStatus aether_query_validate(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherQueryError* out_error
)
{
    if (out_error == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_error = (AetherQueryError){0};
    if (queries == NULL || context == NULL) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    if (context->api_version != AETHER_BACKEND_QUERY_VERSION) {
        return aether_query_error(context, AETHER_STATUS_NOT_SUPPORTED,
                                  AETHER_QUERY_ERROR_VERSION, out_error);
    }
    if (!aether_id_is_valid(context->caller_id) || !valid_trace(context) ||
        (!context->read_all_workspaces &&
         !aether_id_is_valid(context->workspace_scope))) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_query_authorize_workspace(
    const AetherRequestContext* context, AetherId workspace,
    AetherQueryError* out_error
)
{
    if (!context->read_all_workspaces &&
        !aether_id_equal(context->workspace_scope, workspace)) {
        return aether_query_error(context, AETHER_STATUS_PERMISSION_DENIED,
                                  AETHER_QUERY_ERROR_FORBIDDEN, out_error);
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_queries_create(
    AetherWorkspaceRegistry* workspaces, AetherRuntimeRegistry* runtimes,
    AetherSessionRegistry* sessions, AetherMessageRegistry* messages,
    AetherBackendQueries** out_queries
)
{
    AetherBackendQueries* queries;
    if (out_queries == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_queries = NULL;
    if (workspaces == NULL || runtimes == NULL || sessions == NULL || messages == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    queries = malloc(sizeof(*queries));
    if (queries == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    *queries = (AetherBackendQueries){workspaces, runtimes, sessions, messages};
    *out_queries = queries;
    return AETHER_STATUS_OK;
}

void aether_backend_queries_destroy(AetherBackendQueries* queries)
{
    free(queries);
}

AetherStatus aether_backend_query_workspace(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherWorkspaceDto* out_dto, AetherQueryError* out_error
)
{
    AetherWorkspaceSnapshot source;
    AetherWorkspaceDtoState state;
    AetherStatus status;
    if (out_dto != NULL) *out_dto = (AetherWorkspaceDto){0};
    status = aether_query_validate(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_dto == NULL || !aether_id_is_valid(id)) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    status = aether_query_authorize_workspace(context, id, out_error);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_workspace_query(queries->workspaces, (AetherWorkspaceId){id}, &source);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
    }
    switch (source.state) {
    case AETHER_WORKSPACE_OPEN: state = AETHER_WORKSPACE_DTO_OPEN; break;
    case AETHER_WORKSPACE_CLOSING: state = AETHER_WORKSPACE_DTO_CLOSING; break;
    case AETHER_WORKSPACE_CLOSED: state = AETHER_WORKSPACE_DTO_CLOSED; break;
    default: return aether_query_error(context, AETHER_STATUS_NOT_SUPPORTED,
                                        AETHER_QUERY_ERROR_STATE, out_error);
    }
    out_dto->id = source.workspace_id.id;
    out_dto->revision = source.revision;
    out_dto->state = state;
    (void)memcpy(out_dto->display_name, source.display_name, sizeof(source.display_name));
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_query_session(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherSessionDto* out_dto, AetherQueryError* out_error
)
{
    AetherSessionSnapshot source;
    AetherSessionDtoState state;
    AetherStatus status;
    if (out_dto != NULL) *out_dto = (AetherSessionDto){0};
    status = aether_query_validate(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_dto == NULL || !aether_id_is_valid(id)) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    status = aether_session_query(queries->sessions, (AetherSessionId){id}, &source);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
    }
    status = aether_query_authorize_workspace(context, source.workspace_id.id, out_error);
    if (status != AETHER_STATUS_OK) return status;
    switch (source.state) {
    case AETHER_SESSION_CREATED: state = AETHER_SESSION_DTO_CREATED; break;
    case AETHER_SESSION_INITIALIZING: state = AETHER_SESSION_DTO_INITIALIZING; break;
    case AETHER_SESSION_IDLE: state = AETHER_SESSION_DTO_IDLE; break;
    case AETHER_SESSION_ACTIVE: state = AETHER_SESSION_DTO_ACTIVE; break;
    case AETHER_SESSION_INTERRUPTED: state = AETHER_SESSION_DTO_INTERRUPTED; break;
    case AETHER_SESSION_RUNTIME_UNAVAILABLE: state = AETHER_SESSION_DTO_RUNTIME_UNAVAILABLE; break;
    case AETHER_SESSION_CLOSING: state = AETHER_SESSION_DTO_CLOSING; break;
    case AETHER_SESSION_CLOSED: state = AETHER_SESSION_DTO_CLOSED; break;
    case AETHER_SESSION_FAILED: state = AETHER_SESSION_DTO_FAILED; break;
    default: return aether_query_error(context, AETHER_STATUS_NOT_SUPPORTED,
                                        AETHER_QUERY_ERROR_STATE, out_error);
    }
    *out_dto = (AetherSessionDto){source.session_id.id, source.workspace_id.id,
        source.agent_config_id.id, source.runtime_id.id, source.runtime_generation,
        source.revision, source.active_turns, state};
    return AETHER_STATUS_OK;
}

AetherStatus aether_query_message_source(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherMessageSnapshot* out_snapshot, AetherQueryError* out_error
)
{
    AetherSessionDto session;
    AetherStatus status = aether_query_validate(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_message_query(queries->messages, (AetherMessageId){id}, out_snapshot);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
    }
    status = aether_backend_query_session(queries, context,
        out_snapshot->session_id.id, &session, out_error);
    if (status != AETHER_STATUS_OK) *out_snapshot = (AetherMessageSnapshot){0};
    return status;
}
