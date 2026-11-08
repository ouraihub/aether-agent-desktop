#ifndef AETHER_CORE_RUNTIME_REGISTRY_H
#define AETHER_CORE_RUNTIME_REGISTRY_H

#include "aether/foundation/id.h"
#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherRuntimeRegistry AetherRuntimeRegistry;
typedef struct AetherRuntimeRef AetherRuntimeRef;
typedef struct AetherProviderId { AetherId id; } AetherProviderId;
typedef struct AetherRuntimeId { AetherId id; } AetherRuntimeId;

#define AETHER_PROVIDER_NAME_CAPACITY 128U
#define AETHER_PROVIDER_VERSION_CAPACITY 64U
#define AETHER_RUNTIME_CAP_STREAM UINT64_C(1)
#define AETHER_RUNTIME_CAP_SESSION UINT64_C(2)
#define AETHER_RUNTIME_CAP_CANCEL UINT64_C(4)
#define AETHER_RUNTIME_CAP_RESUME UINT64_C(8)
#define AETHER_RUNTIME_CAP_ALL UINT64_C(15)

typedef struct AetherRuntimeCapabilities {
    uint64_t known;
    uint64_t supported;
} AetherRuntimeCapabilities;

typedef enum AetherRuntimeState {
    AETHER_RUNTIME_REGISTERED = 0,
    AETHER_RUNTIME_READY,
    AETHER_RUNTIME_UNAVAILABLE,
    AETHER_RUNTIME_CLOSING
} AetherRuntimeState;

typedef struct AetherProviderSnapshot {
    AetherProviderId provider_id;
    char name[AETHER_PROVIDER_NAME_CAPACITY];
    char version[AETHER_PROVIDER_VERSION_CAPACITY];
} AetherProviderSnapshot;

typedef struct AetherRuntimeSnapshot {
    AetherRuntimeId runtime_id;
    AetherProviderId provider_id;
    uint64_t generation;
    uint64_t revision;
    AetherRuntimeState state;
    AetherRuntimeCapabilities capabilities;
} AetherRuntimeSnapshot;

/* V1 in-memory domain registration, not a Runtime executor or Adapter.
 * Synchronous, no callbacks/IO/timeout/cancel; same-registry calls serialized.
 * Independent registries are independent. Capacities explicit, nonzero;
 * no hidden global default or implicit eviction. Construction allocates arrays;
 * registration/query/update/removal allocate nothing; acquire allocates a Ref.
 * Caller inputs borrowed only during registration and copied before success.
 * Provider names/versions: nonempty UTF-8, no NUL, content < buffer capacity.
 * Names are exact/case-sensitive unique within this registry; versions opaque.
 * Runtime identities process-local; generation starts at 1, reconnect increases
 * it and resets capabilities to unknown/Registered. Stale facts/refs return
 * INVALID_ARGUMENT; capabilities supported must be a subset of known/ALL.
 * Unknown object NOT_FOUND; duplicate provider ALREADY_EXISTS; exhausted arrays
 * OUT_OF_MEMORY; blocked removal/destroy ALREADY_EXISTS; invalid input
 * INVALID_ARGUMENT. Revision/generation exhaustion INTERNAL_ERROR, unchanged.
 * ID generation inherits Foundation's process-local scope (not persistent).
 * Registration is not availability: only explicit current-generation facts
 * can report Ready. Acquiring a non-Ready runtime returns NOT_INITIALIZED.
 * Snapshot outputs zeroed on failure; ownership never shared with caller.
 * begin_unregister idempotently closes the borrow gate, preserves tombstone;
 * finish_unregister requires Closing and zero refs; no external resources freed.
 * Ref owns a domain hold, not a Session/OS object; query rechecks generation.
 * Stale refs must release. NULL release is safe; released pointer is cleared.
 * Registry destroy takes ** and rejects outstanding refs without freeing it.
 * Caller must not copy owning ref/registry pointers to double-release them.
 */
AetherStatus aether_runtime_registry_create(size_t provider_capacity,
    size_t runtime_capacity, AetherRuntimeRegistry** out_registry);
AetherStatus aether_runtime_registry_destroy(AetherRuntimeRegistry** registry);
AetherStatus aether_runtime_provider_register(AetherRuntimeRegistry* registry,
    AetherStringView name, AetherStringView version, AetherProviderId* out_id);
AetherStatus aether_runtime_provider_query(
    const AetherRuntimeRegistry* registry, AetherProviderId id,
    AetherProviderSnapshot* out_snapshot);
AetherStatus aether_runtime_provider_remove(AetherRuntimeRegistry* registry,
    AetherProviderId id);
AetherStatus aether_runtime_register(AetherRuntimeRegistry* registry,
    AetherProviderId provider, AetherRuntimeId* out_id);
AetherStatus aether_runtime_query(const AetherRuntimeRegistry* registry,
    AetherRuntimeId id, AetherRuntimeSnapshot* out_snapshot);
AetherStatus aether_runtime_update(AetherRuntimeRegistry* registry,
    AetherRuntimeId id, uint64_t generation, AetherRuntimeState state,
    AetherRuntimeCapabilities capabilities);
AetherStatus aether_runtime_reconnect(AetherRuntimeRegistry* registry,
    AetherRuntimeId id, uint64_t expected_generation);
AetherStatus aether_runtime_begin_unregister(AetherRuntimeRegistry* registry,
    AetherRuntimeId id);
AetherStatus aether_runtime_finish_unregister(AetherRuntimeRegistry* registry,
    AetherRuntimeId id);
AetherStatus aether_runtime_acquire(AetherRuntimeRegistry* registry,
    AetherRuntimeId id, uint64_t generation, AetherRuntimeRef** out_ref);
AetherStatus aether_runtime_ref_query(const AetherRuntimeRef* reference,
    AetherRuntimeSnapshot* out_snapshot);
void aether_runtime_ref_release(AetherRuntimeRef** reference);

#ifdef __cplusplus
}
#endif

#endif
