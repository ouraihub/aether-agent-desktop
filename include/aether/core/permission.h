#ifndef AETHER_CORE_PERMISSION_H
#define AETHER_CORE_PERMISSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aether/core/workspace.h"
#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherPermissionRegistry AetherPermissionRegistry;
typedef struct AetherPermissionId { AetherId id; } AetherPermissionId;

typedef enum AetherPermissionMode {
    AETHER_PERMISSION_MODE_YOLO = 0,
    AETHER_PERMISSION_MODE_WRITE,
    AETHER_PERMISSION_MODE_ALWAYS_ASK,
    AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED
} AetherPermissionMode;

typedef enum AetherPermissionLevel {
    AETHER_PERMISSION_LEVEL_READ = 0,
    AETHER_PERMISSION_LEVEL_WRITE,
    AETHER_PERMISSION_LEVEL_EXEC
} AetherPermissionLevel;

typedef enum AetherPermissionDecision {
    AETHER_PERMISSION_ALLOW = 0,
    AETHER_PERMISSION_DENY,
    AETHER_PERMISSION_ASK
} AetherPermissionDecision;

typedef struct AetherPermissionInput {
    AetherWorkspaceId workspace_id;
    AetherStringView tool_id;
    AetherStringView target;
    AetherPermissionLevel level;
} AetherPermissionInput;

typedef struct AetherPermissionResult {
    AetherPermissionDecision decision;
    AetherPermissionId permission_id;
    uint64_t policy_revision;
    bool from_memory;
} AetherPermissionResult;

/*
 * In-memory development API; not a production execution authorization gate.
 * The caller serializes this registry and its borrowed Workspace registry;
 * Workspace must outlive it. No IO, callbacks, deadlines or async cancellation.
 * Tool/target are nonempty UTF-8 views without embedded NUL, shorter than
 * 128/512 bytes. NUL termination is not required. evaluate copies accepted
 * Ask input; no caller pointers survive a return. Matching is byte-exact,
 * not path canonicalization, directory matching or OS permission enforcement.
 * Modes and levels must be named enum values; resolve accepts Allow/Deny only.
 * set_mode/evaluate/resolve require an existing Open Workspace. Unknown IDs
 * return NotFound; Closing/Closed return NotInitialized. resolve also rejects
 * changed policy revision (InvalidArgument) and repeat answers (AlreadyExists).
 * Revoke remains available after close. It frees a slot, never reuses its ID.
 * Slots remain until revoke/destroy; no auto-eviction. Capacity/allocation
 * exhaustion is OutOfMemory; invalid/overflowing capacity is InvalidArgument.
 * create clears a non-NULL output on failure. evaluate returns Deny, invalid
 * ID, revision zero and from_memory=false on every failure; status must still
 * be checked. destroy accepts NULL/already-cleared handles.
 * A resolved answer is not an execution-consumption token. Full rule/scope,
 * expiry and execution revalidation semantics remain tracked by APP-05.
 */
AetherStatus aether_permission_registry_create(
    size_t capacity,
    AetherWorkspaceRegistry* workspaces,
    AetherPermissionRegistry** out_registry
);
AetherStatus aether_permission_registry_destroy(
    AetherPermissionRegistry** registry
);
AetherStatus aether_permission_set_mode(
    AetherPermissionRegistry* registry,
    AetherWorkspaceId workspace_id,
    AetherPermissionMode mode
);
AetherStatus aether_permission_evaluate(
    AetherPermissionRegistry* registry,
    AetherPermissionInput input,
    AetherPermissionResult* out_result
);
AetherStatus aether_permission_resolve(
    AetherPermissionRegistry* registry,
    AetherPermissionId permission_id,
    AetherPermissionDecision decision,
    bool remember
);
AetherStatus aether_permission_revoke(
    AetherPermissionRegistry* registry,
    AetherPermissionId permission_id
);

#ifdef __cplusplus
}
#endif

#endif
