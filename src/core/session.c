#include "aether/core/session.h"

#include <stdbool.h>
#include <stdlib.h>

#define AETHER_SESSION_MAX_TURNS 1U

typedef struct SessionRecord {
    bool occupied;
    AetherSessionSnapshot snapshot;
} SessionRecord;

struct AetherSessionRegistry {
    size_t capacity;
    AetherWorkspaceRegistry* workspaces;
    AetherRuntimeRegistry* runtimes;
    SessionRecord* sessions;
};

static SessionRecord* find_session(
    const AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    size_t index;

    for (index = 0U; index < registry->capacity; ++index) {
        SessionRecord* record = &registry->sessions[index];
        if (record->occupied && aether_id_equal(
                record->snapshot.session_id.id, id.id)) {
            return record;
        }
    }
    return NULL;
}

static bool session_accepts_domain_state(AetherSessionState state)
{
    return state != AETHER_SESSION_CLOSING &&
           state != AETHER_SESSION_CLOSED &&
           state != AETHER_SESSION_FAILED;
}

AetherStatus aether_session_registry_create(
    size_t capacity,
    AetherWorkspaceRegistry* workspaces,
    AetherRuntimeRegistry* runtimes,
    AetherSessionRegistry** out_registry
)
{
    AetherSessionRegistry* registry;

    if (out_registry == NULL || capacity == 0U || workspaces == NULL ||
        runtimes == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_registry = NULL;
    registry = calloc(1U, sizeof(*registry));
    if (registry == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->sessions = calloc(capacity, sizeof(*registry->sessions));
    if (registry->sessions == NULL) {
        free(registry);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->capacity = capacity;
    registry->workspaces = workspaces;
    registry->runtimes = runtimes;
    *out_registry = registry;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_registry_destroy(
    AetherSessionRegistry** registry
)
{
    if (registry == NULL || *registry == NULL) {
        return AETHER_STATUS_OK;
    }
    free((*registry)->sessions);
    free(*registry);
    *registry = NULL;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_create(
    AetherSessionRegistry* registry,
    AetherWorkspaceId workspace_id,
    AetherAgentConfigId agent_config_id,
    AetherRuntimeId runtime_id,
    uint64_t runtime_generation,
    AetherSessionId* out_id
)
{
    size_t index;
    AetherWorkspaceSnapshot workspace;
    AetherAgentConfigSnapshot agent;
    AetherRuntimeSnapshot runtime;

    if (out_id == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_id = (AetherSessionId){0};
    if (registry == NULL || runtime_generation == 0U ||
        aether_workspace_query(registry->workspaces, workspace_id, &workspace) !=
            AETHER_STATUS_OK ||
        workspace.state != AETHER_WORKSPACE_OPEN ||
        aether_agent_config_query(registry->workspaces, agent_config_id, &agent) !=
            AETHER_STATUS_OK ||
        aether_runtime_query(registry->runtimes, runtime_id, &runtime) !=
            AETHER_STATUS_OK || runtime.generation != runtime_generation ||
        runtime.state != AETHER_RUNTIME_READY) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < registry->capacity; ++index) {
        SessionRecord* record = &registry->sessions[index];
        if (!record->occupied) {
            AetherId id = aether_id_next();
            if (!aether_id_is_valid(id)) {
                return AETHER_STATUS_INTERNAL_ERROR;
            }
            record->snapshot = (AetherSessionSnapshot){0};
            record->snapshot.session_id = (AetherSessionId){id};
            record->snapshot.workspace_id = workspace_id;
            record->snapshot.agent_config_id = agent_config_id;
            record->snapshot.runtime_id = runtime_id;
            record->snapshot.runtime_generation = runtime_generation;
            record->snapshot.agent_revision = agent.revision;
            record->snapshot.revision = 1U;
            record->snapshot.state = AETHER_SESSION_CREATED;
            record->occupied = true;
            *out_id = record->snapshot.session_id;
            return AETHER_STATUS_OK;
        }
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_session_initialize(
    AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    SessionRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.state != AETHER_SESSION_CREATED) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_SESSION_IDLE;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_start_turn(
    AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    SessionRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.state != AETHER_SESSION_IDLE ||
        record->snapshot.active_turns >= AETHER_SESSION_MAX_TURNS) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_SESSION_ACTIVE;
    record->snapshot.active_turns = 1U;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_finish_turn(
    AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    SessionRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.state != AETHER_SESSION_ACTIVE ||
        record->snapshot.active_turns == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.active_turns = 0U;
    record->snapshot.state = AETHER_SESSION_IDLE;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_mark_runtime_unavailable(
    AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    SessionRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (!session_accepts_domain_state(record->snapshot.state)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_SESSION_RUNTIME_UNAVAILABLE;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_mark_interrupted(
    AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    SessionRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (!session_accepts_domain_state(record->snapshot.state)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_SESSION_INTERRUPTED;
    record->snapshot.active_turns = 0U;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_begin_close(
    AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    SessionRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (!session_accepts_domain_state(record->snapshot.state)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_SESSION_CLOSING;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_finish_close(
    AetherSessionRegistry* registry,
    AetherSessionId id
)
{
    SessionRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.state != AETHER_SESSION_CLOSING ||
        record->snapshot.active_turns != 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_SESSION_CLOSED;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_session_query(
    const AetherSessionRegistry* registry,
    AetherSessionId id,
    AetherSessionSnapshot* out_snapshot
)
{
    const SessionRecord* record;

    if (out_snapshot == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = (AetherSessionSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_session(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    *out_snapshot = record->snapshot;
    return AETHER_STATUS_OK;
}
