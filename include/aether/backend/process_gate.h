#ifndef AETHER_BACKEND_PROCESS_GATE_H
#define AETHER_BACKEND_PROCESS_GATE_H
#include "aether/core/permission.h"
#include "aether/infrastructure/process.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherAuthorizedLaunch AetherAuthorizedLaunch;
/* Trusted composition/execution-only boundary; Frontend must never include.
 * Backend UI contract will submit domain commands, not expose this context.
 * Owns a canonical copied native launch and pins actual exe/cwd/Workspace root
 * through Platform guard. Core/API transport has no OS handles.
 * prepare queries trusted Workspace root, verifies actual cwd lies within it,
 * fixes executable and cwd final identities; environment is exact allowlist,
 * args/environment order and bytes form length-prefixed canonical approval
 * arguments <=2048 bytes. Two targets are actual final exe/cwd, not a caller
 * asserted target list. Guard/copy remains owned until destroy.
 * permission_input returns borrowed views valid until destroy, for ordinary
 * Core begin/resolve. No UI wait occurs in prepare or execute.
 * execute freshly evaluates/consumes approval with actual fixed input and
 * current monotonic now, immediately calls Process Service spawn under same
 * serialized domain. Direct Allow requires zero ID; Ask needs exact valid once
 * or range-approved facts. If authorization fails, spawn is never called.
 * Spawn failure does not restore consumed once approval or auto-retry effect.
 * Scope subject/executor identity comes from trusted host, not external input.
 * One fixed plan permits at most one authorized OS spawn attempt, including
 * direct/memory Allow; construct a new request/plan for an explicit new action.
 * Lifetime ThreadCompatible: serialize with Permission/Workspace/Session state,
 * no await or change between consume and OS launch. Client worker exclusive
 * domain rules still apply; do not call concurrently with another Core owner.
 * This guard is not an OS sandbox; process may use OS account rights outside
 * Workspace internally. Explicitly required isolation not met must be rejected
 * by orchestration; no false claim that argv/file access is intercepted.
 */
AetherStatus aether_authorized_launch_prepare(
    AetherWorkspaceRegistry* workspaces, AetherPlatformProcessInput launch,
    AetherPermissionInput trusted_context, AetherAuthorizedLaunch** out_launch
);
AetherPermissionInput aether_authorized_launch_permission_input(
    const AetherAuthorizedLaunch* launch
);
AetherStatus aether_authorized_launch_execute(
    AetherAuthorizedLaunch* launch, AetherPermissionRegistry* permissions,
    AetherPermissionId approval, AetherProcessService* service,
    AetherTraceContext trace, AetherProcessRef* out_process
);
void aether_authorized_launch_destroy(AetherAuthorizedLaunch* launch);
#ifdef __cplusplus
}
#endif
#endif
