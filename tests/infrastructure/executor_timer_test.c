#include "aether/infrastructure/executor.h"

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

static bool empty_fact(AetherTimerFact fact)
{
    return fact.timer_id.value == 0U && fact.target_id.value == 0U &&
        fact.generation == 0U && fact.deadline_ns == 0U &&
        same_trace(fact.trace, (AetherTraceContext){0});
}

static bool wait_closed(AetherExecutor* executor)
{
    unsigned int attempt;
    for (attempt = 0U; attempt < 100000000U; ++attempt) {
        if (aether_executor_is_closed(executor)) {
            return true;
        }
    }
    return false;
}

int main(void)
{
    AetherExecutor* executor = NULL;
    AetherExecutorLimits limits = {1U, 1U, 1U, 1U, 2U};
    AetherTraceContext trace = trace_context();
    AetherTraceContext original = trace;
    AetherId target = aether_id_next();
    AetherId first;
    AetherId second;
    AetherId third;
    AetherId rejected = {123U};
    AetherTimerFact fact;

    memset(&fact, 0xA5, sizeof(fact));
    CHECK(aether_executor_take_timer(NULL, 1U, &fact) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(empty_fact(fact));
    CHECK(aether_executor_create(limits, &executor) == AETHER_STATUS_OK);
    CHECK(aether_executor_schedule_timer(executor, (AetherId){0U}, 7U, 10U,
              trace, &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(rejected.value == 0U);
    CHECK(aether_executor_schedule_timer(executor, target, 0U, 10U, trace,
              &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    trace.valid = false;
    CHECK(aether_executor_schedule_timer(executor, target, 7U, 10U, trace,
              &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    trace = original;
    CHECK(aether_executor_schedule_timer(executor, target, 7U, 10U, trace,
              &first) == AETHER_STATUS_OK);
    CHECK(aether_executor_schedule_timer(executor, target, 8U, 20U, trace,
              &second) == AETHER_STATUS_OK);
    CHECK(aether_executor_schedule_timer(executor, target, 9U, 0U, trace,
              &rejected) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.value == 0U);
    trace.trace_id.bytes[0] = 99U;
    memset(&fact, 0xA5, sizeof(fact));
    CHECK(aether_executor_take_timer(executor, 9U, &fact) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(empty_fact(fact));
    CHECK(aether_executor_take_timer(executor, 10U, &fact) == AETHER_STATUS_OK);
    CHECK(fact.timer_id.value == first.value && fact.target_id.value == target.value);
    CHECK(fact.deadline_ns == 10U && same_trace(fact.trace, original));
    /* The owner has advanced; a late fact must retain its original generation. */
    CHECK(fact.generation == 7U && fact.generation != 8U);
    CHECK(aether_executor_cancel_timer(executor, first) == AETHER_STATUS_NOT_FOUND);
    CHECK(fact.timer_id.value == first.value && fact.generation == 7U);
    CHECK(aether_executor_schedule_timer(executor, target, 9U, 5U, original,
              &third) == AETHER_STATUS_OK);
    CHECK(third.value != first.value && third.value != second.value);
    CHECK(aether_executor_take_timer(executor, 10U, &fact) == AETHER_STATUS_OK);
    CHECK(fact.timer_id.value == third.value && fact.deadline_ns == 5U &&
          fact.generation == 9U);
    CHECK(aether_executor_cancel_timer(executor, second) == AETHER_STATUS_OK);
    CHECK(aether_executor_cancel_timer(executor, second) == AETHER_STATUS_NOT_FOUND);
    CHECK(aether_executor_take_timer(executor, UINT64_MAX, &fact) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(empty_fact(fact));
    CHECK(aether_executor_schedule_timer(executor, target, 10U, 30U, original,
              &first) == AETHER_STATUS_OK);
    CHECK(aether_executor_schedule_timer(executor, target, 11U, 31U, original,
              &second) == AETHER_STATUS_OK);
    CHECK(aether_executor_take_timer(executor, UINT64_MAX, &fact) == AETHER_STATUS_OK);
    CHECK(fact.timer_id.value == first.value && fact.deadline_ns == 30U);
    CHECK(fact.generation == 10U && fact.target_id.value == target.value);
    CHECK(aether_executor_request_close(executor) == AETHER_STATUS_OK);
    CHECK(wait_closed(executor));
    CHECK(aether_executor_take_timer(executor, UINT64_MAX, &fact) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(empty_fact(fact));
    rejected.value = 123U;
    CHECK(aether_executor_schedule_timer(executor, target, 12U, 32U, original,
              &rejected) == AETHER_STATUS_NOT_INITIALIZED);
    CHECK(rejected.value == 0U);
    CHECK(aether_executor_destroy(&executor) == AETHER_STATUS_OK && executor == NULL);
    return EXIT_SUCCESS;
}
