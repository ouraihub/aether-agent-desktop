#include "runtime_registry_internal.h"

#include <stdlib.h>
#include <string.h>

AetherProviderRecord* aether_registry_find_provider(
    const AetherRuntimeRegistry* registry, AetherProviderId id)
{
    for (size_t i = 0U; i < registry->provider_capacity; ++i) {
        AetherProviderRecord* entry = &registry->providers[i];
        if (entry->occupied && aether_id_equal(entry->snapshot.provider_id.id,
                                               id.id)) {
            return entry;
        }
    }
    return NULL;
}

AetherRuntimeRecord* aether_registry_find_runtime(
    const AetherRuntimeRegistry* registry, AetherRuntimeId id)
{
    for (size_t i = 0U; i < registry->runtime_capacity; ++i) {
        AetherRuntimeRecord* entry = &registry->runtimes[i];
        if (entry->occupied && aether_id_equal(entry->snapshot.runtime_id.id,
                                               id.id)) {
            return entry;
        }
    }
    return NULL;
}

AetherStatus aether_runtime_registry_create(size_t provider_capacity,
    size_t runtime_capacity, AetherRuntimeRegistry** out_registry)
{
    AetherRuntimeRegistry* registry;
    if (out_registry == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_registry = NULL;
    if (provider_capacity == 0U || runtime_capacity == 0U ||
        provider_capacity > SIZE_MAX / sizeof(AetherProviderRecord) ||
        runtime_capacity > SIZE_MAX / sizeof(AetherRuntimeRecord)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    registry = calloc(1U, sizeof(*registry));
    if (registry == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->providers = calloc(provider_capacity,
                                  sizeof(*registry->providers));
    if (registry->providers != NULL) {
        registry->runtimes = calloc(runtime_capacity,
                                     sizeof(*registry->runtimes));
    }
    if (registry->providers == NULL || registry->runtimes == NULL) {
        free(registry->providers);
        free(registry->runtimes);
        free(registry);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    registry->provider_capacity = provider_capacity;
    registry->runtime_capacity = runtime_capacity;
    *out_registry = registry;
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_registry_destroy(AetherRuntimeRegistry** registry)
{
    if (registry == NULL || *registry == NULL) {
        return AETHER_STATUS_OK;
    }
    for (size_t i = 0U; i < (*registry)->runtime_capacity; ++i) {
        if ((*registry)->runtimes[i].references != 0U) {
            return AETHER_STATUS_ALREADY_EXISTS;
        }
    }
    free((*registry)->providers);
    free((*registry)->runtimes);
    free(*registry);
    *registry = NULL;
    return AETHER_STATUS_OK;
}

static bool valid_text(AetherStringView view, size_t capacity)
{
    return view.data != NULL && view.length != 0U && view.length < capacity &&
           aether_string_view_validate(view) == AETHER_STATUS_OK &&
           memchr(view.data, '\0', view.length) == NULL;
}

AetherStatus aether_runtime_provider_register(AetherRuntimeRegistry* registry,
    AetherStringView name, AetherStringView version, AetherProviderId* out_id)
{
    AetherProviderRecord* free_entry = NULL;
    if (out_id == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_id = (AetherProviderId){0};
    if (registry == NULL || !valid_text(name, AETHER_PROVIDER_NAME_CAPACITY) ||
        !valid_text(version, AETHER_PROVIDER_VERSION_CAPACITY)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0U; i < registry->provider_capacity; ++i) {
        AetherProviderRecord* entry = &registry->providers[i];
        if (!entry->occupied) {
            free_entry = entry;
        } else if (strlen(entry->snapshot.name) == name.length &&
                   memcmp(entry->snapshot.name, name.data, name.length) == 0) {
            return AETHER_STATUS_ALREADY_EXISTS;
        }
    }
    if (free_entry == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    AetherId id = aether_id_next();
    if (!aether_id_is_valid(id)) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    free_entry->snapshot = (AetherProviderSnapshot){0};
    free_entry->snapshot.provider_id = (AetherProviderId){id};
    memcpy(free_entry->snapshot.name, name.data, name.length);
    memcpy(free_entry->snapshot.version, version.data, version.length);
    free_entry->occupied = true;
    *out_id = free_entry->snapshot.provider_id;
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_provider_query(
    const AetherRuntimeRegistry* registry, AetherProviderId id,
    AetherProviderSnapshot* out_snapshot)
{
    if (out_snapshot == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = (AetherProviderSnapshot){0};
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherProviderRecord* entry = aether_registry_find_provider(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    *out_snapshot = entry->snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_provider_remove(AetherRuntimeRegistry* registry,
    AetherProviderId id)
{
    if (registry == NULL || !aether_id_is_valid(id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherProviderRecord* entry = aether_registry_find_provider(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    for (size_t i = 0U; i < registry->runtime_capacity; ++i) {
        if (registry->runtimes[i].occupied && aether_id_equal(
                registry->runtimes[i].snapshot.provider_id.id, id.id)) {
            return AETHER_STATUS_ALREADY_EXISTS;
        }
    }
    *entry = (AetherProviderRecord){0};
    return AETHER_STATUS_OK;
}
