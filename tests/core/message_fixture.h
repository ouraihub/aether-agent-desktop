#ifndef AETHER_TEST_MESSAGE_FIXTURE_H
#define AETHER_TEST_MESSAGE_FIXTURE_H

#include "aether/core/message.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#define OK(expression) CHECK((expression) == AETHER_STATUS_OK)

typedef struct MessageFixture {
    AetherWorkspaceRegistry* workspaces;
    AetherRuntimeRegistry* runtimes;
    AetherSessionRegistry* sessions;
    AetherMessageRegistry* messages;
    AetherRuntimeId runtime;
    AetherSessionId session;
    AetherSessionId other_session;
} MessageFixture;

static inline AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}

static inline MessageFixture fixture(AetherMessageLimits limits)
{
    MessageFixture f = {0};
    AetherWorkspaceId workspace;
    AetherAgentConfigId agent;
    AetherProviderId provider;
    OK(aether_workspace_registry_create(1U, 1U, &f.workspaces));
    OK(aether_runtime_registry_create(1U, 1U, &f.runtimes));
    OK(aether_workspace_create(f.workspaces,
        (AetherWorkspaceInput){text("W"), text("C:/w")}, &workspace));
    OK(aether_runtime_provider_register(f.runtimes, text("p"), text("1"), &provider));
    OK(aether_runtime_register(f.runtimes, provider, &f.runtime));
    OK(aether_runtime_update(f.runtimes, f.runtime, 1U, AETHER_RUNTIME_READY,
        (AetherRuntimeCapabilities){AETHER_RUNTIME_CAP_SESSION, AETHER_RUNTIME_CAP_SESSION}));
    OK(aether_agent_config_create(f.workspaces,
        (AetherAgentConfigInput){text("A"), provider, text("m")}, &agent));
    OK(aether_session_registry_create(2U, f.workspaces, f.runtimes, &f.sessions));
    OK(aether_session_create(f.sessions, workspace, agent, f.runtime, 1U, &f.session));
    OK(aether_session_initialize(f.sessions, f.session));
    OK(aether_session_create(f.sessions, workspace, agent, f.runtime, 1U, &f.other_session));
    OK(aether_session_initialize(f.sessions, f.other_session));
    OK(aether_message_registry_create(limits, f.sessions, f.runtimes, &f.messages));
    return f;
}

static inline void dispose(MessageFixture* f)
{
    OK(aether_message_registry_destroy(&f->messages));
    OK(aether_session_registry_destroy(&f->sessions));
    OK(aether_runtime_registry_destroy(&f->runtimes));
    OK(aether_workspace_registry_destroy(&f->workspaces));
    CHECK(f->messages == NULL && f->sessions == NULL);
}

static inline AetherMessageId new_message(MessageFixture* f, AetherMessageRole role)
{
    AetherMessageId id = {0};
    OK(aether_message_create(f->messages, f->session, 1U, role, &id));
    CHECK(id.id.value != 0U);
    return id;
}

static inline AetherMessageFact block_fact(
    MessageFixture* f, AetherMessageId message, uint32_t ordinal,
    AetherBlockKind kind, const char* data, const char* label)
{
    AetherMessageFact fact = {0};
    fact.session_id = f->session;
    fact.message_id = message;
    fact.runtime_generation = 1U;
    fact.operation = AETHER_MESSAGE_CREATE_BLOCK;
    fact.block_id = aether_id_next();
    fact.ordinal = ordinal;
    fact.cursor = 1U;
    fact.payload.kind = kind;
    fact.payload.data = text(data);
    fact.payload.label = text(label);
    return fact;
}

static inline AetherMessageSnapshot metadata(MessageFixture* f, AetherMessageId id)
{
    AetherMessageSnapshot snapshot;
    OK(aether_message_query(f->messages, id, &snapshot));
    return snapshot;
}

static inline AetherBlockSnapshot* block_snapshot(
    MessageFixture* f, AetherMessageId id, uint32_t ordinal)
{
    AetherBlockSnapshot* snapshot = NULL;
    OK(aether_message_query_block(f->messages, id, ordinal, &snapshot));
    CHECK(snapshot != NULL);
    return snapshot;
}

static inline void check_text(AetherStringView actual, const char* expected)
{
    CHECK(actual.length == strlen(expected));
    CHECK(actual.length == 0U || memcmp(actual.data, expected, actual.length) == 0);
}

static inline void finalize(MessageFixture* f, AetherMessageFact fact)
{
    AetherBlockSnapshot* snapshot = block_snapshot(f, fact.message_id, fact.ordinal);
    fact.operation = AETHER_MESSAGE_FINALIZE_BLOCK;
    fact.cursor = snapshot->cursor + 1U;
    fact.expected_revision = snapshot->revision;
    fact.payload = snapshot->payload;
    OK(aether_message_apply(f->messages, fact));
    aether_message_block_snapshot_destroy(snapshot);
}

static inline AetherMessageFact end_fact(
    MessageFixture* f, AetherMessageId id, AetherMessageState state)
{
    AetherMessageFact fact = {0};
    fact.session_id = f->session;
    fact.message_id = id;
    fact.runtime_generation = 1U;
    fact.operation = AETHER_MESSAGE_END;
    fact.terminal_state = state;
    return fact;
}

static inline void end_message(
    MessageFixture* f, AetherMessageId id, AetherMessageState state)
{
    OK(aether_message_apply(f->messages, end_fact(f, id, state)));
}

#endif
