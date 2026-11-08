#include "permission_internal.h"
#include <stdlib.h>

AetherPermissionLimits aether_permission_default_limits(void)
{
    return (AetherPermissionLimits){64U, 256U, 65536U, 128U, 64U};
}

AetherStatus aether_permission_registry_create(
    AetherPermissionLimits limits, AetherWorkspaceRegistry* workspaces,
    AetherSessionRegistry* sessions, AetherRuntimeRegistry* runtimes,
    AetherPermissionRegistry** out_registry
)
{
    AetherPermissionRegistry* registry;
    if (out_registry == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_registry = NULL;
    if (workspaces == NULL || sessions == NULL || runtimes == NULL ||
        limits.waits == 0U || limits.waits > 64U || limits.memories == 0U ||
        limits.memories > 256U || limits.memory_bytes == 0U ||
        limits.memory_bytes > 65536U || limits.rules == 0U || limits.rules > 128U ||
        limits.modes == 0U || limits.modes > 64U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    registry = calloc(1U, sizeof(*registry));
    if (registry == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    registry->limits = limits;
    registry->workspaces = workspaces;
    registry->sessions = sessions;
    registry->runtimes = runtimes;
    registry->revision = 1U;
    registry->modes = calloc(limits.modes, sizeof(*registry->modes));
    registry->rules = calloc(limits.rules, sizeof(*registry->rules));
    registry->waits = calloc(limits.waits, sizeof(*registry->waits));
    registry->memories = calloc(limits.memories, sizeof(*registry->memories));
    if (registry->modes == NULL || registry->rules == NULL ||
        registry->waits == NULL || registry->memories == NULL) {
        (void)aether_permission_registry_destroy(&registry);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    *out_registry = registry;
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_registry_destroy(AetherPermissionRegistry** registry)
{
    size_t index;
    if (registry == NULL || *registry == NULL) return AETHER_STATUS_OK;
    if ((*registry)->waits != NULL) {
        for (index = 0U; index < (*registry)->limits.waits; ++index) {
            free((*registry)->waits[index].storage);
        }
    }
    if ((*registry)->memories != NULL) {
        for (index = 0U; index < (*registry)->limits.memories; ++index) {
            free((*registry)->memories[index]);
        }
    }
    free((*registry)->memories);
    free((*registry)->waits);
    free((*registry)->rules);
    free((*registry)->modes);
    free(*registry);
    *registry = NULL;
    return AETHER_STATUS_OK;
}

PermissionWait* aether_permission_find_wait(
    AetherPermissionRegistry* registry, AetherPermissionId id
)
{
    size_t index;
    for (index = 0U; index < registry->limits.waits; ++index) {
        PermissionWait* wait = &registry->waits[index];
        if (wait->occupied && aether_id_equal(wait->snapshot.permission_id.id,
                                             id.id)) return wait;
    }
    return NULL;
}

AetherStatus aether_permission_refresh(
    AetherPermissionRegistry* registry, PermissionWait* wait, uint64_t now_ns
)
{
    AetherStatus status;
    if (wait->snapshot.state != AETHER_PERMISSION_WAITING &&
        wait->snapshot.state != AETHER_PERMISSION_APPROVED) return AETHER_STATUS_OK;
    if (now_ns >= wait->snapshot.expires_ns) {
        wait->snapshot.state = AETHER_PERMISSION_INVALIDATED;
        return AETHER_STATUS_TIMEOUT;
    }
    status = aether_permission_validate_input(registry, wait->input);
    if (status != AETHER_STATUS_OK ||
        registry->revision != wait->snapshot.policy_revision) {
        wait->snapshot.state = AETHER_PERMISSION_INVALIDATED;
        return status != AETHER_STATUS_OK ? status : AETHER_STATUS_INVALID_ARGUMENT;
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_query(
    AetherPermissionRegistry* registry, AetherPermissionId id, uint64_t now_ns,
    AetherPermissionSnapshot* out_snapshot
)
{
    PermissionWait* wait;
    if (out_snapshot == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_snapshot = (AetherPermissionSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    wait = aether_permission_find_wait(registry, id);
    if (wait == NULL) return AETHER_STATUS_NOT_FOUND;
    (void)aether_permission_refresh(registry, wait, now_ns);
    *out_snapshot = wait->snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_revoke(
    AetherPermissionRegistry* registry, AetherPermissionId id
)
{
    PermissionWait* wait;
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    wait = aether_permission_find_wait(registry, id);
    if (wait == NULL) return AETHER_STATUS_NOT_FOUND;
    wait->snapshot.state = AETHER_PERMISSION_INVALIDATED;
    aether_permission_drop_memories(registry, (AetherSessionId){0}, id);
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_release(
    AetherPermissionRegistry* registry, AetherPermissionId id
)
{
    PermissionWait* wait;
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    wait = aether_permission_find_wait(registry, id);
    if (wait == NULL) return AETHER_STATUS_NOT_FOUND;
    if (wait->snapshot.state == AETHER_PERMISSION_WAITING) {
        return AETHER_STATUS_ALREADY_EXISTS;
    }
    free(wait->storage);
    *wait = (PermissionWait){0};
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_close_session(
    AetherPermissionRegistry* registry, AetherSessionId session
)
{
    size_t index;
    if (registry == NULL || !aether_id_is_valid(session.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0U; index < registry->limits.waits; ++index) {
        PermissionWait* wait = &registry->waits[index];
        if (wait->occupied && aether_id_equal(wait->input.scope.session_id.id,
                                             session.id)) {
            wait->snapshot.state = AETHER_PERMISSION_INVALIDATED;
        }
    }
    aether_permission_drop_memories(registry, session, (AetherPermissionId){0});
    return AETHER_STATUS_OK;
}
