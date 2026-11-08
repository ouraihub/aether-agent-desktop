#include "query_fixture.h"
#include "aether/backend/state_command.h"

static AetherAsyncQueryResult take(AetherAsyncQueries* client)
{
    AetherAsyncQueryResult result;
    AetherStatus status;
    do { status = aether_backend_async_query_take(client, &result); }
    while (status == AETHER_STATUS_NOT_FOUND);
    OK(status);
    return result;
}

int main(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherStateCommandContext writer = {f.scoped, true};
    AetherStateCommandContext reader = {f.scoped, false};
    AetherWorkspaceDto workspace;
    AetherSessionDto session;
    AetherQueryError error;
    AetherAsyncQueryResult result;
    AetherWorkspaceRenameInput rename = {f.workspace[0].id, 1U, text("Renamed")};
    AetherId id;
    char name[] = {'N', 'e', 'w'};
    CHECK(aether_backend_rename_workspace(f.queries, &reader, rename,
        &workspace, &error) == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(workspace.id.value == 0U);
    OK(aether_backend_query_workspace(f.queries, &f.scoped, rename.workspace_id,
                                       &workspace, &error));
    CHECK(strcmp(workspace.display_name, "Primary") == 0 && workspace.revision == 1U);
    OK(aether_backend_async_queries_create(f.queries, 4U, 4096U, &client));
    rename.display_name = (AetherStringView){name, sizeof(name)};
    OK(aether_backend_async_rename_workspace(client, writer, rename, 2U, &id));
    memset(name, 'x', sizeof(name));
    result = take(client);
    CHECK(result.query_id.value == id.value);
    CHECK(result.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE);
    CHECK(result.status == AETHER_STATUS_OK);
    CHECK(strcmp(result.value.workspace.display_name, "New") == 0);
    CHECK(result.value.workspace.revision == 2U);
    aether_backend_async_query_result_dispose(&result);
    rename.display_name = text("Stale");
    OK(aether_backend_async_rename_workspace(client, writer, rename, 2U, &id));
    result = take(client);
    CHECK(result.status == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(result.error.code == AETHER_QUERY_ERROR_REVISION_CONFLICT);
    CHECK(result.value.workspace.id.value == 0U);
    aether_backend_async_query_result_dispose(&result);
    rename.expected_revision = 2U;
    OK(aether_backend_async_rename_workspace(client, reader, rename, 2U, &id));
    result = take(client);
    CHECK(result.status == AETHER_STATUS_PERMISSION_DENIED);
    aether_backend_async_query_result_dispose(&result);
    {
        AetherAsyncQueryInput query = {0};
        query.kind = AETHER_ASYNC_QUERY_WORKSPACE;
        query.context = f.scoped;
        query.id = f.workspace[0].id;
        query.receiver_generation = 2U;
        OK(aether_backend_async_query_submit(client, query, &id));
        result = take(client);
        CHECK(result.status == AETHER_STATUS_OK);
        CHECK(strcmp(result.value.workspace.display_name, "New") == 0);
        aether_backend_async_query_result_dispose(&result);
    }
    OK(aether_backend_async_request_session_close(client, writer,
        (AetherSessionCloseInput){f.session[0].id, 2U}, 2U, &id));
    result = take(client);
    CHECK(result.status == AETHER_STATUS_OK);
    CHECK(result.kind == AETHER_ASYNC_COMMAND_CLOSE_SESSION);
    CHECK(result.value.session.state == AETHER_SESSION_DTO_CLOSING);
    CHECK(result.value.session.revision == 3U);
    aether_backend_async_query_result_dispose(&result);
    OK(aether_backend_async_queries_request_close(client));
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    OK(aether_backend_query_session(f.queries, &f.scoped, f.session[0].id,
                                     &session, &error));
    CHECK(session.state == AETHER_SESSION_DTO_CLOSING);
    dispose(&f);
    return 0;
}
