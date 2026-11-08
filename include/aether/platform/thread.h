#ifndef AETHER_PLATFORM_THREAD_H
#define AETHER_PLATFORM_THREAD_H

#include "aether/foundation/status.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherPlatformMonitor AetherPlatformMonitor;
typedef struct AetherPlatformThread AetherPlatformThread;
typedef void (*AetherPlatformThreadFn)(void* context);

/* Native implementation boundary. No business state or implicit global lock.
 * create clears output on failure. Monitors use nonrecursive exclusive locks;
 * wait atomically releases/reacquires the held lock, and may wake spuriously.
 * lock/unlock/wait/wake require a live monitor and valid lock ownership.
 * Wake all while holding the lock after changing the guarded predicate.
 * Thread creation reserves a 1MiB stack. Context must outlive join. join is
 * blocking, forbidden on the worker itself or UI callbacks; it frees the
 * thread on success. Destroy monitors only after all users have joined.
 * Allocation/OS creation failures return OutOfMemory/IOError, not success.
 * join failure retains the handle; NULL/already-cleared join is InvalidArgument.
 * monitor_destroy(NULL) is safe. Void helpers diagnose invalid usage with
 * assertions; valid default nonrobust monitor usage has no recoverable error.
 */
AetherStatus aether_platform_monitor_create(AetherPlatformMonitor** out_monitor);
void aether_platform_monitor_destroy(AetherPlatformMonitor* monitor);
void aether_platform_monitor_lock(AetherPlatformMonitor* monitor);
void aether_platform_monitor_unlock(AetherPlatformMonitor* monitor);
void aether_platform_monitor_wait(AetherPlatformMonitor* monitor);
void aether_platform_monitor_wake_all(AetherPlatformMonitor* monitor);
AetherStatus aether_platform_thread_create(
    AetherPlatformThreadFn run, void* context, AetherPlatformThread** out_thread
);
AetherStatus aether_platform_thread_join(AetherPlatformThread** thread);
/* Bounded native scheduling pause for background polling only, never UI. */
void aether_platform_thread_pause(uint32_t milliseconds);

#ifdef __cplusplus
}
#endif
#endif
