#include "runtime_registry_internal.h"

#include <stdlib.h>

AetherStatus aether_runtime_acquire(AetherRuntimeRegistry* registry,
    AetherRuntimeId id, uint64_t generation, AetherRuntimeRef** out_ref)
{
    if (out_ref == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_ref = NULL;
    if (registry == NULL || !aether_id_is_valid(id.id) || generation == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherRuntimeRecord* entry = aether_registry_find_runtime(registry, id);
    if (entry == NULL) {
        return AETHER_STATUS_NOT_FOUND;
    }
    if (entry->snapshot.generation != generation) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (entry->snapshot.state != AETHER_RUNTIME_READY) {
        return AETHER_STATUS_NOT_INITIALIZED;
    }
    if (entry->references == SIZE_MAX) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    AetherRuntimeRef* reference = malloc(sizeof(*reference));
    if (reference == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    reference->registry = registry;
    reference->index = (size_t)(entry - registry->runtimes);
    reference->generation = generation;
    ++entry->references;
    *out_ref = reference;
    return AETHER_STATUS_OK;
}

AetherStatus aether_runtime_ref_query(const AetherRuntimeRef* reference,
    AetherRuntimeSnapshot* out_snapshot)
{
    if (out_snapshot == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = (AetherRuntimeSnapshot){0};
    if (reference == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    AetherRuntimeRecord* entry =
        &reference->registry->runtimes[reference->index];
    if (entry->snapshot.generation != reference->generation) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = entry->snapshot;
    return AETHER_STATUS_OK;
}

void aether_runtime_ref_release(AetherRuntimeRef** reference)
{
    if (reference == NULL || *reference == NULL) {
        return;
    }
    --(*reference)->registry->runtimes[(*reference)->index].references;
    free(*reference);
    *reference = NULL;
}
