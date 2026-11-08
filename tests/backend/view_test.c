#include "query_fixture.h"
#include "aether/backend/state_command.h"
#include "aether/platform/thread.h"
#include <time.h>

typedef struct ViewProbe {
    AetherPlatformMonitor* monitor;
    AetherViewEvent events[8];
    size_t count;
    size_t block_at;
    unsigned int closed;
    bool entered;
    bool released;
    bool active;
    bool alive;
    AetherId closed_id;
} ViewProbe;

static time_t deadline(void)
{
    time_t now = time(NULL);
    CHECK(now != (time_t)-1);
    return now + (time_t)10;
}

static void within(time_t end)
{
    time_t now = time(NULL);
    CHECK(now != (time_t)-1 && now <= end);
}

static AetherAsyncQueryResult take(AetherAsyncQueries* client)
{
    AetherAsyncQueryResult result;
    time_t end = deadline();
    AetherStatus status;
    do {
        status = aether_backend_async_query_take(client, &result);
        within(end);
    } while (status == AETHER_STATUS_NOT_FOUND);
    OK(status);
    return result;
}

static void event_received(void* context, const AetherViewEvent* event)
{
    ViewProbe* probe = context;
    aether_platform_monitor_lock(probe->monitor);
    CHECK(probe->alive && !probe->active && probe->closed == 0U);
    CHECK(probe->count < sizeof(probe->events) / sizeof(probe->events[0]));
    probe->active = true;
    /* Delivery payload is borrowed: retain only an independent value copy. */
    probe->events[probe->count++] = *event;
    if (probe->count == probe->block_at) {
        probe->entered = true;
        aether_platform_monitor_wake_all(probe->monitor);
        while (!probe->released) aether_platform_monitor_wait(probe->monitor);
    }
    probe->active = false;
    aether_platform_monitor_unlock(probe->monitor);
}

static void view_closed(void* context, AetherId subscription)
{
    ViewProbe* probe = context;
    aether_platform_monitor_lock(probe->monitor);
    CHECK(probe->alive && !probe->active && probe->closed == 0U);
    probe->closed_id = subscription;
    ++probe->closed;
    aether_platform_monitor_wake_all(probe->monitor);
    aether_platform_monitor_unlock(probe->monitor);
}

static void probe_create(ViewProbe* probe)
{
    memset(probe, 0, sizeof(*probe));
    OK(aether_platform_monitor_create(&probe->monitor));
    probe->alive = true;
}

static AetherViewReceiver receiver(ViewProbe* probe)
{
    return (AetherViewReceiver){event_received, view_closed, probe};
}

static void wait_count(ViewProbe* probe, size_t count, bool entered)
{
    time_t end = deadline();
    bool ready;
    do {
        aether_platform_monitor_lock(probe->monitor);
        ready = probe->count >= count && (!entered || probe->entered);
        aether_platform_monitor_unlock(probe->monitor);
        within(end);
    } while (!ready);
}

static void unblock(ViewProbe* probe)
{
    aether_platform_monitor_lock(probe->monitor);
    probe->released = true;
    aether_platform_monitor_wake_all(probe->monitor);
    aether_platform_monitor_unlock(probe->monitor);
}

static void health_is(AetherAsyncQueries* client, AetherId id, AetherViewHealth expected)
{
    AetherViewHealth actual;
    OK(aether_backend_view_health(client, id, &actual));
    CHECK(actual == expected);
}

static void wait_closed(AetherAsyncQueries* client, AetherId id)
{
    AetherViewHealth health;
    time_t end = deadline();
    do {
        OK(aether_backend_view_health(client, id, &health));
        within(end);
    } while (health != AETHER_VIEW_CLOSED);
}

static AetherAsyncQueryResult rename_workspace(
    AetherAsyncQueries* client, QueryFixture* f, uint64_t revision, const char* name)
{
    AetherAsyncQueryResult result;
    AetherId id;
    OK(aether_backend_async_rename_workspace(client,
        (AetherStateCommandContext){f->scoped, true},
        (AetherWorkspaceRenameInput){f->workspace[0].id, revision, text(name)}, 71U, &id));
    result = take(client);
    CHECK(result.query_id.value == id.value && result.receiver_generation == 71U);
    CHECK(result.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE);
    CHECK(result.status == AETHER_STATUS_OK);
    CHECK(result.value.workspace.revision == revision + 1U);
    CHECK(strcmp(result.value.workspace.display_name, name) == 0);
    return result;
}

static void check_event(const AetherViewEvent* event, QueryFixture* f,
                        AetherId subscription, uint64_t sequence, uint64_t revision)
{
    CHECK(event->schema_version == 1U && event->event_id.value != 0U);
    CHECK(event->subscription_id.value == subscription.value);
    CHECK(event->sequence == sequence && event->revision == revision);
    CHECK(event->workspace_id.value == f->workspace[0].id.value);
    CHECK(event->timestamp_ms != 0U && event->trace.valid);
    CHECK(memcmp(event->trace.trace_id.bytes, f->scoped.trace.trace_id.bytes,
                 sizeof(event->trace.trace_id.bytes)) == 0);
    CHECK(memcmp(event->trace.span_id.bytes, f->scoped.trace.span_id.bytes,
                 sizeof(event->trace.span_id.bytes)) == 0);
    CHECK(event->trace.has_parent == f->scoped.trace.has_parent);
    CHECK(memcmp(event->trace.parent_span_id.bytes, f->scoped.trace.parent_span_id.bytes,
                 sizeof(event->trace.parent_span_id.bytes)) == 0);
}

static AetherViewSnapshot workspace_baseline(
    AetherAsyncQueries* client, QueryFixture* f, ViewProbe* probe)
{
    AetherAsyncQueryResult opened = {0};
    AetherAsyncQueryResult renamed = {0};
    AetherViewSnapshot snapshot;
    AetherViewReceiver callbacks = receiver(probe);
    AetherId open_id;
    AetherId command_id;
    OK(aether_backend_async_open_view(client, f->scoped, AETHER_VIEW_WORKSPACE,
        f->workspace[0].id, 71U, 1U, callbacks, &open_id));
    /* Registration and baseline must precede the very next state-owner command. */
    OK(aether_backend_async_rename_workspace(client,
        (AetherStateCommandContext){f->scoped, true},
        (AetherWorkspaceRenameInput){f->workspace[0].id, 1U, text("First")},
        71U, &command_id));
    memset(&callbacks, 0, sizeof(callbacks));
    for (size_t index = 0U; index < 2U; ++index) {
        AetherAsyncQueryResult result = take(client);
        if (result.query_id.value == open_id.value) opened = result;
        else {
            CHECK(result.query_id.value == command_id.value);
            renamed = result;
        }
    }
    CHECK(opened.query_id.value == open_id.value && opened.status == AETHER_STATUS_OK);
    CHECK(opened.kind == AETHER_ASYNC_OPEN_WORKSPACE_VIEW);
    CHECK(opened.receiver_generation == 71U);
    CHECK(renamed.query_id.value == command_id.value && renamed.status == AETHER_STATUS_OK);
    CHECK(renamed.value.workspace.revision == 2U);
    snapshot = opened.value.view;
    CHECK(snapshot.subscription_id.value != 0U && snapshot.kind == AETHER_VIEW_WORKSPACE);
    CHECK(snapshot.base_revision == 1U && snapshot.sequence == 0U);
    CHECK(snapshot.value.workspace.id.value == f->workspace[0].id.value);
    CHECK(snapshot.value.workspace.revision == snapshot.base_revision);
    CHECK(strcmp(snapshot.value.workspace.display_name, "Primary") == 0);
    aether_backend_async_query_result_dispose(&opened);
    aether_backend_async_query_result_dispose(&renamed);
    wait_count(probe, 1U, false);
    aether_platform_monitor_lock(probe->monitor);
    check_event(&probe->events[0], f, snapshot.subscription_id, 1U, 2U);
    CHECK(probe->events[0].kind == AETHER_VIEW_WORKSPACE);
    CHECK(probe->events[0].object_id.value == f->workspace[0].id.value);
    CHECK(probe->events[0].session_id.value == 0U && probe->events[0].runtime_id.value == 0U);
    CHECK(probe->events[0].value.workspace.revision == 2U);
    CHECK(strcmp(probe->events[0].value.workspace.display_name, "First") == 0);
    aether_platform_monitor_unlock(probe->monitor);
    return snapshot;
}

static void foreign_open(AetherAsyncQueries* client, QueryFixture* f, ViewProbe* probe)
{
    const AetherViewKind kinds[] = {AETHER_VIEW_WORKSPACE, AETHER_VIEW_SESSION};
    const AetherId objects[] = {f->workspace[1].id, f->session[1].id};
    for (size_t index = 0U; index < 2U; ++index) {
        AetherAsyncQueryResult result;
        AetherId id;
        OK(aether_backend_async_open_view(client, f->scoped, kinds[index], objects[index],
            72U, 1U, receiver(probe), &id));
        result = take(client);
        CHECK(result.query_id.value == id.value && result.receiver_generation == 72U);
        CHECK(result.kind == (index == 0U ? AETHER_ASYNC_OPEN_WORKSPACE_VIEW :
                              AETHER_ASYNC_OPEN_SESSION_VIEW));
        CHECK(result.status == AETHER_STATUS_PERMISSION_DENIED);
        CHECK(result.error.code == AETHER_QUERY_ERROR_FORBIDDEN);
        CHECK(result.value.view.subscription_id.value == 0U);
        aether_backend_async_query_result_dispose(&result);
    }
}

static void overflow(AetherAsyncQueries* client, QueryFixture* f, ViewProbe* probe,
                     AetherId subscription)
{
    AetherAsyncQueryResult result;
    AetherAsyncQueryInput query = {0};
    AetherId id;
    wait_count(probe, 1U, true);
    result = rename_workspace(client, f, 2U, "Queued");
    aether_backend_async_query_result_dispose(&result);
    health_is(client, subscription, AETHER_VIEW_HEALTHY);
    result = rename_workspace(client, f, 3U, "Overflowed");
    aether_backend_async_query_result_dispose(&result);
    health_is(client, subscription, AETHER_VIEW_RESYNC_REQUIRED);
    result = rename_workspace(client, f, 4U, "Final");
    aether_backend_async_query_result_dispose(&result);
    health_is(client, subscription, AETHER_VIEW_RESYNC_REQUIRED);
    query.kind = AETHER_ASYNC_QUERY_WORKSPACE;
    query.context = f->scoped;
    query.id = f->workspace[0].id;
    query.receiver_generation = 71U;
    OK(aether_backend_async_query_submit(client, query, &id));
    result = take(client);
    CHECK(result.query_id.value == id.value && result.status == AETHER_STATUS_OK);
    CHECK(result.value.workspace.revision == 5U);
    CHECK(strcmp(result.value.workspace.display_name, "Final") == 0);
    aether_backend_async_query_result_dispose(&result);
    CHECK(aether_backend_view_release(client, subscription) == AETHER_STATUS_ALREADY_EXISTS);
    unblock(probe);
    OK(aether_backend_unsubscribe(client, subscription));
    wait_closed(client, subscription);
    aether_platform_monitor_lock(probe->monitor);
    CHECK(probe->count == 1U && probe->closed == 1U);
    CHECK(probe->closed_id.value == subscription.value);
    check_event(&probe->events[0], f, subscription, 1U, 2U);
    CHECK(strcmp(probe->events[0].value.workspace.display_name, "First") == 0);
    aether_platform_monitor_unlock(probe->monitor);
    OK(aether_backend_view_release(client, subscription));
}

static AetherAsyncQueryResult close_session(
    AetherAsyncQueries* client, QueryFixture* f, uint64_t revision)
{
    AetherAsyncQueryResult result;
    AetherId id;
    OK(aether_backend_async_request_session_close(client,
        (AetherStateCommandContext){f->scoped, true},
        (AetherSessionCloseInput){f->session[0].id, revision}, 73U, &id));
    result = take(client);
    CHECK(result.query_id.value == id.value && result.status == AETHER_STATUS_OK);
    CHECK(result.kind == AETHER_ASYNC_COMMAND_CLOSE_SESSION);
    CHECK(result.value.session.state == AETHER_SESSION_DTO_CLOSING);
    CHECK(result.value.session.revision == 3U);
    return result;
}

static AetherViewSnapshot session_noop(
    AetherAsyncQueries* client, QueryFixture* f, ViewProbe* probe)
{
    AetherAsyncQueryResult result;
    AetherViewSnapshot snapshot;
    AetherId id;
    probe->block_at = 1U;
    OK(aether_backend_async_open_view(client, f->scoped, AETHER_VIEW_SESSION,
        f->session[0].id, 73U, 1U, receiver(probe), &id));
    result = take(client);
    CHECK(result.query_id.value == id.value && result.status == AETHER_STATUS_OK);
    CHECK(result.kind == AETHER_ASYNC_OPEN_SESSION_VIEW && result.receiver_generation == 73U);
    snapshot = result.value.view;
    CHECK(snapshot.kind == AETHER_VIEW_SESSION && snapshot.subscription_id.value != 0U);
    CHECK(snapshot.base_revision == 2U && snapshot.sequence == 0U);
    CHECK(snapshot.value.session.id.value == f->session[0].id.value);
    CHECK(snapshot.value.session.state == AETHER_SESSION_DTO_IDLE);
    CHECK(snapshot.value.session.revision == 2U);
    aether_backend_async_query_result_dispose(&result);
    result = close_session(client, f, 2U);
    aether_backend_async_query_result_dispose(&result);
    wait_count(probe, 1U, true);
    /* Two duplicate publications would overflow queue1 while delivery is gated. */
    for (size_t index = 0U; index < 2U; ++index) {
        result = close_session(client, f, 3U);
        aether_backend_async_query_result_dispose(&result);
    }
    health_is(client, snapshot.subscription_id, AETHER_VIEW_HEALTHY);
    OK(aether_backend_unsubscribe(client, snapshot.subscription_id));
    health_is(client, snapshot.subscription_id, AETHER_VIEW_CLOSING);
    CHECK(aether_backend_view_release(client, snapshot.subscription_id) ==
          AETHER_STATUS_ALREADY_EXISTS);
    unblock(probe);
    wait_closed(client, snapshot.subscription_id);
    aether_platform_monitor_lock(probe->monitor);
    CHECK(probe->count == 1U && probe->closed == 1U);
    check_event(&probe->events[0], f, snapshot.subscription_id, 1U, 3U);
    CHECK(probe->events[0].kind == AETHER_VIEW_SESSION);
    CHECK(probe->events[0].object_id.value == f->session[0].id.value);
    CHECK(probe->events[0].session_id.value == f->session[0].id.value);
    CHECK(probe->events[0].runtime_id.value == f->runtime[0].id.value);
    CHECK(probe->events[0].value.session.state == AETHER_SESSION_DTO_CLOSING);
    CHECK(probe->events[0].value.session.revision == 3U);
    aether_platform_monitor_unlock(probe->monitor);
    OK(aether_backend_view_release(client, snapshot.subscription_id));
    return snapshot;
}

int main(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    ViewProbe workspace;
    ViewProbe session;
    ViewProbe denied;
    AetherViewSnapshot workspace_snapshot;
    AetherViewSnapshot session_snapshot;
    time_t end;
    probe_create(&workspace);
    probe_create(&session);
    probe_create(&denied);
    workspace.block_at = 1U;
    OK(aether_backend_async_queries_create(f.queries, 4U, 65536U, &client));
    workspace_snapshot = workspace_baseline(client, &f, &workspace);
    foreign_open(client, &f, &denied);
    overflow(client, &f, &workspace, workspace_snapshot.subscription_id);
    session_snapshot = session_noop(client, &f, &session);
    CHECK(session_snapshot.subscription_id.value != workspace_snapshot.subscription_id.value);
    OK(aether_backend_async_queries_request_close(client));
    end = deadline();
    while (!aether_backend_async_queries_is_closed(client)) within(end);
    OK(aether_backend_async_queries_destroy(&client));
    dispose(&f);
    /* Value copies and receiver contexts survive source and client destruction. */
    CHECK(strcmp(workspace_snapshot.value.workspace.display_name, "Primary") == 0);
    CHECK(session_snapshot.value.session.state == AETHER_SESSION_DTO_IDLE);
    CHECK(strcmp(workspace.events[0].value.workspace.display_name, "First") == 0);
    CHECK(session.events[0].value.session.state == AETHER_SESSION_DTO_CLOSING);
    CHECK(denied.count == 0U && denied.closed == 0U);
    CHECK(workspace.closed == 1U && session.closed == 1U);
    workspace.alive = false;
    session.alive = false;
    denied.alive = false;
    aether_platform_monitor_destroy(workspace.monitor);
    aether_platform_monitor_destroy(session.monitor);
    aether_platform_monitor_destroy(denied.monitor);
    return 0;
}
