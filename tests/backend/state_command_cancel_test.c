/* Only this target's async_query.c calls the wrapper around the real command. */
#undef aether_backend_rename_workspace
#include "query_fixture.h"
#include "aether/backend/state_command.h"
#include "aether/platform/thread.h"

static AetherPlatformMonitor* gate;
static bool committed;
static bool released;
static unsigned int executions;

AetherStatus aether_test_rename_workspace(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherWorkspaceRenameInput input, AetherWorkspaceDto* out_dto,
    AetherQueryError* out_error
);

AetherStatus aether_test_rename_workspace(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherWorkspaceRenameInput input, AetherWorkspaceDto* out_dto,
    AetherQueryError* out_error
)
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

int main(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherStateCommandContext writer = {f.scoped, true};
    AetherAsyncQueryResult result;
    AetherWorkspaceDto workspace;
    AetherQueryError error;
    AetherId first;
    AetherId second;
    bool saw_first = false;
    bool saw_second = false;
    OK(aether_platform_monitor_create(&gate));
    OK(aether_backend_async_queries_create(f.queries, 2U, 4096U, &client));
    OK(aether_backend_async_rename_workspace(client, writer,
        (AetherWorkspaceRenameInput){f.workspace[0].id, 1U, text("Committed")},
        3U, &first));
    aether_platform_monitor_lock(gate);
    while (!committed) aether_platform_monitor_wait(gate);
    aether_platform_monitor_unlock(gate);
    OK(aether_backend_async_rename_workspace(client, writer,
        (AetherWorkspaceRenameInput){f.workspace[0].id, 2U, text("Never applied")},
        3U, &second));
    OK(aether_backend_async_query_cancel(client, first, 3U));
    OK(aether_backend_async_query_cancel(client, second, 3U));
    OK(aether_backend_async_queries_request_close(client));
    CHECK(!aether_backend_async_queries_is_closed(client));
    aether_platform_monitor_lock(gate);
    released = true;
    aether_platform_monitor_wake_all(gate);
    aether_platform_monitor_unlock(gate);
    while (!aether_backend_async_queries_is_closed(client)) { }
    CHECK(executions == 1U);
    for (size_t index = 0U; index < 2U; ++index) {
        OK(aether_backend_async_query_take(client, &result));
        CHECK(result.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE);
        if (result.query_id.value == first.value) {
            CHECK(!saw_first && result.status == AETHER_STATUS_OK);
            CHECK(strcmp(result.value.workspace.display_name, "Committed") == 0);
            saw_first = true;
        } else {
            CHECK(!saw_second && result.query_id.value == second.value);
            CHECK(result.status == AETHER_STATUS_CANCELLED);
            CHECK(result.value.workspace.id.value == 0U);
            saw_second = true;
        }
        aether_backend_async_query_result_dispose(&result);
    }
    CHECK(saw_first && saw_second);
    OK(aether_backend_async_queries_destroy(&client));
    OK(aether_backend_query_workspace(f.queries, &f.scoped, f.workspace[0].id,
                                       &workspace, &error));
    CHECK(workspace.revision == 2U);
    CHECK(strcmp(workspace.display_name, "Committed") == 0);
    aether_platform_monitor_destroy(gate);
    dispose(&f);
    return 0;
}
