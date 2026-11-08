#include "aether/core/session.h"

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
    AetherWorkspaceId workspace;
    AetherAgentConfigId agent;
    AetherProviderId provider;
    AetherRuntimeId runtime;
    AetherSessionId session;
    AetherSessionSnapshot snapshot;

    assert(aether_workspace_registry_create(2U, 2U, &workspaces) ==
           AETHER_STATUS_OK);
    assert(aether_runtime_registry_create(2U, 2U, &runtimes) ==
           AETHER_STATUS_OK);
    assert(aether_workspace_create(
               workspaces,
               (AetherWorkspaceInput){text("Workspace"), text("C:/workspace")},
               &workspace) == AETHER_STATUS_OK);
    assert(aether_runtime_provider_register(
               runtimes, text("provider"), text("1"), &provider) ==
           AETHER_STATUS_OK);
    assert(aether_runtime_register(runtimes, provider, &runtime) ==
           AETHER_STATUS_OK);
    assert(aether_runtime_update(
               runtimes, runtime, 1U, AETHER_RUNTIME_READY,
               (AetherRuntimeCapabilities){
                   AETHER_RUNTIME_CAP_SESSION, AETHER_RUNTIME_CAP_SESSION}) ==
           AETHER_STATUS_OK);
    assert(aether_agent_config_create(
               workspaces,
               (AetherAgentConfigInput){text("agent"), provider, text("model")},
               &agent) == AETHER_STATUS_OK);
    assert(aether_session_registry_create(
               2U, workspaces, runtimes, &sessions) == AETHER_STATUS_OK);
    assert(aether_session_create(
               sessions, workspace, agent, runtime, 1U, &session) ==
           AETHER_STATUS_OK);
    assert(aether_session_query(sessions, session, &snapshot) ==
           AETHER_STATUS_OK);
    assert(snapshot.state == AETHER_SESSION_CREATED);
    assert(aether_session_start_turn(sessions, session) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_session_initialize(sessions, session) == AETHER_STATUS_OK);
    assert(snapshot.state == AETHER_SESSION_CREATED);
    assert(aether_session_start_turn(sessions, session) == AETHER_STATUS_OK);
    assert(aether_session_start_turn(sessions, session) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_session_mark_interrupted(sessions, session) == AETHER_STATUS_OK);
    assert(aether_session_start_turn(sessions, session) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_session_begin_close(sessions, session) == AETHER_STATUS_OK);
    assert(aether_session_finish_close(sessions, session) == AETHER_STATUS_OK);
    assert(aether_session_query(sessions, session, &snapshot) ==
           AETHER_STATUS_OK);
    assert(snapshot.state == AETHER_SESSION_CLOSED);
    assert(aether_session_start_turn(sessions, session) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    aether_session_registry_destroy(&sessions);
    aether_runtime_registry_destroy(&runtimes);
    aether_workspace_registry_destroy(&workspaces);
    return 0;
}
