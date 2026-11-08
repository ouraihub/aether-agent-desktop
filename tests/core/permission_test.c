#include "permission_fixture.h"

static void modes_and_context(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionResult result = evaluate(&f);
    CHECK(result.decision == AETHER_PERMISSION_ALLOW);
    CHECK(result.permission_id.id.value == 0U);
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    f.input.level = AETHER_PERMISSION_LEVEL_READ;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    f.input.level = AETHER_PERMISSION_LEVEL_WRITE;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    mode(&f, AETHER_PERMISSION_MODE_WRITE);
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    f.input.level = AETHER_PERMISSION_LEVEL_EXEC;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    OK(aether_permission_set_mode(f.permissions, f.input.scope.workspace_id,
        f.input.scope.session_id, AETHER_PERMISSION_MODE_YOLO));
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    f.input.scope.session_id = f.second_session;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    f.input.scope.subject_id.value = 0U;
    CHECK(aether_permission_evaluate(f.permissions, f.input, &result) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(result.decision == AETHER_PERMISSION_DENY && result.policy_revision == 0U);
    dispose(&f);
}

static void rule_precedence_and_inheritance(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionRule allow = rule(AETHER_PERMISSION_ALLOW);
    AetherPermissionRule ask = rule(AETHER_PERMISSION_ASK);
    AetherPermissionRule deny = rule(AETHER_PERMISSION_DENY);
    AetherPermissionRule child = rule(AETHER_PERMISSION_INHERIT);
    AetherPermissionRule parent = rule(AETHER_PERMISSION_ALLOW);
    AetherPermissionResult result;
    deny.scope.workspace_id = f.input.scope.workspace_id;
    allow.scope.session_id = f.input.scope.session_id;
    OK(aether_permission_put_rule(f.permissions, allow));
    OK(aether_permission_put_rule(f.permissions, ask));
    result = evaluate(&f);
    CHECK(result.decision == AETHER_PERMISSION_ASK && !result.can_remember);
    OK(aether_permission_put_rule(f.permissions, deny));
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_DENY);
    OK(aether_permission_remove_rule(f.permissions, deny.rule_id));
    OK(aether_permission_remove_rule(f.permissions, ask.rule_id));
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    f.input.hard_deny = true;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_DENY);
    f.input.hard_deny = false;
    f.input.noncacheable_ask = true;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    f.input.noncacheable_ask = false;
    OK(aether_permission_remove_rule(f.permissions, allow.rule_id));
    child.parent_rule_id = parent.rule_id;
    CHECK(aether_permission_put_rule(f.permissions, child) == AETHER_STATUS_NOT_FOUND);
    parent.scope.tool = text("another-tool");
    OK(aether_permission_put_rule(f.permissions, parent));
    OK(aether_permission_put_rule(f.permissions, child));
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    CHECK(aether_permission_remove_rule(f.permissions, parent.rule_id) ==
          AETHER_STATUS_ALREADY_EXISTS);
    parent.decision = AETHER_PERMISSION_INHERIT;
    parent.parent_rule_id = child.rule_id;
    CHECK(aether_permission_put_rule(f.permissions, parent) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    dispose(&f);
}

static void explicit_views_and_invalid_values(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionRule deny = rule(AETHER_PERMISSION_DENY);
    AetherPermissionResult result;
    char tool[] = {'s', 'h', 'e', 'l', 'l'};
    AetherPermissionTarget target = {{"C:/w/build", 10U}};
    deny.scope.tool = (AetherStringView){tool, sizeof(tool)};
    OK(aether_permission_put_rule(f.permissions, deny));
    memset(tool, 'x', sizeof(tool));
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_DENY);
    f.input.scope.tool.length = 4U;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    f.input.scope.tool = text("shell");
    f.input.level = (AetherPermissionLevel)-1;
    CHECK(aether_permission_evaluate(f.permissions, f.input, &result) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(result.decision == AETHER_PERMISSION_DENY);
    f.input.level = AETHER_PERMISSION_LEVEL_READ;
    f.input.capability_supported = false;
    CHECK(aether_permission_evaluate(f.permissions, f.input, &result) ==
          AETHER_STATUS_NOT_SUPPORTED);
    f.input.capability_supported = true;
    f.input.targets = &target;
    f.input.target_count = 1U;
    target.identity = text("C:/w/../outside");
    CHECK(aether_permission_evaluate(f.permissions, f.input, &result) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    target.identity = text("C:\\w\\file");
    CHECK(aether_permission_evaluate(f.permissions, f.input, &result) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    target.identity = text("C:/w/file");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_DENY);
    CHECK(aether_permission_set_mode(f.permissions, f.input.scope.workspace_id,
          (AetherSessionId){0}, (AetherPermissionMode)-1) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    deny.decision = (AetherPermissionDecision)99;
    CHECK(aether_permission_put_rule(f.permissions, deny) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    dispose(&f);
}

int main(void)
{
    modes_and_context();
    rule_precedence_and_inheritance();
    explicit_views_and_invalid_values();
    return 0;
}
