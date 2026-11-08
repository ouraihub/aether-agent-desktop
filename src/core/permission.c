#include "aether/core/permission.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PERMISSION_TOOL_CAPACITY 128U
#define PERMISSION_TARGET_CAPACITY 512U

typedef struct PermissionRecord {
    bool occupied;
    AetherPermissionId id;
    AetherWorkspaceId workspace_id;
    AetherPermissionLevel level;
    size_t tool_length;
    size_t target_length;
    char tool_id[PERMISSION_TOOL_CAPACITY];
    char target[PERMISSION_TARGET_CAPACITY];
    uint64_t policy_revision;
    AetherPermissionDecision decision;
    bool resolved;
    bool remembered;
} PermissionRecord;

typedef struct WorkspacePolicy {
    bool occupied;
    AetherWorkspaceId workspace_id;
    AetherPermissionMode mode;
    uint64_t revision;
} WorkspacePolicy;

struct AetherPermissionRegistry {
    size_t capacity;
    AetherWorkspaceRegistry* workspaces;
    PermissionRecord* permissions;
    WorkspacePolicy* policies;
};

static bool valid_text(AetherStringView value, size_t capacity)
{
    return value.data != NULL && value.length != 0U && value.length < capacity &&
           aether_string_view_validate(value) == AETHER_STATUS_OK &&
           memchr(value.data, '\0', value.length) == NULL;
}

static WorkspacePolicy* find_policy(
    AetherPermissionRegistry* registry,
    AetherWorkspaceId id
)
{
    size_t index;
    for (index = 0U; index < registry->capacity; ++index) {
        if (registry->policies[index].occupied && aether_id_equal(
                registry->policies[index].workspace_id.id, id.id)) {
            return &registry->policies[index];
        }
    }
    return NULL;
}

static PermissionRecord* find_permission(
    AetherPermissionRegistry* registry,
    AetherPermissionId id
)
{
    size_t index;
    for (index = 0U; index < registry->capacity; ++index) {
        if (registry->permissions[index].occupied && aether_id_equal(
                registry->permissions[index].id.id, id.id)) {
            return &registry->permissions[index];
        }
    }
    return NULL;
}

static bool same_input(
    const PermissionRecord* record,
    AetherPermissionInput input
)
{
    return aether_id_equal(record->workspace_id.id, input.workspace_id.id) &&
           record->level == input.level &&
           record->tool_length == input.tool_id.length &&
           record->target_length == input.target.length &&
           memcmp(record->tool_id, input.tool_id.data, input.tool_id.length) == 0 &&
           memcmp(record->target, input.target.data, input.target.length) == 0;
}

static AetherStatus require_open_workspace(
    AetherPermissionRegistry* registry,
    AetherWorkspaceId id
)
{
    AetherWorkspaceSnapshot snapshot;
    AetherStatus status = aether_workspace_query(registry->workspaces,
                                                id, &snapshot);
    if (status != AETHER_STATUS_OK) return status;
    return snapshot.state == AETHER_WORKSPACE_OPEN
        ? AETHER_STATUS_OK : AETHER_STATUS_NOT_INITIALIZED;
}

AetherStatus aether_permission_registry_create(
    size_t capacity,
    AetherWorkspaceRegistry* workspaces,
    AetherPermissionRegistry** out_registry
)
{
    AetherPermissionRegistry* registry;
    if (out_registry == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_registry = NULL;
    if (capacity == 0U || workspaces == NULL ||
        capacity > SIZE_MAX / sizeof(PermissionRecord) ||
        capacity > SIZE_MAX / sizeof(WorkspacePolicy)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    registry = calloc(1U, sizeof(*registry));
    if (registry == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    registry->permissions = calloc(capacity, sizeof(*registry->permissions));
    registry->policies = calloc(capacity, sizeof(*registry->policies));
    if (registry->permissions == NULL || registry->policies == NULL) {
        free(registry->permissions);
        free(registry->policies);
        free(registry);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->capacity = capacity;
    registry->workspaces = workspaces;
    *out_registry = registry;
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_registry_destroy(
    AetherPermissionRegistry** registry
)
{
    if (registry == NULL || *registry == NULL) return AETHER_STATUS_OK;
    free((*registry)->permissions);
    free((*registry)->policies);
    free(*registry);
    *registry = NULL;
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_set_mode(
    AetherPermissionRegistry* registry,
    AetherWorkspaceId workspace_id,
    AetherPermissionMode mode
)
{
    WorkspacePolicy* policy;
    AetherStatus status;
    if (registry == NULL || !aether_id_is_valid(workspace_id.id) ||
        (mode != AETHER_PERMISSION_MODE_YOLO &&
         mode != AETHER_PERMISSION_MODE_WRITE &&
         mode != AETHER_PERMISSION_MODE_ALWAYS_ASK &&
         mode != AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = require_open_workspace(registry, workspace_id);
    if (status != AETHER_STATUS_OK) return status;
    policy = find_policy(registry, workspace_id);
    if (policy == NULL) {
        size_t index;
        for (index = 0U; index < registry->capacity; ++index) {
            if (!registry->policies[index].occupied) {
                policy = &registry->policies[index];
                policy->occupied = true;
                policy->workspace_id = workspace_id;
                policy->revision = 1U;
                break;
            }
        }
    }
    if (policy == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    if (policy->revision == UINT64_MAX) return AETHER_STATUS_INTERNAL_ERROR;
    policy->mode = mode;
    ++policy->revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_evaluate(
    AetherPermissionRegistry* registry,
    AetherPermissionInput input,
    AetherPermissionResult* out_result
)
{
    WorkspacePolicy* policy;
    size_t index;
    AetherStatus status;

    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_result = (AetherPermissionResult){.decision = AETHER_PERMISSION_DENY};
    if (registry == NULL || !aether_id_is_valid(input.workspace_id.id) ||
        !valid_text(input.tool_id, PERMISSION_TOOL_CAPACITY) ||
        !valid_text(input.target, PERMISSION_TARGET_CAPACITY) ||
        (input.level != AETHER_PERMISSION_LEVEL_READ &&
         input.level != AETHER_PERMISSION_LEVEL_WRITE &&
         input.level != AETHER_PERMISSION_LEVEL_EXEC)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = require_open_workspace(registry, input.workspace_id);
    if (status != AETHER_STATUS_OK) return status;
    policy = find_policy(registry, input.workspace_id);
    if (policy == NULL) return AETHER_STATUS_NOT_INITIALIZED;
    /* Failure outputs remain non-authorizing, including a full queue. */
    if (policy->mode == AETHER_PERMISSION_MODE_YOLO ||
        (policy->mode == AETHER_PERMISSION_MODE_WRITE &&
         input.level != AETHER_PERMISSION_LEVEL_EXEC)) {
        out_result->policy_revision = policy->revision;
        out_result->decision = AETHER_PERMISSION_ALLOW;
        return AETHER_STATUS_OK;
    }
    if (policy->mode == AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED) {
        for (index = 0U; index < registry->capacity; ++index) {
            PermissionRecord* record = &registry->permissions[index];
            if (record->occupied && record->resolved && record->remembered &&
                record->decision == AETHER_PERMISSION_ALLOW &&
                record->policy_revision == policy->revision &&
                same_input(record, input)) {
                out_result->policy_revision = policy->revision;
                out_result->decision = AETHER_PERMISSION_ALLOW;
                out_result->permission_id = record->id;
                out_result->from_memory = true;
                return AETHER_STATUS_OK;
            }
        }
        if (input.level == AETHER_PERMISSION_LEVEL_READ) {
            out_result->policy_revision = policy->revision;
            out_result->decision = AETHER_PERMISSION_ALLOW;
            return AETHER_STATUS_OK;
        }
    }
    for (index = 0U; index < registry->capacity; ++index) {
        PermissionRecord* record = &registry->permissions[index];
        if (!record->occupied) {
            AetherId id = aether_id_next();
            if (!aether_id_is_valid(id)) return AETHER_STATUS_INTERNAL_ERROR;
            record->occupied = true;
            record->id = (AetherPermissionId){id};
            record->workspace_id = input.workspace_id;
            record->level = input.level;
            record->tool_length = input.tool_id.length;
            record->target_length = input.target.length;
            (void)memcpy(record->tool_id, input.tool_id.data,
                         input.tool_id.length);
            (void)memcpy(record->target, input.target.data, input.target.length);
            record->policy_revision = policy->revision;
            record->decision = AETHER_PERMISSION_ASK;
            out_result->policy_revision = policy->revision;
            out_result->decision = AETHER_PERMISSION_ASK;
            out_result->permission_id = record->id;
            return AETHER_STATUS_OK;
        }
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_permission_resolve(
    AetherPermissionRegistry* registry,
    AetherPermissionId permission_id,
    AetherPermissionDecision decision,
    bool remember
)
{
    PermissionRecord* record;
    WorkspacePolicy* policy;
    AetherStatus status;
    if (registry == NULL || !aether_id_is_valid(permission_id.id) ||
        (decision != AETHER_PERMISSION_ALLOW &&
         decision != AETHER_PERMISSION_DENY)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_permission(registry, permission_id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    if (record->resolved) return AETHER_STATUS_ALREADY_EXISTS;
    status = require_open_workspace(registry, record->workspace_id);
    if (status != AETHER_STATUS_OK) return status;
    policy = find_policy(registry, record->workspace_id);
    if (policy == NULL || policy->revision != record->policy_revision) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record->decision = decision;
    record->remembered = remember && decision == AETHER_PERMISSION_ALLOW;
    record->resolved = true;
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_revoke(
    AetherPermissionRegistry* registry,
    AetherPermissionId permission_id
)
{
    PermissionRecord* record;
    if (registry == NULL || !aether_id_is_valid(permission_id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    record = find_permission(registry, permission_id);
    if (record == NULL) return AETHER_STATUS_NOT_FOUND;
    *record = (PermissionRecord){0};
    return AETHER_STATUS_OK;
}
