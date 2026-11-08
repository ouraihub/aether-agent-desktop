#ifndef AETHER_PLATFORM_LAUNCH_GUARD_H
#define AETHER_PLATFORM_LAUNCH_GUARD_H
#include "aether/platform/process.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherPlatformLaunchGuard AetherPlatformLaunchGuard;
/* Windows execution-boundary guard, synchronous OS access, never UI callback.
 * Opens executable, cwd and trusted Workspace root without share-delete or
 * executable share-write, resolves final DOS paths through actual handles,
 * requires cwd be root/descendant using exact final-path component boundary.
 * Rejects directory executable/non-directory cwd/root and remote UNC/device
 * paths. Pins file/dir names for guard lifetime; DOES NOT sandbox the process
 * or constrain arbitrary files argv may ask it to access. No prefix-only path
 * security: comparison is of the final resources actually opened.
 * Executable and cwd views are immutable canonical forward-slash UTF-8 DOS
 * identities borrowed until destroy; each <=511 bytes or InvalidArgument.
 * Caller launches with these guarded names while retaining the guard.
 * Final-path ancestors are also held without share-delete and revalidated.
 * DOS drive remapping/privileged namespace changes are outside this guarantee.
 * Input views borrowed for call; failed creation closes all acquired handles.
 * Missing/denied/invalid/other errors preserve NotFound/PermissionDenied/
 * InvalidArgument/IOError. No environment/argument/permission policy inference.
 */
AetherStatus aether_platform_launch_guard_create(
    AetherStringView executable, AetherStringView cwd, AetherStringView workspace,
    AetherPlatformLaunchGuard** out_guard
);
AetherStringView aether_platform_launch_guard_executable(
    const AetherPlatformLaunchGuard* guard
);
AetherStringView aether_platform_launch_guard_cwd(const AetherPlatformLaunchGuard* guard);
void aether_platform_launch_guard_destroy(AetherPlatformLaunchGuard* guard);
#ifdef __cplusplus
}
#endif
#endif
