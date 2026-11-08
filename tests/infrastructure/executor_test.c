#include "aether/infrastructure/executor.h"
#include "aether/platform/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, \
                #expression); \
        return EXIT_FAILURE; \
    } \
} while (0)

#define POLL_LIMIT 100000000U

typedef struct WorkGate {
    AetherPlatformMonitor* monitor;
    const unsigned char* source;
    unsigned char expected[4];
    AetherTraceContext trace;
    unsigned int entered;
    unsigned int returned;
    bool released;
    bool copied;
    bool metadata;
    bool cancelled;
    bool cooperative;
    AetherStatus outcome;
} WorkGate;

static AetherTraceContext trace_context(void)
{
    AetherTraceContext trace = {0};
    trace.trace_id.bytes[0] = 1U;
    trace.span_id.bytes[0] = 2U;
    trace.valid = true;
    return trace;
}

static bool same_trace(AetherTraceContext left, AetherTraceContext right)
{
    return left.valid == right.valid && left.has_parent == right.has_parent &&
        memcmp(left.trace_id.bytes, right.trace_id.bytes, 16U) == 0 &&
        memcmp(left.span_id.bytes, right.span_id.bytes, 8U) == 0 &&
        memcmp(left.parent_span_id.bytes, right.parent_span_id.bytes, 8U) == 0;
}

static AetherStatus gate_init(WorkGate* gate, const unsigned char* source)
{
    *gate = (WorkGate){0};
    gate->source = source;
    memcpy(gate->expected, source, sizeof(gate->expected));
    gate->trace = trace_context();
    gate->copied = true;
    gate->metadata = true;
    gate->outcome = AETHER_STATUS_OK;
    return aether_platform_monitor_create(&gate->monitor);
}

static AetherStatus gated_work(const AetherWorkContext* work)
{
    WorkGate* gate = work->context;
    AetherStatus outcome;
    bool cancelled;
    aether_platform_monitor_lock(gate->monitor);
    ++gate->entered;
    aether_platform_monitor_wake_all(gate->monitor);
    while (!gate->released) {
        aether_platform_monitor_wait(gate->monitor);
    }
    cancelled = aether_work_cancel_requested(work->cancellation);
    gate->cancelled = gate->cancelled || cancelled;
    gate->copied = gate->copied && work->input != gate->source &&
        work->input_size <= sizeof(gate->expected) &&
        memcmp(work->input, gate->expected, work->input_size) == 0;
    gate->metadata = gate->metadata && work->generation == 7U &&
        aether_id_is_valid(work->work_id) && same_trace(work->trace, gate->trace);
    outcome = cancelled && gate->cooperative ? AETHER_STATUS_CANCELLED :
        gate->outcome;
    ++gate->returned;
    aether_platform_monitor_wake_all(gate->monitor);
    aether_platform_monitor_unlock(gate->monitor);
    return outcome;
}

static AetherWorkInput work_input(WorkGate* gate, size_t size)
{
    return (AetherWorkInput){7U, gate->trace, gate->source, size,
                             gated_work, gate};
}

static void gate_wait(WorkGate* gate, unsigned int count, bool returned)
{
    aether_platform_monitor_lock(gate->monitor);
    while ((returned ? gate->returned : gate->entered) < count) {
        aether_platform_monitor_wait(gate->monitor);
    }
    aether_platform_monitor_unlock(gate->monitor);
}

static void gate_release(WorkGate* gate)
{
    aether_platform_monitor_lock(gate->monitor);
    gate->released = true;
    aether_platform_monitor_wake_all(gate->monitor);
    aether_platform_monitor_unlock(gate->monitor);
}

/* A callback's return handshake precedes the executor's completion publication.
 * Generous bounded polling covers that gap; CTest supplies the outer timeout. */
static bool wait_closed(AetherExecutor* executor)
{
    unsigned int attempt;
    for (attempt = 0U; attempt < POLL_LIMIT; ++attempt) {
        if (aether_executor_is_closed(executor)) {
            return true;
        }
    }
    return false;
}

static AetherStatus take_ready(AetherExecutor* executor, AetherWorkResult* result)
{
    unsigned int attempt;
    AetherStatus status = AETHER_STATUS_NOT_FOUND;
    for (attempt = 0U; attempt < POLL_LIMIT; ++attempt) {
        status = aether_executor_take(executor, result);
        if (status != AETHER_STATUS_NOT_FOUND) {
            break;
        }
    }
    return status;
}

static bool empty_result(AetherWorkResult result)
{
    return result.work_id.value == 0U && result.generation == 0U &&
        same_trace(result.trace, (AetherTraceContext){0}) &&
        result.status == AETHER_STATUS_OK;
}

static int invalid_arguments(void)
{
    AetherExecutorLimits defaults = aether_executor_default_limits();
    AetherExecutorLimits limits = defaults;
    size_t* fields[] = {&limits.queued_count, &limits.queued_bytes,
                       &limits.input_bytes, &limits.workers, &limits.timers};
    const size_t maxima[] = {32U, 2U * 1024U * 1024U, 64U * 1024U, 4U, 128U};
    AetherExecutor* executor = NULL;
    AetherWorkInput input = {0};
    AetherWorkResult result;
    AetherId id;
    size_t index;
    for (index = 0U; index < sizeof(fields) / sizeof(fields[0]); ++index) {
        limits = defaults;
        *fields[index] = 0U;
        executor = (AetherExecutor*)&limits;
        CHECK(aether_executor_create(limits, &executor) == AETHER_STATUS_INVALID_ARGUMENT);
        CHECK(executor == NULL);
        *fields[index] = maxima[index] + 1U;
        executor = (AetherExecutor*)&limits;
        CHECK(aether_executor_create(limits, &executor) == AETHER_STATUS_INVALID_ARGUMENT);
        CHECK(executor == NULL);
    }
    CHECK(aether_executor_create(defaults, NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    id.value = 123U;
    CHECK(aether_executor_submit(NULL, input, &id) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(id.value == 0U);
    memset(&result, 0xA5, sizeof(result));
    CHECK(aether_executor_take(NULL, &result) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(empty_result(result));
    CHECK(aether_executor_request_close(NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_executor_destroy(NULL) == AETHER_STATUS_OK);
    return EXIT_SUCCESS;
}

static int queue_bounds_and_copy(void)
{
    unsigned char bytes[] = {10U, 20U, 30U, 40U};
    WorkGate gate;
    AetherExecutor* executor = NULL;
    AetherExecutorLimits limits = {2U, 5U, 4U, 1U, 1U};
    AetherWorkInput input;
    AetherId ids[3];
    AetherId queued;
    AetherId rejected = {123U};
    AetherWorkResult result;
    bool seen[3] = {false, false, false};
    size_t index;
    CHECK(gate_init(&gate, bytes) == AETHER_STATUS_OK);
    CHECK(aether_executor_create(limits, &executor) == AETHER_STATUS_OK);
    input = work_input(&gate, 4U);
    CHECK(aether_executor_submit(executor, input, &ids[0]) == AETHER_STATUS_OK);
    gate_wait(&gate, 1U, false);
    CHECK(aether_executor_cancel(executor, ids[0], 8U) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_executor_cancel(executor, aether_id_next(), 7U) == AETHER_STATUS_NOT_FOUND);
    CHECK(aether_executor_submit(executor, input, &queued) == AETHER_STATUS_OK);
    input.input_size = 2U;
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.value == 0U); /* Byte limit, with one queue position still free. */
    input.input_size = 1U;
    CHECK(aether_executor_submit(executor, input, &ids[1]) == AETHER_STATUS_OK);
    input.input_size = 0U;
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.value == 0U); /* Count limit, even for an empty input. */
    CHECK(aether_executor_cancel(executor, queued, 8U) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_executor_cancel(executor, queued, 7U) == AETHER_STATUS_OK);
    CHECK(take_ready(executor, &result) == AETHER_STATUS_OK);
    CHECK(result.work_id.value == queued.value && result.status == AETHER_STATUS_CANCELLED);
    CHECK(result.generation == 7U && same_trace(result.trace, gate.trace));
    input.input_size = 4U;
    CHECK(aether_executor_submit(executor, input, &ids[2]) == AETHER_STATUS_OK);
    memset(bytes, 99, sizeof(bytes));
    input.trace.valid = false;
    CHECK(aether_executor_cancel(executor, ids[0], 7U) == AETHER_STATUS_OK);
    gate_release(&gate);
    gate_wait(&gate, 3U, true);
    CHECK(aether_executor_request_close(executor) == AETHER_STATUS_OK);
    CHECK(wait_closed(executor));
    CHECK(gate.entered == 3U && gate.copied && gate.metadata && gate.cancelled);
    for (index = 0U; index < 3U; ++index) {
        size_t match;
        CHECK(aether_executor_take(executor, &result) == AETHER_STATUS_OK);
        CHECK(result.status == AETHER_STATUS_OK && result.generation == 7U);
        CHECK(same_trace(result.trace, gate.trace));
        for (match = 0U; match < 3U; ++match) {
            if (result.work_id.value == ids[match].value) {
                break;
            }
        }
        CHECK(match < 3U && !seen[match]);
        seen[match] = true;
    }
    memset(&result, 0xA5, sizeof(result));
    CHECK(aether_executor_take(executor, &result) == AETHER_STATUS_NOT_FOUND);
    CHECK(empty_result(result));
    CHECK(aether_executor_destroy(&executor) == AETHER_STATUS_OK && executor == NULL);
    aether_platform_monitor_destroy(gate.monitor);
    return EXIT_SUCCESS;
}

static int retention_and_close(void)
{
    const unsigned char bytes[] = {1U, 2U, 3U, 4U};
    WorkGate gate;
    AetherExecutor* executor = NULL;
    AetherExecutor* saved;
    AetherExecutorLimits limits = {1U, 4U, 4U, 1U, 1U};
    AetherWorkInput input;
    AetherWorkResult result;
    AetherId running;
    AetherId queued;
    AetherId replacement;
    AetherId rejected = {123U};
    unsigned int index;
    bool saw_running = false;
    bool saw_queued = false;
    CHECK(gate_init(&gate, bytes) == AETHER_STATUS_OK);
    gate.outcome = AETHER_STATUS_IO_ERROR;
    CHECK(aether_executor_create(limits, &executor) == AETHER_STATUS_OK);
    input = work_input(&gate, 4U);
    CHECK(aether_executor_take(executor, NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_executor_submit(executor, input, NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    input.generation = 0U;
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(rejected.value == 0U);
    input = work_input(&gate, 4U);
    input.trace.valid = false;
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    input = work_input(&gate, 5U);
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    input = work_input(&gate, 4U);
    input.input = NULL;
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    input = work_input(&gate, 4U);
    input.run = NULL;
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    input = work_input(&gate, 4U);
    CHECK(aether_executor_submit(executor, input, &running) == AETHER_STATUS_OK);
    gate_wait(&gate, 1U, false);
    CHECK(aether_executor_submit(executor, input, &queued) == AETHER_STATUS_OK);
    CHECK(aether_executor_cancel(executor, queued, 7U) == AETHER_STATUS_OK);
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.value == 0U); /* Retained result occupies the otherwise free slot. */
    CHECK(take_ready(executor, &result) == AETHER_STATUS_OK);
    CHECK(result.work_id.value == queued.value && result.status == AETHER_STATUS_CANCELLED);
    CHECK(aether_executor_submit(executor, input, &replacement) == AETHER_STATUS_OK);
    CHECK(replacement.value != queued.value && replacement.value != running.value);
    CHECK(aether_executor_request_close(executor) == AETHER_STATUS_OK);
    CHECK(aether_executor_request_close(executor) == AETHER_STATUS_OK);
    CHECK(!aether_executor_is_closed(executor));
    saved = executor;
    CHECK(aether_executor_destroy(&executor) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(executor == saved); /* The blocked callback and borrowed context remain live. */
    rejected.value = 123U;
    CHECK(aether_executor_submit(executor, input, &rejected) == AETHER_STATUS_NOT_INITIALIZED);
    CHECK(rejected.value == 0U);
    gate_release(&gate);
    gate_wait(&gate, 1U, true);
    CHECK(wait_closed(executor));
    CHECK(gate.entered == 1U && gate.cancelled && gate.copied && gate.metadata);
    CHECK(aether_executor_cancel(executor, running, 7U) == AETHER_STATUS_ALREADY_EXISTS);
    for (index = 0U; index < 2U; ++index) {
        CHECK(aether_executor_take(executor, &result) == AETHER_STATUS_OK);
        CHECK(result.generation == 7U && same_trace(result.trace, gate.trace));
        if (result.work_id.value == running.value) {
            CHECK(!saw_running && result.status == AETHER_STATUS_IO_ERROR);
            saw_running = true; /* Cancel intent must not replace the actual failure. */
        } else {
            CHECK(!saw_queued && result.work_id.value == replacement.value);
            CHECK(result.status == AETHER_STATUS_CANCELLED);
            saw_queued = true;
        }
    }
    CHECK(saw_running && saw_queued);
    CHECK(aether_executor_destroy(&executor) == AETHER_STATUS_OK && executor == NULL);
    aether_platform_monitor_destroy(gate.monitor);
    return EXIT_SUCCESS;
}

static int completed_slot_reuse(void)
{
    const unsigned char bytes[] = {1U, 2U, 3U, 4U};
    WorkGate finished;
    WorkGate blocked;
    AetherExecutor* executor = NULL;
    AetherExecutorLimits limits = {1U, 4U, 4U, 1U, 1U};
    AetherId first;
    AetherId second;
    AetherId third = {123U};
    AetherId taken;
    AetherWorkResult result;
    CHECK(gate_init(&finished, bytes) == AETHER_STATUS_OK);
    CHECK(gate_init(&blocked, bytes) == AETHER_STATUS_OK);
    finished.released = true;
    CHECK(aether_executor_create(limits, &executor) == AETHER_STATUS_OK);
    CHECK(aether_executor_submit(executor, work_input(&finished, 4U), &first) ==
          AETHER_STATUS_OK);
    gate_wait(&finished, 1U, true);
    CHECK(aether_executor_submit(executor, work_input(&blocked, 4U), &second) ==
          AETHER_STATUS_OK);
    gate_wait(&blocked, 1U, false); /* One worker proves first result was published. */
    CHECK(aether_executor_cancel(executor, first, 8U) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_executor_cancel(executor, first, 7U) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(aether_executor_submit(executor, work_input(&blocked, 4U), &third) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(third.value == 0U);
    CHECK(aether_executor_take(executor, &result) == AETHER_STATUS_OK);
    CHECK(result.work_id.value == first.value && result.status == AETHER_STATUS_OK);
    CHECK(aether_executor_cancel(executor, first, 7U) == AETHER_STATUS_NOT_FOUND);
    CHECK(aether_executor_submit(executor, work_input(&blocked, 4U), &third) ==
          AETHER_STATUS_OK);
    CHECK(third.value != first.value && third.value != second.value);
    CHECK(aether_executor_request_close(executor) == AETHER_STATUS_OK);
    gate_release(&blocked);
    gate_wait(&blocked, 1U, true);
    CHECK(wait_closed(executor));
    CHECK(blocked.entered == 1U);
    CHECK(aether_executor_take(executor, &result) == AETHER_STATUS_OK);
    CHECK(result.work_id.value == second.value || result.work_id.value == third.value);
    taken = result.work_id;
    CHECK(aether_executor_take(executor, &result) == AETHER_STATUS_OK);
    CHECK(result.work_id.value == second.value || result.work_id.value == third.value);
    CHECK(result.work_id.value != taken.value);
    CHECK(aether_executor_destroy(&executor) == AETHER_STATUS_OK);
    aether_platform_monitor_destroy(blocked.monitor);
    aether_platform_monitor_destroy(finished.monitor);
    return EXIT_SUCCESS;
}

static int four_workers_cancel(void)
{
    const unsigned char bytes[] = {1U, 2U, 3U, 4U};
    WorkGate gate;
    AetherExecutor* executor = NULL;
    AetherExecutorLimits limits = {1U, 4U, 4U, 4U, 1U};
    AetherId ids[4];
    AetherWorkResult result;
    unsigned int index;
    CHECK(gate_init(&gate, bytes) == AETHER_STATUS_OK);
    gate.cooperative = true;
    CHECK(aether_executor_create(limits, &executor) == AETHER_STATUS_OK);
    for (index = 0U; index < 4U; ++index) {
        CHECK(aether_executor_submit(executor, work_input(&gate, 4U), &ids[index]) ==
              AETHER_STATUS_OK);
        gate_wait(&gate, index + 1U, false);
        CHECK(aether_executor_cancel(executor, ids[index], 7U) == AETHER_STATUS_OK);
    }
    gate_release(&gate);
    gate_wait(&gate, 4U, true);
    CHECK(aether_executor_request_close(executor) == AETHER_STATUS_OK);
    CHECK(wait_closed(executor));
    for (index = 0U; index < 4U; ++index) {
        CHECK(aether_executor_take(executor, &result) == AETHER_STATUS_OK);
        CHECK(result.status == AETHER_STATUS_CANCELLED && result.generation == 7U);
    }
    CHECK(gate.entered == 4U && gate.cancelled && gate.copied && gate.metadata);
    CHECK(aether_executor_destroy(&executor) == AETHER_STATUS_OK);
    aether_platform_monitor_destroy(gate.monitor);
    return EXIT_SUCCESS;
}

int main(void)
{
    CHECK(invalid_arguments() == EXIT_SUCCESS);
    CHECK(queue_bounds_and_copy() == EXIT_SUCCESS);
    CHECK(retention_and_close() == EXIT_SUCCESS);
    CHECK(completed_slot_reuse() == EXIT_SUCCESS);
    CHECK(four_workers_cancel() == EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
