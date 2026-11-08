#include "query_fixture.h"
#include "aether/backend/state_command.h"
#include "aether/platform/thread.h"
#include <time.h>

typedef struct CloseProbe {
    AetherPlatformMonitor* monitor;
    AetherAsyncQueries* client;
    AetherViewEvent event;
    AetherId closed_id;
    unsigned int events;
    unsigned int closed;
    bool alive;
    bool active;
    bool event_entered;
    bool event_release;
    bool closed_entered;
    bool closed_release;
    bool unsubscribe_inside;
    AetherStatus unsubscribe_status;
    AetherStatus health_status;
    AetherViewHealth callback_health;
} CloseProbe;

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
    AetherStatus status;
    time_t end = deadline();
    do {
        status = aether_backend_async_query_take(client, &result);
        within(end);
    } while (status == AETHER_STATUS_NOT_FOUND);
    OK(status);
    return result;
}

static void event_received(void* context, const AetherViewEvent* event)
{
    CloseProbe* probe = context;
    AetherStatus unsubscribe_status = AETHER_STATUS_OK;
    AetherStatus health_status = AETHER_STATUS_OK;
    AetherViewHealth health = AETHER_VIEW_HEALTHY;
    aether_platform_monitor_lock(probe->monitor);
    CHECK(probe->alive && !probe->active && !probe->closed_entered);
    CHECK(probe->events == 0U);
    probe->active = true;
    probe->event = *event;
    ++probe->events;
    probe->event_entered = true;
    aether_platform_monitor_wake_all(probe->monitor);
    while (!probe->event_release) aether_platform_monitor_wait(probe->monitor);
    aether_platform_monitor_unlock(probe->monitor);
    if (probe->unsubscribe_inside) {
        unsubscribe_status = aether_backend_unsubscribe(probe->client, event->subscription_id);
        health_status = aether_backend_view_health(probe->client, event->subscription_id, &health);
    }
    aether_platform_monitor_lock(probe->monitor);
    probe->unsubscribe_status = unsubscribe_status;
    probe->health_status = health_status;
    probe->callback_health = health;
    probe->active = false;
    aether_platform_monitor_unlock(probe->monitor);
}

static void view_closed(void* context, AetherId subscription)
{
    CloseProbe* probe = context;
    aether_platform_monitor_lock(probe->monitor);
    CHECK(probe->alive && !probe->active && probe->closed == 0U);
    probe->closed_entered = true;
    probe->closed_id = subscription;
    ++probe->closed;
    aether_platform_monitor_wake_all(probe->monitor);
    while (!probe->closed_release) aether_platform_monitor_wait(probe->monitor);
    CHECK(probe->alive);
    aether_platform_monitor_unlock(probe->monitor);
}

static void wait_entered(CloseProbe* probe, bool closed)
{
    bool ready;
    time_t end = deadline();
    do {
        aether_platform_monitor_lock(probe->monitor);
        ready = closed ? probe->closed_entered : probe->event_entered;
        aether_platform_monitor_unlock(probe->monitor);
        within(end);
    } while (!ready);
}

static void allow_return(CloseProbe* probe, bool closed)
{
    aether_platform_monitor_lock(probe->monitor);
    if (closed) probe->closed_release = true;
    else probe->event_release = true;
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

static void wait_client_closed(AetherAsyncQueries* client)
{
    time_t end = deadline();
    while (!aether_backend_async_queries_is_closed(client)) within(end);
}

static AetherId open_workspace(AetherAsyncQueries* client, QueryFixture* f, CloseProbe* probe)
{
    AetherViewReceiver receiver = {event_received, view_closed, probe};
    AetherAsyncQueryResult result;
    AetherId query;
    AetherId subscription;
    OK(aether_backend_async_open_view(client, f->scoped, AETHER_VIEW_WORKSPACE,
        f->workspace[0].id, 91U, 2U, receiver, &query));
    /* The receiver record is copied, but its context remains borrowed until close returns. */
    memset(&receiver, 0, sizeof(receiver));
    result = take(client);
    CHECK(result.query_id.value == query.value && result.receiver_generation == 91U);
    CHECK(result.kind == AETHER_ASYNC_OPEN_WORKSPACE_VIEW && result.status == AETHER_STATUS_OK);
    CHECK(result.value.view.sequence == 0U && result.value.view.base_revision == 1U);
    subscription = result.value.view.subscription_id;
    CHECK(subscription.value != 0U);
    aether_backend_async_query_result_dispose(&result);
    return subscription;
}

static void rename_workspace(
    AetherAsyncQueries* client, QueryFixture* f, uint64_t revision, const char* name)
{
    AetherAsyncQueryResult result;
    AetherId id;
    OK(aether_backend_async_rename_workspace(client,
        (AetherStateCommandContext){f->scoped, true},
        (AetherWorkspaceRenameInput){f->workspace[0].id, revision, text(name)}, 91U, &id));
    result = take(client);
    CHECK(result.query_id.value == id.value && result.status == AETHER_STATUS_OK);
    CHECK(result.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE);
    CHECK(result.value.workspace.revision == revision + 1U);
    CHECK(strcmp(result.value.workspace.display_name, name) == 0);
    aether_backend_async_query_result_dispose(&result);
}

static void create_probe(CloseProbe* probe, AetherAsyncQueries* client, bool unsubscribe)
{
    memset(probe, 0, sizeof(*probe));
    OK(aether_platform_monitor_create(&probe->monitor));
    probe->client = client;
    probe->alive = true;
    probe->unsubscribe_inside = unsubscribe;
}

static void check_probe(CloseProbe* probe, AetherId subscription)
{
    aether_platform_monitor_lock(probe->monitor);
    CHECK(probe->alive && !probe->active);
    CHECK(probe->events == 1U && probe->closed == 1U);
    CHECK(probe->event.subscription_id.value == subscription.value);
    CHECK(probe->closed_id.value == subscription.value);
    CHECK(probe->event.sequence == 1U && probe->event.revision == 2U);
    CHECK(strcmp(probe->event.value.workspace.display_name, "Entered") == 0);
    aether_platform_monitor_unlock(probe->monitor);
}

static void callback_unsubscribe(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    CloseProbe probe;
    AetherId subscription;
    AetherViewHealth health;
    OK(aether_backend_async_queries_create(f.queries, 4U, 65536U, &client));
    create_probe(&probe, client, true);
    subscription = open_workspace(client, &f, &probe);
    rename_workspace(client, &f, 1U, "Entered");
    wait_entered(&probe, false);
    rename_workspace(client, &f, 2U, "Queued");
    health_is(client, subscription, AETHER_VIEW_HEALTHY);
    CHECK(aether_backend_view_release(client, subscription) == AETHER_STATUS_ALREADY_EXISTS);
    allow_return(&probe, false);
    wait_entered(&probe, true);
    aether_platform_monitor_lock(probe.monitor);
    CHECK(probe.unsubscribe_status == AETHER_STATUS_OK);
    CHECK(probe.health_status == AETHER_STATUS_OK);
    CHECK(probe.callback_health == AETHER_VIEW_CLOSING);
    CHECK(probe.events == 1U && !probe.active);
    aether_platform_monitor_unlock(probe.monitor);
    health_is(client, subscription, AETHER_VIEW_CLOSING);
    OK(aether_backend_unsubscribe(client, subscription));
    CHECK(aether_backend_view_release(client, subscription) == AETHER_STATUS_ALREADY_EXISTS);
    rename_workspace(client, &f, 3U, "After unsubscribe");
    allow_return(&probe, true);
    wait_closed(client, subscription);
    OK(aether_backend_unsubscribe(client, subscription));
    rename_workspace(client, &f, 4U, "After closed");
    check_probe(&probe, subscription);
    OK(aether_backend_view_release(client, subscription));
    CHECK(aether_backend_view_release(client, subscription) == AETHER_STATUS_NOT_FOUND);
    CHECK(aether_backend_unsubscribe(client, subscription) == AETHER_STATUS_NOT_FOUND);
    CHECK(aether_backend_view_health(client, subscription, &health) == AETHER_STATUS_NOT_FOUND);
    OK(aether_backend_async_queries_request_close(client));
    wait_client_closed(client);
    OK(aether_backend_async_queries_destroy(&client));
    dispose(&f);
    check_probe(&probe, subscription);
    probe.alive = false;
    aether_platform_monitor_destroy(probe.monitor);
}

static void client_close_during_callback(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherAsyncQueries* saved;
    CloseProbe probe;
    AetherId subscription;
    AetherId rejected = {0};
    OK(aether_backend_async_queries_create(f.queries, 4U, 65536U, &client));
    create_probe(&probe, client, false);
    subscription = open_workspace(client, &f, &probe);
    rename_workspace(client, &f, 1U, "Entered");
    wait_entered(&probe, false);
    rename_workspace(client, &f, 2U, "Queued");
    OK(aether_backend_async_queries_request_close(client));
    OK(aether_backend_async_queries_request_close(client));
    CHECK(!aether_backend_async_queries_is_closed(client));
    saved = client;
    CHECK(aether_backend_async_queries_destroy(&client) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(client == saved);
    health_is(client, subscription, AETHER_VIEW_CLOSING);
    CHECK(aether_backend_view_release(client, subscription) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(aether_backend_async_open_view(client, f.scoped, AETHER_VIEW_WORKSPACE,
        f.workspace[0].id, 91U, 1U, (AetherViewReceiver){event_received, view_closed, &probe},
        &rejected) == AETHER_STATUS_NOT_INITIALIZED);
    CHECK(rejected.value == 0U);
    allow_return(&probe, false);
    wait_entered(&probe, true);
    /* Closing the event gate is not enough: context remains live through closed return. */
    CHECK(!aether_backend_async_queries_is_closed(client));
    CHECK(aether_backend_async_queries_destroy(&client) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(client == saved);
    health_is(client, subscription, AETHER_VIEW_CLOSING);
    CHECK(aether_backend_view_release(client, subscription) == AETHER_STATUS_ALREADY_EXISTS);
    allow_return(&probe, true);
    wait_client_closed(client);
    health_is(client, subscription, AETHER_VIEW_CLOSED);
    check_probe(&probe, subscription);
    OK(aether_backend_view_release(client, subscription));
    OK(aether_backend_async_queries_destroy(&client));
    CHECK(client == NULL);
    dispose(&f);
    check_probe(&probe, subscription);
    probe.alive = false;
    aether_platform_monitor_destroy(probe.monitor);
}

int main(void)
{
    callback_unsubscribe();
    client_close_during_callback();
    return 0;
}
