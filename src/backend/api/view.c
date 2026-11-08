#include "async_internal.h"
#include "query_internal.h"
#include "aether/platform/platform.h"

AetherStatus aether_backend_async_open_view(
    AetherAsyncQueries* client, AetherRequestContext context,
    AetherViewKind kind, AetherId object, uint64_t receiver_generation,
    size_t queue_capacity, AetherViewReceiver receiver, AetherId* out_query
)
{
    AetherAsyncQueryInput input = {0};
    if (out_query != NULL) *out_query = (AetherId){0};
    if ((kind != AETHER_VIEW_WORKSPACE && kind != AETHER_VIEW_SESSION) ||
        queue_capacity == 0U || queue_capacity > 256U ||
        receiver.event == NULL || receiver.closed == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    input.kind = kind == AETHER_VIEW_WORKSPACE ? AETHER_ASYNC_OPEN_WORKSPACE_VIEW
                                               : AETHER_ASYNC_OPEN_SESSION_VIEW;
    input.context = context;
    input.id = object;
    input.receiver_generation = receiver_generation;
    return aether_async_submit_work(client, input, NULL, 0U,
        (AetherStringView){0}, &receiver, queue_capacity, 0U, out_query);
}

AetherStatus aether_async_run_open(AsyncQuerySlot* slot)
{
    AetherViewSnapshot snapshot = {0};
    AetherAsyncQueryInput* input = &slot->input;
    AetherStatus status;
    if (input->kind == AETHER_ASYNC_OPEN_WORKSPACE_VIEW) {
        snapshot.kind = AETHER_VIEW_WORKSPACE;
        status = aether_backend_query_workspace(slot->owner->queries, &input->context,
            input->id, &snapshot.value.workspace, &slot->result.error);
        snapshot.base_revision = snapshot.value.workspace.revision;
    } else {
        snapshot.kind = AETHER_VIEW_SESSION;
        status = aether_backend_query_session(slot->owner->queries, &input->context,
            input->id, &snapshot.value.session, &slot->result.error);
        snapshot.base_revision = snapshot.value.session.revision;
    }
    if (status == AETHER_STATUS_OK) {
        status = aether_view_open(slot->owner->views, snapshot, input->id,
            slot->receiver, slot->queue_capacity, &slot->result.value.view);
    }
    if (status != AETHER_STATUS_OK) {
        (void)aether_query_error(&input->context, status, AETHER_QUERY_ERROR_NONE,
                                 &slot->result.error);
    }
    slot->result.status = status;
    return status;
}

void aether_async_publish(AsyncQuerySlot* slot)
{
    AetherViewEvent event = {0};
    AetherPlatformClock clock;
    bool changed;
    if (slot->result.status != AETHER_STATUS_OK) return;
    if (slot->input.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE) {
        event.kind = AETHER_VIEW_WORKSPACE;
        event.object_id = slot->result.value.workspace.id;
        event.workspace_id = event.object_id;
        event.revision = slot->result.value.workspace.revision;
        event.value.workspace = slot->result.value.workspace;
    } else {
        event.kind = AETHER_VIEW_SESSION;
        event.object_id = slot->result.value.session.id;
        event.workspace_id = slot->result.value.session.workspace_id;
        event.session_id = event.object_id;
        event.runtime_id = slot->result.value.session.runtime_id;
        event.revision = slot->result.value.session.revision;
        event.value.session = slot->result.value.session;
    }
    changed = event.revision != slot->expected_revision;
    if (!changed) return;
    event.schema_version = 1U;
    event.trace = slot->input.context.trace;
    event.event_id = aether_id_next();
    /* A zero event identity marks an unpublishable fact; the hub invalidates
     * affected views rather than emitting an apparently complete stream. */
    if (aether_platform_clock_now(&clock) != AETHER_STATUS_OK) {
        event.event_id = (AetherId){0};
    } else {
        event.timestamp_ms = clock.wall_clock_ms;
    }
    aether_view_publish(slot->owner->views, event);
}
