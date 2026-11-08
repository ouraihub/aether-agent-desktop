#include "aether/backend/async_query.h"
#include "aether/backend/state_command.h"
#include "aether/infrastructure/executor.h"
#include "aether/platform/thread.h"
#include <stdlib.h>
#include <string.h>

typedef struct AsyncQuerySlot {
    bool occupied;
    AetherId work_id;
    size_t charge;
    AetherAsyncQueries* owner;
    AetherAsyncQueryInput input;
    bool write_allowed;
    uint64_t expected_revision;
    size_t name_length;
    char name[128];
    AetherAsyncQueryResult result;
} AsyncQuerySlot;

struct AetherAsyncQueries {
    AetherBackendQueries* queries;
    AetherExecutor* executor;
    AetherPlatformMonitor* monitor;
    AsyncQuerySlot* slots;
    size_t capacity;
    size_t result_bytes;
    size_t reserved_bytes;
    bool closing;
};

static void cancelled_result(AsyncQuerySlot* slot)
{
    aether_backend_async_query_result_dispose(&slot->result);
    slot->result.kind = slot->input.kind;
    slot->result.status = AETHER_STATUS_CANCELLED;
    slot->result.error.status = AETHER_STATUS_CANCELLED;
    slot->result.error.code = AETHER_QUERY_ERROR_STATE;
    slot->result.error.diagnostic_available = true;
    slot->result.error.diagnostic_id = slot->input.context.trace.trace_id;
    (void)memcpy(slot->result.error.message, "Query cancelled.",
                 sizeof("Query cancelled."));
}

static AetherStatus run_query(const AetherWorkContext* work)
{
    AsyncQuerySlot* slot = work->context;
    AetherAsyncQueryInput* input = &slot->input;
    AetherAsyncQueryResult* result = &slot->result;
    if (aether_work_cancel_requested(work->cancellation)) {
        return AETHER_STATUS_CANCELLED;
    }
    result->kind = input->kind;
    switch (input->kind) {
    case AETHER_ASYNC_QUERY_WORKSPACE:
        result->status = aether_backend_query_workspace(slot->owner->queries,
            &input->context, input->id, &result->value.workspace, &result->error);
        break;
    case AETHER_ASYNC_QUERY_SESSION:
        result->status = aether_backend_query_session(slot->owner->queries,
            &input->context, input->id, &result->value.session, &result->error);
        break;
    case AETHER_ASYNC_QUERY_RUNTIME:
        result->status = aether_backend_query_runtime(slot->owner->queries,
            &input->context, input->session_id, input->id,
            &result->value.runtime, &result->error);
        break;
    case AETHER_ASYNC_QUERY_MESSAGE:
        result->status = aether_backend_query_message(slot->owner->queries,
            &input->context, input->id, &result->value.message, &result->error);
        break;
    case AETHER_ASYNC_QUERY_BLOCK:
        result->status = aether_backend_query_block(slot->owner->queries,
            &input->context, input->id, input->ordinal, input->max_bytes,
            &result->value.block, &result->error);
        break;
    case AETHER_ASYNC_COMMAND_RENAME_WORKSPACE:
    {
        AetherStateCommandContext context = {input->context, slot->write_allowed};
        result->status = aether_backend_rename_workspace(slot->owner->queries,
            &context, (AetherWorkspaceRenameInput){input->id, slot->expected_revision,
                {slot->name, slot->name_length}}, &result->value.workspace,
            &result->error);
        return result->status;
    }
    case AETHER_ASYNC_COMMAND_CLOSE_SESSION:
    {
        AetherStateCommandContext context = {input->context, slot->write_allowed};
        result->status = aether_backend_request_session_close(slot->owner->queries,
            &context, (AetherSessionCloseInput){input->id, slot->expected_revision},
            &result->value.session, &result->error);
        return result->status;
    }
    default:
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (aether_work_cancel_requested(work->cancellation)) {
        cancelled_result(slot);
        return AETHER_STATUS_CANCELLED;
    }
    return result->status;
}

AetherStatus aether_backend_async_queries_create(
    AetherBackendQueries* queries, size_t capacity, size_t result_bytes,
    AetherAsyncQueries** out_client
)
{
    AetherAsyncQueries* client;
    AetherStatus status;
    AetherExecutorLimits limits = aether_executor_default_limits();
    if (out_client == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_client = NULL;
    if (queries == NULL || capacity == 0U || capacity > 32U || result_bytes == 0U ||
        result_bytes > 4U * 1024U * 1024U) return AETHER_STATUS_INVALID_ARGUMENT;
    client = calloc(1U, sizeof(*client));
    if (client == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    client->slots = calloc(capacity, sizeof(*client->slots));
    if (client->slots == NULL) {
        free(client);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    status = aether_platform_monitor_create(&client->monitor);
    if (status != AETHER_STATUS_OK) {
        free(client->slots);
        free(client);
        return status;
    }
    limits.queued_count = capacity;
    limits.input_bytes = 1U;
    limits.queued_bytes = 1U;
    status = aether_executor_create(limits, &client->executor);
    if (status != AETHER_STATUS_OK) {
        aether_platform_monitor_destroy(client->monitor);
        free(client->slots);
        free(client);
        return status;
    }
    client->queries = queries;
    client->capacity = capacity;
    client->result_bytes = result_bytes;
    *out_client = client;
    return AETHER_STATUS_OK;
}

static bool valid_input(AetherAsyncQueryInput input)
{
    bool trace = false;
    bool span = false;
    size_t index;
    for (index = 0U; index < sizeof(input.context.trace.trace_id.bytes); ++index) {
        if (input.context.trace.trace_id.bytes[index] != 0U) trace = true;
    }
    for (index = 0U; index < sizeof(input.context.trace.span_id.bytes); ++index) {
        if (input.context.trace.span_id.bytes[index] != 0U) span = true;
    }
    return input.receiver_generation != 0U && input.context.trace.valid &&
        trace && span && aether_id_is_valid(input.id) &&
        (input.kind == AETHER_ASYNC_QUERY_WORKSPACE ||
         input.kind == AETHER_ASYNC_QUERY_SESSION ||
         input.kind == AETHER_ASYNC_QUERY_RUNTIME ||
         input.kind == AETHER_ASYNC_QUERY_MESSAGE ||
         input.kind == AETHER_ASYNC_QUERY_BLOCK ||
         input.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE ||
         input.kind == AETHER_ASYNC_COMMAND_CLOSE_SESSION) &&
        (input.kind != AETHER_ASYNC_QUERY_RUNTIME ||
         aether_id_is_valid(input.session_id)) &&
        (input.kind != AETHER_ASYNC_QUERY_BLOCK ||
         (input.max_bytes != 0U && input.max_bytes <= 1024U * 1024U));
}

static AetherStatus submit_work(
    AetherAsyncQueries* client, AetherAsyncQueryInput input,
    const AetherStateCommandContext* command, uint64_t expected_revision,
    AetherStringView name, AetherId* out_query
)
{
    size_t index;
    size_t charge = sizeof(AetherAsyncQueryResult);
    AsyncQuerySlot* slot = NULL;
    AetherStatus status;
    if (out_query == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_query = (AetherId){0};
    if (client == NULL || !valid_input(input)) return AETHER_STATUS_INVALID_ARGUMENT;
    if (input.kind == AETHER_ASYNC_QUERY_BLOCK) charge += input.max_bytes;
    aether_platform_monitor_lock(client->monitor);
    if (client->closing) {
        aether_platform_monitor_unlock(client->monitor);
        return AETHER_STATUS_NOT_INITIALIZED;
    }
    for (index = 0U; index < client->capacity; ++index) {
        if (!client->slots[index].occupied) { slot = &client->slots[index]; break; }
    }
    if (slot == NULL || charge > client->result_bytes - client->reserved_bytes) {
        aether_platform_monitor_unlock(client->monitor);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    *slot = (AsyncQuerySlot){0};
    slot->occupied = true;
    slot->owner = client;
    slot->input = input;
    slot->charge = charge;
    if (command != NULL) {
        slot->write_allowed = command->write_allowed;
        slot->expected_revision = expected_revision;
        slot->name_length = name.length;
        if (name.length != 0U) (void)memcpy(slot->name, name.data, name.length);
    }
    status = aether_executor_submit(client->executor,
        (AetherWorkInput){input.receiver_generation, input.context.trace,
                         NULL, 0U, run_query, slot}, out_query);
    if (status == AETHER_STATUS_OK) {
        slot->work_id = *out_query;
        client->reserved_bytes += charge;
    } else {
        *slot = (AsyncQuerySlot){0};
    }
    aether_platform_monitor_unlock(client->monitor);
    return status;
}

AetherStatus aether_backend_async_query_submit(
    AetherAsyncQueries* client, AetherAsyncQueryInput input, AetherId* out_query
)
{
    if (input.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE ||
        input.kind == AETHER_ASYNC_COMMAND_CLOSE_SESSION) {
        if (out_query != NULL) *out_query = (AetherId){0};
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return submit_work(client, input, NULL, 0U, (AetherStringView){0}, out_query);
}

AetherStatus aether_backend_async_rename_workspace(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherWorkspaceRenameInput input, uint64_t receiver_generation,
    AetherId* out_command
)
{
    AetherAsyncQueryInput work = {0};
    if (out_command != NULL) *out_command = (AetherId){0};
    if (input.expected_revision == 0U || input.display_name.length == 0U ||
        input.display_name.length >= 128U ||
        aether_string_view_validate(input.display_name) != AETHER_STATUS_OK ||
        memchr(input.display_name.data, '\0', input.display_name.length) != NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    work.kind = AETHER_ASYNC_COMMAND_RENAME_WORKSPACE;
    work.context = context.request;
    work.id = input.workspace_id;
    work.receiver_generation = receiver_generation;
    return submit_work(client, work, &context, input.expected_revision,
                        input.display_name, out_command);
}

AetherStatus aether_backend_async_request_session_close(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherSessionCloseInput input, uint64_t receiver_generation,
    AetherId* out_command
)
{
    AetherAsyncQueryInput work = {0};
    if (out_command != NULL) *out_command = (AetherId){0};
    if (input.expected_revision == 0U) return AETHER_STATUS_INVALID_ARGUMENT;
    work.kind = AETHER_ASYNC_COMMAND_CLOSE_SESSION;
    work.context = context.request;
    work.id = input.session_id;
    work.receiver_generation = receiver_generation;
    return submit_work(client, work, &context, input.expected_revision,
                        (AetherStringView){0}, out_command);
}

AetherStatus aether_backend_async_query_cancel(
    AetherAsyncQueries* client, AetherId query, uint64_t receiver_generation
)
{
    AetherStatus status;
    if (client == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(client->monitor);
    status = aether_executor_cancel(client->executor, query, receiver_generation);
    aether_platform_monitor_unlock(client->monitor);
    return status;
}

AetherStatus aether_backend_async_query_take(
    AetherAsyncQueries* client, AetherAsyncQueryResult* out_result
)
{
    AetherWorkResult result;
    AetherStatus status;
    size_t index;
    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_result = (AetherAsyncQueryResult){0};
    if (client == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(client->monitor);
    status = aether_executor_take(client->executor, &result);
    if (status != AETHER_STATUS_OK) {
        aether_platform_monitor_unlock(client->monitor);
        return status;
    }
    for (index = 0U; index < client->capacity; ++index) {
        AsyncQuerySlot* slot = &client->slots[index];
        if (!slot->occupied || !aether_id_equal(slot->work_id, result.work_id)) continue;
        if (result.status == AETHER_STATUS_CANCELLED) cancelled_result(slot);
        slot->result.kind = slot->input.kind;
        slot->result.query_id = result.work_id;
        slot->result.receiver_generation = result.generation;
        slot->result.status = result.status;
        *out_result = slot->result;
        client->reserved_bytes -= slot->charge;
        *slot = (AsyncQuerySlot){0};
        aether_platform_monitor_unlock(client->monitor);
        return AETHER_STATUS_OK;
    }
    aether_platform_monitor_unlock(client->monitor);
    return AETHER_STATUS_INTERNAL_ERROR;
}

void aether_backend_async_query_result_dispose(AetherAsyncQueryResult* result)
{
    if (result == NULL) return;
    if (result->kind == AETHER_ASYNC_QUERY_BLOCK) {
        aether_backend_block_destroy(result->value.block);
    }
    *result = (AetherAsyncQueryResult){0};
}

AetherStatus aether_backend_async_queries_request_close(AetherAsyncQueries* client)
{
    AetherStatus status;
    if (client == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(client->monitor);
    client->closing = true;
    status = aether_executor_request_close(client->executor);
    aether_platform_monitor_unlock(client->monitor);
    return status;
}

bool aether_backend_async_queries_is_closed(AetherAsyncQueries* client)
{
    return client != NULL && aether_executor_is_closed(client->executor);
}

AetherStatus aether_backend_async_queries_destroy(AetherAsyncQueries** client)
{
    AetherStatus status;
    size_t index;
    if (client == NULL || *client == NULL) return AETHER_STATUS_OK;
    status = aether_executor_destroy(&(*client)->executor);
    if (status != AETHER_STATUS_OK) return status;
    for (index = 0U; index < (*client)->capacity; ++index) {
        aether_backend_async_query_result_dispose(&(*client)->slots[index].result);
    }
    aether_platform_monitor_destroy((*client)->monitor);
    free((*client)->slots);
    free(*client);
    *client = NULL;
    return AETHER_STATUS_OK;
}
