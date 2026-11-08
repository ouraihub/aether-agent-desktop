/* Substitution applies only to this test target's executor sources. */
#undef malloc
#undef calloc
#undef free
#undef aether_platform_thread_create
#undef aether_platform_monitor_create
#include "aether/infrastructure/executor.h"
#include "aether/platform/thread.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static size_t allocations;
static size_t fail_allocation;
static size_t live;
static size_t thread_calls;
static size_t fail_thread;
static bool fail_monitor;

void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);
AetherStatus aether_test_thread_create(
    AetherPlatformThreadFn run, void* context, AetherPlatformThread** out_thread
);
AetherStatus aether_test_monitor_create(AetherPlatformMonitor** out_monitor);

void* aether_test_malloc(size_t size)
{
    void* pointer;
    if (++allocations == fail_allocation) return NULL;
    pointer = malloc(size);
    if (pointer != NULL) ++live;
    return pointer;
}

void* aether_test_calloc(size_t count, size_t size)
{
    void* pointer;
    if (size != 0U && count > SIZE_MAX / size) return NULL;
    pointer = aether_test_malloc(count * size);
    if (pointer != NULL) (void)memset(pointer, 0, count * size);
    return pointer;
}

void aether_test_free(void* pointer)
{
    if (pointer != NULL) --live;
    free(pointer);
}

AetherStatus aether_test_thread_create(
    AetherPlatformThreadFn run, void* context, AetherPlatformThread** out_thread
)
{
    if (++thread_calls == fail_thread) {
        *out_thread = NULL;
        return AETHER_STATUS_IO_ERROR;
    }
    return aether_platform_thread_create(run, context, out_thread);
}

AetherStatus aether_test_monitor_create(AetherPlatformMonitor** out_monitor)
{
    if (fail_monitor) {
        *out_monitor = NULL;
        return AETHER_STATUS_IO_ERROR;
    }
    return aether_platform_monitor_create(out_monitor);
}

int main(void)
{
    size_t failure;
    AetherExecutorLimits limits = aether_executor_default_limits();
    limits.workers = 4U;
    for (failure = 1U; failure <= 4U; ++failure) {
        AetherExecutor* executor = NULL;
        allocations = 0U;
        fail_allocation = failure;
        thread_calls = 0U;
        assert(aether_executor_create(limits, &executor) ==
               AETHER_STATUS_OUT_OF_MEMORY);
        assert(executor == NULL && live == 0U && thread_calls == 0U);
    }
    fail_allocation = 0U;
    for (failure = 1U; failure <= 4U; ++failure) {
        AetherExecutor* executor = NULL;
        allocations = 0U;
        thread_calls = 0U;
        fail_thread = failure;
        assert(aether_executor_create(limits, &executor) ==
               AETHER_STATUS_IO_ERROR);
        assert(executor == NULL && live == 0U);
        assert(thread_calls == failure);
    }
    {
        AetherExecutor* executor = NULL;
        fail_monitor = true;
        thread_calls = 0U;
        assert(aether_executor_create(limits, &executor) ==
               AETHER_STATUS_IO_ERROR);
        assert(executor == NULL && live == 0U && thread_calls == 0U);
    }
    return 0;
}
