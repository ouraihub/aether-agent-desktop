#include "../internal/permission_internal.h"

#include <stdlib.h>
#include <string.h>

static bool same_memory_scope(AetherPermissionInput remembered, AetherPermissionInput actual)
{
    AetherPermissionScope left = remembered.scope;
    AetherPermissionScope right = actual.scope;
    return aether_id_equal(left.workspace_id.id, right.workspace_id.id) &&
           aether_id_equal(left.session_id.id, right.session_id.id) &&
           aether_id_equal(left.runtime_id.id, right.runtime_id.id) &&
           aether_id_equal(left.provider_id.id, right.provider_id.id) &&
           aether_id_equal(left.subject_id, right.subject_id) &&
           aether_id_equal(left.executor_id, right.executor_id) &&
           aether_string_view_equal(left.tool, right.tool) &&
           aether_string_view_equal(left.subtool, right.subtool) &&
           remembered.runtime_generation == actual.runtime_generation;
}

static bool direct_entry(AetherStringView directory, AetherStringView target)
{
    size_t offset = directory.length;
    if (aether_string_view_equal(directory, target)) return true;
    if (directory.length == 0U || target.length <= directory.length ||
        memcmp(directory.data, target.data, directory.length) != 0) {
        return false;
    }
    if (directory.data[directory.length - 1U] != '/') {
        if (target.data[offset] != '/') return false;
        ++offset;
    }
    return offset < target.length &&
           memchr(target.data + offset, '/', target.length - offset) == NULL;
}

static bool covers_target(const PermissionMemory* memory, AetherStringView target)
{
    size_t index;
    for (index = 0U; index < memory->input.target_count; ++index) {
        AetherStringView stored = memory->input.targets[index].identity;
        if ((memory->kind == AETHER_APPROVAL_SESSION_TARGETS &&
             aether_string_view_equal(stored, target)) ||
            (memory->kind == AETHER_APPROVAL_SESSION_DIRECTORIES &&
             direct_entry(stored, target))) {
            return true;
        }
    }
    return false;
}

bool aether_permission_memory_matches(
    const AetherPermissionRegistry* registry, AetherPermissionInput input
)
{
    size_t memory_index;
    size_t target_index;
    if (registry == NULL) return false;
    for (memory_index = 0U; memory_index < registry->limits.memories; ++memory_index) {
        const PermissionMemory* memory = registry->memories[memory_index];
        if (memory != NULL && memory->kind == AETHER_APPROVAL_SESSION_TOOL &&
            memory->input.level == input.level && same_memory_scope(memory->input, input)) {
            return true;
        }
    }
    if (input.target_count == 0U) return false;
    for (target_index = 0U; target_index < input.target_count; ++target_index) {
        bool covered = false;
        for (memory_index = 0U; memory_index < registry->limits.memories; ++memory_index) {
            const PermissionMemory* memory = registry->memories[memory_index];
            if (memory != NULL && memory->input.level >= input.level &&
                same_memory_scope(memory->input, input) &&
                covers_target(memory, input.targets[target_index].identity)) {
                covered = true;
                break;
            }
        }
        if (!covered) return false;
    }
    return true;
}

static AetherStringView parent_directory(AetherStringView target)
{
    size_t slash = target.length;
    while (slash > 0U && target.data[slash - 1U] != '/') --slash;
    if (slash == 1U) {
        target.length = 1U;
    } else if (slash == 3U && target.data[1] == ':') {
        target.length = 3U;
    } else if (slash > 0U) {
        target.length = slash - 1U;
    }
    return target;
}

AetherStatus aether_permission_remember(
    AetherPermissionRegistry* registry, const PermissionWait* wait, AetherApprovalScope kind
)
{
    AetherPermissionInput input;
    AetherPermissionTarget directories[8];
    PermissionMemory* memory;
    size_t slot;
    size_t index;
    size_t bytes;
    size_t charge;
    const size_t alignment = _Alignof(AetherPermissionTarget);
    const size_t offset = sizeof(PermissionMemory) +
        (alignment - sizeof(PermissionMemory) % alignment) % alignment;
    if (registry == NULL || wait == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    if (kind == AETHER_APPROVAL_ONCE) return AETHER_STATUS_OK;
    if (kind != AETHER_APPROVAL_SESSION_TOOL && kind != AETHER_APPROVAL_SESSION_TARGETS &&
        kind != AETHER_APPROVAL_SESSION_DIRECTORIES) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    input = wait->input;
    if (kind == AETHER_APPROVAL_SESSION_TOOL) {
        input.targets = NULL;
        input.target_count = 0U;
    } else if (input.target_count == 0U || input.target_count > 8U || input.targets == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (kind == AETHER_APPROVAL_SESSION_DIRECTORIES) {
        for (index = 0U; index < input.target_count; ++index) {
            directories[index].identity = parent_directory(input.targets[index].identity);
        }
        input.targets = directories;
    }
    bytes = aether_permission_input_bytes(input, false);
    if (bytes == 0U || bytes > SIZE_MAX - offset) return AETHER_STATUS_INVALID_ARGUMENT;
    charge = offset + bytes;
    if (registry->memory_bytes > registry->limits.memory_bytes ||
        charge > registry->limits.memory_bytes - registry->memory_bytes) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    for (slot = 0U; slot < registry->limits.memories; ++slot) {
        if (registry->memories[slot] == NULL) break;
    }
    if (slot == registry->limits.memories) return AETHER_STATUS_OUT_OF_MEMORY;
    memory = malloc(charge);
    if (memory == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    memory->origin = wait->snapshot.permission_id;
    memory->kind = kind;
    memory->charge = charge;
    aether_permission_copy_into(input, false, &memory->input, (char*)memory + offset);
    registry->memories[slot] = memory;
    registry->memory_bytes += charge;
    return AETHER_STATUS_OK;
}

void aether_permission_drop_memories(
    AetherPermissionRegistry* registry, AetherSessionId session, AetherPermissionId origin
)
{
    size_t index;
    if (registry == NULL) return;
    for (index = 0U; index < registry->limits.memories; ++index) {
        PermissionMemory* memory = registry->memories[index];
        if (memory != NULL &&
            (!aether_id_is_valid(session.id) ||
             aether_id_equal(session.id, memory->input.scope.session_id.id)) &&
            (!aether_id_is_valid(origin.id) || aether_id_equal(origin.id, memory->origin.id))) {
            registry->memory_bytes -= memory->charge;
            registry->memories[index] = NULL;
            free(memory);
        }
    }
}

AetherStatus aether_permission_clear_memory(AetherPermissionRegistry* registry)
{
    AetherSessionId session = {{0U}};
    AetherPermissionId origin = {{0U}};
    if (registry == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_permission_drop_memories(registry, session, origin);
    return AETHER_STATUS_OK;
}
