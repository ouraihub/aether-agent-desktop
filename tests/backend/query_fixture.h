#ifndef AETHER_TEST_QUERY_FIXTURE_H
#define AETHER_TEST_QUERY_FIXTURE_H

#include "aether/backend/query_service.h"
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

typedef struct QueryFixture {
    AetherWorkspaceRegistry* workspaces;
    AetherRuntimeRegistry* runtimes;
    AetherSessionRegistry* sessions;
    AetherMessageRegistry* messages;
    AetherBackendQueries* queries;
    AetherWorkspaceId workspace[2];
    AetherRuntimeId runtime[2];
    AetherSessionId session[2];
    AetherMessageId message[2];
    AetherProviderId provider;
    AetherAgentConfigId agent;
    AetherId block[2];
    AetherId structured;
    AetherRequestContext all;
    AetherRequestContext scoped;
} QueryFixture;

static inline AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}

static inline AetherMessageFact content_fact(
    QueryFixture* f, size_t index, uint32_t ordinal, AetherId block,
    AetherBlockKind kind, const char* data, const char* label)
{
    AetherMessageFact fact = {0};
    fact.session_id = f->session[index];
    fact.message_id = f->message[index];
    fact.runtime_generation = 1U;
    fact.operation = AETHER_MESSAGE_CREATE_BLOCK;
    fact.block_id = block;
    fact.ordinal = ordinal;
    fact.cursor = 1U;
    fact.payload.kind = kind;
    fact.payload.data = text(data);
    fact.payload.label = text(label);
    return fact;
}

static inline QueryFixture fixture(void)
{
    QueryFixture f = {0};
    const char* names[] = {"Primary", "Foreign"};
    char source[] = "hello";
    char label[] = "csv";
    AetherMessageFact fact;
    size_t i;
    OK(aether_workspace_registry_create(2U, 1U, &f.workspaces));
    OK(aether_runtime_registry_create(1U, 2U, &f.runtimes));
    OK(aether_runtime_provider_register(f.runtimes, text("provider"), text("1"),
        &f.provider));
    OK(aether_agent_config_create(f.workspaces,
        (AetherAgentConfigInput){text("Agent"), f.provider, text("secret-model")}, &f.agent));
    OK(aether_session_registry_create(2U, f.workspaces, f.runtimes, &f.sessions));
    OK(aether_message_registry_create(aether_message_default_limits(), f.sessions,
        f.runtimes, &f.messages));
    for (i = 0U; i < 2U; ++i) {
        OK(aether_workspace_create(f.workspaces,
            (AetherWorkspaceInput){text(names[i]), text("C:/private/secret-root")},
            &f.workspace[i]));
        OK(aether_runtime_register(f.runtimes, f.provider, &f.runtime[i]));
        OK(aether_runtime_update(f.runtimes, f.runtime[i], 1U, AETHER_RUNTIME_READY,
            (AetherRuntimeCapabilities){AETHER_RUNTIME_CAP_ALL, AETHER_RUNTIME_CAP_SESSION}));
        OK(aether_session_create(f.sessions, f.workspace[i], f.agent, f.runtime[i], 1U,
            &f.session[i]));
        OK(aether_session_initialize(f.sessions, f.session[i]));
        OK(aether_message_create(f.messages, f.session[i], 1U,
            AETHER_MESSAGE_ROLE_ASSISTANT, &f.message[i]));
        f.block[i] = aether_id_next();
        fact = content_fact(&f, i, 0U, f.block[i], AETHER_BLOCK_TEXT, source, "");
        OK(aether_message_apply(f.messages, fact));
    }
    f.structured = aether_id_next();
    fact = content_fact(&f, 0U, 1U, f.structured, AETHER_BLOCK_STRUCTURED, "row", label);
    OK(aether_message_apply(f.messages, fact));
    memset(source, 'x', strlen(source));
    memset(label, 'x', strlen(label));
    OK(aether_backend_queries_create(f.workspaces, f.runtimes, f.sessions, f.messages,
        &f.queries));
    f.all.api_version = AETHER_BACKEND_QUERY_VERSION;
    f.all.caller_id = aether_id_next();
    f.all.read_all_workspaces = true;
    f.all.trace.valid = true;
    f.all.trace.trace_id.bytes[0] = 0x41U;
    f.all.trace.span_id.bytes[0] = 0x42U;
    f.scoped = f.all;
    f.scoped.read_all_workspaces = false;
    f.scoped.workspace_scope = f.workspace[0].id;
    return f;
}

static inline void dispose(QueryFixture* f)
{
    aether_backend_queries_destroy(f->queries);
    f->queries = NULL;
    OK(aether_message_registry_destroy(&f->messages));
    OK(aether_session_registry_destroy(&f->sessions));
    OK(aether_runtime_registry_destroy(&f->runtimes));
    OK(aether_workspace_registry_destroy(&f->workspaces));
}

static inline void check_text(AetherStringView actual, const char* expected)
{
    CHECK(actual.length == strlen(expected));
    CHECK(actual.data != NULL);
    CHECK(memcmp(actual.data, expected, actual.length) == 0);
    CHECK(actual.data[actual.length] == '\0');
}

#endif
