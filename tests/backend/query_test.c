#include "query_fixture.h"

static void check_zero(const void* value, size_t size)
{
    const unsigned char* bytes = value;
    size_t i;
    for (i = 0U; i < size; ++i) CHECK(bytes[i] == 0U);
}

static void check_error(const AetherQueryError* error, AetherStatus status,
    AetherQueryErrorCode code, const AetherRequestContext* context)
{
    CHECK(error->status == status && error->code == code);
    CHECK(error->message[0] != '\0');
    CHECK(memchr(error->message, '\0', sizeof(error->message)) != NULL);
    CHECK(strstr(error->message, "secret") == NULL);
    CHECK(strstr(error->message, "C:/") == NULL);
    if (context != NULL) {
        CHECK(error->diagnostic_available);
        CHECK(memcmp(error->diagnostic_id.bytes, context->trace.trace_id.bytes,
            sizeof(error->diagnostic_id.bytes)) == 0);
    }
}

static void test_authorized_consumers(void)
{
    QueryFixture f = fixture();
    const AetherRequestContext* contexts[] = {&f.all, &f.scoped};
    AetherWorkspaceDto workspace;
    AetherSessionDto session;
    AetherRuntimeDto runtime;
    AetherMessageDto message;
    AetherBlockDto* block = NULL;
    AetherQueryError error;
    size_t i;
    for (i = 0U; i < 2U; ++i) {
        OK(aether_backend_query_workspace(f.queries, contexts[i], f.workspace[0].id,
            &workspace, &error));
        CHECK(workspace.id.value == f.workspace[0].id.value);
        CHECK(workspace.revision == 1U && workspace.state == AETHER_WORKSPACE_DTO_OPEN);
        CHECK(strcmp(workspace.display_name, "Primary") == 0);
        OK(aether_backend_query_session(f.queries, contexts[i], f.session[0].id,
            &session, &error));
        CHECK(session.id.value == f.session[0].id.value);
        CHECK(session.workspace_id.value == workspace.id.value);
        CHECK(session.agent_id.value == f.agent.id.value);
        CHECK(session.runtime_id.value == f.runtime[0].id.value);
        CHECK(session.runtime_generation == 1U && session.active_turns == 0U);
        CHECK(session.state == AETHER_SESSION_DTO_IDLE && session.revision > 0U);
        OK(aether_backend_query_runtime(f.queries, contexts[i], session.id,
            session.runtime_id, &runtime, &error));
        CHECK(runtime.id.value == session.runtime_id.value);
        CHECK(runtime.provider_id.value == f.provider.id.value);
        CHECK(runtime.generation == 1U && runtime.revision > 0U);
        CHECK(runtime.state == AETHER_RUNTIME_DTO_READY);
        CHECK(runtime.capabilities_known == AETHER_RUNTIME_CAP_ALL);
        CHECK(runtime.capabilities_supported == AETHER_RUNTIME_CAP_SESSION);
        OK(aether_backend_query_message(f.queries, contexts[i], f.message[0].id,
            &message, &error));
        CHECK(message.id.value == f.message[0].id.value);
        CHECK(message.session_id.value == session.id.value);
        CHECK(message.runtime_generation == 1U && message.block_count == 2U);
        CHECK(message.role == AETHER_MESSAGE_DTO_ASSISTANT);
        CHECK(message.state == AETHER_MESSAGE_DTO_RECEIVING && message.revision > 0U);
        OK(aether_backend_query_block(f.queries, contexts[i], message.id, 0U,
            1024U, &block, &error));
        CHECK(block != NULL && block->id.value == f.block[0].value);
        CHECK(block->kind == AETHER_BLOCK_DTO_TEXT && block->ordinal == 0U);
        CHECK(block->cursor == 1U && block->revision == 1U && !block->finalized);
        CHECK(block->resource_id.value == 0U && block->tool_call_id.value == 0U);
        CHECK(block->tool_generation == 0U);
        check_text(block->data, "hello");
        check_text(block->label, "");
        CHECK(error.status == AETHER_STATUS_OK && error.code == AETHER_QUERY_ERROR_NONE);
        aether_backend_block_destroy(block);
    }
    OK(aether_backend_query_workspace(f.queries, &f.all, f.workspace[1].id,
        &workspace, &error));
    CHECK(strcmp(workspace.display_name, "Foreign") == 0);
    OK(aether_backend_query_session(f.queries, &f.all, f.session[1].id, &session, &error));
    OK(aether_backend_query_runtime(f.queries, &f.all, f.session[1].id, f.runtime[1].id,
        &runtime, &error));
    OK(aether_backend_query_message(f.queries, &f.all, f.message[1].id, &message, &error));
    OK(aether_backend_query_block(f.queries, &f.all, f.message[1].id, 0U,
        1024U, &block, &error));
    check_text(block->data, "hello");
    aether_backend_block_destroy(block);
    dispose(&f);
}

static void test_scope_and_runtime_binding(void)
{
    QueryFixture f = fixture();
    AetherWorkspaceDto workspace;
    AetherSessionDto session;
    AetherRuntimeDto runtime;
    AetherBlockDto sentinel = {0};
    AetherMessageDto message;
    AetherBlockDto* block;
    AetherQueryError error;
    memset(&workspace, 0xA5, sizeof(workspace));
    CHECK(aether_backend_query_workspace(f.queries, &f.scoped, f.workspace[1].id,
        &workspace, &error) == AETHER_STATUS_PERMISSION_DENIED);
    check_zero(&workspace, sizeof(workspace));
    check_error(&error, AETHER_STATUS_PERMISSION_DENIED, AETHER_QUERY_ERROR_FORBIDDEN,
        &f.scoped);
    memset(&session, 0xA5, sizeof(session));
    CHECK(aether_backend_query_session(f.queries, &f.scoped, f.session[1].id,
        &session, &error) == AETHER_STATUS_PERMISSION_DENIED);
    check_zero(&session, sizeof(session));
    check_error(&error, AETHER_STATUS_PERMISSION_DENIED, AETHER_QUERY_ERROR_FORBIDDEN,
        &f.scoped);
    memset(&message, 0xA5, sizeof(message));
    CHECK(aether_backend_query_message(f.queries, &f.scoped, f.message[1].id,
        &message, &error) == AETHER_STATUS_PERMISSION_DENIED);
    check_zero(&message, sizeof(message));
    check_error(&error, AETHER_STATUS_PERMISSION_DENIED, AETHER_QUERY_ERROR_FORBIDDEN,
        &f.scoped);
    block = &sentinel;
    CHECK(aether_backend_query_block(f.queries, &f.scoped, f.message[1].id, 0U,
        1024U, &block, &error) == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(block == NULL);
    check_error(&error, AETHER_STATUS_PERMISSION_DENIED, AETHER_QUERY_ERROR_FORBIDDEN,
        &f.scoped);
    memset(&runtime, 0xA5, sizeof(runtime));
    CHECK(aether_backend_query_runtime(f.queries, &f.scoped, f.session[1].id,
        f.runtime[1].id, &runtime, &error) == AETHER_STATUS_PERMISSION_DENIED);
    check_zero(&runtime, sizeof(runtime));
    check_error(&error, AETHER_STATUS_PERMISSION_DENIED, AETHER_QUERY_ERROR_FORBIDDEN,
        &f.scoped);
    memset(&runtime, 0xA5, sizeof(runtime));
    CHECK(aether_backend_query_runtime(f.queries, &f.all, f.session[0].id,
        f.runtime[1].id, &runtime, &error) == AETHER_STATUS_PERMISSION_DENIED);
    check_zero(&runtime, sizeof(runtime));
    check_error(&error, AETHER_STATUS_PERMISSION_DENIED, AETHER_QUERY_ERROR_FORBIDDEN,
        &f.all);
    dispose(&f);
}

static void reject_context(QueryFixture* f, const AetherRequestContext* context,
    AetherStatus status, AetherQueryErrorCode code)
{
    AetherWorkspaceDto workspace;
    AetherSessionDto session;
    AetherRuntimeDto runtime;
    AetherMessageDto message;
    AetherBlockDto sentinel = {0};
    AetherBlockDto* block = &sentinel;
    AetherQueryError error;
    memset(&workspace, 0xA5, sizeof(workspace));
    CHECK(aether_backend_query_workspace(f->queries, context, f->workspace[0].id,
        &workspace, &error) == status);
    check_zero(&workspace, sizeof(workspace));
    check_error(&error, status, code, NULL);
    memset(&session, 0xA5, sizeof(session));
    CHECK(aether_backend_query_session(f->queries, context, f->session[0].id,
        &session, &error) == status);
    check_zero(&session, sizeof(session));
    check_error(&error, status, code, NULL);
    memset(&runtime, 0xA5, sizeof(runtime));
    CHECK(aether_backend_query_runtime(f->queries, context, f->session[0].id,
        f->runtime[0].id, &runtime, &error) == status);
    check_zero(&runtime, sizeof(runtime));
    check_error(&error, status, code, NULL);
    memset(&message, 0xA5, sizeof(message));
    CHECK(aether_backend_query_message(f->queries, context, f->message[0].id,
        &message, &error) == status);
    check_zero(&message, sizeof(message));
    check_error(&error, status, code, NULL);
    CHECK(aether_backend_query_block(f->queries, context, f->message[0].id, 0U,
        1024U, &block, &error) == status);
    CHECK(block == NULL);
    check_error(&error, status, code, NULL);
}

static void test_invalid_contexts(void)
{
    QueryFixture f = fixture();
    AetherRequestContext bad = f.all;
    bad.api_version = AETHER_BACKEND_QUERY_VERSION + 1U;
    reject_context(&f, &bad, AETHER_STATUS_NOT_SUPPORTED, AETHER_QUERY_ERROR_VERSION);
    bad = f.all;
    bad.caller_id = (AetherId){0};
    reject_context(&f, &bad, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT);
    bad = f.all;
    bad.read_all_workspaces = false;
    reject_context(&f, &bad, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT);
    bad = f.all;
    bad.trace.valid = false;
    reject_context(&f, &bad, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT);
    bad = f.all;
    memset(&bad.trace.trace_id, 0, sizeof(bad.trace.trace_id));
    reject_context(&f, &bad, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT);
    bad = f.all;
    memset(&bad.trace.span_id, 0, sizeof(bad.trace.span_id));
    reject_context(&f, &bad, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT);
    reject_context(&f, NULL, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT);
    dispose(&f);
}

static void test_missing_objects(void)
{
    QueryFixture f = fixture();
    AetherId missing = aether_id_next();
    AetherWorkspaceDto workspace;
    AetherSessionDto session;
    AetherRuntimeDto runtime;
    AetherMessageDto message;
    AetherBlockDto sentinel = {0};
    AetherBlockDto* block = &sentinel;
    AetherQueryError error;
    memset(&workspace, 0xA5, sizeof(workspace));
    CHECK(aether_backend_query_workspace(f.queries, &f.all, missing, &workspace,
        &error) == AETHER_STATUS_NOT_FOUND);
    check_zero(&workspace, sizeof(workspace));
    check_error(&error, AETHER_STATUS_NOT_FOUND, AETHER_QUERY_ERROR_MISSING, &f.all);
    memset(&session, 0xA5, sizeof(session));
    CHECK(aether_backend_query_session(f.queries, &f.all, missing, &session,
        &error) == AETHER_STATUS_NOT_FOUND);
    check_zero(&session, sizeof(session));
    check_error(&error, AETHER_STATUS_NOT_FOUND, AETHER_QUERY_ERROR_MISSING, &f.all);
    memset(&runtime, 0xA5, sizeof(runtime));
    CHECK(aether_backend_query_runtime(f.queries, &f.all, missing, f.runtime[0].id,
        &runtime, &error) == AETHER_STATUS_NOT_FOUND);
    check_zero(&runtime, sizeof(runtime));
    check_error(&error, AETHER_STATUS_NOT_FOUND, AETHER_QUERY_ERROR_MISSING, &f.all);
    memset(&message, 0xA5, sizeof(message));
    CHECK(aether_backend_query_message(f.queries, &f.all, missing, &message,
        &error) == AETHER_STATUS_NOT_FOUND);
    check_zero(&message, sizeof(message));
    check_error(&error, AETHER_STATUS_NOT_FOUND, AETHER_QUERY_ERROR_MISSING, &f.all);
    CHECK(aether_backend_query_block(f.queries, &f.all, missing, 0U, 1024U,
        &block, &error) == AETHER_STATUS_NOT_FOUND);
    CHECK(block == NULL);
    check_error(&error, AETHER_STATUS_NOT_FOUND, AETHER_QUERY_ERROR_MISSING, &f.all);
    block = &sentinel;
    CHECK(aether_backend_query_block(f.queries, &f.all, f.message[0].id, 2U, 1024U,
        &block, &error) == AETHER_STATUS_NOT_FOUND);
    CHECK(block == NULL);
    check_error(&error, AETHER_STATUS_NOT_FOUND, AETHER_QUERY_ERROR_MISSING, &f.all);
    dispose(&f);
}

static void test_exact_response_limit(void)
{
    QueryFixture f = fixture();
    AetherBlockDto sentinel = {0};
    AetherBlockDto* block = &sentinel;
    AetherQueryError error;
    size_t exact = sizeof(AetherBlockDto) + strlen("row") + strlen("csv") + 2U;
    CHECK(aether_backend_query_block(f.queries, &f.scoped, f.message[0].id, 1U,
        exact - 1U, &block, &error) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(block == NULL);
    check_error(&error, AETHER_STATUS_OUT_OF_MEMORY, AETHER_QUERY_ERROR_CAPACITY,
        &f.scoped);
    OK(aether_backend_query_block(f.queries, &f.scoped, f.message[0].id, 1U,
        exact, &block, &error));
    CHECK(block->kind == AETHER_BLOCK_DTO_STRUCTURED);
    check_text(block->data, "row");
    check_text(block->label, "csv");
    aether_backend_block_destroy(block);
    block = &sentinel;
    CHECK(aether_backend_query_block(f.queries, &f.all, f.message[0].id, 0U,
        0U, &block, &error) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(block == NULL);
    check_error(&error, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT, &f.all);
    block = &sentinel;
    CHECK(aether_backend_query_block(f.queries, &f.all, f.message[0].id, 0U,
        1024U * 1024U + 1U, &block, &error) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(block == NULL);
    check_error(&error, AETHER_STATUS_INVALID_ARGUMENT, AETHER_QUERY_ERROR_INPUT, &f.all);
    dispose(&f);
}

static void test_independent_owned_lifetimes(void)
{
    QueryFixture f = fixture();
    AetherWorkspaceDto workspace;
    AetherWorkspaceDto current_workspace;
    AetherSessionDto session;
    AetherSessionDto current_session;
    AetherRuntimeDto runtime;
    AetherRuntimeDto current_runtime;
    AetherMessageDto message;
    AetherMessageDto current_message;
    AetherBlockDto* first;
    AetherBlockDto* second;
    AetherBlockDto* structured;
    AetherBlockDto* current;
    AetherBlockSnapshot* core;
    AetherQueryError error;
    AetherMessageFact change;
    OK(aether_backend_query_workspace(f.queries, &f.scoped, f.workspace[0].id,
        &workspace, &error));
    OK(aether_backend_query_session(f.queries, &f.scoped, f.session[0].id, &session, &error));
    OK(aether_backend_query_runtime(f.queries, &f.scoped, session.id, session.runtime_id,
        &runtime, &error));
    OK(aether_backend_query_message(f.queries, &f.scoped, f.message[0].id, &message, &error));
    OK(aether_backend_query_block(f.queries, &f.scoped, message.id, 0U,
        1024U, &first, &error));
    OK(aether_backend_query_block(f.queries, &f.scoped, message.id, 0U,
        1024U, &second, &error));
    OK(aether_backend_query_block(f.queries, &f.scoped, message.id, 1U,
        1024U, &structured, &error));
    OK(aether_message_query_block(f.messages, f.message[0], 0U, &core));
    CHECK(first != second && first->data.data != second->data.data);
    CHECK(first->label.data != second->label.data);
    CHECK(first->data.data != core->payload.data.data);
    CHECK(first->label.data != core->payload.label.data);
    aether_message_block_snapshot_destroy(core);
    aether_backend_block_destroy(second);
    OK(aether_workspace_update_name(f.workspaces, f.workspace[0], workspace.revision,
        text("Renamed")));
    OK(aether_session_start_turn(f.sessions, f.session[0]));
    OK(aether_runtime_update(f.runtimes, f.runtime[0], 1U, AETHER_RUNTIME_READY,
        (AetherRuntimeCapabilities){AETHER_RUNTIME_CAP_ALL, AETHER_RUNTIME_CAP_ALL}));
    change = content_fact(&f, 0U, 0U, f.block[0], AETHER_BLOCK_TEXT, "changed", "");
    change.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    change.cursor = 2U;
    change.expected_revision = 1U;
    OK(aether_message_apply(f.messages, change));
    OK(aether_backend_query_block(f.queries, &f.scoped, message.id, 0U,
        1024U, &current, &error));
    check_text(current->data, "changed");
    CHECK(current->revision > first->revision && current->cursor == 2U);
    aether_backend_block_destroy(current);
    OK(aether_backend_query_workspace(f.queries, &f.scoped, workspace.id,
        &current_workspace, &error));
    CHECK(strcmp(current_workspace.display_name, "Renamed") == 0);
    OK(aether_backend_query_session(f.queries, &f.scoped, session.id,
        &current_session, &error));
    CHECK(current_session.state == AETHER_SESSION_DTO_ACTIVE);
    CHECK(current_session.active_turns == 1U && current_session.revision > session.revision);
    OK(aether_backend_query_runtime(f.queries, &f.scoped, session.id, runtime.id,
        &current_runtime, &error));
    CHECK(current_runtime.capabilities_supported == AETHER_RUNTIME_CAP_ALL);
    CHECK(current_runtime.revision > runtime.revision);
    OK(aether_backend_query_message(f.queries, &f.scoped, message.id,
        &current_message, &error));
    CHECK(current_message.revision > message.revision);
    change = content_fact(&f, 0U, 1U, f.structured, AETHER_BLOCK_STRUCTURED, "new", "csv");
    change.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    change.cursor = 2U;
    change.expected_revision = 1U;
    OK(aether_message_apply(f.messages, change));
    dispose(&f);
    CHECK(strcmp(workspace.display_name, "Primary") == 0 && workspace.revision == 1U);
    CHECK(session.state == AETHER_SESSION_DTO_IDLE && session.active_turns == 0U);
    CHECK(runtime.capabilities_supported == AETHER_RUNTIME_CAP_SESSION);
    CHECK(message.state == AETHER_MESSAGE_DTO_RECEIVING && message.block_count == 2U);
    CHECK(first->kind == AETHER_BLOCK_DTO_TEXT && first->cursor == 1U);
    check_text(first->data, "hello");
    check_text(first->label, "");
    check_text(structured->data, "row");
    check_text(structured->label, "csv");
    aether_backend_block_destroy(first);
    aether_backend_block_destroy(structured);
    aether_backend_block_destroy(NULL);
}

int main(void)
{
    test_authorized_consumers();
    test_scope_and_runtime_binding();
    test_invalid_contexts();
    test_missing_objects();
    test_exact_response_limit();
    test_independent_owned_lifetimes();
    return EXIT_SUCCESS;
}
