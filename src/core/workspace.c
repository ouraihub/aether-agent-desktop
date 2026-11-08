#include "aether/core/workspace.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define AETHER_WORKSPACE_NAME_CAPACITY 128U
#define AETHER_WORKSPACE_PATH_CAPACITY 4096U
#define AETHER_AGENT_MODEL_CAPACITY 128U

typedef struct WorkspaceRecord {
    bool occupied;
    AetherWorkspaceSnapshot snapshot;
} WorkspaceRecord;

typedef struct AgentRecord {
    bool occupied;
    AetherAgentConfigSnapshot snapshot;
} AgentRecord;

struct AetherWorkspaceRegistry {
    size_t workspace_capacity;
    size_t agent_capacity;
    WorkspaceRecord* workspaces;
    AgentRecord* agents;
};

static bool valid_text(AetherStringView view, size_t capacity)
{
    return view.data != NULL && view.length != 0U && view.length < capacity &&
           aether_string_view_validate(view) == AETHER_STATUS_OK &&
           memchr(view.data, '\0', view.length) == NULL;
}

static WorkspaceRecord* find_workspace(
    const AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id
)
{
    size_t index;

    for (index = 0U; index < registry->workspace_capacity; ++index) {
        WorkspaceRecord* record = &registry->workspaces[index];
        if (record->occupied && aether_id_equal(
                record->snapshot.workspace_id.id, id.id)) {
            return record;
        }
    }
    return NULL;
}

static AgentRecord* find_agent(
    const AetherWorkspaceRegistry* registry,
    AetherAgentConfigId id
)
{
    size_t index;

    for (index = 0U; index < registry->agent_capacity; ++index) {
        AgentRecord* record = &registry->agents[index];
        if (record->occupied && aether_id_equal(
                record->snapshot.agent_config_id.id, id.id)) {
            return record;
        }
    }
    return NULL;
}

AetherStatus aether_workspace_registry_create(
    size_t workspace_capacity,
    size_t agent_capacity,
    AetherWorkspaceRegistry** out_registry
)
{
    AetherWorkspaceRegistry* registry;

    if (out_registry == NULL || workspace_capacity == 0U ||
        agent_capacity == 0U ||
        workspace_capacity > SIZE_MAX / sizeof(WorkspaceRecord) ||
        agent_capacity > SIZE_MAX / sizeof(AgentRecord)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_registry = NULL;
    registry = calloc(1U, sizeof(*registry));
    if (registry == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->workspaces = calloc(workspace_capacity,
                                  sizeof(*registry->workspaces));
    registry->agents = calloc(agent_capacity, sizeof(*registry->agents));
    if (registry->workspaces == NULL || registry->agents == NULL) {
        free(registry->workspaces);
        free(registry->agents);
        free(registry);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->workspace_capacity = workspace_capacity;
    registry->agent_capacity = agent_capacity;
    *out_registry = registry;
    return AETHER_STATUS_OK;
}

AetherStatus aether_workspace_registry_destroy(
    AetherWorkspaceRegistry** registry
)
{
    if (registry == NULL || *registry == NULL) {
        return AETHER_STATUS_OK;
    }
    free((*registry)->workspaces);
    free((*registry)->agents);
    free(*registry);
    *registry = NULL;
    return AETHER_STATUS_OK;
}

AetherStatus aether_workspace_create(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceInput input,
    AetherWorkspaceId* out_id
)
{
    size_t index;

    if (out_id == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_id = (AetherWorkspaceId){0};
    if (registry == NULL || !valid_text(input.display_name,
                                         AETHER_WORKSPACE_NAME_CAPACITY) ||
        !valid_text(input.root_path, AETHER_WORKSPACE_PATH_CAPACITY)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < registry->workspace_capacity; ++index) {
        WorkspaceRecord* record = &registry->workspaces[index];
        if (!record->occupied) {
            AetherId id = aether_id_next();
            if (!aether_id_is_valid(id)) {
                return AETHER_STATUS_INTERNAL_ERROR;
            }
            record->snapshot = (AetherWorkspaceSnapshot){0};
            record->snapshot.workspace_id = (AetherWorkspaceId){id};
            record->snapshot.revision = 1U;
            record->snapshot.state = AETHER_WORKSPACE_OPEN;
            (void)memcpy(record->snapshot.display_name, input.display_name.data,
                         input.display_name.length);
            (void)memcpy(record->snapshot.root_path, input.root_path.data,
                         input.root_path.length);
            record->occupied = true;
            *out_id = record->snapshot.workspace_id;
            return AETHER_STATUS_OK;
        }
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_workspace_query(
    const AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id,
    AetherWorkspaceSnapshot* out_snapshot
)
{
    WorkspaceRecord* record;

    if (out_snapshot == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = (AetherWorkspaceSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_workspace(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    *out_snapshot = record->snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_workspace_update_name(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id,
    uint64_t expected_revision,
    AetherStringView display_name
)
{
    WorkspaceRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id) ||
        !valid_text(display_name, AETHER_WORKSPACE_NAME_CAPACITY)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_workspace(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.state != AETHER_WORKSPACE_OPEN ||
        record->snapshot.revision != expected_revision) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(record->snapshot.display_name, 0,
                 sizeof(record->snapshot.display_name));
    (void)memcpy(record->snapshot.display_name, display_name.data,
                 display_name.length);
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_workspace_begin_close(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id,
    uint64_t expected_revision
)
{
    WorkspaceRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_workspace(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.state == AETHER_WORKSPACE_CLOSED ||
        record->snapshot.revision != expected_revision) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_WORKSPACE_CLOSING;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_workspace_finish_close(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id
)
{
    WorkspaceRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_workspace(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.state != AETHER_WORKSPACE_CLOSING) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->snapshot.state = AETHER_WORKSPACE_CLOSED;
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_agent_config_create(
    AetherWorkspaceRegistry* registry,
    AetherAgentConfigInput input,
    AetherAgentConfigId* out_id
)
{
    size_t index;

    if (out_id == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_id = (AetherAgentConfigId){0};
    if (registry == NULL || !aether_id_is_valid(input.provider_id.id) ||
        !valid_text(input.display_name, AETHER_WORKSPACE_NAME_CAPACITY) ||
        !valid_text(input.model, AETHER_AGENT_MODEL_CAPACITY)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < registry->agent_capacity; ++index) {
        AgentRecord* record = &registry->agents[index];
        if (!record->occupied) {
            AetherId id = aether_id_next();
            if (!aether_id_is_valid(id)) {
                return AETHER_STATUS_INTERNAL_ERROR;
            }
            record->snapshot = (AetherAgentConfigSnapshot){0};
            record->snapshot.agent_config_id = (AetherAgentConfigId){id};
            record->snapshot.provider_id = input.provider_id;
            record->snapshot.revision = 1U;
            (void)memcpy(record->snapshot.display_name,
                         input.display_name.data, input.display_name.length);
            (void)memcpy(record->snapshot.model, input.model.data,
                         input.model.length);
            record->occupied = true;
            *out_id = record->snapshot.agent_config_id;
            return AETHER_STATUS_OK;
        }
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_agent_config_query(
    const AetherWorkspaceRegistry* registry,
    AetherAgentConfigId id,
    AetherAgentConfigSnapshot* out_snapshot
)
{
    AgentRecord* record;

    if (out_snapshot == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = (AetherAgentConfigSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_agent(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    *out_snapshot = record->snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_agent_config_update_model(
    AetherWorkspaceRegistry* registry,
    AetherAgentConfigId id,
    uint64_t expected_revision,
    AetherStringView model
)
{
    AgentRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id) ||
        !valid_text(model, AETHER_AGENT_MODEL_CAPACITY)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_agent(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (record->snapshot.revision != expected_revision) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(record->snapshot.model, 0, sizeof(record->snapshot.model));
    (void)memcpy(record->snapshot.model, model.data, model.length);
    ++record->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_agent_config_remove(
    AetherWorkspaceRegistry* registry,
    AetherAgentConfigId id
)
{
    AgentRecord* record;

    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_agent(registry, id);
    if (record == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    *record = (AgentRecord){0};
    return AETHER_STATUS_OK;
}
