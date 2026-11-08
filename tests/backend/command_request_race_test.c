/* The target substitutes these calls in async_query.c and command_record.c only. */
#undef aether_backend_rename_workspace
#undef aether_platform_clock_now
#include "query_fixture.h"
#include "aether/backend/command_request.h"
#include "aether/platform/platform.h"
#include "aether/platform/thread.h"

static AetherPlatformMonitor* gate;
static uint64_t now_ns;
static bool committed;
static bool released;
static unsigned int executions;

AetherStatus aether_test_clock_now(AetherPlatformClock* out_clock);
AetherStatus aether_test_rename_workspace(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherWorkspaceRenameInput input, AetherWorkspaceDto* out_dto,
    AetherQueryError* out_error);

AetherStatus aether_test_clock_now(AetherPlatformClock* out_clock)
{
    if (out_clock == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(gate);
    *out_clock = (AetherPlatformClock){now_ns, 0U};
    aether_platform_monitor_unlock(gate);
    return AETHER_STATUS_OK;
}

AetherStatus aether_test_rename_workspace(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherWorkspaceRenameInput input, AetherWorkspaceDto* out_dto,
    AetherQueryError* out_error)
{
    AetherStatus status = aether_backend_rename_workspace(queries, context, input,
                                                         out_dto, out_error);
    aether_platform_monitor_lock(gate);
    ++executions;
    committed = true;
    aether_platform_monitor_wake_all(gate);
    while (!released) aether_platform_monitor_wait(gate);
    aether_platform_monitor_unlock(gate);
    return status;
}

static AetherCommandRequestSnapshot query_request(
    AetherAsyncQueries* client, const AetherRequestContext* context, AetherId id)
{
    AetherCommandRequestSnapshot snapshot;
    AetherQueryError error;
    OK(aether_backend_query_command_request(client, context, id, &snapshot, &error));
    CHECK(snapshot.request_id.value == id.value);
    return snapshot;
}

static AetherAsyncQueryResult take_work(AetherAsyncQueries* client)
{
    AetherAsyncQueryResult result;
    AetherStatus status;
    do { status = aether_backend_async_query_take(client, &result); }
    while (status == AETHER_STATUS_NOT_FOUND);
    OK(status);
    return result;
}

static void expect_no_completion(
    AetherAsyncQueries* client, const AetherRequestContext* context)
{
    AetherCommandRequestSnapshot snapshot;
    AetherQueryError error;
    memset(&snapshot, 0xA5, sizeof(snapshot));
    CHECK(aether_backend_take_command_completed(client, context, &snapshot, &error) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.request_id.value == 0U && snapshot.terminal_revision == 0U);
}

static void check_running(AetherCommandRequestSnapshot snapshot, AetherId id)
{
    CHECK(snapshot.request_id.value == id.value);
    CHECK(snapshot.state == AETHER_COMMAND_RUNNING);
    CHECK(snapshot.deadline_ns == 200U);
    CHECK(snapshot.execution_started && !snapshot.execution_complete);
    CHECK(snapshot.terminal_revision == 0U && snapshot.retain_until_ns == 0U);
}

static void exercise_race(bool expire)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherStateCommandContext writer = {f.scoped, true};
    AetherStateCommandContext reader = {f.scoped, false};
    AetherCommandRequestSnapshot snapshot;
    AetherCommandRequestSnapshot completion = {0};
    AetherAsyncQueryResult work;
    AetherWorkspaceDto workspace;
    AetherQueryError error;
    AetherId id;
    uint64_t retain_until = 0U;
    aether_platform_monitor_lock(gate);
    now_ns = 100U;
    committed = false;
    released = false;
    executions = 0U;
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_async_queries_create(f.queries, 2U, 4096U, &client));
    OK(aether_backend_submit_rename_request(client, writer,
        (AetherWorkspaceRenameInput){f.workspace[0].id, 1U, text("Committed")},
        4U, 100U, &id));
    aether_platform_monitor_lock(gate);
    while (!committed) aether_platform_monitor_wait(gate);
    CHECK(executions == 1U);
    aether_platform_monitor_unlock(gate);
    snapshot = query_request(client, &f.scoped, id);
    check_running(snapshot, id);
    CHECK(!snapshot.cancel_requested);
    expect_no_completion(client, &f.scoped);
    if (expire) {
        aether_platform_monitor_lock(gate);
        now_ns = 200U;
        aether_platform_monitor_unlock(gate);
        snapshot = query_request(client, &f.scoped, id);
        CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT);
        CHECK(snapshot.result == AETHER_STATUS_TIMEOUT);
        CHECK(snapshot.error.status == AETHER_STATUS_TIMEOUT);
        CHECK(snapshot.execution_started && !snapshot.execution_complete);
        CHECK(snapshot.terminal_revision == 1U);
        CHECK(snapshot.retain_until_ns == UINT64_C(600000000200));
        retain_until = snapshot.retain_until_ns;
        OK(aether_backend_cancel_command_request(client, &writer, id, &snapshot, &error));
        CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT && !snapshot.cancel_requested);
        CHECK(aether_backend_release_command_request(client, &f.scoped, id, &error) ==
              AETHER_STATUS_ALREADY_EXISTS);
        OK(aether_backend_take_command_completed(client, &f.scoped, &completion, &error));
        CHECK(completion.request_id.value == id.value);
        CHECK(completion.state == AETHER_COMMAND_TIMED_OUT);
        CHECK(completion.result == AETHER_STATUS_TIMEOUT);
        CHECK(!completion.execution_complete && completion.terminal_revision == 1U);
        expect_no_completion(client, &f.scoped);
        CHECK(aether_backend_release_command_request(client, &f.scoped, id, &error) ==
              AETHER_STATUS_ALREADY_EXISTS);
        snapshot = query_request(client, &f.scoped, id);
        CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT && !snapshot.execution_complete);
        CHECK(snapshot.retain_until_ns == retain_until && snapshot.terminal_revision == 1U);
    } else {
        memset(&snapshot, 0xA5, sizeof(snapshot));
        CHECK(aether_backend_cancel_command_request(client, &reader, id, &snapshot, &error) ==
              AETHER_STATUS_PERMISSION_DENIED);
        CHECK(snapshot.request_id.value == 0U);
        snapshot = query_request(client, &f.scoped, id);
        check_running(snapshot, id);
        CHECK(!snapshot.cancel_requested);
        /* Cancellation carries its own operation trace, not the target's trace. */
        writer.request.trace.trace_id.bytes[0] = 0x71U;
        writer.request.trace.span_id.bytes[0] = 0x72U;
        OK(aether_backend_cancel_command_request(client, &writer, id, &snapshot, &error));
        check_running(snapshot, id);
        CHECK(snapshot.cancel_requested);
        snapshot = query_request(client, &f.scoped, id);
        check_running(snapshot, id);
        CHECK(snapshot.cancel_requested);
        CHECK(snapshot.trace.trace_id.bytes[0] == f.scoped.trace.trace_id.bytes[0]);
        expect_no_completion(client, &f.scoped);
        CHECK(aether_backend_release_command_request(client, &f.scoped, id, &error) ==
              AETHER_STATUS_ALREADY_EXISTS);
    }
    aether_platform_monitor_lock(gate);
    released = true;
    aether_platform_monitor_wake_all(gate);
    aether_platform_monitor_unlock(gate);
    work = take_work(client);
    CHECK(work.query_id.value == id.value);
    CHECK(work.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE);
    CHECK(work.value.workspace.revision == 2U);
    CHECK(strcmp(work.value.workspace.display_name, "Committed") == 0);
    if (!expire) CHECK(work.status == AETHER_STATUS_OK);
    aether_backend_async_query_result_dispose(&work);
    snapshot = query_request(client, &f.scoped, id);
    CHECK(snapshot.execution_started && snapshot.execution_complete);
    CHECK(snapshot.execution_status == AETHER_STATUS_OK);
    CHECK(snapshot.value.workspace.id.value == f.workspace[0].id.value);
    CHECK(snapshot.value.workspace.revision == 2U);
    CHECK(strcmp(snapshot.value.workspace.display_name, "Committed") == 0);
    CHECK(snapshot.terminal_revision == 1U);
    if (expire) {
        CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT);
        CHECK(snapshot.result == AETHER_STATUS_TIMEOUT);
        CHECK(snapshot.error.status == AETHER_STATUS_TIMEOUT);
        CHECK(snapshot.retain_until_ns == retain_until);
        CHECK(!snapshot.cancel_requested);
        CHECK(!completion.execution_complete);
    } else {
        CHECK(snapshot.state == AETHER_COMMAND_SUCCEEDED);
        CHECK(snapshot.result == AETHER_STATUS_OK && snapshot.cancel_requested);
        CHECK(aether_backend_release_command_request(client, &f.scoped, id, &error) ==
              AETHER_STATUS_ALREADY_EXISTS);
        OK(aether_backend_take_command_completed(client, &f.scoped, &completion, &error));
        CHECK(completion.request_id.value == id.value);
        CHECK(completion.state == AETHER_COMMAND_SUCCEEDED);
        CHECK(completion.execution_complete && completion.execution_status == AETHER_STATUS_OK);
        CHECK(completion.cancel_requested && completion.terminal_revision == 1U);
    }
    expect_no_completion(client, &f.scoped);
    OK(aether_backend_release_command_request(client, &f.scoped, id, &error));
    memset(&snapshot, 0xA5, sizeof(snapshot));
    CHECK(aether_backend_query_command_request(client, &f.scoped, id, &snapshot, &error) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.request_id.value == 0U);
    expect_no_completion(client, &f.scoped);
    OK(aether_backend_async_queries_request_close(client));
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    OK(aether_backend_query_workspace(f.queries, &f.scoped, f.workspace[0].id,
                                      &workspace, &error));
    CHECK(workspace.revision == 2U);
    CHECK(strcmp(workspace.display_name, "Committed") == 0);
    dispose(&f);
}

static void queued_deadline(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherStateCommandContext writer = {f.scoped, true};
    AetherCommandRequestSnapshot snapshot;
    AetherAsyncQueryResult work;
    AetherWorkspaceDto workspace;
    AetherQueryError error;
    AetherId running;
    AetherId queued;
    AetherId admitted;
    AetherId replacement;
    bool saw_running = false;
    bool saw_queued = false;
    bool saw_admitted = false;
    aether_platform_monitor_lock(gate);
    now_ns = 100U;
    committed = false;
    released = false;
    executions = 0U;
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_async_queries_create(f.queries, 3U, 4096U, &client));
    OK(aether_backend_submit_rename_request(client, writer,
        (AetherWorkspaceRenameInput){f.workspace[0].id, 1U, text("Only mutation")},
        5U, UINT64_C(600000000000), &running));
    aether_platform_monitor_lock(gate);
    while (!committed) aether_platform_monitor_wait(gate);
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_submit_rename_request(client, writer,
        (AetherWorkspaceRenameInput){f.workspace[0].id, 2U, text("Never applied")},
        5U, 100U, &queued));
    snapshot = query_request(client, &f.scoped, queued);
    CHECK(snapshot.state == AETHER_COMMAND_ACCEPTED);
    CHECK(!snapshot.execution_started && !snapshot.execution_complete);
    aether_platform_monitor_lock(gate);
    now_ns = 200U;
    aether_platform_monitor_unlock(gate);
    snapshot = query_request(client, &f.scoped, queued);
    CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT);
    CHECK(snapshot.result == AETHER_STATUS_TIMEOUT && snapshot.terminal_revision == 1U);
    CHECK(!snapshot.execution_started && !snapshot.execution_complete);
    CHECK(snapshot.retain_until_ns == UINT64_C(600000000200));
    OK(aether_backend_take_command_completed(client, &f.scoped, &snapshot, &error));
    CHECK(snapshot.request_id.value == queued.value);
    CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT && !snapshot.execution_complete);
    aether_platform_monitor_lock(gate);
    now_ns = UINT64_C(600000000200);
    aether_platform_monitor_unlock(gate);
    CHECK(aether_backend_release_command_request(client, &f.scoped, queued, &error) ==
          AETHER_STATUS_ALREADY_EXISTS);
    /* Admission at retention expiry cannot recycle the queued work's record. */
    OK(aether_backend_submit_close_request(client, writer,
        (AetherSessionCloseInput){f.session[0].id, 2U}, 5U, 100U, &admitted));
    snapshot = query_request(client, &f.scoped, queued);
    CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT && !snapshot.execution_complete);
    CHECK(snapshot.terminal_revision == 1U);
    aether_platform_monitor_lock(gate);
    released = true;
    aether_platform_monitor_wake_all(gate);
    aether_platform_monitor_unlock(gate);
    do { snapshot = query_request(client, &f.scoped, queued); }
    while (!snapshot.execution_complete);
    CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT && !snapshot.execution_started);
    /* Even an expired, completion-consumed record waits for the common take. */
    CHECK(aether_backend_release_command_request(client, &f.scoped, queued, &error) ==
          AETHER_STATUS_ALREADY_EXISTS);
    for (size_t index = 0U; index < 3U; ++index) {
        work = take_work(client);
        if (work.query_id.value == running.value) {
            CHECK(!saw_running);
            saw_running = true;
        } else if (work.query_id.value == queued.value) {
            CHECK(!saw_queued && work.status == AETHER_STATUS_TIMEOUT);
            CHECK(work.value.workspace.id.value == 0U);
            saw_queued = true;
        } else {
            CHECK(work.query_id.value == admitted.value && !saw_admitted);
            CHECK(work.status == AETHER_STATUS_OK);
            saw_admitted = true;
        }
        aether_backend_async_query_result_dispose(&work);
    }
    CHECK(saw_running && saw_queued && saw_admitted);
    snapshot = query_request(client, &f.scoped, queued);
    CHECK(snapshot.state == AETHER_COMMAND_TIMED_OUT && snapshot.execution_complete);
    CHECK(!snapshot.execution_started && snapshot.execution_status == AETHER_STATUS_TIMEOUT);
    CHECK(snapshot.terminal_revision == 1U);
    aether_platform_monitor_lock(gate);
    ++now_ns;
    aether_platform_monitor_unlock(gate);
    writer.write_allowed = false;
    OK(aether_backend_submit_close_request(client, writer,
        (AetherSessionCloseInput){f.session[0].id, 3U}, 5U, 100U, &replacement));
    CHECK(aether_backend_query_command_request(client, &f.scoped, queued, &snapshot, &error) ==
          AETHER_STATUS_NOT_FOUND);
    work = take_work(client);
    CHECK(work.query_id.value == replacement.value);
    CHECK(work.status == AETHER_STATUS_PERMISSION_DENIED);
    aether_backend_async_query_result_dispose(&work);
    for (size_t index = 0U; index < 3U; ++index) {
        OK(aether_backend_take_command_completed(client, &f.scoped, &snapshot, &error));
        CHECK(snapshot.request_id.value != queued.value);
        CHECK(snapshot.request_id.value == running.value ||
              snapshot.request_id.value == admitted.value ||
              snapshot.request_id.value == replacement.value);
        OK(aether_backend_release_command_request(client, &f.scoped,
                                                   snapshot.request_id, &error));
    }
    expect_no_completion(client, &f.scoped);
    OK(aether_backend_async_queries_request_close(client));
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    aether_platform_monitor_lock(gate);
    CHECK(executions == 1U);
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_query_workspace(f.queries, &f.scoped, f.workspace[0].id,
                                      &workspace, &error));
    CHECK(workspace.revision == 2U);
    CHECK(strcmp(workspace.display_name, "Only mutation") == 0);
    dispose(&f);
}

static void retained_until_completion(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherStateCommandContext writer = {f.all, true};
    AetherCommandRequestSnapshot snapshot;
    AetherAsyncQueryResult work;
    AetherQueryError error;
    AetherId first;
    AetherId second;
    aether_platform_monitor_lock(gate);
    now_ns = 100U;
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_async_queries_create(f.queries, 1U, 4096U, &client));
    OK(aether_backend_submit_close_request(client, writer,
        (AetherSessionCloseInput){f.session[0].id, 2U}, 6U, 100U, &first));
    work = take_work(client);
    CHECK(work.query_id.value == first.value && work.status == AETHER_STATUS_OK);
    aether_backend_async_query_result_dispose(&work);
    snapshot = query_request(client, &f.all, first);
    CHECK(snapshot.state == AETHER_COMMAND_SUCCEEDED);
    CHECK(snapshot.retain_until_ns == UINT64_C(600000000100));
    aether_platform_monitor_lock(gate);
    now_ns = snapshot.retain_until_ns;
    aether_platform_monitor_unlock(gate);
    CHECK(aether_backend_release_command_request(client, &f.all, first, &error) ==
          AETHER_STATUS_ALREADY_EXISTS);
    /* A free common work slot is not permission to lose an untaken completion. */
    OK(aether_backend_submit_close_request(client, writer,
        (AetherSessionCloseInput){f.session[1].id, 2U}, 6U, 100U, &second));
    snapshot = query_request(client, &f.all, first);
    CHECK(snapshot.state == AETHER_COMMAND_SUCCEEDED && snapshot.execution_complete);
    work = take_work(client);
    CHECK(work.query_id.value == second.value && work.status == AETHER_STATUS_OK);
    aether_backend_async_query_result_dispose(&work);
    for (size_t index = 0U; index < 2U; ++index) {
        OK(aether_backend_take_command_completed(client, &f.all, &snapshot, &error));
        CHECK(snapshot.request_id.value == first.value ||
              snapshot.request_id.value == second.value);
        OK(aether_backend_release_command_request(client, &f.all, snapshot.request_id, &error));
    }
    expect_no_completion(client, &f.all);
    OK(aether_backend_async_queries_request_close(client));
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    dispose(&f);
}

static void retained_capacity(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherStateCommandContext reader = {f.scoped, false};
    AetherCommandRequestSnapshot snapshot;
    AetherAsyncQueryResult work;
    AetherQueryError error;
    AetherId requests[128];
    AetherId rejected;
    aether_platform_monitor_lock(gate);
    now_ns = 100U;
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_async_queries_create(f.queries, 1U, 4096U, &client));
    for (size_t index = 0U; index < 128U; ++index) {
        OK(aether_backend_submit_close_request(client, reader,
            (AetherSessionCloseInput){f.session[0].id, 2U}, 8U, 100U, &requests[index]));
        work = take_work(client);
        CHECK(work.query_id.value == requests[index].value);
        CHECK(work.status == AETHER_STATUS_PERMISSION_DENIED);
        aether_backend_async_query_result_dispose(&work);
    }
    rejected = aether_id_next();
    CHECK(aether_backend_submit_close_request(client, reader,
        (AetherSessionCloseInput){f.session[0].id, 2U}, 8U, 100U, &rejected) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.value == 0U);
    aether_platform_monitor_lock(gate);
    now_ns = UINT64_C(600000000100);
    aether_platform_monitor_unlock(gate);
    CHECK(aether_backend_submit_close_request(client, reader,
        (AetherSessionCloseInput){f.session[0].id, 2U}, 8U, 100U, &rejected) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    OK(aether_backend_take_command_completed(client, &f.scoped, &snapshot, &error));
    CHECK(snapshot.state == AETHER_COMMAND_FAILED && snapshot.execution_complete);
    CHECK(snapshot.result == AETHER_STATUS_PERMISSION_DENIED);
    requests[0] = snapshot.request_id;
    OK(aether_backend_submit_close_request(client, reader,
        (AetherSessionCloseInput){f.session[0].id, 2U}, 8U, 100U, &rejected));
    CHECK(aether_backend_query_command_request(client, &f.scoped, requests[0],
                                               &snapshot, &error) == AETHER_STATUS_NOT_FOUND);
    work = take_work(client);
    CHECK(work.query_id.value == rejected.value && work.status == AETHER_STATUS_PERMISSION_DENIED);
    aether_backend_async_query_result_dispose(&work);
    for (size_t index = 0U; index < 128U; ++index) {
        OK(aether_backend_take_command_completed(client, &f.scoped, &snapshot, &error));
        CHECK(snapshot.request_id.value != requests[0].value);
        CHECK(snapshot.state == AETHER_COMMAND_FAILED && snapshot.execution_complete);
        OK(aether_backend_release_command_request(client, &f.scoped, snapshot.request_id, &error));
    }
    expect_no_completion(client, &f.scoped);
    OK(aether_backend_async_queries_request_close(client));
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    dispose(&f);
}

int main(void)
{
    OK(aether_platform_monitor_create(&gate));
    exercise_race(false);
    exercise_race(true);
    queued_deadline();
    retained_until_completion();
    retained_capacity();
    aether_platform_monitor_destroy(gate);
    return 0;
}
