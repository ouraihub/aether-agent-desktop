#include "message_internal.h"

static bool same_scope(const MessageRecord* left, const MessageRecord* right)
{
    return aether_id_equal(left->snapshot.session_id.id,
                           right->snapshot.session_id.id) &&
           left->snapshot.runtime_generation == right->snapshot.runtime_generation;
}

AetherStatus aether_message_check_tool(
    const AetherMessageRegistry* registry, const MessageRecord* owner,
    AetherMessageFact fact, const MessageBlock* previous
)
{
    size_t message_index;
    const MessageBlock* call = NULL;
    bool is_result = fact.payload.kind == AETHER_BLOCK_TOOL_RESULT;
    if (fact.payload.kind != AETHER_BLOCK_TOOL_CALL && !is_result) {
        return AETHER_STATUS_OK;
    }
    for (message_index = 0U; message_index < registry->limits.messages;
         ++message_index) {
        const MessageRecord* record = &registry->records[message_index];
        size_t ordinal;
        if (!record->occupied || !same_scope(owner, record)) continue;
        for (ordinal = 0U; ordinal < registry->limits.blocks_per_message;
             ++ordinal) {
            const MessageBlock* block = record->blocks[ordinal];
            const AetherBlockPayload* payload;
            if (block == NULL || block == previous) continue;
            payload = &block->snapshot.payload;
            if (!aether_id_equal(payload->tool_call_id,
                                  fact.payload.tool_call_id)) continue;
            if (payload->kind == AETHER_BLOCK_TOOL_CALL) {
                if (!is_result) return AETHER_STATUS_ALREADY_EXISTS;
                if (payload->tool_generation == fact.payload.tool_generation &&
                    block->snapshot.finalized) call = block;
            }
            if (is_result && payload->kind == AETHER_BLOCK_TOOL_RESULT &&
                payload->tool_generation == fact.payload.tool_generation) {
                return AETHER_STATUS_ALREADY_EXISTS;
            }
        }
    }
    return !is_result || call != NULL
        ? AETHER_STATUS_OK : AETHER_STATUS_INVALID_ARGUMENT;
}

AetherStatus aether_message_release(
    AetherMessageRegistry* registry, AetherMessageId id
)
{
    MessageRecord* record;
    size_t index;
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = aether_message_find(registry, id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    if (record->snapshot.state == AETHER_MESSAGE_RECEIVING) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    /* A retained result keeps its referenced call queryable, not a bare ID. */
    for (index = 0U; index < registry->limits.blocks_per_message; ++index) {
        const MessageBlock* call = record->blocks[index];
        size_t other_index;
        if (call == NULL || call->snapshot.payload.kind != AETHER_BLOCK_TOOL_CALL) {
            continue;
        }
        for (other_index = 0U; other_index < registry->limits.messages;
             ++other_index) {
            const MessageRecord* other = &registry->records[other_index];
            size_t ordinal;
            if (other == record || !other->occupied || !same_scope(record, other)) {
                continue;
            }
            for (ordinal = 0U; ordinal < registry->limits.blocks_per_message;
                 ++ordinal) {
                const MessageBlock* result = other->blocks[ordinal];
                if (result != NULL &&
                    result->snapshot.payload.kind == AETHER_BLOCK_TOOL_RESULT &&
                    aether_id_equal(result->snapshot.payload.tool_call_id,
                                    call->snapshot.payload.tool_call_id) &&
                    result->snapshot.payload.tool_generation ==
                        call->snapshot.payload.tool_generation) {
                    return AETHER_STATUS_ALREADY_EXISTS;
                }
            }
        }
    }
    aether_message_clear_record(registry, record);
    return AETHER_STATUS_OK;
}
