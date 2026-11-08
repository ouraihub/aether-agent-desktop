/* Private substitution in Backend query sources, Core allocation stays real. */
#undef malloc
#undef free
#include "query_fixture.h"

static size_t calls;
static size_t fail_at;
static size_t live;
void* aether_test_malloc(size_t size);
void aether_test_free(void* pointer);

void* aether_test_malloc(size_t size)
{
    void* pointer;
    if (++calls == fail_at) return NULL;
    pointer = malloc(size);
    if (pointer != NULL) ++live;
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
    AetherBlockDto* dto = NULL;
    AetherQueryError error;
    AetherMessageSnapshot before;
    AetherMessageSnapshot after;
    CHECK(live == 1U);
    aether_backend_queries_destroy(f.queries);
    f.queries = NULL;
    fail_at = calls + 1U;
    CHECK(aether_backend_queries_create(f.workspaces, f.runtimes, f.sessions,
        f.messages, &f.queries) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(f.queries == NULL && live == 0U);
    fail_at = 0U;
    OK(aether_backend_queries_create(f.workspaces, f.runtimes, f.sessions,
                                     f.messages, &f.queries));
    OK(aether_message_query(f.messages, f.message[0], &before));
    fail_at = calls + 1U;
    CHECK(aether_backend_query_block(f.queries, &f.scoped, f.message[0].id,
        0U, 1024U, &dto, &error) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(dto == NULL && live == 1U);
    CHECK(error.code == AETHER_QUERY_ERROR_CAPACITY && error.diagnostic_available);
    OK(aether_message_query(f.messages, f.message[0], &after));
    CHECK(before.revision == after.revision && before.stored_bytes == after.stored_bytes);
    fail_at = 0U;
    OK(aether_backend_query_block(f.queries, &f.scoped, f.message[0].id,
                                  0U, 1024U, &dto, &error));
    CHECK(live == 2U);
    dispose(&f);
    CHECK(live == 1U);
    check_text(dto->data, "hello");
    aether_backend_block_destroy(dto);
    CHECK(live == 0U);
    return 0;
}
