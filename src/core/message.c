#include "message_internal.h"

#include <stdlib.h>
#include <string.h>

AetherMessageLimits aether_message_default_limits(void)
{
    return (AetherMessageLimits){16U, 256U, 4U * 1024U * 1024U,
                                32U * 1024U * 1024U, 256U * 1024U};
}

MessageRecord* aether_message_find(
    const AetherMessageRegistry* registry, AetherMessageId id
)
{
    size_t index;
    for (index = 0U; index < registry->limits.messages; ++index) {
        MessageRecord* record = &registry->records[index];
        if (record->occupied &&
            aether_id_equal(record->snapshot.message_id.id, id.id)) return record;
    }
    return NULL;
}

AetherStatus aether_message_check_binding(
    const AetherMessageRegistry* registry, const MessageRecord* record,
    AetherMessageFact fact, bool require_live
)
{
    AetherSessionSnapshot session;
    AetherRuntimeSnapshot runtime;
    AetherStatus status;
    if (!aether_id_equal(record->snapshot.session_id.id, fact.session_id.id) ||
        record->snapshot.runtime_generation != fact.runtime_generation) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (!require_live) return AETHER_STATUS_OK;
    status = aether_session_query(registry->sessions, fact.session_id, &session);
    if (status != AETHER_STATUS_OK) return status;
    if ((session.state != AETHER_SESSION_IDLE &&
         session.state != AETHER_SESSION_ACTIVE) ||
        session.runtime_generation != fact.runtime_generation) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_runtime_query(registry->runtimes, session.runtime_id, &runtime);
    if (status != AETHER_STATUS_OK) return status;
    return runtime.state == AETHER_RUNTIME_READY &&
           runtime.generation == fact.runtime_generation
        ? AETHER_STATUS_OK : AETHER_STATUS_INVALID_ARGUMENT;
}

AetherStatus aether_message_registry_create(
    AetherMessageLimits limits, AetherSessionRegistry* sessions,
    AetherRuntimeRegistry* runtimes, AetherMessageRegistry** out_registry
)
{
    AetherMessageRegistry* registry;
    if (out_registry == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_registry = NULL;
    if (sessions == NULL || runtimes == NULL || limits.messages == 0U ||
        limits.messages > 16U || limits.blocks_per_message == 0U ||
        limits.blocks_per_message > 256U || limits.message_bytes == 0U ||
        limits.message_bytes > 4U * 1024U * 1024U || limits.total_bytes == 0U ||
        limits.total_bytes > 32U * 1024U * 1024U ||
        limits.tool_argument_bytes == 0U || limits.tool_argument_bytes > 262144U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    registry = calloc(1U, sizeof(*registry));
    if (registry == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    registry->records = calloc(limits.messages, sizeof(*registry->records));
    if (registry->records == NULL) {
        free(registry);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->limits = limits;
    registry->sessions = sessions;
    registry->runtimes = runtimes;
    *out_registry = registry;
    return AETHER_STATUS_OK;
}

void aether_message_clear_record(AetherMessageRegistry* registry,
                                 MessageRecord* record)
{
    size_t index;
    if (!record->occupied) return;
    for (index = 0U; index < registry->limits.blocks_per_message; ++index) {
        aether_message_free_block(record->blocks[index]);
    }
    free(record->blocks);
    registry->stored_bytes -= record->snapshot.stored_bytes;
    *record = (MessageRecord){0};
}

AetherStatus aether_message_registry_destroy(AetherMessageRegistry** registry)
{
    size_t index;
    if (registry == NULL || *registry == NULL) return AETHER_STATUS_OK;
    for (index = 0U; index < (*registry)->limits.messages; ++index) {
        aether_message_clear_record(*registry, &(*registry)->records[index]);
    }
    free((*registry)->records);
    free(*registry);
    *registry = NULL;
    return AETHER_STATUS_OK;
}

AetherStatus aether_message_create(
    AetherMessageRegistry* registry, AetherSessionId session_id,
    uint64_t runtime_generation, AetherMessageRole role, AetherMessageId* out_id
)
{
    size_t index;
    size_t charge;
    MessageRecord provisional = {0};
    AetherMessageFact binding = {0};
    AetherStatus status;
    if (out_id == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_id = (AetherMessageId){0};
    if (registry == NULL || runtime_generation == 0U ||
        (role != AETHER_MESSAGE_ROLE_USER && role != AETHER_MESSAGE_ROLE_SYSTEM &&
         role != AETHER_MESSAGE_ROLE_ASSISTANT && role != AETHER_MESSAGE_ROLE_TOOL)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    provisional.snapshot.session_id = session_id;
    provisional.snapshot.runtime_generation = runtime_generation;
    binding.session_id = session_id;
    binding.runtime_generation = runtime_generation;
    status = aether_message_check_binding(registry, &provisional, binding, true);
    if (status != AETHER_STATUS_OK) return status;
    charge = sizeof(MessageRecord) +
             registry->limits.blocks_per_message * sizeof(MessageBlock*);
    if (charge > registry->limits.message_bytes ||
        charge > registry->limits.total_bytes - registry->stored_bytes) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    for (index = 0U; index < registry->limits.messages; ++index) {
        MessageRecord* record = &registry->records[index];
        AetherId id;
        if (record->occupied) continue;
        provisional.blocks = calloc(registry->limits.blocks_per_message,
                                     sizeof(*provisional.blocks));
        if (provisional.blocks == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
        id = aether_id_next();
        if (!aether_id_is_valid(id)) {
            free(provisional.blocks);
            return AETHER_STATUS_INTERNAL_ERROR;
        }
        provisional.occupied = true;
        provisional.snapshot.message_id = (AetherMessageId){id};
        provisional.snapshot.role = role;
        provisional.snapshot.state = AETHER_MESSAGE_RECEIVING;
        provisional.snapshot.revision = 1U;
        provisional.snapshot.stored_bytes = charge;
        *record = provisional;
        registry->stored_bytes += charge;
        *out_id = record->snapshot.message_id;
        return AETHER_STATUS_OK;
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_message_query(
    const AetherMessageRegistry* registry, AetherMessageId id,
    AetherMessageSnapshot* out_snapshot
)
{
    MessageRecord* record;
    if (out_snapshot == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_snapshot = (AetherMessageSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = aether_message_find(registry, id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    *out_snapshot = record->snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_message_query_block(
    const AetherMessageRegistry* registry, AetherMessageId message_id,
    uint32_t ordinal, AetherBlockSnapshot** out_snapshot
)
{
    MessageRecord* record;
    MessageBlock* block;
    AetherBlockSnapshot* copy;
    char* bytes;
    size_t size;
    if (out_snapshot == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_snapshot = NULL;
    if (registry == NULL || !aether_id_is_valid(message_id.id) ||
        ordinal >= registry->limits.blocks_per_message) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = aether_message_find(registry, message_id);
    if (record == NULL || record->blocks[ordinal] == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    block = record->blocks[ordinal];
    size = block->snapshot.payload.data.length +
           block->snapshot.payload.label.length + 2U;
    copy = malloc(sizeof(*copy) + size);
    if (copy == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    *copy = block->snapshot;
    bytes = (char*)(copy + 1);
    if (copy->payload.data.length != 0U) {
        (void)memcpy(bytes, copy->payload.data.data, copy->payload.data.length);
    }
    bytes[copy->payload.data.length] = '\0';
    copy->payload.data.data = bytes;
    bytes += copy->payload.data.length + 1U;
    if (copy->payload.label.length != 0U) {
        (void)memcpy(bytes, copy->payload.label.data, copy->payload.label.length);
    }
    bytes[copy->payload.label.length] = '\0';
    copy->payload.label.data = bytes;
    *out_snapshot = copy;
    return AETHER_STATUS_OK;
}

void aether_message_block_snapshot_destroy(AetherBlockSnapshot* snapshot)
{
    free(snapshot);
}
