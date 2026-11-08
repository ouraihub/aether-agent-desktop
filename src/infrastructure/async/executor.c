#include "executor_internal.h"

#include <stdlib.h>

AetherExecutorLimits aether_executor_default_limits(void)
{
    return (AetherExecutorLimits){32U, 2U * 1024U * 1024U, 65536U, 1U, 128U};
}

void aether_executor_close_locked(AetherExecutor* executor)
{
    size_t index;
    executor->closing = true;
    for (index = 0U; index < executor->slot_count; ++index) {
        WorkSlot* slot = &executor->slots[index];
        if (slot->state == WORK_QUEUED || slot->state == WORK_RUNNING) {
            slot->cancellation.requested = true;
            if (slot->state == WORK_QUEUED) {
                slot->result = AETHER_STATUS_CANCELLED;
                slot->state = WORK_DONE;
            }
        }
    }
    executor->queued_count = 0U;
    executor->queued_bytes = 0U;
    for (index = 0U; index < executor->limits.timers; ++index) {
        executor->timers[index].occupied = false;
    }
    aether_platform_monitor_wake_all(executor->monitor);
}

static void free_executor(AetherExecutor* executor)
{
    aether_platform_monitor_destroy(executor->monitor);
    free(executor->timers);
    free(executor->inputs);
    free(executor->slots);
    free(executor);
}

AetherStatus aether_executor_create(
    AetherExecutorLimits limits, AetherExecutor** out_executor
)
{
    AetherExecutor* executor;
    AetherStatus status;
    size_t index;
    if (out_executor == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_executor = NULL;
    if (limits.queued_count == 0U || limits.queued_count > 32U ||
        limits.queued_bytes == 0U || limits.queued_bytes > 2U * 1024U * 1024U ||
        limits.input_bytes == 0U || limits.input_bytes > 65536U ||
        limits.workers == 0U || limits.workers > 4U ||
        limits.timers == 0U || limits.timers > 128U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    executor = calloc(1U, sizeof(*executor));
    if (executor == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    executor->limits = limits;
    executor->slot_count = limits.queued_count + limits.workers;
    executor->slots = calloc(executor->slot_count, sizeof(*executor->slots));
    executor->inputs = malloc(executor->slot_count * limits.input_bytes);
    executor->timers = calloc(limits.timers, sizeof(*executor->timers));
    if (executor->slots == NULL || executor->inputs == NULL ||
        executor->timers == NULL) {
        free_executor(executor);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    status = aether_platform_monitor_create(&executor->monitor);
    if (status != AETHER_STATUS_OK) {
        free_executor(executor);
        return status;
    }
    /* Hold monitor until live count includes every successfully started worker. */
    aether_platform_monitor_lock(executor->monitor);
    for (index = 0U; index < limits.workers; ++index) {
        status = aether_platform_thread_create(aether_executor_worker, executor,
                                               &executor->threads[index]);
        if (status != AETHER_STATUS_OK) break;
        ++executor->live_workers;
    }
    if (status != AETHER_STATUS_OK) aether_executor_close_locked(executor);
    aether_platform_monitor_unlock(executor->monitor);
    if (status != AETHER_STATUS_OK) {
        size_t started;
        for (started = 0U; started < index; ++started) {
            AetherStatus joined = aether_platform_thread_join(
                &executor->threads[started]);
            if (joined != AETHER_STATUS_OK) return joined;
        }
        free_executor(executor);
        return status;
    }
    *out_executor = executor;
    return AETHER_STATUS_OK;
}

AetherStatus aether_executor_request_close(AetherExecutor* executor)
{
    if (executor == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(executor->monitor);
    aether_executor_close_locked(executor);
    aether_platform_monitor_unlock(executor->monitor);
    return AETHER_STATUS_OK;
}

bool aether_executor_is_closed(AetherExecutor* executor)
{
    bool closed;
    if (executor == NULL) return false;
    aether_platform_monitor_lock(executor->monitor);
    closed = executor->closing && executor->live_workers == 0U;
    aether_platform_monitor_unlock(executor->monitor);
    return closed;
}

AetherStatus aether_executor_destroy(AetherExecutor** executor)
{
    size_t index;
    if (executor == NULL || *executor == NULL) return AETHER_STATUS_OK;
    if (!aether_executor_is_closed(*executor)) return AETHER_STATUS_ALREADY_EXISTS;
    for (index = 0U; index < (*executor)->limits.workers; ++index) {
        AetherStatus status;
        if ((*executor)->threads[index] == NULL) continue;
        status = aether_platform_thread_join(&(*executor)->threads[index]);
        if (status != AETHER_STATUS_OK) return status;
    }
    free_executor(*executor);
    *executor = NULL;
    return AETHER_STATUS_OK;
}
