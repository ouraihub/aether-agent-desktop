#include "message_internal.h"

#include <stdlib.h>
#include <string.h>

#define MESSAGE_LABEL_MAX_BYTES 127U

static bool valid_text(AetherStringView view)
{
    return aether_string_view_validate(view) == AETHER_STATUS_OK &&
           (view.length == 0U || memchr(view.data, '\0', view.length) == NULL);
}

static bool bytes_equal(AetherStringView left, AetherStringView right)
{
    if (left.length != right.length) return false;
    if (left.length == 0U) return true;
    return left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0;
}

AetherStatus aether_message_payload_validate(
    AetherBlockPayload payload, size_t tool_argument_bytes
)
{
    bool has_resource = aether_id_is_valid(payload.resource_id);
    bool has_call = aether_id_is_valid(payload.tool_call_id);
    bool has_generation = payload.tool_generation != 0U;

    switch (payload.kind) {
    case AETHER_BLOCK_TEXT:
    case AETHER_BLOCK_IMAGE:
    case AETHER_BLOCK_FILE:
    case AETHER_BLOCK_TOOL_CALL:
    case AETHER_BLOCK_TOOL_RESULT:
    case AETHER_BLOCK_STRUCTURED:
        break;
    default:
        return AETHER_STATUS_NOT_SUPPORTED;
    }
    if (payload.label.length > MESSAGE_LABEL_MAX_BYTES ||
        !valid_text(payload.label) || !valid_text(payload.data)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    switch (payload.kind) {
    case AETHER_BLOCK_TEXT:
        if (payload.label.length != 0U || has_resource || has_call ||
            has_generation) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        break;
    case AETHER_BLOCK_IMAGE:
    case AETHER_BLOCK_FILE:
        if (!has_resource || payload.label.length == 0U ||
            payload.data.length != 0U || has_call || has_generation) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        break;
    case AETHER_BLOCK_TOOL_CALL:
        if (has_resource || !has_call || !has_generation ||
            payload.label.length == 0U) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        if (payload.data.length > tool_argument_bytes) {
            return AETHER_STATUS_OUT_OF_MEMORY;
        }
        break;
    case AETHER_BLOCK_TOOL_RESULT:
        if (has_resource || !has_call || !has_generation ||
            payload.label.length != 0U) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        break;
    case AETHER_BLOCK_STRUCTURED:
        if (has_resource || has_call || has_generation ||
            payload.label.length == 0U) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        break;
    default:
        return AETHER_STATUS_NOT_SUPPORTED;
    }
    return AETHER_STATUS_OK;
}

bool aether_message_fact_equal(AetherMessageFact left, AetherMessageFact right)
{
    return aether_id_equal(left.session_id.id, right.session_id.id) &&
           aether_id_equal(left.message_id.id, right.message_id.id) &&
           left.runtime_generation == right.runtime_generation &&
           left.operation == right.operation &&
           aether_id_equal(left.block_id, right.block_id) &&
           left.ordinal == right.ordinal && left.cursor == right.cursor &&
           left.expected_revision == right.expected_revision &&
           left.terminal_state == right.terminal_state &&
           left.payload.kind == right.payload.kind &&
           aether_id_equal(left.payload.resource_id, right.payload.resource_id) &&
           aether_id_equal(left.payload.tool_call_id, right.payload.tool_call_id) &&
           left.payload.tool_generation == right.payload.tool_generation &&
           bytes_equal(left.payload.label, right.payload.label) &&
           bytes_equal(left.payload.data, right.payload.data);
}

static bool add_size(size_t* size, size_t extra)
{
    if (extra > SIZE_MAX - *size) return false;
    *size += extra;
    return true;
}

static AetherStringView copy_view(char** destination, AetherStringView source)
{
    AetherStringView copy = {*destination, source.length};
    if (source.length != 0U) {
        (void)memcpy(*destination, source.data, source.length);
    }
    (*destination)[source.length] = '\0';
    *destination += source.length + 1U;
    return copy;
}

AetherStatus aether_message_build_block(
    AetherMessageRegistry* registry, MessageRecord* record,
    const MessageBlock* previous, AetherMessageFact fact,
    MessageBlock** out_block
)
{
    AetherBlockPayload payload = fact.payload;
    AetherStringView prefix = {0};
    AetherStringView suffix = fact.payload.data;
    AetherStatus status;
    MessageBlock* block;
    char* destination;
    size_t data_bytes;
    size_t charge = sizeof(MessageBlock);
    bool shared_label;
    bool shared_data;

    if (out_block == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_block = NULL;
    if (registry == NULL || record == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    switch (fact.operation) {
    case AETHER_MESSAGE_CREATE_BLOCK:
        if (previous != NULL) return AETHER_STATUS_INVALID_ARGUMENT;
        break;
    case AETHER_MESSAGE_APPEND_BLOCK:
        if (previous == NULL || previous->snapshot.payload.kind != AETHER_BLOCK_TEXT ||
            fact.payload.kind != AETHER_BLOCK_TEXT) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        payload = previous->snapshot.payload;
        prefix = payload.data;
        break;
    case AETHER_MESSAGE_REPLACE_BLOCK:
        if (previous == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
        break;
    case AETHER_MESSAGE_FINALIZE_BLOCK:
        if (previous == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
        payload = previous->snapshot.payload;
        prefix = payload.data;
        suffix = (AetherStringView){0};
        break;
    default:
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (previous != NULL && previous->snapshot.revision == UINT64_MAX) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (fact.payload.data.length > registry->limits.message_bytes) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    status = aether_message_payload_validate(
        fact.payload, registry->limits.tool_argument_bytes
    );
    if (status != AETHER_STATUS_OK) return status;

    data_bytes = prefix.length;
    shared_label = bytes_equal(payload.label, fact.payload.label);
    shared_data = fact.operation == AETHER_MESSAGE_FINALIZE_BLOCK ?
        bytes_equal(payload.data, fact.payload.data) : prefix.length == 0U;
    if (!add_size(&data_bytes, suffix.length) ||
        data_bytes > registry->limits.message_bytes ||
        !add_size(&charge, data_bytes) || !add_size(&charge, 1U) ||
        !add_size(&charge, payload.label.length) || !add_size(&charge, 1U) ||
        (!shared_data &&
         (!add_size(&charge, fact.payload.data.length) || !add_size(&charge, 1U))) ||
        (!shared_label &&
         (!add_size(&charge, fact.payload.label.length) || !add_size(&charge, 1U)))) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    /* Keep the old allocation charged until the parent commits this block. */
    if (record->snapshot.stored_bytes > registry->limits.message_bytes ||
        charge > registry->limits.message_bytes - record->snapshot.stored_bytes ||
        registry->stored_bytes > registry->limits.total_bytes ||
        charge > registry->limits.total_bytes - registry->stored_bytes) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    block = malloc(charge);
    if (block == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    *block = (MessageBlock){0};
    block->storage = (char*)(block + 1);
    block->storage_bytes = charge - sizeof(*block);
    block->charge = charge;
    block->snapshot = previous != NULL ? previous->snapshot :
        (AetherBlockSnapshot){.block_id = fact.block_id, .ordinal = fact.ordinal};
    block->snapshot.revision = previous != NULL ? previous->snapshot.revision + 1U : 1U;
    block->snapshot.cursor = fact.cursor;
    block->snapshot.finalized = fact.operation == AETHER_MESSAGE_FINALIZE_BLOCK;
    block->snapshot.payload = payload;
    block->last = fact;

    /* Every source remains live; even views aliasing previous are safe here. */
    destination = block->storage;
    if (prefix.length != 0U) {
        (void)memcpy(destination, prefix.data, prefix.length);
    }
    if (suffix.length != 0U) {
        (void)memcpy(destination + prefix.length, suffix.data, suffix.length);
    }
    destination[data_bytes] = '\0';
    block->snapshot.payload.data = (AetherStringView){destination, data_bytes};
    destination += data_bytes + 1U;
    block->snapshot.payload.label = copy_view(&destination, payload.label);
    block->last.payload.data = shared_data ? block->snapshot.payload.data :
        copy_view(&destination, fact.payload.data);
    block->last.payload.label = shared_label ? block->snapshot.payload.label :
        copy_view(&destination, fact.payload.label);
    *out_block = block;
    return AETHER_STATUS_OK;
}

void aether_message_free_block(MessageBlock* block)
{
    /* Byte storage follows the block in the same allocation. */
    free(block);
}
