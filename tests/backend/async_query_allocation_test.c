/* Private allocator substitution in async_query.c only. */
#undef malloc
#undef calloc
#undef free
#include "query_fixture.h"
#include "aether/backend/async_query.h"

static size_t allocations;
static size_t fail_at;
static size_t live;
void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);

void* aether_test_malloc(size_t size)
{
    void* pointer;
    if (++allocations == fail_at) return NULL;
    pointer = malloc(size);
    if (pointer != NULL) ++live;
    return pointer;
}

void* aether_test_calloc(size_t count, size_t size)
{
    void* pointer;
    if (size != 0U && count > SIZE_MAX / size) return NULL;
    pointer = aether_test_malloc(count * size);
    if (pointer != NULL) (void)memset(pointer, 0, count * size);
    return pointer;
}

void aether_test_free(void* pointer)
{
    if (pointer != NULL) {
        CHECK(live != 0U);
        --live;
    }
    free(pointer);
}

int main(void)
{
    QueryFixture f = fixture();
    AetherAsyncQueries* client = NULL;
    AetherAsyncQueryInput input = {0};
    AetherAsyncQueryResult result;
    AetherStatus status;
    AetherId id;
    size_t before;
    for (size_t failure = 1U; failure <= 2U; ++failure) {
        allocations = 0U;
        fail_at = failure;
        CHECK(aether_backend_async_queries_create(f.queries, 1U, 4096U, &client) ==
              AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(client == NULL && live == 0U);
    }
    fail_at = 0U;
    OK(aether_backend_async_queries_create(f.queries, 1U, 4096U, &client));
    before = allocations;
    input.kind = AETHER_ASYNC_QUERY_MESSAGE;
    input.id = f.message[0].id;
    input.context = f.scoped;
    input.receiver_generation = 2U;
    fail_at = allocations + 1U;
    OK(aether_backend_async_query_submit(client, input, &id));
    do { status = aether_backend_async_query_take(client, &result); }
    while (status == AETHER_STATUS_NOT_FOUND);
    OK(status);
    CHECK(allocations == before && result.status == AETHER_STATUS_OK);
    CHECK(result.value.message.block_count == 2U);
    aether_backend_async_query_result_dispose(&result);
    OK(aether_backend_async_queries_request_close(client));
    while (!aether_backend_async_queries_is_closed(client)) { }
    OK(aether_backend_async_queries_destroy(&client));
    CHECK(live == 0U);
    dispose(&f);
    return 0;
}
