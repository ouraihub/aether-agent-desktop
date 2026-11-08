#include "permission_fixture.h"

static AetherPermissionId remember(PermissionFixture* f, AetherApprovalScope scope)
{
    AetherPermissionId id = begin(f);
    AetherApprovalResult answer;
    OK(aether_permission_resolve(f->permissions, id, f->input, 11U,
        AETHER_PERMISSION_ALLOW, scope, &answer));
    CHECK(answer.approved && answer.memory_status == AETHER_STATUS_OK);
    OK(aether_permission_consume(f->permissions, id, f->input, 12U));
    OK(aether_permission_release(f->permissions, id));
    f->input.request_id = aether_id_next();
    return id;
}

static void tool_memory_exact_level(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionId memory_id;
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED);
    memory_id = remember(&f, AETHER_APPROVAL_SESSION_TOOL);
    CHECK(evaluate(&f).source == AETHER_PERMISSION_SOURCE_MEMORY);
    {
        AetherPermissionTarget target = {text("C:/new-target")};
        f.input.targets = &target;
        f.input.target_count = 1U;
        CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
        f.input.targets = NULL;
        f.input.target_count = 0U;
    }
    OK(aether_permission_consume(f.permissions, (AetherPermissionId){0}, f.input, 20U));
    f.input.level = AETHER_PERMISSION_LEVEL_WRITE;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    CHECK(aether_permission_consume(f.permissions, (AetherPermissionId){0}, f.input, 20U) ==
          AETHER_STATUS_PERMISSION_DENIED);
    f.input.level = AETHER_PERMISSION_LEVEL_EXEC;
    OK(aether_permission_revoke(f.permissions, memory_id));
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    f.input.level = AETHER_PERMISSION_LEVEL_EXEC;
    f.input.scope.executor_id = aether_id_next();
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    dispose(&f);
}

static void exact_targets_and_union(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionTarget targets[2] = {{text("C:/w/a")}, {text("C:/w/b")}};
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED);
    f.input.targets = targets;
    f.input.target_count = 1U;
    f.input.level = AETHER_PERMISSION_LEVEL_WRITE;
    (void)remember(&f, AETHER_APPROVAL_SESSION_TARGETS);
    f.input.level = AETHER_PERMISSION_LEVEL_READ;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    f.input.level = AETHER_PERMISSION_LEVEL_EXEC;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    f.input.level = AETHER_PERMISSION_LEVEL_WRITE;
    targets[0].identity = text("C:/w/a/child");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    targets[0].identity = text("C:/w/ab");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    targets[0].identity = text("C:/w/b");
    (void)remember(&f, AETHER_APPROVAL_SESSION_TARGETS);
    targets[0].identity = text("C:/w/a");
    f.input.target_count = 2U;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    CHECK(evaluate(&f).source == AETHER_PERMISSION_SOURCE_MEMORY);
    targets[1].identity = text("C:/w/uncovered");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    CHECK(aether_permission_consume(f.permissions, (AetherPermissionId){0}, f.input, 20U) ==
          AETHER_STATUS_PERMISSION_DENIED);
    dispose(&f);
}

static void root_directory_boundary(void)
{
    const char* original[] = {"/file", "C:/file"};
    const char* roots[] = {"/", "C:/"};
    const char* siblings[] = {"/sibling", "C:/sibling"};
    const char* nested[] = {"/sub/file", "C:/sub/file"};
    const char* other[] = {"C:/file", "D:/file"};
    for (size_t index = 0U; index < 2U; ++index) {
        PermissionFixture f = fixture(aether_permission_default_limits());
        AetherPermissionTarget target = {text(original[index])};
        mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED);
        f.input.targets = &target;
        f.input.target_count = 1U;
        (void)remember(&f, AETHER_APPROVAL_SESSION_DIRECTORIES);
        target.identity = text(roots[index]);
        CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
        target.identity = text(siblings[index]);
        CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
        target.identity = text(nested[index]);
        CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
        target.identity = text(other[index]);
        CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
        dispose(&f);
    }
}

static void copied_unterminated_views(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    char tool[] = {'s', 'h', 'e', 'l', 'l'};
    char subtool[] = {'r', 'u', 'n'};
    char arguments[] = {'a', 'r', 'g', 's'};
    char identity[] = {'C', ':', '/', 'w', '/', 'f', 'i', 'l', 'e'};
    AetherPermissionTarget target = {{identity, sizeof(identity)}};
    AetherPermissionId id;
    AetherApprovalResult answer;
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED);
    f.input.scope.tool = (AetherStringView){tool, sizeof(tool)};
    f.input.scope.subtool = (AetherStringView){subtool, sizeof(subtool)};
    f.input.arguments = (AetherStringView){arguments, sizeof(arguments)};
    f.input.targets = &target;
    f.input.target_count = 1U;
    id = begin(&f);
    memset(tool, 'x', sizeof(tool));
    memset(subtool, 'x', sizeof(subtool));
    memset(arguments, 'x', sizeof(arguments));
    memset(identity, 'x', sizeof(identity));
    f.input.scope.tool = text("shell");
    f.input.scope.subtool = text("run");
    f.input.arguments = text("args");
    target.identity = text("C:/w/file");
    OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TARGETS, &answer));
    CHECK(answer.approved && answer.memory_status == AETHER_STATUS_OK);
    OK(aether_permission_consume(f.permissions, id, f.input, 12U));
    OK(aether_permission_release(f.permissions, id));
    f.input.request_id = aether_id_next();
    f.input.arguments = text("different arguments");
    CHECK(evaluate(&f).source == AETHER_PERMISSION_SOURCE_MEMORY);
    dispose(&f);
}

static void exact_actual_at_both_gates(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionId id;
    AetherPermissionInput changed;
    AetherApprovalResult answer;
    AetherPermissionSnapshot snapshot;
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    id = begin(&f);
    changed = f.input;
    changed.request_id = aether_id_next();
    CHECK(aether_permission_resolve(f.permissions, id, changed, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(!answer.approved);
    changed = f.input;
    ++changed.runtime_generation;
    CHECK(aether_permission_resolve(f.permissions, id, changed, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(!answer.approved);
    OK(aether_permission_query(f.permissions, id, 11U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_WAITING);
    OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer));
    changed = f.input;
    changed.request_id = aether_id_next();
    CHECK(aether_permission_consume(f.permissions, id, changed, 12U) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    changed = f.input;
    changed.arguments = text("command=other");
    CHECK(aether_permission_consume(f.permissions, id, changed, 12U) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    changed = f.input;
    ++changed.runtime_generation;
    CHECK(aether_permission_consume(f.permissions, id, changed, 12U) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    OK(aether_permission_query(f.permissions, id, 12U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_APPROVED);
    OK(aether_permission_consume(f.permissions, id, f.input, 12U));
    dispose(&f);
}

static void target_cannot_escalate_to_tool(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionTarget target = {text("C:/w/file")};
    AetherPermissionId id;
    AetherApprovalResult answer;
    AetherPermissionSnapshot snapshot;
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED);
    f.input.targets = &target;
    f.input.target_count = 1U;
    id = begin(&f);
    CHECK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TOOL, &answer) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(!answer.approved);
    OK(aether_permission_query(f.permissions, id, 11U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_WAITING);
    OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TARGETS, &answer));
    CHECK(answer.approved && answer.memory_status == AETHER_STATUS_OK);
    target.identity = text("C:/w/other");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    f.input.targets = NULL;
    f.input.target_count = 0U;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    dispose(&f);
}

static void closed_live_scopes(void)
{
    for (size_t close_workspace = 0U; close_workspace < 2U; ++close_workspace) {
        PermissionFixture f = fixture(aether_permission_default_limits());
        AetherPermissionId id;
        AetherApprovalResult answer;
        AetherPermissionSnapshot snapshot;
        mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
        id = begin(&f);
        if (close_workspace != 0U) {
            AetherWorkspaceSnapshot workspace;
            OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
                AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer));
            OK(aether_workspace_query(f.workspaces, f.input.scope.workspace_id, &workspace));
            OK(aether_workspace_begin_close(f.workspaces, f.input.scope.workspace_id,
                                            workspace.revision));
            CHECK(aether_permission_consume(f.permissions, id, f.input, 12U) ==
                  AETHER_STATUS_NOT_INITIALIZED);
        } else {
            OK(aether_session_begin_close(f.sessions, f.input.scope.session_id));
            OK(aether_session_finish_close(f.sessions, f.input.scope.session_id));
            CHECK(aether_permission_resolve(f.permissions, id, f.input, 12U,
                AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TOOL, &answer) ==
                AETHER_STATUS_NOT_INITIALIZED);
            CHECK(!answer.approved);
        }
        OK(aether_permission_query(f.permissions, id, 12U, &snapshot));
        CHECK(snapshot.state == AETHER_PERMISSION_INVALIDATED);
        CHECK(aether_permission_consume(f.permissions, (AetherPermissionId){0}, f.input, 12U) ==
              AETHER_STATUS_NOT_INITIALIZED);
        dispose(&f);
    }
}

static void reconnect_and_stale_answer(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionId id;
    AetherApprovalResult answer;
    AetherPermissionSnapshot snapshot;
    AetherPermissionResult result;
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    id = begin(&f);
    OK(aether_runtime_reconnect(f.runtimes, f.input.scope.runtime_id, 1U));
    OK(aether_runtime_update(f.runtimes, f.input.scope.runtime_id, 2U,
        AETHER_RUNTIME_READY, (AetherRuntimeCapabilities){0}));
    CHECK(aether_permission_resolve(f.permissions, id, f.input, 12U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TOOL, &answer) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(!answer.approved);
    OK(aether_permission_query(f.permissions, id, 12U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_INVALIDATED);
    CHECK(aether_permission_consume(f.permissions, id, f.input, 12U) ==
          AETHER_STATUS_ALREADY_EXISTS);
    CHECK(aether_permission_evaluate(f.permissions, f.input, &result) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(result.decision == AETHER_PERMISSION_DENY);
    dispose(&f);
    f = fixture(aether_permission_default_limits());
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    id = begin(&f);
    mode(&f, AETHER_PERMISSION_MODE_YOLO);
    CHECK(aether_permission_resolve(f.permissions, id, f.input, 12U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TOOL, &answer) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(!answer.approved);
    CHECK(aether_permission_consume(f.permissions, id, f.input, 12U) ==
          AETHER_STATUS_ALREADY_EXISTS);
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    dispose(&f);
}

int main(void)
{
    tool_memory_exact_level();
    exact_targets_and_union();
    root_directory_boundary();
    copied_unterminated_views();
    exact_actual_at_both_gates();
    closed_live_scopes();
    reconnect_and_stale_answer();
    target_cannot_escalate_to_tool();
    return 0;
}
