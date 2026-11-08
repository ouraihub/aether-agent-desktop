/* Only async_query.c substitutes this Backend function in this test target. */
#undef aether_backend_query_block
#include "query_fixture.h"
#include "aether/backend/async_query.h"
#include "aether/platform/thread.h"

static AetherPlatformMonitor* gate;
static bool entered;
static bool released;
static unsigned int executions;

AetherStatus aether_test_query_block(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId message, uint32_t ordinal, size_t max_bytes,
    AetherBlockDto** out_dto, AetherQueryError* out_error
);

AetherStatus aether_test_query_block(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId message, uint32_t ordinal, size_t max_bytes,
    AetherBlockDto** out_dto, AetherQueryError* out_error
)
{
    aether_platform_monitor_lock(gate);
    ++executions;
    entered = true;
    aether_platform_monitor_wake_all(gate);
    while (!released) aether_platform_monitor_wait(gate);
    aether_platform_monitor_unlock(gate);
    return aether_backend_query_block(queries, context, message, ordinal,
                                      max_bytes, out_dto, out_error);
}

int main(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherAsyncQueries* saved;
    AetherAsyncQueryInput input = {0};
    AetherAsyncQueryResult result;
    AetherId running;
    AetherId queued;
    AetherId rejected;
    bool seen_running = false;
    bool seen_queued = false;
    OK(aether_platform_monitor_create(&gate));
    OK(aether_backend_async_queries_create(f.queries, 2U, 4096U, &client));
    input.kind = AETHER_ASYNC_QUERY_BLOCK;
    input.context = f.scoped;
    input.id = f.message[0].id;
    input.max_bytes = 1024U;
    input.receiver_generation = 31U;
    OK(aether_backend_async_query_submit(client, input, &running));
    aether_platform_monitor_lock(gate);
    while (!entered) aether_platform_monitor_wait(gate);
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_async_query_submit(client, input, &queued));
    OK(aether_backend_async_queries_request_close(client));
    CHECK(!aether_backend_async_queries_is_closed(client));
    saved = client;
    CHECK(aether_backend_async_queries_destroy(&client) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(client == saved);
    CHECK(aether_backend_async_query_submit(client, input, &rejected) ==
          AETHER_STATUS_NOT_INITIALIZED);
    CHECK(rejected.value == 0U);
    aether_platform_monitor_lock(gate);
    released = true;
    aether_platform_monitor_wake_all(gate);
    aether_platform_monitor_unlock(gate);
    while (!aether_backend_async_queries_is_closed(client)) { }
    CHECK(executions == 1U);
    for (size_t index = 0U; index < 2U; ++index) {
        OK(aether_backend_async_query_take(client, &result));
        CHECK(result.status == AETHER_STATUS_CANCELLED);
        CHECK(result.kind == AETHER_ASYNC_QUERY_BLOCK && result.value.block == NULL);
        CHECK(result.receiver_generation == 31U && result.error.diagnostic_available);
        CHECK(result.error.diagnostic_id.bytes[0] == f.scoped.trace.trace_id.bytes[0]);
        if (result.query_id.value == running.value) {
            CHECK(!seen_running);
            seen_running = true;
        } else {
            CHECK(result.query_id.value == queued.value && !seen_queued);
            seen_queued = true;
        }
        aether_backend_async_query_result_dispose(&result);
    }
    CHECK(seen_running && seen_queued);
    OK(aether_backend_async_queries_destroy(&client));
    aether_platform_monitor_destroy(gate);
    dispose(&f);
    return 0;
}
