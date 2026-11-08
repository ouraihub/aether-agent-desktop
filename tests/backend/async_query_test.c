#include "query_fixture.h"
#include "aether/backend/async_query.h"

static AetherAsyncQueryResult take(AetherAsyncQueries* client)
{
    AetherAsyncQueryResult result;
    AetherStatus status;
    do { status = aether_backend_async_query_take(client, &result); }
    while (status == AETHER_STATUS_NOT_FOUND);
    OK(status);
    return result;
}

static void close_client(AetherAsyncQueries** client)
{
    OK(aether_backend_async_queries_request_close(*client));
    while (!aether_backend_async_queries_is_closed(*client)) { }
    OK(aether_backend_async_queries_destroy(client));
}

static AetherAsyncQueryInput input(QueryFixture* f, AetherAsyncQueryKind kind)
{
    AetherAsyncQueryInput result = {0};
    result.kind = kind;
    result.context = f->scoped;
    result.id = f->message[0].id;
    result.max_bytes = 1024U;
    result.receiver_generation = 9U;
    return result;
}

static void copied_context_and_retained_capacity(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherAsyncQueryInput request = input(&f, AETHER_ASYNC_QUERY_BLOCK);
    AetherAsyncQueryResult result;
    AetherId id;
    AetherId rejected = {7U};
    OK(aether_backend_async_queries_create(f.queries, 1U, 4096U, &client));
    OK(aether_backend_async_query_submit(client, request, &id));
    request.context.workspace_scope = f.workspace[1].id;
    CHECK(aether_backend_async_query_submit(client, request, &rejected) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.value == 0U);
    CHECK(aether_backend_async_query_cancel(client, id, 8U) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    result = take(client);
    CHECK(result.query_id.value == id.value && result.receiver_generation == 9U);
    CHECK(result.status == AETHER_STATUS_OK);
    CHECK(result.value.block != NULL);
    check_text(result.value.block->data, "hello");
    close_client(&client);
    dispose(&f);
    check_text(result.value.block->data, "hello");
    aether_backend_async_query_result_dispose(&result);
    CHECK(result.query_id.value == 0U);
}

static void errors_budgets_and_cancel(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherAsyncQueryInput request = input(&f, AETHER_ASYNC_QUERY_BLOCK);
    AetherAsyncQueryResult result;
    AetherId id;
    AetherStatus cancelled;
    OK(aether_backend_async_queries_create(f.queries, 2U,
        sizeof(AetherAsyncQueryResult) + 1023U, &client));
    CHECK(aether_backend_async_query_submit(client, request, &id) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(id.value == 0U);
    request.max_bytes = 1023U;
    request.context.api_version = 99U;
    OK(aether_backend_async_query_submit(client, request, &id));
    result = take(client);
    CHECK(result.status == AETHER_STATUS_NOT_SUPPORTED);
    CHECK(result.error.code == AETHER_QUERY_ERROR_VERSION);
    CHECK(result.value.block == NULL);
    aether_backend_async_query_result_dispose(&result);
    request.context = f.scoped;
    request.id = f.message[1].id;
    OK(aether_backend_async_query_submit(client, request, &id));
    result = take(client);
    CHECK(result.status == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(result.value.block == NULL);
    aether_backend_async_query_result_dispose(&result);
    request.id = f.message[0].id;
    OK(aether_backend_async_query_submit(client, request, &id));
    cancelled = aether_backend_async_query_cancel(client, id, 9U);
    CHECK(cancelled == AETHER_STATUS_OK || cancelled == AETHER_STATUS_ALREADY_EXISTS);
    result = take(client);
    CHECK(result.status == AETHER_STATUS_OK || result.status == AETHER_STATUS_CANCELLED);
    if (result.status == AETHER_STATUS_OK) check_text(result.value.block->data, "hello");
    else CHECK(result.value.block == NULL);
    aether_backend_async_query_result_dispose(&result);
    OK(aether_backend_async_queries_request_close(client));
    CHECK(aether_backend_async_query_submit(client, request, &id) ==
          AETHER_STATUS_NOT_INITIALIZED);
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    dispose(&f);
}

int main(void)
{
    copied_context_and_retained_capacity();
    errors_budgets_and_cancel();
    return 0;
}
