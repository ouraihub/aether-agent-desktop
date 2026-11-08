#include "aether/core/workspace.h"

#include <assert.h>
#include <string.h>

static AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}

int main(void)
{
    AetherWorkspaceRegistry* registry = NULL;
    AetherWorkspaceId workspace;
    AetherWorkspaceSnapshot workspace_snapshot;
    AetherAgentConfigId agent;
    AetherAgentConfigSnapshot agent_snapshot;
    AetherProviderId provider = {{42U}};

    assert(aether_workspace_registry_create(2U, 2U, &registry) ==
           AETHER_STATUS_OK);
    assert(aether_workspace_create(
               registry, (AetherWorkspaceInput){text("Project"), text("C:/project")},
               &workspace) == AETHER_STATUS_OK);
    assert(aether_workspace_query(registry, workspace, &workspace_snapshot) ==
           AETHER_STATUS_OK);
    assert(workspace_snapshot.revision == 1U);
    assert(workspace_snapshot.state == AETHER_WORKSPACE_OPEN);
    assert(strcmp(workspace_snapshot.display_name, "Project") == 0);
    assert(aether_workspace_update_name(
               registry, workspace, 99U, text("Wrong")) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_workspace_update_name(
               registry, workspace, 1U, text("Renamed")) == AETHER_STATUS_OK);
    assert(aether_workspace_begin_close(
               registry, workspace, 1U) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_workspace_begin_close(
               registry, workspace, 2U) == AETHER_STATUS_OK);
    assert(aether_workspace_finish_close(registry, workspace) ==
           AETHER_STATUS_OK);
    assert(aether_workspace_update_name(
               registry, workspace, 3U, text("Closed")) ==
           AETHER_STATUS_INVALID_ARGUMENT);

    assert(aether_agent_config_create(
               registry,
               (AetherAgentConfigInput){
                   text("Default"), provider, text("model-a")},
               &agent) == AETHER_STATUS_OK);
    assert(aether_agent_config_query(registry, agent, &agent_snapshot) ==
           AETHER_STATUS_OK);
    assert(agent_snapshot.revision == 1U);
    assert(strcmp(agent_snapshot.model, "model-a") == 0);
    assert(aether_agent_config_update_model(
               registry, agent, 1U, text("model-b")) == AETHER_STATUS_OK);
    assert(aether_agent_config_query(registry, agent, &agent_snapshot) ==
           AETHER_STATUS_OK);
    assert(agent_snapshot.revision == 2U);
    assert(strcmp(agent_snapshot.model, "model-b") == 0);
    assert(aether_agent_config_remove(registry, agent) == AETHER_STATUS_OK);
    assert(aether_agent_config_query(registry, agent, &agent_snapshot) ==
           AETHER_STATUS_NOT_FOUND);
    assert(aether_workspace_registry_destroy(&registry) == AETHER_STATUS_OK);
    return 0;
}
