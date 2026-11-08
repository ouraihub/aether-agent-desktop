#ifndef AETHER_INFRASTRUCTURE_EXECUTOR_H
#define AETHER_INFRASTRUCTURE_EXECUTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aether/foundation/id.h"
#include "aether/foundation/trace.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherExecutor AetherExecutor;
typedef struct AetherWorkCancellation AetherWorkCancellation;

typedef struct AetherExecutorLimits {
    size_t queued_count;
    size_t queued_bytes;
    size_t input_bytes;
    size_t workers;
    size_t timers;
} AetherExecutorLimits;

typedef struct AetherWorkContext {
    AetherId work_id;
    uint64_t generation;
    AetherTraceContext trace;
    const unsigned char* input;
    size_t input_size;
    void* context;
    const AetherWorkCancellation* cancellation;
} AetherWorkContext;

typedef AetherStatus (*AetherWorkFn)(const AetherWorkContext* work);

typedef struct AetherWorkInput {
    uint64_t generation;
    AetherTraceContext trace;
    const unsigned char* input;
    size_t input_size;
    AetherWorkFn run;
    void* context;
} AetherWorkInput;

typedef struct AetherWorkResult {
    AetherId work_id;
    uint64_t generation;
    AetherTraceContext trace;
    AetherStatus status;
} AetherWorkResult;

typedef struct AetherTimerFact {
    AetherId timer_id;
    AetherId target_id;
    uint64_t generation;
    uint64_t deadline_ns;
    AetherTraceContext trace;
} AetherTimerFact;

/* V1 bounded executor. Thread Safe except destroy: caller must exclude other
 * API calls during destroy. No Core/Backend/UI dependency, no user callbacks
 * while holding the monitor. Worker callbacks may submit/cancel/close, but
 * never wait for completion or destroy this executor. No forced thread kill.
 * Defaults: queue 32 / 2MiB, input 64KiB, 1 worker (max 4), 128 timers.
 * Configured limits may only lower these maxima; all fields must be nonzero.
 * create starts workers; failure joins any started workers and clears output.
 * submit copies input and trace before success; caller retains its input on
 * every path. context is borrowed until the result is taken or close-complete.
 * Callbacks receive private input valid only until return, and must cooperate
 * with cancellation. A cancel intent never overwrites a running result: the
 * callback reports its actual outcome (including Cancelled when appropriate).
 * Queued cancellation produces a Cancelled result without invoking the work.
 * cancel requires matching ID/generation; stale generations return
 * InvalidArgument, missing IDs NotFound, already completed AlreadyExists.
 * Queue count/byte or retained-result capacity full is OutOfMemory; no inline
 * execution, blocking on capacity, eviction, retries or implicit deduplication.
 * All task slots (queued+running+completed) are bounded by queued_count+workers;
 * take returns one result and releases its slot; NotFound means none ready.
 * Completion order is not guaranteed; IDs never reused. Status success is
 * technical work completion, not a Backend business request terminal state.
 * request_close stops admission and cancels queued/running work; repeat safe.
 * is_closed means all worker callbacks returned and workers left their loops;
 * results remain queryable via take. destroy before close-complete returns
 * AlreadyExists without releasing anything; afterwards joins native threads.
 * No UI waits/joins. No cancellation deadline can free a still-running context.
 * Timers have no callbacks or timer thread: state owner calls take_timer(now)
 * with local monotonic nanoseconds (e.g. Platform clock). Earliest due entry is
 * returned once, with original target/generation/trace; owner MUST revalidate
 * these before applying. Cancel can remove pending entries; a previously taken
 * fact cannot be recalled. No wall-clock time, periodic timers or auto-resubmit.
 * Closing discards pending timers. Timer capacity is separate from work queue.
 * Every failing output is cleared. Invalid inputs return InvalidArgument;
 * closed admission NotInitialized; allocation exhaustion OutOfMemory, OS
 * failure IOError. A valid trace and nonzero generation are required.
 */
AetherExecutorLimits aether_executor_default_limits(void);
AetherStatus aether_executor_create(
    AetherExecutorLimits limits, AetherExecutor** out_executor
);
AetherStatus aether_executor_submit(
    AetherExecutor* executor, AetherWorkInput input, AetherId* out_id
);
AetherStatus aether_executor_cancel(
    AetherExecutor* executor, AetherId id, uint64_t generation
);
bool aether_work_cancel_requested(const AetherWorkCancellation* cancellation);
AetherStatus aether_executor_take(
    AetherExecutor* executor, AetherWorkResult* out_result
);
AetherStatus aether_executor_schedule_timer(
    AetherExecutor* executor, AetherId target, uint64_t generation,
    uint64_t deadline_ns, AetherTraceContext trace, AetherId* out_timer
);
AetherStatus aether_executor_cancel_timer(AetherExecutor* executor, AetherId id);
AetherStatus aether_executor_take_timer(
    AetherExecutor* executor, uint64_t now_ns, AetherTimerFact* out_fact
);
AetherStatus aether_executor_request_close(AetherExecutor* executor);
bool aether_executor_is_closed(AetherExecutor* executor);
AetherStatus aether_executor_destroy(AetherExecutor** executor);

#ifdef __cplusplus
}
#endif
#endif
