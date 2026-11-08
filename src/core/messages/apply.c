#include "message_internal.h"

static bool empty_payload(AetherBlockPayload payload)
{
    return payload.kind == AETHER_BLOCK_TEXT && payload.data.length == 0U &&
           payload.label.length == 0U && payload.resource_id.value == 0U &&
           payload.tool_call_id.value == 0U && payload.tool_generation == 0U;
}

static bool same_metadata(AetherBlockPayload left, AetherBlockPayload right)
{
    return left.kind == right.kind &&
        aether_string_view_equal(left.label, right.label) &&
        aether_id_equal(left.resource_id, right.resource_id) &&
        aether_id_equal(left.tool_call_id, right.tool_call_id) &&
        left.tool_generation == right.tool_generation;
}

static AetherStatus end_message(
    AetherMessageRegistry* registry, MessageRecord* record, AetherMessageFact fact
)
{
    size_t index;
    AetherStatus status;
    if (fact.block_id.value != 0U || fact.cursor != 0U || fact.ordinal != 0U ||
        fact.expected_revision != 0U || !empty_payload(fact.payload) ||
        (fact.terminal_state != AETHER_MESSAGE_COMPLETED &&
         fact.terminal_state != AETHER_MESSAGE_INTERRUPTED &&
         fact.terminal_state != AETHER_MESSAGE_FAILED)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (record->snapshot.state != AETHER_MESSAGE_RECEIVING) {
        return record->snapshot.state == fact.terminal_state
            ? AETHER_STATUS_OK : AETHER_STATUS_ALREADY_EXISTS;
    }
    status = aether_message_check_binding(registry, record, fact,
        fact.terminal_state == AETHER_MESSAGE_COMPLETED);
    if (status != AETHER_STATUS_OK) return status;
    if (fact.terminal_state == AETHER_MESSAGE_COMPLETED) {
        for (index = 0U; index < registry->limits.blocks_per_message; ++index) {
            if (record->blocks[index] != NULL &&
                !record->blocks[index]->snapshot.finalized) {
                return AETHER_STATUS_INVALID_ARGUMENT;
            }
        }
    }
    if (record->snapshot.revision == UINT64_MAX) return AETHER_STATUS_INTERNAL_ERROR;
    record->snapshot.state = fact.terminal_state;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

static AetherStatus find_block(
    const AetherMessageRegistry* registry, MessageRecord* record,
    AetherMessageFact fact, MessageBlock** out_block
)
{
    size_t message_index;
    *out_block = NULL;
    /* Block identity is unique across retained messages, not an ordinal alias. */
    for (message_index = 0U; message_index < registry->limits.messages;
         ++message_index) {
        MessageRecord* other = &registry->records[message_index];
        size_t ordinal;
        if (!other->occupied) continue;
        for (ordinal = 0U; ordinal < registry->limits.blocks_per_message;
             ++ordinal) {
            MessageBlock* block = other->blocks[ordinal];
            if (block != NULL &&
                aether_id_equal(block->snapshot.block_id, fact.block_id)) {
                if (other != record || ordinal != fact.ordinal) {
                    return AETHER_STATUS_INVALID_ARGUMENT;
                }
                *out_block = block;
                return AETHER_STATUS_OK;
            }
        }
    }
    return record->blocks[fact.ordinal] == NULL
        ? AETHER_STATUS_OK : AETHER_STATUS_ALREADY_EXISTS;
}

AetherStatus aether_message_apply(
    AetherMessageRegistry* registry, AetherMessageFact fact
)
{
    MessageRecord* record;
    MessageBlock* previous;
    MessageBlock* replacement;
    AetherStatus status;
    if (registry == NULL || !aether_id_is_valid(fact.message_id.id) ||
        !aether_id_is_valid(fact.session_id.id) || fact.runtime_generation == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = aether_message_find(registry, fact.message_id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    status = aether_message_check_binding(registry, record, fact, false);
    if (status != AETHER_STATUS_OK) return status;
    if (fact.operation == AETHER_MESSAGE_END) return end_message(registry, record, fact);
    if ((fact.operation != AETHER_MESSAGE_CREATE_BLOCK &&
         fact.operation != AETHER_MESSAGE_APPEND_BLOCK &&
         fact.operation != AETHER_MESSAGE_REPLACE_BLOCK &&
         fact.operation != AETHER_MESSAGE_FINALIZE_BLOCK) ||
        !aether_id_is_valid(fact.block_id) || fact.cursor == 0U ||
        fact.ordinal >= registry->limits.blocks_per_message ||
        fact.terminal_state != AETHER_MESSAGE_RECEIVING) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_message_payload_validate(fact.payload,
                                            registry->limits.tool_argument_bytes);
    if (status != AETHER_STATUS_OK) return status;
    status = find_block(registry, record, fact, &previous);
    if (status != AETHER_STATUS_OK) return status;
    if (previous != NULL && fact.cursor == previous->snapshot.cursor) {
        return aether_message_fact_equal(previous->last, fact)
            ? AETHER_STATUS_OK : AETHER_STATUS_PROTOCOL_ERROR;
    }
    if (record->snapshot.state != AETHER_MESSAGE_RECEIVING ||
        (previous != NULL && previous->snapshot.finalized)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_message_check_binding(registry, record, fact, true);
    if (status != AETHER_STATUS_OK) return status;
    if (previous == NULL) {
        if (fact.operation != AETHER_MESSAGE_CREATE_BLOCK) {
            return AETHER_STATUS_NOT_FOUND;
        }
        if (fact.cursor != 1U) return AETHER_STATUS_PROTOCOL_ERROR;
        if (fact.expected_revision != 0U) return AETHER_STATUS_INVALID_ARGUMENT;
    } else {
        if (fact.cursor < previous->snapshot.cursor) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        if (previous->snapshot.cursor == UINT64_MAX ||
            fact.cursor != previous->snapshot.cursor + 1U) {
            return AETHER_STATUS_PROTOCOL_ERROR;
        }
        if (fact.operation == AETHER_MESSAGE_CREATE_BLOCK ||
            fact.expected_revision != previous->snapshot.revision ||
            !same_metadata(previous->snapshot.payload, fact.payload)) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        if (fact.operation == AETHER_MESSAGE_APPEND_BLOCK &&
            fact.payload.kind != AETHER_BLOCK_TEXT) return AETHER_STATUS_NOT_SUPPORTED;
        if (fact.operation == AETHER_MESSAGE_FINALIZE_BLOCK &&
            !aether_string_view_equal(previous->snapshot.payload.data,
                                       fact.payload.data)) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        if (previous->snapshot.revision == UINT64_MAX) return AETHER_STATUS_INTERNAL_ERROR;
    }
    status = aether_message_check_tool(registry, record, fact, previous);
    if (status != AETHER_STATUS_OK) return status;
    if (record->snapshot.revision == UINT64_MAX) return AETHER_STATUS_INTERNAL_ERROR;
    status = aether_message_build_block(registry, record, previous, fact,
                                        &replacement);
    if (status != AETHER_STATUS_OK) return status;
    if (previous != NULL) {
        record->snapshot.stored_bytes -= previous->charge;
        registry->stored_bytes -= previous->charge;
    } else {
        ++record->snapshot.block_count;
    }
    record->blocks[fact.ordinal] = replacement;
    record->snapshot.stored_bytes += replacement->charge;
    registry->stored_bytes += replacement->charge;
    ++record->snapshot.revision;
    aether_message_free_block(previous);
    return AETHER_STATUS_OK;
}
