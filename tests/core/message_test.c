#include "aether/core/message.h"

#include <assert.h>
#include <string.h>

static AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
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
    AetherMessageSnapshot snapshot;

    assert(aether_workspace_registry_create(1U, 1U, &workspaces) ==
           AETHER_STATUS_OK);
    assert(aether_runtime_registry_create(1U, 1U, &runtimes) ==
           AETHER_STATUS_OK);
    assert(aether_workspace_create(
               workspaces, (AetherWorkspaceInput){text("W"), text("C:/w")},
               &workspace) == AETHER_STATUS_OK);
    assert(aether_runtime_provider_register(
               runtimes, text("p"), text("1"), &provider) == AETHER_STATUS_OK);
    assert(aether_runtime_register(runtimes, provider, &runtime) ==
           AETHER_STATUS_OK);
    assert(aether_runtime_update(
               runtimes, runtime, 1U, AETHER_RUNTIME_READY,
               (AetherRuntimeCapabilities){
                   AETHER_RUNTIME_CAP_SESSION, AETHER_RUNTIME_CAP_SESSION}) ==
           AETHER_STATUS_OK);
    assert(aether_agent_config_create(
               workspaces, (AetherAgentConfigInput){text("A"), provider, text("m")},
               &agent) == AETHER_STATUS_OK);
    assert(aether_session_registry_create(1U, workspaces, runtimes, &sessions) ==
           AETHER_STATUS_OK);
    assert(aether_session_create(sessions, workspace, agent, runtime, 1U, &session)
           == AETHER_STATUS_OK);
    assert(aether_session_initialize(sessions, session) == AETHER_STATUS_OK);
    assert(aether_message_registry_create(2U, sessions, &messages) ==
           AETHER_STATUS_OK);
    assert(aether_message_create(
               messages, session, AETHER_MESSAGE_ROLE_ASSISTANT, &message) ==
           AETHER_STATUS_OK);
    assert(aether_message_append_text(messages, message, 2U, text("bad")) ==
           AETHER_STATUS_PROTOCOL_ERROR);
    assert(aether_message_append_text(messages, message, 1U, text("hello")) ==
           AETHER_STATUS_OK);
    assert(aether_message_append_text(messages, message, 2U, text(" world")) ==
           AETHER_STATUS_OK);
    assert(aether_message_query(messages, message, &snapshot) == AETHER_STATUS_OK);
    assert(strcmp(snapshot.text, "hello world") == 0);
    assert(snapshot.cursor == 2U && snapshot.revision == 3U);
    assert(aether_message_replace_text(
               messages, message, snapshot.revision, text("replaced")) ==
           AETHER_STATUS_OK);
    assert(aether_message_finish(
               messages, message, AETHER_MESSAGE_COMPLETED) == AETHER_STATUS_OK);
    assert(aether_message_append_text(messages, message, 4U, text("late")) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_message_finish(
               messages, message, AETHER_MESSAGE_FAILED) ==
           AETHER_STATUS_ALREADY_EXISTS);
    aether_message_registry_destroy(&messages);
    aether_session_registry_destroy(&sessions);
    aether_runtime_registry_destroy(&runtimes);
    aether_workspace_registry_destroy(&workspaces);
    return 0;
}
