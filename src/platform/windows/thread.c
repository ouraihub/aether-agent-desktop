#include "aether/platform/thread.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <process.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

struct AetherPlatformMonitor {
    SRWLOCK lock;
    CONDITION_VARIABLE condition;
};

struct AetherPlatformThread {
    HANDLE handle;
    AetherPlatformThreadFn callback;
    void* context;
};

static unsigned int __stdcall thread_entry(void* context)
{
    AetherPlatformThread* thread = context;
    thread->callback(thread->context);
    return 0U;
}

AetherStatus aether_platform_monitor_create(AetherPlatformMonitor** out_monitor)
{
    AetherPlatformMonitor* monitor;

    if (out_monitor == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_monitor = NULL;
    monitor = malloc(sizeof(*monitor));
    if (monitor == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    InitializeSRWLock(&monitor->lock);
    InitializeConditionVariable(&monitor->condition);
    *out_monitor = monitor;
    return AETHER_STATUS_OK;
}

void aether_platform_monitor_destroy(AetherPlatformMonitor* monitor)
{
    /* SRW locks and condition variables have no native destruction step. */
    free(monitor);
}

void aether_platform_monitor_lock(AetherPlatformMonitor* monitor)
{
    AcquireSRWLockExclusive(&monitor->lock);
}

void aether_platform_monitor_unlock(AetherPlatformMonitor* monitor)
{
    ReleaseSRWLockExclusive(&monitor->lock);
}

void aether_platform_monitor_wait(AetherPlatformMonitor* monitor)
{
    BOOL result = SleepConditionVariableSRW(&monitor->condition, &monitor->lock,
                                            INFINITE, 0U);
    /* INFINITE excludes the documented timeout error; a live monitor and
     * exclusive lock ownership are programmer preconditions. */
    assert(result != 0);
    (void)result;
}

void aether_platform_monitor_wake_all(AetherPlatformMonitor* monitor)
{
    WakeAllConditionVariable(&monitor->condition);
}

AetherStatus aether_platform_thread_create(
    AetherPlatformThreadFn run, void* context, AetherPlatformThread** out_thread
)
{
    AetherPlatformThread* thread;
    uintptr_t handle;

    if (out_thread == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_thread = NULL;
    if (run == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    thread = malloc(sizeof(*thread));
    if (thread == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    thread->callback = run;
    thread->context = context;
    handle = _beginthreadex(NULL, 1024U * 1024U, thread_entry, thread,
                           STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (handle == 0U) {
        int error = errno;
        free(thread);
        return error == ENOMEM ? AETHER_STATUS_OUT_OF_MEMORY
                               : AETHER_STATUS_IO_ERROR;
    }
    thread->handle = (HANDLE)handle;
    *out_thread = thread;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_thread_join(AetherPlatformThread** thread)
{
    AetherPlatformThread* owned;

    if (thread == NULL || *thread == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    owned = *thread;
    if (WaitForSingleObject(owned->handle, INFINITE) != WAIT_OBJECT_0) {
        return AETHER_STATUS_IO_ERROR;
    }
    if (!CloseHandle(owned->handle)) {
        return AETHER_STATUS_IO_ERROR;
    }
    free(owned);
    *thread = NULL;
    return AETHER_STATUS_OK;
}
