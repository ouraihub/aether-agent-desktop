#include "aether/core/message.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define AETHER_MESSAGE_TEXT_CAPACITY 65536U

typedef struct MessageRecord {
    bool occupied;
    AetherMessageSnapshot snapshot;
} MessageRecord;

struct AetherMessageRegistry {
    size_t capacity;
    AetherSessionRegistry* sessions;
    MessageRecord* messages;
};

static MessageRecord* find_message(
    const AetherMessageRegistry* registry,
    AetherMessageId id
)
{
    size_t index;
    for (index = 0U; index < registry->capacity; ++index) {
        MessageRecord* record = &registry->messages[index];
        if (record->occupied && aether_id_equal(
                record->snapshot.message_id.id, id.id)) {
            return record;
        }
    }
    return NULL;
}

AetherStatus aether_message_registry_create(
    size_t capacity,
    AetherSessionRegistry* sessions,
    AetherMessageRegistry** out_registry
)
{
    AetherMessageRegistry* registry;
    if (out_registry == NULL || capacity == 0U || sessions == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_registry = NULL;
    registry = calloc(1U, sizeof(*registry));
    if (registry == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    registry->messages = calloc(capacity, sizeof(*registry->messages));
    if (registry->messages == NULL) {
        free(registry);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->capacity = capacity;
    registry->sessions = sessions;
    *out_registry = registry;
    return AETHER_STATUS_OK;
}

AetherStatus aether_message_registry_destroy(AetherMessageRegistry** registry)
{
    if (registry == NULL || *registry == NULL) return AETHER_STATUS_OK;
    free((*registry)->messages);
    free(*registry);
    *registry = NULL;
    return AETHER_STATUS_OK;
}

AetherStatus aether_message_create(
    AetherMessageRegistry* registry,
    AetherSessionId session_id,
    AetherMessageRole role,
    AetherMessageId* out_id
)
{
    size_t index;
    AetherSessionSnapshot session;
    if (out_id == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_id = (AetherMessageId){0};
    if (registry == NULL || role > AETHER_MESSAGE_ROLE_TOOL ||
        aether_session_query(registry->sessions, session_id, &session) !=
            AETHER_STATUS_OK || session.state != AETHER_SESSION_IDLE) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < registry->capacity; ++index) {
        MessageRecord* record = &registry->messages[index];
        if (!record->occupied) {
            AetherId id = aether_id_next();
            if (!aether_id_is_valid(id)) return AETHER_STATUS_INTERNAL_ERROR;
            record->snapshot = (AetherMessageSnapshot){0};
            record->snapshot.message_id = (AetherMessageId){id};
            record->snapshot.session_id = session_id;
            record->snapshot.role = role;
            record->snapshot.state = AETHER_MESSAGE_RECEIVING;
            record->snapshot.revision = 1U;
            record->occupied = true;
            *out_id = record->snapshot.message_id;
            return AETHER_STATUS_OK;
        }
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_message_query(
    const AetherMessageRegistry* registry,
    AetherMessageId id,
    AetherMessageSnapshot* out_snapshot
)
{
    MessageRecord* record;
    if (out_snapshot == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_snapshot = (AetherMessageSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_message(registry, id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    *out_snapshot = record->snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_message_append_text(
    AetherMessageRegistry* registry,
    AetherMessageId id,
    uint64_t cursor,
    AetherStringView text
)
{
    MessageRecord* record;
    size_t current_length;
    if (registry == NULL || !aether_id_is_valid(id.id) ||
        aether_string_view_validate(text) != AETHER_STATUS_OK ||
        (text.length != 0U && text.data == NULL)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_message(registry, id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    if (record->snapshot.state != AETHER_MESSAGE_RECEIVING) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (cursor <= record->snapshot.cursor) {
        return cursor == record->snapshot.cursor
            ? AETHER_STATUS_OK : AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (cursor != record->snapshot.cursor + 1U) {
        return AETHER_STATUS_PROTOCOL_ERROR;
    }
    current_length = strlen(record->snapshot.text);
    if (text.length > sizeof(record->snapshot.text) - current_length - 1U) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    (void)memcpy(record->snapshot.text + current_length, text.data, text.length);
    record->snapshot.text[current_length + text.length] = '\0';
    record->snapshot.cursor = cursor;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_message_replace_text(
    AetherMessageRegistry* registry,
    AetherMessageId id,
    uint64_t expected_revision,
    AetherStringView text
)
{
    MessageRecord* record;
    if (registry == NULL || !aether_id_is_valid(id.id) ||
        aether_string_view_validate(text) != AETHER_STATUS_OK ||
        text.length >= AETHER_MESSAGE_TEXT_CAPACITY) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_message(registry, id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    if (record->snapshot.state != AETHER_MESSAGE_RECEIVING ||
        record->snapshot.revision != expected_revision) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(record->snapshot.text, 0, sizeof(record->snapshot.text));
    (void)memcpy(record->snapshot.text, text.data, text.length);
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_message_finish(
    AetherMessageRegistry* registry,
    AetherMessageId id,
    AetherMessageState terminal_state
)
{
    MessageRecord* record;
    if (registry == NULL || !aether_id_is_valid(id.id) ||
        (terminal_state != AETHER_MESSAGE_COMPLETED &&
         terminal_state != AETHER_MESSAGE_INTERRUPTED &&
         terminal_state != AETHER_MESSAGE_FAILED)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_message(registry, id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    if (record->snapshot.state != AETHER_MESSAGE_RECEIVING) {
        return AETHER_STATUS_ALREADY_EXISTS;
    }
    record->snapshot.state = terminal_state;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}
