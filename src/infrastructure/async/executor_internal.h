#ifndef AETHER_EXECUTOR_INTERNAL_H
#define AETHER_EXECUTOR_INTERNAL_H

#include "aether/infrastructure/executor.h"
#include "aether/platform/thread.h"

typedef enum WorkState {
    WORK_FREE = 0, WORK_QUEUED, WORK_RUNNING, WORK_DONE
} WorkState;

struct AetherWorkCancellation {
    AetherExecutor* executor;
    bool requested;
};

typedef struct WorkSlot {
    WorkState state;
    AetherWorkContext work;
    AetherWorkFn run;
    AetherStatus result;
    AetherWorkCancellation cancellation;
} WorkSlot;

typedef struct TimerSlot {
    bool occupied;
    AetherTimerFact fact;
} TimerSlot;

struct AetherExecutor {
    AetherExecutorLimits limits;
    AetherPlatformMonitor* monitor;
    AetherPlatformThread* threads[4];
    size_t live_workers;
    bool closing;
    size_t slot_count;
    size_t queued_count;
    size_t queued_bytes;
    WorkSlot* slots;
    unsigned char* inputs;
    TimerSlot* timers;
};

/* Caller holds executor monitor. */
void aether_executor_close_locked(AetherExecutor* executor);
void aether_executor_worker(void* context);

#endif
