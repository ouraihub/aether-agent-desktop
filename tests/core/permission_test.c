#include "aether/core/permission.h"

#include <assert.h>
#include <string.h>

static AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}

int main(void)
{
    AetherWorkspaceRegistry* workspaces = NULL;
    AetherPermissionRegistry* permissions = NULL;
    AetherWorkspaceId workspace;
    AetherPermissionId pending_permission;
    AetherPermissionResult result;
    AetherPermissionInput input;

    assert(aether_workspace_registry_create(1U, 1U, &workspaces) ==
           AETHER_STATUS_OK);
    assert(aether_workspace_create(
               workspaces, (AetherWorkspaceInput){text("W"), text("C:/w")},
               &workspace) == AETHER_STATUS_OK);
    assert(aether_permission_registry_create(4U, workspaces, &permissions) ==
           AETHER_STATUS_OK);
    input = (AetherPermissionInput){
        workspace, text("shell"), text("C:/w/build"),
        AETHER_PERMISSION_LEVEL_EXEC};
    assert(aether_permission_evaluate(permissions, input, &result) ==
           AETHER_STATUS_NOT_INITIALIZED);
    assert(aether_permission_set_mode(
               permissions, workspace, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED) ==
           AETHER_STATUS_OK);
    assert(aether_permission_evaluate(permissions, input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ASK);
    pending_permission = result.permission_id;
    assert(pending_permission.id.value != 0U);
    assert(aether_permission_resolve(
               permissions, pending_permission, AETHER_PERMISSION_ALLOW, true) ==
           AETHER_STATUS_OK);
    assert(aether_permission_resolve(
               permissions, pending_permission, AETHER_PERMISSION_ALLOW, true) ==
           AETHER_STATUS_ALREADY_EXISTS);
    assert(aether_permission_evaluate(permissions, input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ALLOW);
    assert(result.from_memory);
    assert(aether_permission_set_mode(
               permissions, workspace, AETHER_PERMISSION_MODE_YOLO) ==
           AETHER_STATUS_OK);
    input.level = AETHER_PERMISSION_LEVEL_WRITE;
    assert(aether_permission_evaluate(permissions, input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ALLOW);
    assert(aether_permission_revoke(
               permissions, pending_permission) == AETHER_STATUS_OK);
    aether_permission_registry_destroy(&permissions);
    aether_workspace_registry_destroy(&workspaces);
    return 0;

}
