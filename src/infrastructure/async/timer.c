#include "executor_internal.h"

_Static_assert(sizeof(TimerSlot) <= 128U, "Timer metadata exceeds budget");

AetherStatus aether_executor_schedule_timer(
    AetherExecutor* executor, AetherId target, uint64_t generation,
    uint64_t deadline_ns, AetherTraceContext trace, AetherId* out_timer
)
{
    size_t index;
    if (out_timer == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_timer = (AetherId){0};
    if (executor == NULL || !aether_id_is_valid(target) || generation == 0U ||
        !trace.valid) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(executor->monitor);
    if (executor->closing) {
        aether_platform_monitor_unlock(executor->monitor);
        return AETHER_STATUS_NOT_INITIALIZED;
    }
    for (index = 0U; index < executor->limits.timers; ++index) {
        TimerSlot* timer = &executor->timers[index];
        if (!timer->occupied) {
            AetherId id = aether_id_next();
            if (!aether_id_is_valid(id)) {
                aether_platform_monitor_unlock(executor->monitor);
                return AETHER_STATUS_INTERNAL_ERROR;
            }
            timer->fact = (AetherTimerFact){id, target, generation,
                                           deadline_ns, trace};
            timer->occupied = true;
            *out_timer = id;
            aether_platform_monitor_unlock(executor->monitor);
            return AETHER_STATUS_OK;
        }
    }
    aether_platform_monitor_unlock(executor->monitor);
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_executor_cancel_timer(AetherExecutor* executor, AetherId id)
{
    size_t index;
    if (executor == NULL || !aether_id_is_valid(id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_platform_monitor_lock(executor->monitor);
    for (index = 0U; index < executor->limits.timers; ++index) {
        TimerSlot* timer = &executor->timers[index];
        if (timer->occupied && aether_id_equal(timer->fact.timer_id, id)) {
            timer->occupied = false;
            aether_platform_monitor_unlock(executor->monitor);
            return AETHER_STATUS_OK;
        }
    }
    aether_platform_monitor_unlock(executor->monitor);
    return AETHER_STATUS_NOT_FOUND;
}

AetherStatus aether_executor_take_timer(
    AetherExecutor* executor, uint64_t now_ns, AetherTimerFact* out_fact
)
{
    size_t index;
    TimerSlot* selected = NULL;
    if (out_fact == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_fact = (AetherTimerFact){0};
    if (executor == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(executor->monitor);
    for (index = 0U; index < executor->limits.timers; ++index) {
        TimerSlot* timer = &executor->timers[index];
        if (timer->occupied && timer->fact.deadline_ns <= now_ns &&
            (selected == NULL || timer->fact.deadline_ns <
                                 selected->fact.deadline_ns)) {
            selected = timer;
        }
    }
    if (selected == NULL) {
        aether_platform_monitor_unlock(executor->monitor);
        return AETHER_STATUS_NOT_FOUND;
    }
    *out_fact = selected->fact;
    selected->occupied = false;
    aether_platform_monitor_unlock(executor->monitor);
    return AETHER_STATUS_OK;
}
