#include "runtime_registry_internal.h"

static bool valid_capabilities(AetherRuntimeCapabilities capabilities)
{
    return (capabilities.known & ~AETHER_RUNTIME_CAP_ALL) == 0U &&
           (capabilities.supported & ~capabilities.known) == 0U;
}

AetherStatus aether_runtime_register(AetherRuntimeRegistry* registry,
    AetherProviderId provider, AetherRuntimeId* out_id)
{
    if (out_id == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_id = (AetherRuntimeId){0};
    if (registry == NULL || !aether_id_is_valid(provider.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (aether_registry_find_provider(registry, provider) == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    for (size_t i = 0U; i < registry->runtime_capacity; ++i) {
        AetherRuntimeRecord* entry = &registry->runtimes[i];
        if (entry->occupied) {
            continue;
        }
        AetherId id = aether_id_next();
        if (!aether_id_is_valid(id)) {
            return AETHER_STATUS_INTERNAL_ERROR;
        }
        *entry = (AetherRuntimeRecord){0};
        entry->snapshot.runtime_id = (AetherRuntimeId){id};
        entry->snapshot.provider_id = provider;
        entry->snapshot.generation = 1U;
        entry->snapshot.revision = 1U;
        entry->snapshot.state = AETHER_RUNTIME_REGISTERED;
        entry->occupied = true;
        *out_id = entry->snapshot.runtime_id;
        return AETHER_STATUS_OK;
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_runtime_query(const AetherRuntimeRegistry* registry,
    AetherRuntimeId id, AetherRuntimeSnapshot* out_snapshot)
{
    if (out_snapshot == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = (AetherRuntimeSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherRuntimeRecord* entry = aether_registry_find_runtime(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    *out_snapshot = entry->snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_update(AetherRuntimeRegistry* registry,
    AetherRuntimeId id, uint64_t generation, AetherRuntimeState state,
    AetherRuntimeCapabilities capabilities)
{
    if (registry == NULL || !aether_id_is_valid(id.id) || generation == 0U ||
        (state != AETHER_RUNTIME_READY && state != AETHER_RUNTIME_UNAVAILABLE) ||
        !valid_capabilities(capabilities)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherRuntimeRecord* entry = aether_registry_find_runtime(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    AetherRuntimeSnapshot* snapshot = &entry->snapshot;
    if (snapshot->generation != generation ||
        snapshot->state == AETHER_RUNTIME_CLOSING) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (snapshot->state == state &&
        snapshot->capabilities.known == capabilities.known &&
        snapshot->capabilities.supported == capabilities.supported) {
        return AETHER_STATUS_OK;
    }
    if (snapshot->revision == UINT64_MAX) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    snapshot->state = state;
    snapshot->capabilities = capabilities;
    ++snapshot->revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_reconnect(AetherRuntimeRegistry* registry,
    AetherRuntimeId id, uint64_t expected_generation)
{
    if (registry == NULL || !aether_id_is_valid(id.id) ||
        expected_generation == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherRuntimeRecord* entry = aether_registry_find_runtime(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    AetherRuntimeSnapshot* snapshot = &entry->snapshot;
    if (snapshot->generation != expected_generation ||
        snapshot->state == AETHER_RUNTIME_CLOSING) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (snapshot->generation == UINT64_MAX || snapshot->revision == UINT64_MAX) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    ++snapshot->generation;
    ++snapshot->revision;
    snapshot->state = AETHER_RUNTIME_REGISTERED;
    snapshot->capabilities = (AetherRuntimeCapabilities){0};
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_begin_unregister(AetherRuntimeRegistry* registry,
    AetherRuntimeId id)
{
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherRuntimeRecord* entry = aether_registry_find_runtime(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (entry->snapshot.state == AETHER_RUNTIME_CLOSING) {
        return AETHER_STATUS_OK;
    }
    if (entry->snapshot.revision == UINT64_MAX) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    entry->snapshot.state = AETHER_RUNTIME_CLOSING;
    ++entry->snapshot.revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_finish_unregister(AetherRuntimeRegistry* registry,
    AetherRuntimeId id)
{
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherRuntimeRecord* entry = aether_registry_find_runtime(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (entry->snapshot.state != AETHER_RUNTIME_CLOSING) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (entry->references != 0U) {
        return AETHER_STATUS_ALREADY_EXISTS;
    }
    *entry = (AetherRuntimeRecord){0};
    return AETHER_STATUS_OK;
}
