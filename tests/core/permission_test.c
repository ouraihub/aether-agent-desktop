#include "aether/core/permission.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct Fixture {
    AetherWorkspaceRegistry* workspaces;
    AetherPermissionRegistry* permissions;
    AetherWorkspaceId workspace;
    AetherPermissionInput input;
} Fixture;

static AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}

static Fixture fixture(size_t capacity)
{
    Fixture f = {0};
    assert(aether_workspace_registry_create(1U, 1U, &f.workspaces) ==
           AETHER_STATUS_OK);
    assert(aether_workspace_create(f.workspaces,
               (AetherWorkspaceInput){text("W"), text("C:/w")}, &f.workspace) ==
           AETHER_STATUS_OK);
    assert(aether_permission_registry_create(capacity, f.workspaces,
                                             &f.permissions) ==
           AETHER_STATUS_OK);
    f.input = (AetherPermissionInput){f.workspace, text("shell"),
        text("C:/w/build"), AETHER_PERMISSION_LEVEL_EXEC};
    return f;
}

static void dispose(Fixture* f)
{
    assert(aether_permission_registry_destroy(&f->permissions) ==
           AETHER_STATUS_OK);
    assert(aether_workspace_registry_destroy(&f->workspaces) ==
           AETHER_STATUS_OK);
}

static void set_cached(Fixture* f)
{
    assert(aether_permission_set_mode(f->permissions, f->workspace,
               AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED) == AETHER_STATUS_OK);
}

static void denied_output(AetherPermissionResult result)
{
    assert(result.decision == AETHER_PERMISSION_DENY);
    assert(result.permission_id.id.value == 0U);
    assert(result.policy_revision == 0U);
    assert(!result.from_memory);
}

static AetherPermissionId ask(Fixture* f)
{
    AetherPermissionResult result;
    assert(aether_permission_evaluate(f->permissions, f->input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ASK);
    assert(result.permission_id.id.value != 0U);
    assert(!result.from_memory);
    return result.permission_id;
}

static void test_view_identity_and_ownership(void)
{
    Fixture f = fixture(3U);
    AetherPermissionResult result;
    AetherPermissionId id;
    char tool[] = {'s', 'h', 'e', 'l', 'l'};
    char target[] = {'C', ':', '/', 'w', '/', 'b', 'u', 'i', 'l', 'd'};
    set_cached(&f);
    f.input.tool_id = (AetherStringView){tool, sizeof(tool)};
    f.input.target = (AetherStringView){target, sizeof(target)};
    id = ask(&f);
    memset(tool, 'x', sizeof(tool));
    memset(target, 'x', sizeof(target));
    assert(aether_permission_resolve(f.permissions, id,
               AETHER_PERMISSION_ALLOW, true) == AETHER_STATUS_OK);
    f.input.tool_id = text("shell");
    f.input.target = text("C:/w/build");
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ALLOW && result.from_memory);
    assert(result.permission_id.id.value == id.id.value);
    /* Query views themselves need no trailing byte accessible to strcmp. */
    memcpy(tool, "shell", sizeof(tool));
    memcpy(target, "C:/w/build", sizeof(target));
    f.input.tool_id = (AetherStringView){tool, sizeof(tool)};
    f.input.target = (AetherStringView){target, sizeof(target)};
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ALLOW && result.from_memory);
    f.input.tool_id = text("shell");
    f.input.target = text("C:/w/build");

    /* Same backing bytes and NUL, different views: never reuse approval. */
    --f.input.target.length;
    assert(aether_permission_revoke(f.permissions, ask(&f)) == AETHER_STATUS_OK);
    ++f.input.target.length;
    --f.input.tool_id.length;
    assert(aether_permission_revoke(f.permissions, ask(&f)) == AETHER_STATUS_OK);
    ++f.input.tool_id.length;
    assert(aether_permission_revoke(f.permissions, id) == AETHER_STATUS_OK);
    id = ask(&f);
    assert(aether_permission_resolve(f.permissions, id,
               AETHER_PERMISSION_DENY, true) == AETHER_STATUS_OK);
    assert(aether_permission_revoke(f.permissions, ask(&f)) == AETHER_STATUS_OK);
    dispose(&f);
}

static void test_invalid_inputs_preserve_pending(void)
{
    Fixture f = fixture(2U);
    AetherPermissionResult result;
    AetherPermissionId id;
    AetherPermissionInput invalid;
    AetherPermissionRegistry* output = f.permissions;
    assert(aether_permission_registry_create(0U, f.workspaces, &output) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(output == NULL);
    output = f.permissions;
    assert(aether_permission_registry_create(SIZE_MAX, f.workspaces, &output) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(output == NULL);
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_NOT_INITIALIZED);
    denied_output(result);
    set_cached(&f);
    id = ask(&f);
    assert(aether_permission_set_mode(f.permissions, f.workspace,
               (AetherPermissionMode)-1) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_permission_set_mode(f.permissions, f.workspace,
               (AetherPermissionMode)99) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid = f.input;
    invalid.level = (AetherPermissionLevel)-1;
    assert(aether_permission_evaluate(f.permissions, invalid, &result) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    denied_output(result);
    invalid.level = (AetherPermissionLevel)99;
    assert(aether_permission_evaluate(f.permissions, invalid, &result) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    denied_output(result);
    invalid = f.input;
    invalid.target = (AetherStringView){"a\0b", 3U};
    assert(aether_permission_evaluate(f.permissions, invalid, &result) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    denied_output(result);
    invalid.target = (AetherStringView){NULL, 1U};
    assert(aether_permission_evaluate(f.permissions, invalid, &result) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    denied_output(result);
    assert(aether_permission_resolve(f.permissions, id,
               (AetherPermissionDecision)-1, true) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_permission_resolve(f.permissions, id,
               (AetherPermissionDecision)99, true) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_permission_resolve(f.permissions, id,
               AETHER_PERMISSION_ASK, true) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_permission_resolve(f.permissions, id,
               AETHER_PERMISSION_ALLOW, true) == AETHER_STATUS_OK);
    assert(aether_permission_resolve(f.permissions, id,
               AETHER_PERMISSION_ALLOW, true) == AETHER_STATUS_ALREADY_EXISTS);
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ALLOW && result.from_memory);
    dispose(&f);
}

static void test_policy_and_workspace_invalidation(void)
{
    Fixture f = fixture(3U);
    AetherPermissionResult result;
    AetherPermissionId stale;
    AetherPermissionId pending;
    AetherWorkspaceId unknown = {aether_id_next()};
    assert(aether_permission_set_mode(f.permissions, unknown,
               AETHER_PERMISSION_MODE_YOLO) == AETHER_STATUS_NOT_FOUND);
    set_cached(&f);
    stale = ask(&f);
    set_cached(&f);
    assert(aether_permission_resolve(f.permissions, stale,
               AETHER_PERMISSION_ALLOW, true) == AETHER_STATUS_INVALID_ARGUMENT);
    pending = ask(&f);
    assert(aether_permission_resolve(f.permissions, pending,
               AETHER_PERMISSION_ALLOW, true) == AETHER_STATUS_OK);
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ALLOW && result.from_memory);
    f.input.target = text("C:/w/other");
    pending = ask(&f);
    assert(aether_workspace_begin_close(f.workspaces, f.workspace, 1U) ==
           AETHER_STATUS_OK);
    assert(aether_permission_resolve(f.permissions, pending,
               AETHER_PERMISSION_ALLOW, true) == AETHER_STATUS_NOT_INITIALIZED);
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_NOT_INITIALIZED);
    denied_output(result);
    f.input.target = text("C:/w/build");
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_NOT_INITIALIZED);
    denied_output(result);
    assert(aether_permission_set_mode(f.permissions, f.workspace,
               AETHER_PERMISSION_MODE_YOLO) == AETHER_STATUS_NOT_INITIALIZED);
    assert(aether_workspace_finish_close(f.workspaces, f.workspace) ==
           AETHER_STATUS_OK);
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_NOT_INITIALIZED);
    denied_output(result);
    assert(aether_permission_revoke(f.permissions, pending) == AETHER_STATUS_OK);
    dispose(&f);
}

static void test_capacity_and_reuse(void)
{
    Fixture f = fixture(1U);
    AetherPermissionId id;
    AetherPermissionResult result;
    set_cached(&f);
    id = ask(&f);
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_OUT_OF_MEMORY);
    denied_output(result);
    assert(aether_permission_resolve(f.permissions, id,
               AETHER_PERMISSION_ALLOW, true) == AETHER_STATUS_OK);
    assert(aether_permission_evaluate(f.permissions, f.input, &result) ==
           AETHER_STATUS_OK);
    assert(result.decision == AETHER_PERMISSION_ALLOW && result.from_memory);
    assert(aether_permission_revoke(f.permissions, id) == AETHER_STATUS_OK);
    assert(aether_permission_revoke(f.permissions, id) == AETHER_STATUS_NOT_FOUND);
    assert(ask(&f).id.value != id.id.value);
    dispose(&f);
}

int main(void)
{
    test_view_identity_and_ownership();
    test_invalid_inputs_preserve_pending();
    test_policy_and_workspace_invalidation();
    test_capacity_and_reuse();
    return 0;
}
