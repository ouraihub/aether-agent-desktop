#include "permission_fixture.h"

static void once_and_invalidation(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionId id;
    AetherApprovalResult answer;
    AetherPermissionSnapshot snapshot;
    AetherPermissionInput changed;
    AetherPermissionRule deny = rule(AETHER_PERMISSION_DENY);
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    id = begin(&f);
    CHECK(aether_permission_consume(f.permissions, id, f.input, 11U) ==
          AETHER_STATUS_PERMISSION_DENIED);
    changed = f.input;
    changed.arguments = text("command=other");
    CHECK(aether_permission_resolve(f.permissions, id, changed, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(!answer.approved);
    OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer));
    CHECK(answer.approved);
    CHECK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer) ==
        AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_consume(f.permissions, id, f.input, 12U));
    CHECK(aether_permission_consume(f.permissions, id, f.input, 12U) ==
          AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_release(f.permissions, id));
    f.input.request_id = aether_id_next();
    id = begin(&f);
    OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer));
    OK(aether_permission_put_rule(f.permissions, deny));
    CHECK(aether_permission_consume(f.permissions, id, f.input, 12U) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    OK(aether_permission_query(f.permissions, id, 12U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_INVALIDATED);
    OK(aether_permission_release(f.permissions, id));
    OK(aether_permission_remove_rule(f.permissions, deny.rule_id));
    f.input.request_id = aether_id_next();
    id = begin(&f);
    CHECK(aether_permission_resolve(f.permissions, id, f.input, 1000U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer) ==
        AETHER_STATUS_TIMEOUT);
    OK(aether_permission_query(f.permissions, id, 1000U, &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_INVALIDATED);
    OK(aether_permission_release(f.permissions, id));
    dispose(&f);
}

static void directory_and_multitarget(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionTarget targets[2] = {{text("C:/w/file")}, {text("C:/other/a")}};
    AetherPermissionId id;
    AetherApprovalResult answer;
    AetherPermissionRule ask = rule(AETHER_PERMISSION_ASK);
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED);
    f.input.targets = targets;
    f.input.target_count = 1U;
    id = begin(&f);
    OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_DIRECTORIES, &answer));
    CHECK(answer.approved && answer.memory_status == AETHER_STATUS_OK);
    OK(aether_permission_consume(f.permissions, id, f.input, 12U));
    f.input.request_id = aether_id_next();
    f.input.arguments = text("new arguments");
    targets[0].identity = text("C:/w/sibling");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    targets[0].identity = text("C:/w");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    targets[0].identity = text("C:/w/sub/file");
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    targets[0].identity = text("C:/w/file");
    f.input.target_count = 2U;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    f.input.target_count = 1U;
    f.input.level = AETHER_PERMISSION_LEVEL_READ;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    OK(aether_permission_put_rule(f.permissions, ask));
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    CHECK(!evaluate(&f).can_remember);
    OK(aether_permission_remove_rule(f.permissions, ask.rule_id));
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ALLOW);
    f.input.scope.session_id = f.second_session;
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    OK(aether_permission_clear_memory(f.permissions));
    dispose(&f);
}

static void memory_quota_and_close(void)
{
    AetherPermissionLimits limits = aether_permission_default_limits();
    PermissionFixture f;
    AetherPermissionId id;
    AetherApprovalResult answer;
    AetherPermissionResult result;
    limits.memory_bytes = 1U;
    limits.waits = 1U;
    f = fixture(limits);
    mode(&f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    id = begin(&f);
    f.input.request_id = aether_id_next();
    CHECK(aether_permission_begin(f.permissions, f.input, 11U, 1000U, &result) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(result.decision == AETHER_PERMISSION_DENY && result.permission_id.id.value == 0U);
    OK(aether_permission_query(f.permissions, id, 11U,
                               &(AetherPermissionSnapshot){0}));
    /* Restore exact waiting request identity from its snapshot. */
    {
        AetherPermissionSnapshot snapshot;
        OK(aether_permission_query(f.permissions, id, 11U, &snapshot));
        f.input.request_id = snapshot.request_id;
    }
    OK(aether_permission_resolve(f.permissions, id, f.input, 12U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TOOL, &answer));
    CHECK(answer.approved && answer.memory_status == AETHER_STATUS_OUT_OF_MEMORY);
    OK(aether_permission_consume(f.permissions, id, f.input, 13U));
    OK(aether_permission_release(f.permissions, id));
    f.input.request_id = aether_id_next();
    id = begin(&f);
    OK(aether_permission_close_session(f.permissions, f.input.scope.session_id));
    CHECK(aether_permission_resolve(f.permissions, id, f.input, 12U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer) ==
        AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_release(f.permissions, id));
    dispose(&f);
}

static void explicit_ask_cannot_remember(void)
{
    PermissionFixture f = fixture(aether_permission_default_limits());
    AetherPermissionRule ask = rule(AETHER_PERMISSION_ASK);
    AetherPermissionId id;
    AetherApprovalResult answer;
    OK(aether_permission_put_rule(f.permissions, ask));
    id = begin(&f);
    CHECK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_SESSION_TOOL, &answer) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    OK(aether_permission_resolve(f.permissions, id, f.input, 11U,
        AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &answer));
    OK(aether_permission_revoke(f.permissions, id));
    CHECK(aether_permission_consume(f.permissions, id, f.input, 12U) ==
          AETHER_STATUS_ALREADY_EXISTS);
    CHECK(evaluate(&f).decision == AETHER_PERMISSION_ASK);
    dispose(&f);
}

int main(void)
{
    once_and_invalidation();
    directory_and_multitarget();
    memory_quota_and_close();
    explicit_ask_cannot_remember();
    return 0;
}
