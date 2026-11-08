#include "permission_internal.h"
#include <stdlib.h>

#define APPROVAL_LIFETIME_NS UINT64_C(300000000000)

AetherStatus aether_permission_begin(
    AetherPermissionRegistry* registry, AetherPermissionInput input,
    uint64_t now_ns, uint64_t deadline_ns, AetherPermissionResult* out_result
)
{
    AetherPermissionResult evaluation;
    AetherPermissionInput copy;
    void* storage = NULL;
    size_t size;
    size_t index;
    PermissionWait* slot = NULL;
    AetherId id;
    AetherStatus status;
    uint64_t expires;
    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_result = (AetherPermissionResult){.decision = AETHER_PERMISSION_DENY};
    status = aether_permission_evaluate(registry, input, &evaluation);
    if (status != AETHER_STATUS_OK) return status;
    if (evaluation.decision != AETHER_PERMISSION_ASK) {
        *out_result = evaluation;
        return AETHER_STATUS_OK;
    }
    if (deadline_ns <= now_ns) return AETHER_STATUS_TIMEOUT;
    expires = now_ns > UINT64_MAX - APPROVAL_LIFETIME_NS
        ? UINT64_MAX : now_ns + APPROVAL_LIFETIME_NS;
    if (deadline_ns < expires) expires = deadline_ns;
    for (index = 0U; index < registry->limits.waits; ++index) {
        PermissionWait* wait = &registry->waits[index];
        if (wait->occupied &&
            aether_id_equal(wait->input.request_id, input.request_id)) {
            return AETHER_STATUS_ALREADY_EXISTS;
        }
        if (!wait->occupied && slot == NULL) slot = wait;
    }
    if (slot == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    status = aether_permission_copy_input(input, true, &copy, &storage, &size);
    if (status != AETHER_STATUS_OK) return status;
    id = aether_id_next();
    if (!aether_id_is_valid(id)) {
        free(storage);
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    slot->occupied = true;
    slot->storage = storage;
    slot->storage_bytes = size;
    slot->input = copy;
    slot->snapshot = (AetherPermissionSnapshot){(AetherPermissionId){id},
        input.request_id, AETHER_PERMISSION_WAITING, input.scope.session_id,
        registry->revision, expires, evaluation.can_remember};
    evaluation.permission_id = slot->snapshot.permission_id;
    *out_result = evaluation;
    return AETHER_STATUS_OK;
}

static AetherStatus check_actual(
    AetherPermissionRegistry* registry, PermissionWait* wait,
    AetherPermissionInput actual, uint64_t now_ns
)
{
    AetherStatus status = aether_permission_refresh(registry, wait, now_ns);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_permission_validate_input(registry, actual);
    if (status != AETHER_STATUS_OK) return status;
    if (!aether_permission_same_input(wait->input, actual)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_resolve(
    AetherPermissionRegistry* registry, AetherPermissionId id,
    AetherPermissionInput actual, uint64_t now_ns,
    AetherPermissionDecision answer, AetherApprovalScope scope,
    AetherApprovalResult* out_result
)
{
    PermissionWait* wait;
    AetherPermissionResult current;
    AetherStatus status;
    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_result = (AetherApprovalResult){0};
    if (registry == NULL || !aether_id_is_valid(id.id) ||
        (answer != AETHER_PERMISSION_ALLOW && answer != AETHER_PERMISSION_DENY) ||
        (scope != AETHER_APPROVAL_ONCE && scope != AETHER_APPROVAL_SESSION_TOOL &&
         scope != AETHER_APPROVAL_SESSION_TARGETS &&
         scope != AETHER_APPROVAL_SESSION_DIRECTORIES)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    wait = aether_permission_find_wait(registry, id);
    if (wait == NULL) return AETHER_STATUS_NOT_FOUND;
    if (wait->snapshot.state != AETHER_PERMISSION_WAITING) {
        return AETHER_STATUS_ALREADY_EXISTS;
    }
    status = check_actual(registry, wait, actual, now_ns);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_permission_evaluate(registry, actual, &current);
    if (status != AETHER_STATUS_OK) return status;
    if (current.decision == AETHER_PERMISSION_DENY) {
        wait->snapshot.state = AETHER_PERMISSION_INVALIDATED;
        return AETHER_STATUS_PERMISSION_DENIED;
    }
    if (scope != AETHER_APPROVAL_ONCE &&
        (!wait->snapshot.can_remember || answer == AETHER_PERMISSION_DENY)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (answer == AETHER_PERMISSION_ALLOW &&
        ((scope == AETHER_APPROVAL_SESSION_TOOL && actual.target_count != 0U) ||
         ((scope == AETHER_APPROVAL_SESSION_TARGETS ||
           scope == AETHER_APPROVAL_SESSION_DIRECTORIES) &&
          actual.target_count == 0U))) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (answer == AETHER_PERMISSION_DENY) {
        wait->snapshot.state = AETHER_PERMISSION_REJECTED;
        return AETHER_STATUS_OK;
    }
    wait->snapshot.state = AETHER_PERMISSION_APPROVED;
    out_result->approved = true;
    if (scope != AETHER_APPROVAL_ONCE) {
        out_result->memory_status = aether_permission_remember(registry, wait, scope);
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_permission_consume(
    AetherPermissionRegistry* registry, AetherPermissionId id,
    AetherPermissionInput actual, uint64_t now_ns
)
{
    PermissionWait* wait = NULL;
    AetherPermissionResult current;
    AetherStatus status;
    if (registry == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    if (aether_id_is_valid(id.id)) {
        wait = aether_permission_find_wait(registry, id);
        if (wait == NULL) return AETHER_STATUS_NOT_FOUND;
        if (wait->snapshot.state != AETHER_PERMISSION_APPROVED) {
            return wait->snapshot.state == AETHER_PERMISSION_WAITING
                ? AETHER_STATUS_PERMISSION_DENIED : AETHER_STATUS_ALREADY_EXISTS;
        }
        status = check_actual(registry, wait, actual, now_ns);
        if (status != AETHER_STATUS_OK) return status;
    }
    status = aether_permission_evaluate(registry, actual, &current);
    if (status != AETHER_STATUS_OK) return status;
    if (current.decision == AETHER_PERMISSION_DENY) {
        return AETHER_STATUS_PERMISSION_DENIED;
    }
    if (wait == NULL && current.decision != AETHER_PERMISSION_ALLOW) {
        return AETHER_STATUS_PERMISSION_DENIED;
    }
    if (wait != NULL) wait->snapshot.state = AETHER_PERMISSION_CONSUMED;
    return AETHER_STATUS_OK;
}
