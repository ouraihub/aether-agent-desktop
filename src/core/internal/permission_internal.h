#ifndef AETHER_PERMISSION_INTERNAL_H
#define AETHER_PERMISSION_INTERNAL_H
#include "aether/core/permission.h"

typedef struct PermissionModeEntry {
    bool occupied;
    AetherWorkspaceId workspace;
    AetherSessionId session;
    AetherPermissionMode mode;
} PermissionModeEntry;

typedef struct PermissionRuleEntry {
    bool occupied;
    AetherPermissionRule rule;
    char tool[128];
    char subtool[128];
} PermissionRuleEntry;

typedef struct PermissionWait {
    bool occupied;
    AetherPermissionSnapshot snapshot;
    AetherPermissionInput input;
    void* storage;
    size_t storage_bytes;
} PermissionWait;

typedef struct PermissionMemory {
    AetherPermissionId origin;
    AetherApprovalScope kind;
    AetherPermissionInput input;
    size_t charge;
    /* Owned copied input and targets follow allocation. */
} PermissionMemory;

struct AetherPermissionRegistry {
    AetherPermissionLimits limits;
    AetherWorkspaceRegistry* workspaces;
    AetherSessionRegistry* sessions;
    AetherRuntimeRegistry* runtimes;
    uint64_t revision;
    size_t memory_bytes;
    PermissionModeEntry* modes;
    PermissionRuleEntry* rules;
    PermissionWait* waits;
    PermissionMemory** memories;
};

bool aether_permission_valid_view(AetherStringView view, size_t maximum,
                                   bool required);
bool aether_permission_scope_matches(AetherPermissionScope filter,
                                      AetherPermissionScope actual);
bool aether_permission_same_input(AetherPermissionInput left,
                                  AetherPermissionInput right);
AetherStatus aether_permission_validate_input(
    const AetherPermissionRegistry* registry, AetherPermissionInput input
);
/* Copy views/target array into a single owned allocation, <=8KiB.
 * include_arguments=false strips arguments/request_id for range memory.
 * Caller owns *storage; input and storage clear on failure. */
AetherStatus aether_permission_copy_input(
    AetherPermissionInput input, bool include_arguments,
    AetherPermissionInput* out_input, void** out_storage, size_t* out_size
);
/* Returns complete input byte storage requirement; zero means invalid/overflow. */
size_t aether_permission_input_bytes(AetherPermissionInput input,
                                     bool include_arguments);
void aether_permission_copy_into(AetherPermissionInput input,
    bool include_arguments, AetherPermissionInput* out_input, void* storage);

AetherStatus aether_permission_policy(
    AetherPermissionRegistry* registry, AetherPermissionInput input,
    AetherPermissionResult* out_result
);
/* Memory matching only after policy says mode Ask (or cached mode special).
 * Match never bypasses explicit rule/safety Ask/Deny. All input views validated.
 */
bool aether_permission_memory_matches(
    const AetherPermissionRegistry* registry, AetherPermissionInput input
);
PermissionWait* aether_permission_find_wait(
    AetherPermissionRegistry* registry, AetherPermissionId id
);
AetherStatus aether_permission_refresh(
    AetherPermissionRegistry* registry, PermissionWait* wait, uint64_t now_ns
);
AetherStatus aether_permission_remember(
    AetherPermissionRegistry* registry, const PermissionWait* wait,
    AetherApprovalScope kind
);
void aether_permission_drop_memories(AetherPermissionRegistry* registry,
                                     AetherSessionId session,
                                     AetherPermissionId origin);
#endif
