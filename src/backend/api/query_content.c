#include "../internal/query_internal.h"

#include <stdlib.h>
#include <string.h>

#define QUERY_BLOCK_MAX_BYTES (1024U * 1024U)

static AetherStatus runtime_state(AetherRuntimeState state, AetherRuntimeDtoState* out)
{
    switch (state) {
    case AETHER_RUNTIME_REGISTERED: *out = AETHER_RUNTIME_DTO_REGISTERED; break;
    case AETHER_RUNTIME_READY: *out = AETHER_RUNTIME_DTO_READY; break;
    case AETHER_RUNTIME_UNAVAILABLE: *out = AETHER_RUNTIME_DTO_UNAVAILABLE; break;
    case AETHER_RUNTIME_CLOSING: *out = AETHER_RUNTIME_DTO_CLOSING; break;
    default: return AETHER_STATUS_NOT_SUPPORTED;
    }
    return AETHER_STATUS_OK;
}

static AetherStatus message_role(AetherMessageRole role, AetherMessageDtoRole* out)
{
    switch (role) {
    case AETHER_MESSAGE_ROLE_USER: *out = AETHER_MESSAGE_DTO_USER; break;
    case AETHER_MESSAGE_ROLE_ASSISTANT: *out = AETHER_MESSAGE_DTO_ASSISTANT; break;
    case AETHER_MESSAGE_ROLE_TOOL: *out = AETHER_MESSAGE_DTO_TOOL; break;
    case AETHER_MESSAGE_ROLE_SYSTEM: *out = AETHER_MESSAGE_DTO_SYSTEM; break;
    default: return AETHER_STATUS_NOT_SUPPORTED;
    }
    return AETHER_STATUS_OK;
}

static AetherStatus message_state(AetherMessageState state, AetherMessageDtoState* out)
{
    switch (state) {
    case AETHER_MESSAGE_RECEIVING: *out = AETHER_MESSAGE_DTO_RECEIVING; break;
    case AETHER_MESSAGE_COMPLETED: *out = AETHER_MESSAGE_DTO_COMPLETED; break;
    case AETHER_MESSAGE_INTERRUPTED: *out = AETHER_MESSAGE_DTO_INTERRUPTED; break;
    case AETHER_MESSAGE_FAILED: *out = AETHER_MESSAGE_DTO_FAILED; break;
    default: return AETHER_STATUS_NOT_SUPPORTED;
    }
    return AETHER_STATUS_OK;
}

static AetherStatus block_kind(AetherBlockKind kind, AetherBlockDtoKind* out)
{
    switch (kind) {
    case AETHER_BLOCK_TEXT: *out = AETHER_BLOCK_DTO_TEXT; break;
    case AETHER_BLOCK_IMAGE: *out = AETHER_BLOCK_DTO_IMAGE; break;
    case AETHER_BLOCK_FILE: *out = AETHER_BLOCK_DTO_FILE; break;
    case AETHER_BLOCK_TOOL_CALL: *out = AETHER_BLOCK_DTO_TOOL_CALL; break;
    case AETHER_BLOCK_TOOL_RESULT: *out = AETHER_BLOCK_DTO_TOOL_RESULT; break;
    case AETHER_BLOCK_STRUCTURED: *out = AETHER_BLOCK_DTO_STRUCTURED; break;
    default: return AETHER_STATUS_NOT_SUPPORTED;
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_query_runtime(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId session_id, AetherId runtime_id,
    AetherRuntimeDto* out_dto, AetherQueryError* out_error
)
{
    AetherSessionDto session;
    AetherRuntimeSnapshot snapshot;
    AetherRuntimeDto dto = {0};
    AetherStatus status;
    if (out_dto != NULL) *out_dto = (AetherRuntimeDto){0};
    status = aether_query_validate(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_dto == NULL || !aether_id_is_valid(session_id) ||
        !aether_id_is_valid(runtime_id)) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_NONE, out_error);
    }
    status = aether_backend_query_session(queries, context, session_id, &session, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (!aether_id_equal(session.runtime_id, runtime_id)) {
        return aether_query_error(context, AETHER_STATUS_PERMISSION_DENIED,
                                  AETHER_QUERY_ERROR_NONE, out_error);
    }
    status = aether_runtime_query(queries->runtimes, (AetherRuntimeId){runtime_id}, &snapshot);
    if (status == AETHER_STATUS_OK) status = runtime_state(snapshot.state, &dto.state);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
    }
    dto.id = snapshot.runtime_id.id;
    dto.provider_id = snapshot.provider_id.id;
    dto.generation = snapshot.generation;
    dto.revision = snapshot.revision;
    dto.capabilities_known = snapshot.capabilities.known;
    dto.capabilities_supported = snapshot.capabilities.supported;
    *out_dto = dto;
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_query_message(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherMessageDto* out_dto, AetherQueryError* out_error
)
{
    AetherMessageSnapshot snapshot;
    AetherMessageDto dto = {0};
    AetherStatus status;
    if (out_dto != NULL) *out_dto = (AetherMessageDto){0};
    status = aether_query_validate(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_dto == NULL || !aether_id_is_valid(id)) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_NONE, out_error);
    }
    status = aether_query_message_source(queries, context, id, &snapshot, out_error);
    if (status != AETHER_STATUS_OK) return status;
    status = message_role(snapshot.role, &dto.role);
    if (status == AETHER_STATUS_OK) status = message_state(snapshot.state, &dto.state);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
    }
    dto.id = snapshot.message_id.id;
    dto.session_id = snapshot.session_id.id;
    dto.runtime_generation = snapshot.runtime_generation;
    dto.revision = snapshot.revision;
    dto.block_count = snapshot.block_count;
    *out_dto = dto;
    return AETHER_STATUS_OK;
}

static AetherStatus block_copy(
    const AetherBlockSnapshot* snapshot, size_t max_bytes, AetherBlockDto** out_dto
)
{
    AetherBlockDtoKind kind;
    AetherBlockDto* dto;
    char* bytes;
    size_t size = sizeof(*dto);
    AetherStatus status = block_kind(snapshot->payload.kind, &kind);
    if (status != AETHER_STATUS_OK) return status;
    /* Include both terminators in the bounded allocation; subtract before adding. */
    if (size > max_bytes || max_bytes - size < 2U) return AETHER_STATUS_OUT_OF_MEMORY;
    size += 2U;
    if (snapshot->payload.data.length > max_bytes - size) return AETHER_STATUS_OUT_OF_MEMORY;
    size += snapshot->payload.data.length;
    if (snapshot->payload.label.length > max_bytes - size) return AETHER_STATUS_OUT_OF_MEMORY;
    size += snapshot->payload.label.length;
    dto = malloc(size);
    if (dto == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    *dto = (AetherBlockDto){0};
    dto->id = snapshot->block_id;
    dto->ordinal = snapshot->ordinal;
    dto->revision = snapshot->revision;
    dto->cursor = snapshot->cursor;
    dto->finalized = snapshot->finalized;
    dto->kind = kind;
    dto->resource_id = snapshot->payload.resource_id;
    dto->tool_call_id = snapshot->payload.tool_call_id;
    dto->tool_generation = snapshot->payload.tool_generation;
    bytes = (char*)(dto + 1);
    dto->data = (AetherStringView){bytes, snapshot->payload.data.length};
    if (dto->data.length != 0U) {
        (void)memcpy(bytes, snapshot->payload.data.data, dto->data.length);
    }
    bytes[dto->data.length] = '\0';
    bytes += dto->data.length + 1U;
    dto->label = (AetherStringView){bytes, snapshot->payload.label.length};
    if (dto->label.length != 0U) {
        (void)memcpy(bytes, snapshot->payload.label.data, dto->label.length);
    }
    bytes[dto->label.length] = '\0';
    *out_dto = dto;
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_query_block(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId message_id, uint32_t ordinal, size_t max_bytes,
    AetherBlockDto** out_dto, AetherQueryError* out_error
)
{
    AetherMessageSnapshot message;
    AetherBlockSnapshot* snapshot = NULL;
    AetherStatus status;
    if (out_dto != NULL) *out_dto = NULL;
    status = aether_query_validate(queries, context, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_dto == NULL || !aether_id_is_valid(message_id) || max_bytes == 0U ||
        max_bytes > QUERY_BLOCK_MAX_BYTES) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_NONE, out_error);
    }
    status = aether_query_message_source(queries, context, message_id, &message, out_error);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_message_query_block(queries->messages, (AetherMessageId){message_id},
                                       ordinal, &snapshot);
    if (status == AETHER_STATUS_OK) status = block_copy(snapshot, max_bytes, out_dto);
    aether_message_block_snapshot_destroy(snapshot);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
    }
    return AETHER_STATUS_OK;
}

void aether_backend_block_destroy(AetherBlockDto* dto)
{
    free(dto);
}
