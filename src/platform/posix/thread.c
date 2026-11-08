#define _POSIX_C_SOURCE 200809L
#include "aether/platform/thread.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

struct AetherPlatformMonitor {
    pthread_mutex_t lock;
    pthread_cond_t condition;
};

struct AetherPlatformThread {
    pthread_t handle;
    AetherPlatformThreadFn callback;
    void* context;
};

static AetherStatus native_error(int error)
{
    return error == ENOMEM ? AETHER_STATUS_OUT_OF_MEMORY
                           : AETHER_STATUS_IO_ERROR;
}

/* Default nonrobust synchronization has no recoverable valid-use failures.
 * Keep native calls outside assert so release builds still perform them. */
static void check_precondition(int result)
{
    assert(result == 0);
    (void)result;
}

static void* thread_entry(void* context)
{
    AetherPlatformThread* thread = context;
    thread->callback(thread->context);
    return NULL;
}

AetherStatus aether_platform_monitor_create(AetherPlatformMonitor** out_monitor)
{
    AetherPlatformMonitor* monitor;
    int result;

    if (out_monitor == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_monitor = NULL;
    monitor = malloc(sizeof(*monitor));
    if (monitor == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    result = pthread_mutex_init(&monitor->lock, NULL);
    if (result != 0) {
        free(monitor);
        return native_error(result);
    }
    result = pthread_cond_init(&monitor->condition, NULL);
    if (result != 0) {
        int cleanup = pthread_mutex_destroy(&monitor->lock);
        free(monitor);
        return native_error(cleanup != 0 ? cleanup : result);
    }
    *out_monitor = monitor;
    return AETHER_STATUS_OK;
}

void aether_platform_monitor_destroy(AetherPlatformMonitor* monitor)
{
    if (monitor == NULL) {
        return;
    }
    check_precondition(pthread_cond_destroy(&monitor->condition));
    check_precondition(pthread_mutex_destroy(&monitor->lock));
    free(monitor);
}

void aether_platform_monitor_lock(AetherPlatformMonitor* monitor)
{
    check_precondition(pthread_mutex_lock(&monitor->lock));
}

void aether_platform_monitor_unlock(AetherPlatformMonitor* monitor)
{
    check_precondition(pthread_mutex_unlock(&monitor->lock));
}

void aether_platform_monitor_wait(AetherPlatformMonitor* monitor)
{
    check_precondition(pthread_cond_wait(&monitor->condition, &monitor->lock));
}

void aether_platform_monitor_wake_all(AetherPlatformMonitor* monitor)
{
    check_precondition(pthread_cond_broadcast(&monitor->condition));
}

AetherStatus aether_platform_thread_create(
    AetherPlatformThreadFn run, void* context, AetherPlatformThread** out_thread
)
{
    AetherPlatformThread* thread;
    pthread_attr_t attributes;
    int result;

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
    result = pthread_attr_init(&attributes);
    if (result != 0) {
        free(thread);
        return native_error(result);
    }
    result = pthread_attr_setstacksize(&attributes, 1024U * 1024U);
    if (result == 0) {
        result = pthread_create(&thread->handle, &attributes,
                                thread_entry, thread);
    }
    /* POSIX defines no failure for destruction of a valid local attr object. */
    check_precondition(pthread_attr_destroy(&attributes));
    if (result != 0) {
        free(thread);
        return native_error(result);
    }
    *out_thread = thread;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_thread_join(AetherPlatformThread** thread)
{
    AetherPlatformThread* owned;
    int result;

    if (thread == NULL || *thread == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    owned = *thread;
    result = pthread_join(owned->handle, NULL);
    if (result != 0) {
        return native_error(result);
    }
    free(owned);
    *thread = NULL;
    return AETHER_STATUS_OK;
}

void aether_platform_thread_pause(uint32_t milliseconds)
{
    struct timespec remaining;
    remaining.tv_sec = (time_t)(milliseconds / 1000U);
    remaining.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) { }
}
