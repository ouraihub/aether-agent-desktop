#ifndef AETHER_RUNTIME_REGISTRY_INTERNAL_H
#define AETHER_RUNTIME_REGISTRY_INTERNAL_H

#include "aether/core/runtime_registry.h"

#include <stdbool.h>

typedef struct AetherProviderRecord {
    bool occupied;
    AetherProviderSnapshot snapshot;
} AetherProviderRecord;

typedef struct AetherRuntimeRecord {
    bool occupied;
    size_t references;
    AetherRuntimeSnapshot snapshot;
} AetherRuntimeRecord;

struct AetherRuntimeRegistry {
    size_t provider_capacity;
    size_t runtime_capacity;
    AetherProviderRecord* providers;
    AetherRuntimeRecord* runtimes;
};

struct AetherRuntimeRef {
    AetherRuntimeRegistry* registry;
    size_t index;
    uint64_t generation;
};

AetherProviderRecord* aether_registry_find_provider(
    const AetherRuntimeRegistry* registry, AetherProviderId id);
AetherRuntimeRecord* aether_registry_find_runtime(
    const AetherRuntimeRegistry* registry, AetherRuntimeId id);

#endif
