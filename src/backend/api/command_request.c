#include "async_internal.h"
#include <string.h>

AetherStatus aether_backend_submit_rename_request(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherWorkspaceRenameInput input, uint64_t receiver_generation,
    uint64_t timeout_ns, AetherId* out_request
)
{
    AetherAsyncQueryInput work = {0};
    if (out_request != NULL) *out_request = (AetherId){0};
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
    return aether_async_submit_work(client, work, &context, input.expected_revision,
        input.display_name, NULL, 0U, timeout_ns, out_request);
}

AetherStatus aether_backend_submit_close_request(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherSessionCloseInput input, uint64_t receiver_generation,
    uint64_t timeout_ns, AetherId* out_request
)
{
    AetherAsyncQueryInput work = {0};
    if (out_request != NULL) *out_request = (AetherId){0};
    if (input.expected_revision == 0U) return AETHER_STATUS_INVALID_ARGUMENT;
    work.kind = AETHER_ASYNC_COMMAND_CLOSE_SESSION;
    work.context = context.request;
    work.id = input.session_id;
    work.receiver_generation = receiver_generation;
    return aether_async_submit_work(client, work, &context, input.expected_revision,
        (AetherStringView){0}, NULL, 0U, timeout_ns, out_request);
}
