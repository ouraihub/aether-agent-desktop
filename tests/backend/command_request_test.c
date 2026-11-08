#include "query_fixture.h"
#include "aether/backend/command_request.h"

static AetherAsyncQueryResult take_work(AetherAsyncQueries* client)
{
    AetherAsyncQueryResult result;
    AetherStatus status;
    do { status = aether_backend_async_query_take(client, &result); }
    while (status == AETHER_STATUS_NOT_FOUND);
    OK(status);
    return result;
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

static void expect_empty_completion(
    AetherAsyncQueries* client, const AetherRequestContext* context)
{
    AetherCommandRequestSnapshot snapshot;
    AetherQueryError error;
    memset(&snapshot, 0xA5, sizeof(snapshot));
    CHECK(aether_backend_take_command_completed(client, context, &snapshot, &error) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.request_id.value == 0U);
    CHECK(snapshot.terminal_revision == 0U);
}

static void consume_and_release(
    AetherAsyncQueries* client, const AetherRequestContext* context, AetherId id,
    AetherCommandRequestState state, AetherStatus result)
{
    AetherCommandRequestSnapshot snapshot;
    AetherQueryError error;
    CHECK(aether_backend_release_command_request(client, context, id, &error) ==
          AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_backend_take_command_completed(client, context, &snapshot, &error));
    CHECK(snapshot.request_id.value == id.value);
    CHECK(snapshot.state == state && snapshot.result == result);
    CHECK(snapshot.terminal_revision == 1U && snapshot.execution_complete);
    expect_empty_completion(client, context);
    snapshot = query_request(client, context, id);
    CHECK(snapshot.state == state && snapshot.terminal_revision == 1U);
    OK(aether_backend_release_command_request(client, context, id, &error));
    memset(&snapshot, 0xA5, sizeof(snapshot));
    CHECK(aether_backend_query_command_request(client, context, id, &snapshot, &error) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.request_id.value == 0U && snapshot.terminal_revision == 0U);
    CHECK(aether_backend_release_command_request(client, context, id, &error) ==
          AETHER_STATUS_NOT_FOUND);
    expect_empty_completion(client, context);
}

static void check_authorization(
    QueryFixture* f, AetherAsyncQueries* client, AetherId id)
{
    AetherRequestContext other = f->scoped;
    AetherCommandRequestSnapshot snapshot;
    AetherQueryError error;
    other.caller_id = aether_id_next();
    memset(&snapshot, 0xA5, sizeof(snapshot));
    CHECK(aether_backend_query_command_request(client, &other, id, &snapshot, &error) ==
          AETHER_STATUS_PERMISSION_DENIED);
    CHECK(snapshot.request_id.value == 0U && snapshot.terminal_revision == 0U);
    CHECK(error.code == AETHER_QUERY_ERROR_FORBIDDEN);
    other = f->scoped;
    other.workspace_scope = f->workspace[1].id;
    memset(&snapshot, 0xA5, sizeof(snapshot));
    CHECK(aether_backend_query_command_request(client, &other, id, &snapshot, &error) ==
          AETHER_STATUS_PERMISSION_DENIED);
    CHECK(snapshot.request_id.value == 0U && snapshot.terminal_revision == 0U);
    CHECK(error.code == AETHER_QUERY_ERROR_FORBIDDEN);
    CHECK(aether_backend_release_command_request(client, &other, id, &error) ==
          AETHER_STATUS_PERMISSION_DENIED);
}

static void check_success(QueryFixture* f, AetherAsyncQueries* client)
{
    AetherStateCommandContext writer = {f->scoped, true};
    AetherAsyncQueryResult work;
    AetherCommandRequestSnapshot snapshot;
    AetherCommandRequestSnapshot independent;
    AetherId id;
    char name[] = "Retained";
    OK(aether_backend_async_rename_workspace(client, writer,
        (AetherWorkspaceRenameInput){f->workspace[0].id, 1U, text(name)}, 7U, &id));
    memset(name, 'x', sizeof(name) - 1U);
    work = take_work(client);
    CHECK(work.query_id.value == id.value);
    CHECK(work.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE);
    CHECK(work.status == AETHER_STATUS_OK);
    CHECK(strcmp(work.value.workspace.display_name, "Retained") == 0);
    CHECK(work.value.workspace.revision == 2U);
    aether_backend_async_query_result_dispose(&work);
    snapshot = query_request(client, &f->scoped, id);
    CHECK(snapshot.state == AETHER_COMMAND_SUCCEEDED);
    CHECK(snapshot.kind == AETHER_ASYNC_COMMAND_RENAME_WORKSPACE);
    CHECK(snapshot.receiver_generation == 7U);
    CHECK(snapshot.result == AETHER_STATUS_OK && snapshot.execution_status == AETHER_STATUS_OK);
    CHECK(snapshot.execution_started && snapshot.execution_complete);
    CHECK(snapshot.terminal_revision == 1U && snapshot.retain_until_ns >= snapshot.deadline_ns);
    CHECK(snapshot.trace.trace_id.bytes[0] == f->scoped.trace.trace_id.bytes[0]);
    CHECK(snapshot.value.workspace.id.value == f->workspace[0].id.value);
    CHECK(snapshot.value.workspace.revision == 2U);
    CHECK(strcmp(snapshot.value.workspace.display_name, "Retained") == 0);
    independent = snapshot;
    snapshot.value.workspace.display_name[0] = 'X';
    snapshot = query_request(client, &f->scoped, id);
    CHECK(strcmp(snapshot.value.workspace.display_name, "Retained") == 0);
    check_authorization(f, client, id);
    consume_and_release(client, &f->scoped, id, AETHER_COMMAND_SUCCEEDED, AETHER_STATUS_OK);
    CHECK(strcmp(independent.value.workspace.display_name, "Retained") == 0);
}

static void check_denied_mutation(QueryFixture* f, AetherAsyncQueries* client)
{
    AetherStateCommandContext reader = {f->scoped, false};
    AetherAsyncQueryResult work;
    AetherCommandRequestSnapshot snapshot;
    AetherId id;
    OK(aether_backend_submit_rename_request(client, reader,
        (AetherWorkspaceRenameInput){f->workspace[0].id, 2U, text("Forbidden")},
        8U, UINT64_C(600000000000), &id));
    work = take_work(client);
    CHECK(work.query_id.value == id.value && work.status == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(work.value.workspace.id.value == 0U);
    aether_backend_async_query_result_dispose(&work);
    snapshot = query_request(client, &f->scoped, id);
    CHECK(snapshot.state == AETHER_COMMAND_FAILED);
    CHECK(snapshot.execution_status == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(snapshot.error.code == AETHER_QUERY_ERROR_FORBIDDEN);
    CHECK(snapshot.error.status == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(snapshot.value.workspace.id.value == 0U);
    consume_and_release(client, &f->scoped, id, AETHER_COMMAND_FAILED,
                        AETHER_STATUS_PERMISSION_DENIED);
}

static void check_stale_rename(QueryFixture* f, AetherAsyncQueries* client)
{
    AetherStateCommandContext writer = {f->scoped, true};
    AetherAsyncQueryResult work;
    AetherCommandRequestSnapshot snapshot;
    AetherId id;
    OK(aether_backend_submit_rename_request(client, writer,
        (AetherWorkspaceRenameInput){f->workspace[0].id, 1U, text("Stale")},
        9U, UINT64_C(600000000000), &id));
    work = take_work(client);
    CHECK(work.query_id.value == id.value && work.status == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(work.error.code == AETHER_QUERY_ERROR_REVISION_CONFLICT);
    aether_backend_async_query_result_dispose(&work);
    snapshot = query_request(client, &f->scoped, id);
    CHECK(snapshot.state == AETHER_COMMAND_FAILED);
    CHECK(snapshot.execution_status == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(snapshot.error.code == AETHER_QUERY_ERROR_REVISION_CONFLICT);
    CHECK(snapshot.value.workspace.id.value == 0U);
    consume_and_release(client, &f->scoped, id, AETHER_COMMAND_FAILED,
                        AETHER_STATUS_INVALID_ARGUMENT);
}

static void check_close(QueryFixture* f, AetherAsyncQueries* client)
{
    AetherStateCommandContext writer = {f->scoped, true};
    AetherAsyncQueryResult work;
    AetherCommandRequestSnapshot snapshot;
    AetherId id;
    OK(aether_backend_submit_close_request(client, writer,
        (AetherSessionCloseInput){f->session[0].id, 2U}, 10U,
        UINT64_C(600000000000), &id));
    work = take_work(client);
    CHECK(work.query_id.value == id.value && work.status == AETHER_STATUS_OK);
    CHECK(work.kind == AETHER_ASYNC_COMMAND_CLOSE_SESSION);
    CHECK(work.value.session.state == AETHER_SESSION_DTO_CLOSING);
    aether_backend_async_query_result_dispose(&work);
    snapshot = query_request(client, &f->scoped, id);
    CHECK(snapshot.state == AETHER_COMMAND_SUCCEEDED);
    CHECK(snapshot.kind == AETHER_ASYNC_COMMAND_CLOSE_SESSION);
    CHECK(snapshot.value.session.id.value == f->session[0].id.value);
    CHECK(snapshot.value.session.state == AETHER_SESSION_DTO_CLOSING);
    CHECK(snapshot.value.session.revision == 3U);
    consume_and_release(client, &f->scoped, id, AETHER_COMMAND_SUCCEEDED, AETHER_STATUS_OK);
}

int main(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherWorkspaceDto workspace;
    AetherSessionDto session;
    AetherQueryError error;
    OK(aether_backend_async_queries_create(f.queries, 2U, 4096U, &client));
    check_success(&f, client);
    check_denied_mutation(&f, client);
    check_stale_rename(&f, client);
    check_close(&f, client);
    OK(aether_backend_async_queries_request_close(client));
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    OK(aether_backend_query_workspace(f.queries, &f.scoped, f.workspace[0].id,
                                      &workspace, &error));
    CHECK(workspace.revision == 2U);
    CHECK(strcmp(workspace.display_name, "Retained") == 0);
    OK(aether_backend_query_session(f.queries, &f.scoped, f.session[0].id, &session, &error));
    CHECK(session.state == AETHER_SESSION_DTO_CLOSING && session.revision == 3U);
    dispose(&f);
    return 0;
}
