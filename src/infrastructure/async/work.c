#include "executor_internal.h"

#include <string.h>

static WorkSlot* next_work(AetherExecutor* executor)
{
    WorkSlot* selected = NULL;
    size_t index;
    for (index = 0U; index < executor->slot_count; ++index) {
        WorkSlot* slot = &executor->slots[index];
        if (slot->state == WORK_QUEUED &&
            (selected == NULL || slot->work.work_id.value <
                                 selected->work.work_id.value)) {
            selected = slot;
        }
    }
    return selected;
}

void aether_executor_worker(void* context)
{
    AetherExecutor* executor = context;
    aether_platform_monitor_lock(executor->monitor);
    for (;;) {
        WorkSlot* slot = next_work(executor);
        if (slot == NULL) {
            if (executor->closing) break;
            aether_platform_monitor_wait(executor->monitor);
            continue;
        }
        slot->state = WORK_RUNNING;
        --executor->queued_count;
        executor->queued_bytes -= slot->work.input_size;
        aether_platform_monitor_unlock(executor->monitor);
        {
            AetherStatus result = slot->run(&slot->work);
            aether_platform_monitor_lock(executor->monitor);
            slot->result = result;
        }
        slot->state = WORK_DONE;
    }
    --executor->live_workers;
    aether_platform_monitor_wake_all(executor->monitor);
    aether_platform_monitor_unlock(executor->monitor);
}

bool aether_work_cancel_requested(const AetherWorkCancellation* cancellation)
{
    bool requested;
    if (cancellation == NULL) return false;
    aether_platform_monitor_lock(cancellation->executor->monitor);
    requested = cancellation->requested;
    aether_platform_monitor_unlock(cancellation->executor->monitor);
    return requested;
}

AetherStatus aether_executor_submit(
    AetherExecutor* executor, AetherWorkInput input, AetherId* out_id
)
{
    WorkSlot* slot = NULL;
    size_t index;
    AetherId id;
    if (out_id == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_id = (AetherId){0};
    if (executor == NULL || input.run == NULL || input.generation == 0U ||
        !input.trace.valid || input.input_size > executor->limits.input_bytes ||
        (input.input_size != 0U && input.input == NULL)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_platform_monitor_lock(executor->monitor);
    if (executor->closing) {
        aether_platform_monitor_unlock(executor->monitor);
        return AETHER_STATUS_NOT_INITIALIZED;
    }
    for (index = 0U; index < executor->slot_count; ++index) {
        if (executor->slots[index].state == WORK_FREE) {
            slot = &executor->slots[index];
            break;
        }
    }
    if (slot == NULL || executor->queued_count == executor->limits.queued_count ||
        input.input_size > executor->limits.queued_bytes -
                           executor->queued_bytes) {
        aether_platform_monitor_unlock(executor->monitor);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    id = aether_id_next();
    if (!aether_id_is_valid(id)) {
        aether_platform_monitor_unlock(executor->monitor);
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    *slot = (WorkSlot){0};
    slot->state = WORK_QUEUED;
    slot->run = input.run;
    slot->cancellation.executor = executor;
    slot->work = (AetherWorkContext){id, input.generation, input.trace,
        executor->inputs + index * executor->limits.input_bytes,
        input.input_size, input.context, &slot->cancellation};
    if (input.input_size != 0U) {
        (void)memcpy(executor->inputs + index * executor->limits.input_bytes,
                     input.input, input.input_size);
    }
    ++executor->queued_count;
    executor->queued_bytes += input.input_size;
    *out_id = id;
    aether_platform_monitor_wake_all(executor->monitor);
    aether_platform_monitor_unlock(executor->monitor);
    return AETHER_STATUS_OK;
}

AetherStatus aether_executor_cancel(
    AetherExecutor* executor, AetherId id, uint64_t generation
)
{
    size_t index;
    AetherStatus result = AETHER_STATUS_NOT_FOUND;
    if (executor == NULL || !aether_id_is_valid(id) || generation == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_platform_monitor_lock(executor->monitor);
    for (index = 0U; index < executor->slot_count; ++index) {
        WorkSlot* slot = &executor->slots[index];
        if (slot->state == WORK_FREE || !aether_id_equal(slot->work.work_id, id)) {
            continue;
        }
        if (slot->work.generation != generation) {
            result = AETHER_STATUS_INVALID_ARGUMENT;
        } else if (slot->state == WORK_DONE) {
            result = AETHER_STATUS_ALREADY_EXISTS;
        } else {
            slot->cancellation.requested = true;
            if (slot->state == WORK_QUEUED) {
                --executor->queued_count;
                executor->queued_bytes -= slot->work.input_size;
                slot->result = AETHER_STATUS_CANCELLED;
                slot->state = WORK_DONE;
            }
            result = AETHER_STATUS_OK;
        }
        break;
    }
    aether_platform_monitor_unlock(executor->monitor);
    return result;
}

AetherStatus aether_executor_take(
    AetherExecutor* executor, AetherWorkResult* out_result
)
{
    size_t index;
    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_result = (AetherWorkResult){0};
    if (executor == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(executor->monitor);
    for (index = 0U; index < executor->slot_count; ++index) {
        WorkSlot* slot = &executor->slots[index];
        if (slot->state == WORK_DONE) {
            *out_result = (AetherWorkResult){slot->work.work_id,
                slot->work.generation, slot->work.trace, slot->result};
            slot->state = WORK_FREE;
            aether_platform_monitor_unlock(executor->monitor);
            return AETHER_STATUS_OK;
        }
    }
    aether_platform_monitor_unlock(executor->monitor);
    return AETHER_STATUS_NOT_FOUND;
}
