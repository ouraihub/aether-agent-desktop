#include "query_internal.h"
#include "aether/backend/state_command.h"
#include <string.h>

static AetherStatus validate_write(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherQueryError* error
)
{
    AetherStatus status = aether_query_validate(queries,
        context != NULL ? &context->request : NULL, error);
    if (status != AETHER_STATUS_OK) return status;
    if (!context->write_allowed) {
        return aether_query_error(&context->request, AETHER_STATUS_PERMISSION_DENIED,
                                  AETHER_QUERY_ERROR_FORBIDDEN, error);
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_rename_workspace(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherWorkspaceRenameInput input, AetherWorkspaceDto* out_dto,
    AetherQueryError* out_error
)
{
    AetherWorkspaceDto current;
    AetherStatus status;
    if (out_dto != NULL) *out_dto = (AetherWorkspaceDto){0};
    status = validate_write(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_dto == NULL || input.expected_revision == 0U ||
        input.display_name.length == 0U || input.display_name.length >= 128U ||
        aether_string_view_validate(input.display_name) != AETHER_STATUS_OK ||
        memchr(input.display_name.data, '\0', input.display_name.length) != NULL) {
        return aether_query_error(&context->request, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    status = aether_backend_query_workspace(queries, &context->request,
                                            input.workspace_id, &current, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (current.revision != input.expected_revision) {
        return aether_query_error(&context->request, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_REVISION_CONFLICT, out_error);
    }
    if (current.revision == UINT64_MAX) {
        return aether_query_error(&context->request, AETHER_STATUS_INTERNAL_ERROR,
                                  AETHER_QUERY_ERROR_INTERNAL, out_error);
    }
    status = aether_workspace_update_name(queries->workspaces,
        (AetherWorkspaceId){input.workspace_id}, input.expected_revision,
        input.display_name);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(&context->request, status,
                                  AETHER_QUERY_ERROR_STATE, out_error);
    }
    return aether_backend_query_workspace(queries, &context->request,
                                           input.workspace_id, out_dto, out_error);
}

AetherStatus aether_backend_request_session_close(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherSessionCloseInput input, AetherSessionDto* out_dto,
    AetherQueryError* out_error
)
{
    AetherSessionDto current;
    AetherStatus status;
    if (out_dto != NULL) *out_dto = (AetherSessionDto){0};
    status = validate_write(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_dto == NULL || input.expected_revision == 0U) {
        return aether_query_error(&context->request, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    status = aether_backend_query_session(queries, &context->request,
                                          input.session_id, &current, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (current.revision != input.expected_revision) {
        return aether_query_error(&context->request, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_REVISION_CONFLICT, out_error);
    }
    if (current.state == AETHER_SESSION_DTO_CLOSING ||
        current.state == AETHER_SESSION_DTO_CLOSED) {
        *out_dto = current;
        return AETHER_STATUS_OK;
    }
    if (current.revision == UINT64_MAX) {
        return aether_query_error(&context->request, AETHER_STATUS_INTERNAL_ERROR,
                                  AETHER_QUERY_ERROR_INTERNAL, out_error);
    }
    status = aether_session_begin_close(queries->sessions,
                                        (AetherSessionId){input.session_id});
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(&context->request, status,
                                  AETHER_QUERY_ERROR_STATE, out_error);
    }
    return aether_backend_query_session(queries, &context->request,
                                         input.session_id, out_dto, out_error);
}
