/* Allocator substitution is private to this test's message implementation. */
#undef malloc
#undef calloc
#undef free
#include "aether/core/message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "Message allocation check failed: %d\n", __LINE__); \
    return 1; } } while (0)

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
    if (pointer != NULL) --live;
    free(pointer);
}

static AetherStringView text(const char* s)
{
    return (AetherStringView){s, strlen(s)};
}

int main(void)
{
    AetherWorkspaceRegistry* workspaces = NULL;
    AetherRuntimeRegistry* runtimes = NULL;
    AetherSessionRegistry* sessions = NULL;
    AetherMessageRegistry* messages = NULL;
    AetherWorkspaceId workspace;
    AetherAgentConfigId agent;
    AetherProviderId provider;
    AetherRuntimeId runtime;
    AetherSessionId session;
    AetherMessageId message;
    AetherMessageSnapshot before;
    AetherMessageSnapshot after;
    AetherBlockSnapshot* block = NULL;
    AetherMessageFact fact = {0};
    size_t failure;
    CHECK(aether_workspace_registry_create(1U, 1U, &workspaces) == AETHER_STATUS_OK);
    CHECK(aether_runtime_registry_create(1U, 1U, &runtimes) == AETHER_STATUS_OK);
    CHECK(aether_workspace_create(workspaces,
        (AetherWorkspaceInput){text("W"), text("C:/w")}, &workspace) ==
        AETHER_STATUS_OK);
    CHECK(aether_runtime_provider_register(runtimes, text("p"), text("1"),
                                            &provider) == AETHER_STATUS_OK);
    CHECK(aether_runtime_register(runtimes, provider, &runtime) == AETHER_STATUS_OK);
    CHECK(aether_runtime_update(runtimes, runtime, 1U, AETHER_RUNTIME_READY,
                                (AetherRuntimeCapabilities){0}) == AETHER_STATUS_OK);
    CHECK(aether_agent_config_create(workspaces,
        (AetherAgentConfigInput){text("A"), provider, text("m")}, &agent) ==
        AETHER_STATUS_OK);
    CHECK(aether_session_registry_create(1U, workspaces, runtimes, &sessions) ==
        AETHER_STATUS_OK);
    CHECK(aether_session_create(sessions, workspace, agent, runtime, 1U,
                                &session) == AETHER_STATUS_OK);
    CHECK(aether_session_initialize(sessions, session) == AETHER_STATUS_OK);
    for (failure = 1U; failure <= 2U; ++failure) {
        calls = 0U;
        fail_at = failure;
        CHECK(aether_message_registry_create(aether_message_default_limits(),
            sessions, runtimes, &messages) == AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(messages == NULL && live == 0U);
    }
    fail_at = 0U;
    CHECK(aether_message_registry_create(aether_message_default_limits(),
        sessions, runtimes, &messages) == AETHER_STATUS_OK);
    fail_at = calls + 1U;
    CHECK(aether_message_create(messages, session, 1U,
        AETHER_MESSAGE_ROLE_ASSISTANT, &message) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(message.id.value == 0U && live == 2U);
    fail_at = 0U;
    CHECK(aether_message_create(messages, session, 1U,
        AETHER_MESSAGE_ROLE_ASSISTANT, &message) == AETHER_STATUS_OK);
    fact.session_id = session;
    fact.message_id = message;
    fact.runtime_generation = 1U;
    fact.block_id = aether_id_next();
    fact.cursor = 1U;
    fact.payload.data = text("kept");
    CHECK(aether_message_query(messages, message, &before) == AETHER_STATUS_OK);
    fail_at = calls + 1U;
    CHECK(aether_message_apply(messages, fact) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(aether_message_query(messages, message, &after) == AETHER_STATUS_OK);
    CHECK(before.revision == after.revision && after.block_count == 0U &&
          before.stored_bytes == after.stored_bytes);
    fail_at = 0U;
    CHECK(aether_message_apply(messages, fact) == AETHER_STATUS_OK);
    fact.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    fact.expected_revision = 1U;
    fact.cursor = 2U;
    fact.payload.data = text("must not commit");
    CHECK(aether_message_query(messages, message, &before) == AETHER_STATUS_OK);
    fail_at = calls + 1U;
    CHECK(aether_message_apply(messages, fact) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(aether_message_query(messages, message, &after) == AETHER_STATUS_OK);
    CHECK(before.revision == after.revision &&
          before.stored_bytes == after.stored_bytes);
    fail_at = 0U;
    CHECK(aether_message_query_block(messages, message, 0U, &block) ==
          AETHER_STATUS_OK);
    CHECK(block->revision == 1U && block->payload.data.length == 4U &&
          memcmp(block->payload.data.data, "kept", 4U) == 0);
    aether_message_block_snapshot_destroy(block);
    block = NULL;
    fail_at = calls + 1U;
    CHECK(aether_message_query_block(messages, message, 0U, &block) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(block == NULL);
    CHECK(aether_message_registry_destroy(&messages) == AETHER_STATUS_OK);
    CHECK(live == 0U);
    CHECK(aether_session_registry_destroy(&sessions) == AETHER_STATUS_OK);
    CHECK(aether_runtime_registry_destroy(&runtimes) == AETHER_STATUS_OK);
    CHECK(aether_workspace_registry_destroy(&workspaces) == AETHER_STATUS_OK);
    return 0;
}
