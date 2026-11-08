#ifndef AETHER_CORE_PERMISSION_H
#define AETHER_CORE_PERMISSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aether/core/session.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif
#define AETHER_PERMISSION_API_VERSION 2U

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
    AETHER_PERMISSION_ASK,
    AETHER_PERMISSION_INHERIT
} AetherPermissionDecision;

typedef struct AetherPermissionScope {
    AetherWorkspaceId workspace_id;
    AetherSessionId session_id;
    AetherRuntimeId runtime_id;
    AetherProviderId provider_id;
    AetherId subject_id;
    AetherId executor_id;
    /* Rule filters: empty tool/subtool are wildcards. */
    AetherStringView tool;
    AetherStringView subtool;
} AetherPermissionScope;

typedef struct AetherPermissionRule {
    AetherId rule_id;
    AetherId parent_rule_id;
    AetherPermissionScope scope;
    AetherPermissionDecision decision;
} AetherPermissionRule;

typedef struct AetherPermissionTarget {
    /* Trusted execution-boundary canonical identity, forward slash separators.
     * No filesystem access/normalization or OS sandbox claim is made here. */
    AetherStringView identity;
} AetherPermissionTarget;

typedef struct AetherPermissionInput {
    AetherId request_id;
    AetherPermissionScope scope;
    uint64_t runtime_generation;
    AetherPermissionLevel level;
    const AetherPermissionTarget* targets;
    size_t target_count;
    /* Exact canonical argument bytes; not a hash, not implicitly JSON. */
    AetherStringView arguments;
    bool capability_supported;
    bool hard_deny;
    bool noncacheable_ask;
} AetherPermissionInput;

typedef enum AetherApprovalScope {
    AETHER_APPROVAL_ONCE = 0,
    AETHER_APPROVAL_SESSION_TOOL,
    AETHER_APPROVAL_SESSION_TARGETS,
    AETHER_APPROVAL_SESSION_DIRECTORIES
} AetherApprovalScope;

typedef enum AetherPermissionState {
    AETHER_PERMISSION_WAITING = 0,
    AETHER_PERMISSION_APPROVED,
    AETHER_PERMISSION_REJECTED,
    AETHER_PERMISSION_CONSUMED,
    AETHER_PERMISSION_INVALIDATED
} AetherPermissionState;

typedef enum AetherPermissionSource {
    AETHER_PERMISSION_SOURCE_MODE = 0,
    AETHER_PERMISSION_SOURCE_RULE,
    AETHER_PERMISSION_SOURCE_MEMORY,
    AETHER_PERMISSION_SOURCE_HARD_DENY,
    AETHER_PERMISSION_SOURCE_SAFETY
} AetherPermissionSource;

typedef struct AetherPermissionResult {
    AetherPermissionDecision decision;
    AetherPermissionSource source;
    uint64_t policy_revision;
    AetherPermissionId permission_id;
    bool can_remember;
} AetherPermissionResult;

typedef struct AetherPermissionSnapshot {
    AetherPermissionId permission_id;
    AetherId request_id;
    AetherPermissionState state;
    AetherSessionId session_id;
    uint64_t policy_revision;
    uint64_t expires_ns;
    bool can_remember;
} AetherPermissionSnapshot;

typedef struct AetherApprovalResult {
    bool approved;
    /* Approval can succeed while bounded remember fails: independent status. */
    AetherStatus memory_status;
} AetherApprovalResult;

typedef struct AetherPermissionLimits {
    size_t waits;       /* <=64, each owned input <=8KiB */
    size_t memories;    /* <=256 */
    size_t memory_bytes; /* <=64KiB including entries/owned strings */
    size_t rules;      /* <=128 */
    size_t modes;      /* <=64 */
} AetherPermissionLimits;

/* V2 clean cutover from Workspace-only development API; signatures changed.
 * Thread Compatible: state owner serializes ALL calls and borrowed registries.
 * Workspace/Session/Runtime registries outlive this registry. No OS/UI/Store IO.
 * Inputs borrowed for call, retained Ask/rule/memory copy owned data. Views are
 * UTF-8, no embedded NUL, no required terminator. tool nonempty <128 bytes,
 * subtool <128, arguments <=2048 bytes, targets <=8 each nonempty <512 bytes.
 * Targets must be canonical absolute slash identities without dot segments,
 * doubled separators or backslashes; caller resolves case/links/final resources.
 * Empty targets are valid. Missing context never acquires default permission.
 * Input scope requires existing Open Workspace, matching live Session/Runtime
 * Ready generation/Provider and nonzero subject/executor/request identities.
 * Idle/Active Session only; unavailable capability returns NotSupported.
 * Rule optional IDs are wildcards, filters intersect. Inherit follows explicit
 * existing parent only while parent scope matches; cycle/missing parent rejected.
 * Deny > Ask > Allow, then session mode > workspace mode > trusted default yolo.
 * Rules never create a fake Workspace->Session->Runtime hierarchy.
 * evaluate is read-only and creates no waits; begin only registers actual Ask,
 * five-minute monotonic expiry bounded by business deadline. Caller supplies now
 * from a monotonic clock; clocks/deadlines use nanoseconds and cannot wrap.
 * Explicit Ask/safety checks permit ONCE only, ordinary mode Ask permits memory.
 * resolve only Allow/Deny, checks exact actual input/request/revision/generation;
 * success is an answer, not execution. consume is the final serialized execution
 * gate: re-evaluates actual input and atomically consumes ONCE. Caller invokes
 * its protected effect immediately on the same execution boundary; no await or
 * parameter/policy change between consume and effect. It does not stop Runtime
 * internal operations that bypass Aether or provide filesystem isolation.
 * consume with zero approval ID allows only a current direct/memory Allow.
 * A nonzero ID MUST match and remain approved even if mode now allows directly.
 * Session memories bind workspace/session/provider/runtime/subject/executor and
 * tool/subtool. Tool memory matches exact level; target/directory memory higher
 * level may cover lower. Directories cover only self/direct entries, not subtree;
 * all targets must match. New request/argument does not invalidate range memory.
 * New Deny/Ask always beats memory. Persistent approval storage belongs APP-05-C;
 * no persistent option is exposed by this in-memory contract.
 * Invalidate/revoke/expiry/session close never grant; late/repeat answer or
 * consume AlreadyExists/InvalidArgument/Timeout. Query rechecks liveness/expiry.
 * release removes nonwaiting records only; cancel first to release a wait.
 * close_session clears its waits/memory; no automatic unbounded tombstones.
 * Queue/memory full OutOfMemory, invalid config/input InvalidArgument, unknown
 * IDs NotFound, closed scope NotInitialized. Failing results Deny/zero; no fake
 * success. No allocation on evaluate/consume; create rollback and destroy(NULL)
 * safe. Actual argument copies and memory entries count against fixed quotas.
 */
AetherPermissionLimits aether_permission_default_limits(void);
AetherStatus aether_permission_registry_create(
    AetherPermissionLimits limits, AetherWorkspaceRegistry* workspaces,
    AetherSessionRegistry* sessions, AetherRuntimeRegistry* runtimes,
    AetherPermissionRegistry** out_registry
);
AetherStatus aether_permission_registry_destroy(AetherPermissionRegistry** registry);
AetherStatus aether_permission_set_mode(
    AetherPermissionRegistry* registry, AetherWorkspaceId workspace,
    AetherSessionId session, AetherPermissionMode mode
);
AetherStatus aether_permission_put_rule(
    AetherPermissionRegistry* registry, AetherPermissionRule rule
);
AetherStatus aether_permission_remove_rule(
    AetherPermissionRegistry* registry, AetherId rule_id
);
AetherStatus aether_permission_evaluate(
    AetherPermissionRegistry* registry, AetherPermissionInput input,
    AetherPermissionResult* out_result
);
AetherStatus aether_permission_begin(
    AetherPermissionRegistry* registry, AetherPermissionInput input,
    uint64_t now_ns, uint64_t deadline_ns, AetherPermissionResult* out_result
);
AetherStatus aether_permission_resolve(
    AetherPermissionRegistry* registry, AetherPermissionId id,
    AetherPermissionInput actual, uint64_t now_ns,
    AetherPermissionDecision answer, AetherApprovalScope scope,
    AetherApprovalResult* out_result
);
AetherStatus aether_permission_consume(
    AetherPermissionRegistry* registry, AetherPermissionId id,
    AetherPermissionInput actual, uint64_t now_ns
);
AetherStatus aether_permission_query(
    AetherPermissionRegistry* registry, AetherPermissionId id, uint64_t now_ns,
    AetherPermissionSnapshot* out_snapshot
);
AetherStatus aether_permission_revoke(
    AetherPermissionRegistry* registry, AetherPermissionId id
);
AetherStatus aether_permission_release(
    AetherPermissionRegistry* registry, AetherPermissionId id
);
AetherStatus aether_permission_close_session(
    AetherPermissionRegistry* registry, AetherSessionId session
);
AetherStatus aether_permission_clear_memory(AetherPermissionRegistry* registry);

#ifdef __cplusplus
}
#endif
#endif
