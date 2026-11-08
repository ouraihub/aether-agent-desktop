#include "../internal/permission_internal.h"

#include <string.h>

static bool valid_mode(AetherPermissionMode mode)
{
    return mode == AETHER_PERMISSION_MODE_YOLO ||
           mode == AETHER_PERMISSION_MODE_WRITE ||
           mode == AETHER_PERMISSION_MODE_ALWAYS_ASK ||
           mode == AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED;
}

static bool valid_decision(AetherPermissionDecision decision)
{
    return decision == AETHER_PERMISSION_ALLOW || decision == AETHER_PERMISSION_DENY ||
           decision == AETHER_PERMISSION_ASK || decision == AETHER_PERMISSION_INHERIT;
}

static PermissionRuleEntry* find_rule(AetherPermissionRegistry* registry, AetherId id)
{
    size_t index;
    for (index = 0U; index < registry->limits.rules; ++index) {
        PermissionRuleEntry* entry = &registry->rules[index];
        if (entry->occupied && aether_id_equal(entry->rule.rule_id, id)) return entry;
    }
    return NULL;
}

static bool same_scope(AetherPermissionScope left, AetherPermissionScope right)
{
    return aether_id_equal(left.workspace_id.id, right.workspace_id.id) &&
           aether_id_equal(left.session_id.id, right.session_id.id) &&
           aether_id_equal(left.runtime_id.id, right.runtime_id.id) &&
           aether_id_equal(left.provider_id.id, right.provider_id.id) &&
           aether_id_equal(left.subject_id, right.subject_id) &&
           aether_id_equal(left.executor_id, right.executor_id) &&
           aether_string_view_equal(left.tool, right.tool) &&
           aether_string_view_equal(left.subtool, right.subtool);
}

AetherStatus aether_permission_set_mode(
    AetherPermissionRegistry* registry, AetherWorkspaceId workspace,
    AetherSessionId session, AetherPermissionMode mode
)
{
    AetherWorkspaceSnapshot workspace_snapshot;
    AetherSessionSnapshot session_snapshot;
    PermissionModeEntry* slot = NULL;
    AetherStatus status;
    size_t index;
    if (registry == NULL || !aether_id_is_valid(workspace.id) || !valid_mode(mode)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_workspace_query(registry->workspaces, workspace, &workspace_snapshot);
    if (status != AETHER_STATUS_OK) return status;
    if (workspace_snapshot.state != AETHER_WORKSPACE_OPEN) {
        return AETHER_STATUS_NOT_INITIALIZED;
    }
    if (aether_id_is_valid(session.id)) {
        status = aether_session_query(registry->sessions, session, &session_snapshot);
        if (status != AETHER_STATUS_OK) return status;
        if (!aether_id_equal(session_snapshot.workspace_id.id, workspace.id)) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        if (session_snapshot.state != AETHER_SESSION_IDLE &&
            session_snapshot.state != AETHER_SESSION_ACTIVE) {
            return AETHER_STATUS_NOT_INITIALIZED;
        }
    }
    for (index = 0U; index < registry->limits.modes; ++index) {
        PermissionModeEntry* entry = &registry->modes[index];
        if (entry->occupied && aether_id_equal(entry->workspace.id, workspace.id) &&
            aether_id_equal(entry->session.id, session.id)) {
            if (entry->mode == mode) return AETHER_STATUS_OK;
            slot = entry;
            break;
        }
        if (!entry->occupied && slot == NULL) slot = entry;
    }
    if (slot == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    if (registry->revision == UINT64_MAX) return AETHER_STATUS_INTERNAL_ERROR;
    *slot = (PermissionModeEntry){true, workspace, session, mode};
    ++registry->revision;
    return AETHER_STATUS_OK;
}

static AetherStatus validate_parent(
    AetherPermissionRegistry* registry, AetherPermissionRule rule
)
{
    AetherId parent = rule.parent_rule_id;
    size_t depth = 0U;
    if (!aether_id_is_valid(parent)) {
        return rule.decision == AETHER_PERMISSION_INHERIT
            ? AETHER_STATUS_INVALID_ARGUMENT : AETHER_STATUS_OK;
    }
    while (aether_id_is_valid(parent)) {
        PermissionRuleEntry* entry;
        if (aether_id_equal(parent, rule.rule_id) || depth >= registry->limits.rules) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        entry = find_rule(registry, parent);
        if (entry == NULL) return AETHER_STATUS_NOT_FOUND;
        parent = entry->rule.parent_rule_id;
        ++depth;
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_put_rule(
    AetherPermissionRegistry* registry, AetherPermissionRule rule
)
{
    PermissionRuleEntry candidate = {0};
    PermissionRuleEntry* slot;
    AetherStatus status;
    size_t index;
    if (registry == NULL || !aether_id_is_valid(rule.rule_id) ||
        !valid_decision(rule.decision) ||
        !aether_permission_valid_view(rule.scope.tool, 127U, false) ||
        !aether_permission_valid_view(rule.scope.subtool, 127U, false)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = validate_parent(registry, rule);
    if (status != AETHER_STATUS_OK) return status;
    slot = find_rule(registry, rule.rule_id);
    if (slot != NULL && slot->rule.decision == rule.decision &&
        aether_id_equal(slot->rule.parent_rule_id, rule.parent_rule_id) &&
        same_scope(slot->rule.scope, rule.scope)) {
        return AETHER_STATUS_OK;
    }
    if (slot == NULL) {
        for (index = 0U; index < registry->limits.rules; ++index) {
            if (!registry->rules[index].occupied) {
                slot = &registry->rules[index];
                break;
            }
        }
    }
    if (slot == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    if (registry->revision == UINT64_MAX) return AETHER_STATUS_INTERNAL_ERROR;
    candidate.occupied = true;
    candidate.rule = rule;
    if (rule.scope.tool.length != 0U) {
        memcpy(candidate.tool, rule.scope.tool.data, rule.scope.tool.length);
    }
    if (rule.scope.subtool.length != 0U) {
        memcpy(candidate.subtool, rule.scope.subtool.data, rule.scope.subtool.length);
    }
    *slot = candidate;
    slot->rule.scope.tool.data = slot->tool;
    slot->rule.scope.subtool.data = slot->subtool;
    ++registry->revision;
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_remove_rule(
    AetherPermissionRegistry* registry, AetherId rule_id
)
{
    PermissionRuleEntry* entry;
    size_t index;
    if (registry == NULL || !aether_id_is_valid(rule_id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    entry = find_rule(registry, rule_id);
    if (entry == NULL) return AETHER_STATUS_NOT_FOUND;
    for (index = 0U; index < registry->limits.rules; ++index) {
        if (registry->rules[index].occupied &&
            aether_id_equal(registry->rules[index].rule.parent_rule_id, rule_id)) {
            return AETHER_STATUS_ALREADY_EXISTS;
        }
    }
    if (registry->revision == UINT64_MAX) return AETHER_STATUS_INTERNAL_ERROR;
    memset(entry, 0, sizeof(*entry));
    ++registry->revision;
    return AETHER_STATUS_OK;
}

static AetherPermissionDecision resolve_rule(
    AetherPermissionRegistry* registry, const PermissionRuleEntry* entry,
    AetherPermissionScope actual
)
{
    size_t depth;
    for (depth = 0U; depth < registry->limits.rules; ++depth) {
        if (!aether_permission_scope_matches(entry->rule.scope, actual)) {
            return AETHER_PERMISSION_INHERIT;
        }
        if (entry->rule.decision != AETHER_PERMISSION_INHERIT) {
            return valid_decision(entry->rule.decision)
                ? entry->rule.decision : AETHER_PERMISSION_DENY;
        }
        if (!aether_id_is_valid(entry->rule.parent_rule_id)) return AETHER_PERMISSION_DENY;
        entry = find_rule(registry, entry->rule.parent_rule_id);
        if (entry == NULL) return AETHER_PERMISSION_DENY;
    }
    return AETHER_PERMISSION_DENY;
}

static AetherPermissionMode mode_for(
    const AetherPermissionRegistry* registry, AetherPermissionScope scope
)
{
    AetherPermissionMode mode = AETHER_PERMISSION_MODE_YOLO;
    size_t index;
    for (index = 0U; index < registry->limits.modes; ++index) {
        const PermissionModeEntry* entry = &registry->modes[index];
        if (!entry->occupied ||
            !aether_id_equal(entry->workspace.id, scope.workspace_id.id)) continue;
        if (aether_id_equal(entry->session.id, scope.session_id.id)) return entry->mode;
        if (!aether_id_is_valid(entry->session.id)) mode = entry->mode;
    }
    return mode;
}

static void deny_result(AetherPermissionResult* result)
{
    memset(result, 0, sizeof(*result));
    result->decision = AETHER_PERMISSION_DENY;
}

AetherStatus aether_permission_policy(
    AetherPermissionRegistry* registry, AetherPermissionInput input,
    AetherPermissionResult* out_result
)
{
    AetherPermissionMode mode;
    bool rule_allow = false;
    bool rule_ask = false;
    size_t index;
    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    deny_result(out_result);
    if (registry == NULL || (input.level != AETHER_PERMISSION_LEVEL_READ &&
        input.level != AETHER_PERMISSION_LEVEL_WRITE &&
        input.level != AETHER_PERMISSION_LEVEL_EXEC)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    out_result->policy_revision = registry->revision;
    if (input.hard_deny) {
        out_result->source = AETHER_PERMISSION_SOURCE_HARD_DENY;
        return AETHER_STATUS_OK;
    }
    for (index = 0U; index < registry->limits.rules; ++index) {
        AetherPermissionDecision decision;
        if (!registry->rules[index].occupied) continue;
        decision = resolve_rule(registry, &registry->rules[index], input.scope);
        if (decision == AETHER_PERMISSION_DENY) {
            out_result->source = AETHER_PERMISSION_SOURCE_RULE;
            return AETHER_STATUS_OK;
        }
        if (decision == AETHER_PERMISSION_ASK) rule_ask = true;
        if (decision == AETHER_PERMISSION_ALLOW) rule_allow = true;
    }
    if (rule_ask) {
        out_result->decision = AETHER_PERMISSION_ASK;
        out_result->source = AETHER_PERMISSION_SOURCE_RULE;
        return AETHER_STATUS_OK;
    }
    if (input.noncacheable_ask) {
        out_result->decision = AETHER_PERMISSION_ASK;
        out_result->source = AETHER_PERMISSION_SOURCE_SAFETY;
        return AETHER_STATUS_OK;
    }
    if (rule_allow) {
        out_result->decision = AETHER_PERMISSION_ALLOW;
        out_result->source = AETHER_PERMISSION_SOURCE_RULE;
        return AETHER_STATUS_OK;
    }
    mode = mode_for(registry, input.scope);
    if (!valid_mode(mode)) {
        deny_result(out_result);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    out_result->source = AETHER_PERMISSION_SOURCE_MODE;
    out_result->decision = AETHER_PERMISSION_ASK;
    switch (mode) {
        case AETHER_PERMISSION_MODE_YOLO:
            out_result->decision = AETHER_PERMISSION_ALLOW;
            break;
        case AETHER_PERMISSION_MODE_WRITE:
            if (input.level != AETHER_PERMISSION_LEVEL_EXEC) {
                out_result->decision = AETHER_PERMISSION_ALLOW;
            }
            break;
        case AETHER_PERMISSION_MODE_ALWAYS_ASK:
            if (input.level == AETHER_PERMISSION_LEVEL_READ) {
                out_result->decision = AETHER_PERMISSION_ALLOW;
            }
            break;
        case AETHER_PERMISSION_MODE_ALWAYS_ASK_CACHED:
            if (input.target_count == 0U && input.level == AETHER_PERMISSION_LEVEL_READ) {
                out_result->decision = AETHER_PERMISSION_ALLOW;
            }
            break;
        default:
            deny_result(out_result);
            return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (out_result->decision == AETHER_PERMISSION_ASK) {
        if (aether_permission_memory_matches(registry, input)) {
            out_result->decision = AETHER_PERMISSION_ALLOW;
            out_result->source = AETHER_PERMISSION_SOURCE_MEMORY;
        } else {
            out_result->can_remember = true;
        }
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_evaluate(
    AetherPermissionRegistry* registry, AetherPermissionInput input,
    AetherPermissionResult* out_result
)
{
    AetherStatus status;
    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    deny_result(out_result);
    if (registry == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    status = aether_permission_validate_input(registry, input);
    if (status != AETHER_STATUS_OK) return status;
    return aether_permission_policy(registry, input, out_result);
}
