#include "async_internal.h"
#include "query_internal.h"
#include "aether/platform/platform.h"

#define COMMAND_RETAIN_NS UINT64_C(600000000000)

static uint64_t add_time(uint64_t now, uint64_t duration)
{
    return duration > UINT64_MAX - now ? UINT64_MAX : now + duration;
}

static AetherStatus command_now(uint64_t* out_now)
{
    AetherPlatformClock clock = {0};
    AetherStatus status = aether_platform_clock_now(&clock);
    *out_now = status == AETHER_STATUS_OK ? clock.monotonic_ns : 0U;
    return status;
}

static void terminal(
    CommandRecord* record, AetherStatus status, const AetherQueryError* error,
    uint64_t now, bool clock_valid
)
{
    AetherCommandRequestSnapshot* snapshot = &record->snapshot;
    if (snapshot->terminal_revision != 0U) return;
    if (clock_valid && now >= snapshot->deadline_ns) status = AETHER_STATUS_TIMEOUT;
    switch (status) {
    case AETHER_STATUS_OK: snapshot->state = AETHER_COMMAND_SUCCEEDED; break;
    case AETHER_STATUS_TIMEOUT: snapshot->state = AETHER_COMMAND_TIMED_OUT; break;
    case AETHER_STATUS_CANCELLED: snapshot->state = AETHER_COMMAND_CANCELLED; break;
    default: snapshot->state = AETHER_COMMAND_FAILED; break;
    }
    snapshot->result = status;
    snapshot->terminal_revision = 1U;
    snapshot->retain_until_ns = clock_valid ? add_time(now, COMMAND_RETAIN_NS) : UINT64_MAX;
    if (error != NULL && error->status == status && status != AETHER_STATUS_TIMEOUT &&
        status != AETHER_STATUS_CANCELLED) {
        snapshot->error = *error;
    } else {
        (void)aether_query_error(&record->context, status, AETHER_QUERY_ERROR_NONE,
                                 &snapshot->error);
    }
}

static void expire(CommandRecord* record, uint64_t now)
{
    if (now >= record->snapshot.deadline_ns) {
        terminal(record, AETHER_STATUS_TIMEOUT, NULL, now, true);
    }
}

static void copy_result(AsyncQuerySlot* slot)
{
    slot->result.kind = slot->input.kind;
    slot->result.status = slot->request->snapshot.result;
    slot->result.error = slot->request->snapshot.error;
}

CommandRecord* aether_command_reserve(AetherAsyncQueries* client, uint64_t now_ns)
{
    for (size_t index = 0U; index < sizeof(client->requests) / sizeof(client->requests[0]);
         ++index) {
        CommandRecord* record = &client->requests[index];
        const AetherCommandRequestSnapshot* snapshot = &record->snapshot;
        if (!record->occupied ||
            (snapshot->terminal_revision != 0U && snapshot->execution_complete &&
             !record->work_pending && record->completion_taken &&
             now_ns >= snapshot->retain_until_ns)) {
            *record = (CommandRecord){0};
            return record;
        }
    }
    return NULL;
}

void aether_command_accept(CommandRecord* record, AsyncQuerySlot* slot,
    AetherId id, uint64_t now_ns, uint64_t timeout_ns)
{
    *record = (CommandRecord){0};
    record->occupied = true;
    record->work_pending = true;
    record->context = slot->input.context;
    record->workspace = slot->input.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE ?
        slot->input.id : (record->context.read_all_workspaces ? (AetherId){0} :
                         record->context.workspace_scope);
    record->snapshot.request_id = id;
    record->snapshot.kind = slot->input.kind;
    record->snapshot.receiver_generation = slot->input.receiver_generation;
    record->snapshot.deadline_ns = add_time(now_ns, timeout_ns);
    record->snapshot.trace = slot->input.context.trace;
    slot->request = record;
}

void aether_command_clock_failed_locked(AsyncQuerySlot* slot, AetherStatus status)
{
    CommandRecord* record = slot->request;
    if (record == NULL) return;
    if (!record->snapshot.execution_started && !record->snapshot.execution_complete) {
        record->snapshot.execution_complete = true;
        record->snapshot.execution_status = status;
    }
    terminal(record, status, NULL, 0U, false);
    copy_result(slot);
}

AetherStatus aether_command_start(AsyncQuerySlot* slot)
{
    uint64_t now;
    AetherStatus status;
    AetherAsyncQueries* client = slot->owner;
    aether_platform_monitor_lock(client->monitor);
    status = command_now(&now);
    if (status != AETHER_STATUS_OK) {
        aether_command_clock_failed_locked(slot, status);
    } else {
        CommandRecord* record = slot->request;
        AetherCommandRequestSnapshot* snapshot = &record->snapshot;
        expire(record, now);
        if (snapshot->cancel_requested) {
            terminal(record, AETHER_STATUS_CANCELLED, NULL, now, true);
        }
        if (snapshot->terminal_revision != 0U) {
            snapshot->execution_complete = true;
            snapshot->execution_status = snapshot->result;
            copy_result(slot);
        } else {
            snapshot->state = AETHER_COMMAND_RUNNING;
            snapshot->execution_started = true;
        }
    }
    status = slot->request->snapshot.terminal_revision != 0U ?
        slot->request->snapshot.result : AETHER_STATUS_OK;
    aether_platform_monitor_unlock(client->monitor);
    return status;
}

AetherStatus aether_command_finish(AsyncQuerySlot* slot)
{
    uint64_t now;
    AetherStatus status;
    AetherAsyncQueries* client = slot->owner;
    aether_platform_monitor_lock(client->monitor);
    CommandRecord* record = slot->request;
    AetherCommandRequestSnapshot* snapshot = &record->snapshot;
    snapshot->execution_complete = true;
    snapshot->execution_status = slot->result.status;
    if (slot->input.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE) {
        snapshot->value.workspace = slot->result.value.workspace;
    } else {
        snapshot->value.session = slot->result.value.session;
        if (record->context.read_all_workspaces && slot->result.status == AETHER_STATUS_OK) {
            record->workspace = slot->result.value.session.workspace_id;
        }
    }
    status = command_now(&now);
    if (status == AETHER_STATUS_OK) {
        terminal(record, slot->result.status, &slot->result.error, now, true);
        copy_result(slot);
    } else {
        aether_command_clock_failed_locked(slot, status);
    }
    status = snapshot->result;
    aether_platform_monitor_unlock(client->monitor);
    return status;
}

void aether_command_cancelled_locked(AsyncQuerySlot* slot, uint64_t now_ns)
{
    CommandRecord* record = slot->request;
    if (record == NULL) return;
    record->snapshot.cancel_requested = true;
    if (!record->snapshot.execution_complete) {
        record->snapshot.execution_complete = true;
        record->snapshot.execution_status = AETHER_STATUS_CANCELLED;
    }
    terminal(record, AETHER_STATUS_CANCELLED, NULL, now_ns, true);
    copy_result(slot);
}

void aether_command_close_locked(AetherAsyncQueries* client, uint64_t now_ns)
{
    for (size_t index = 0U; index < sizeof(client->requests) / sizeof(client->requests[0]);
         ++index) {
        CommandRecord* record = &client->requests[index];
        if (!record->occupied || record->snapshot.execution_complete) continue;
        record->snapshot.cancel_requested = true;
        if (!record->snapshot.execution_started) {
            record->snapshot.execution_complete = true;
            record->snapshot.execution_status = AETHER_STATUS_CANCELLED;
            terminal(record, AETHER_STATUS_CANCELLED, NULL, now_ns, true);
        }
    }
}

static CommandRecord* find_record(AetherAsyncQueries* client, AetherId id)
{
    for (size_t index = 0U; index < sizeof(client->requests) / sizeof(client->requests[0]);
         ++index) {
        CommandRecord* record = &client->requests[index];
        if (record->occupied && aether_id_equal(record->snapshot.request_id, id)) return record;
    }
    return NULL;
}

static bool authorized(const CommandRecord* record, const AetherRequestContext* context)
{
    if (!aether_id_equal(record->context.caller_id, context->caller_id)) return false;
    if (!record->context.read_all_workspaces &&
        (context->read_all_workspaces ||
         !aether_id_equal(record->context.workspace_scope, context->workspace_scope))) return false;
    return context->read_all_workspaces ||
        (aether_id_is_valid(record->workspace) &&
         aether_id_equal(context->workspace_scope, record->workspace));
}

static AetherStatus validate(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherCommandRequestSnapshot* out_snapshot, AetherQueryError* out_error
)
{
    if (out_snapshot != NULL) *out_snapshot = (AetherCommandRequestSnapshot){0};
    return aether_query_validate(client == NULL ? NULL : client->queries, context, out_error);
}

static AetherStatus lookup(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherId id, CommandRecord** out_record, uint64_t* out_now, AetherQueryError* out_error
)
{
    AetherStatus status;
    *out_record = find_record(client, id);
    if (*out_record == NULL) {
        return aether_query_error(context, AETHER_STATUS_NOT_FOUND,
                                  AETHER_QUERY_ERROR_NONE, out_error);
    }
    if (!authorized(*out_record, context)) {
        return aether_query_error(context, AETHER_STATUS_PERMISSION_DENIED,
                                  AETHER_QUERY_ERROR_FORBIDDEN, out_error);
    }
    status = command_now(out_now);
    if (status != AETHER_STATUS_OK) {
        return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
    }
    expire(*out_record, *out_now);
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_query_command_request(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherId request, AetherCommandRequestSnapshot* out_snapshot, AetherQueryError* out_error
)
{
    CommandRecord* record;
    uint64_t now;
    AetherStatus status = validate(client, context, out_snapshot, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_snapshot == NULL || !aether_id_is_valid(request)) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    aether_platform_monitor_lock(client->monitor);
    status = lookup(client, context, request, &record, &now, out_error);
    if (status == AETHER_STATUS_OK) *out_snapshot = record->snapshot;
    aether_platform_monitor_unlock(client->monitor);
    return status;
}

AetherStatus aether_backend_cancel_command_request(
    AetherAsyncQueries* client, const AetherStateCommandContext* context,
    AetherId request, AetherCommandRequestSnapshot* out_snapshot, AetherQueryError* out_error
)
{
    const AetherRequestContext* request_context = context == NULL ? NULL : &context->request;
    CommandRecord* record;
    uint64_t now;
    AetherStatus status = validate(client, request_context, out_snapshot, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_snapshot == NULL || !aether_id_is_valid(request)) {
        return aether_query_error(request_context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    if (!context->write_allowed) {
        return aether_query_error(request_context, AETHER_STATUS_PERMISSION_DENIED,
                                  AETHER_QUERY_ERROR_FORBIDDEN, out_error);
    }
    aether_platform_monitor_lock(client->monitor);
    status = lookup(client, request_context, request, &record, &now, out_error);
    if (status == AETHER_STATUS_OK) {
        AetherCommandRequestSnapshot* snapshot = &record->snapshot;
        if (snapshot->terminal_revision == 0U && !snapshot->execution_complete) {
            AetherStatus cancelled = aether_executor_cancel(client->executor,
                request, snapshot->receiver_generation);
            if (cancelled != AETHER_STATUS_OK &&
                cancelled != AETHER_STATUS_ALREADY_EXISTS) {
                aether_platform_monitor_unlock(client->monitor);
                return aether_query_error(request_context, cancelled,
                                          AETHER_QUERY_ERROR_STATE, out_error);
            }
            snapshot->cancel_requested = true;
            if (!snapshot->execution_started) {
                snapshot->execution_complete = true;
                snapshot->execution_status = AETHER_STATUS_CANCELLED;
                terminal(record, AETHER_STATUS_CANCELLED, NULL, now, true);
            }
        }
        *out_snapshot = *snapshot;
    }
    aether_platform_monitor_unlock(client->monitor);
    return status;
}

AetherStatus aether_backend_take_command_completed(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherCommandRequestSnapshot* out_snapshot, AetherQueryError* out_error
)
{
    uint64_t now;
    AetherStatus status = validate(client, context, out_snapshot, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (out_snapshot == NULL) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    aether_platform_monitor_lock(client->monitor);
    status = command_now(&now);
    if (status == AETHER_STATUS_OK) {
        status = AETHER_STATUS_NOT_FOUND;
        for (size_t index = 0U; index < sizeof(client->requests) / sizeof(client->requests[0]);
             ++index) {
            CommandRecord* record = &client->requests[index];
            if (!record->occupied || !authorized(record, context)) continue;
            expire(record, now);
            if (record->snapshot.terminal_revision != 0U && !record->completion_taken) {
                record->completion_taken = true;
                *out_snapshot = record->snapshot;
                status = AETHER_STATUS_OK;
                break;
            }
        }
    }
    aether_platform_monitor_unlock(client->monitor);
    return aether_query_error(context, status, AETHER_QUERY_ERROR_NONE, out_error);
}

AetherStatus aether_backend_release_command_request(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherId request, AetherQueryError* out_error
)
{
    CommandRecord* record;
    uint64_t now;
    AetherStatus status = validate(client, context, NULL, out_error);
    if (status != AETHER_STATUS_OK) return status;
    if (!aether_id_is_valid(request)) {
        return aether_query_error(context, AETHER_STATUS_INVALID_ARGUMENT,
                                  AETHER_QUERY_ERROR_INPUT, out_error);
    }
    aether_platform_monitor_lock(client->monitor);
    status = lookup(client, context, request, &record, &now, out_error);
    if (status == AETHER_STATUS_OK) {
        if (record->snapshot.terminal_revision == 0U || !record->snapshot.execution_complete ||
            record->work_pending || !record->completion_taken) {
            status = aether_query_error(context, AETHER_STATUS_ALREADY_EXISTS,
                                        AETHER_QUERY_ERROR_STATE, out_error);
        } else {
            *record = (CommandRecord){0};
        }
    }
    aether_platform_monitor_unlock(client->monitor);
    return status;
}
