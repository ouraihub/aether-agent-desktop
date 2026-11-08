/* Allocator substitution is private to this test's permission implementation. */
#undef malloc
#undef calloc
#undef free
#include "permission_fixture.h"

static size_t calls;
static size_t fail_at;
static size_t live;

void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);

void* aether_test_malloc(size_t size)
{
    void* pointer;
    if (++calls == fail_at) return NULL;
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

static void constructor_failures(PermissionFixture* f, AetherPermissionLimits limits)
{
    CHECK(live == 5U);
    OK(aether_permission_registry_destroy(&f->permissions));
    CHECK(live == 0U);
    for (size_t failure = 1U; failure <= 5U; ++failure) {
        calls = 0U;
        fail_at = failure;
        CHECK(aether_permission_registry_create(limits, f->workspaces, f->sessions,
            f->runtimes, &f->permissions) == AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(f->permissions == NULL && live == 0U);
    }
    calls = 0U;
    fail_at = 0U;
    OK(aether_permission_registry_create(limits, f->workspaces, f->sessions,
                                         f->runtimes, &f->permissions));
    CHECK(calls == 5U && live == 5U);
}

static void begin_failure_and_once_survives_memory_failure(PermissionFixture* f)
{
    AetherPermissionResult result;
    AetherPermissionSnapshot snapshot;
    AetherPermissionId id;
    AetherApprovalResult answer;
    size_t before;
    mode(f, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED);
    fail_at = calls + 1U;
    CHECK(aether_permission_begin(f->permissions, f->input, 10U, 1000U, &result) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(live == 5U && result.decision == AETHER_PERMISSION_DENY &&
          result.permission_id.id.value == 0U && result.policy_revision == 0U);
    fail_at = 0U;
    /* One wait slot and the exact same request expose either kind of ghost commit. */
    id = begin(f);
    CHECK(live == 6U);
    OK(aether_permission_query(f->permissions, id, 11U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_WAITING);
    CHECK(snapshot.request_id.value == f->input.request_id.value);
    fail_at = calls + 1U;
    OK(aether_permission_resolve(f->permissions, id, f->input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TOOL, &answer));
    CHECK(answer.approved && answer.memory_status == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(live == 6U);
    OK(aether_permission_query(f->permissions, id, 11U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_APPROVED);
    before = calls;
    fail_at = calls + 1U;
    CHECK(evaluate(f).decision == AETHER_PERMISSION_ASK);
    OK(aether_permission_consume(f->permissions, id, f->input, 12U));
    CHECK(calls == before);
    CHECK(aether_permission_consume(f->permissions, id, f->input, 12U) ==
          AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_release(f->permissions, id));
    CHECK(live == 5U);
    f->input.request_id = aether_id_next();
    CHECK(aether_permission_consume(f->permissions, (AetherPermissionId){0},
                                    f->input, 13U) == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(calls == before);
    fail_at = 0U;
}

static void successful_memory_and_destroy(PermissionFixture* f)
{
    AetherPermissionTarget target = {text("C:/w/file")};
    AetherPermissionId id;
    AetherApprovalResult answer;
    size_t before;
    f->input.targets = &target;
    f->input.target_count = 1U;
    id = begin(f);
    CHECK(live == 6U);
    before = calls;
    OK(aether_permission_resolve(f->permissions, id, f->input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TARGETS, &answer));
    CHECK(answer.approved && answer.memory_status == AETHER_STATUS_OK);
    CHECK(calls == before + 1U && live == 7U);
    before = calls;
    fail_at = calls + 1U;
    OK(aether_permission_consume(f->permissions, id, f->input, 12U));
    OK(aether_permission_release(f->permissions, id));
    CHECK(live == 6U);
    f->input.request_id = aether_id_next();
    CHECK(evaluate(f).source == AETHER_PERMISSION_SOURCE_MEMORY);
    OK(aether_permission_consume(f->permissions, (AetherPermissionId){0}, f->input, 13U));
    CHECK(calls == before);
    /* Leave owned target memory and another wait for destroy's complete cleanup. */
    fail_at = 0U;
    target.identity = text("C:/w/other");
    (void)begin(f);
    CHECK(live == 7U);
    dispose(f);
    CHECK(live == 0U && f->permissions == NULL);
}

int main(void)
{
    AetherPermissionLimits limits = aether_permission_default_limits();
    PermissionFixture f;
    limits.waits = 1U;
    f = fixture(limits);
    constructor_failures(&f, limits);
    begin_failure_and_once_survives_memory_failure(&f);
    successful_memory_and_destroy(&f);
    return 0;
}
