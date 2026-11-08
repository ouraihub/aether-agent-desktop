#include "permission_internal.h"

#include <stdlib.h>
#include <string.h>

#define PERMISSION_INPUT_MAX_BYTES 8192U
#define PERMISSION_TARGET_MAX_COUNT 8U

bool aether_permission_valid_view(AetherStringView view, size_t maximum, bool required)
{
    return view.length <= maximum && (!required || view.length != 0U) &&
           aether_string_view_validate(view) == AETHER_STATUS_OK &&
           (view.length == 0U || memchr(view.data, '\0', view.length) == NULL);
}

static bool bytes_equal(AetherStringView left, AetherStringView right)
{
    if (left.length != right.length) return false;
    if (left.length == 0U) return true;
    return left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0;
}

static bool id_matches(AetherId filter, AetherId actual)
{
    return !aether_id_is_valid(filter) || aether_id_equal(filter, actual);
}

bool aether_permission_scope_matches(AetherPermissionScope filter,
                                     AetherPermissionScope actual)
{
    return id_matches(filter.workspace_id.id, actual.workspace_id.id) &&
           id_matches(filter.session_id.id, actual.session_id.id) &&
           id_matches(filter.runtime_id.id, actual.runtime_id.id) &&
           id_matches(filter.provider_id.id, actual.provider_id.id) &&
           id_matches(filter.subject_id, actual.subject_id) &&
           id_matches(filter.executor_id, actual.executor_id) &&
           (filter.tool.length == 0U || bytes_equal(filter.tool, actual.tool)) &&
           (filter.subtool.length == 0U || bytes_equal(filter.subtool, actual.subtool));
}

static bool same_scope(AetherPermissionScope left, AetherPermissionScope right)
{
    return aether_id_equal(left.workspace_id.id, right.workspace_id.id) &&
           aether_id_equal(left.session_id.id, right.session_id.id) &&
           aether_id_equal(left.runtime_id.id, right.runtime_id.id) &&
           aether_id_equal(left.provider_id.id, right.provider_id.id) &&
           aether_id_equal(left.subject_id, right.subject_id) &&
           aether_id_equal(left.executor_id, right.executor_id) &&
           bytes_equal(left.tool, right.tool) && bytes_equal(left.subtool, right.subtool);
}

bool aether_permission_same_input(AetherPermissionInput left, AetherPermissionInput right)
{
    if (!aether_id_equal(left.request_id, right.request_id) ||
        !same_scope(left.scope, right.scope) ||
        left.runtime_generation != right.runtime_generation || left.level != right.level ||
        left.target_count != right.target_count ||
        !bytes_equal(left.arguments, right.arguments) ||
        left.capability_supported != right.capability_supported ||
        left.hard_deny != right.hard_deny || left.noncacheable_ask != right.noncacheable_ask) {
        return false;
    }
    if (left.target_count != 0U && (left.targets == NULL || right.targets == NULL)) {
        return false;
    }
    for (size_t index = 0U; index < left.target_count; ++index) {
        if (!bytes_equal(left.targets[index].identity, right.targets[index].identity)) {
            return false;
        }
    }
    return true;
}

static bool valid_identity(AetherStringView identity)
{
    size_t root_length;
    size_t segment;

    if (!aether_permission_valid_view(identity, 511U, true) ||
        memchr(identity.data, '\\', identity.length) != NULL) {
        return false;
    }
    if (identity.data[0] == '/') {
        root_length = 1U;
    } else if (identity.length >= 3U && identity.data[1] == ':' &&
               identity.data[2] == '/' &&
               ((identity.data[0] >= 'A' && identity.data[0] <= 'Z') ||
                (identity.data[0] >= 'a' && identity.data[0] <= 'z'))) {
        root_length = 3U;
    } else {
        return false;
    }
    if (identity.length == root_length) return true;
    segment = root_length;
    for (size_t index = root_length; index <= identity.length; ++index) {
        if (index == identity.length || identity.data[index] == '/') {
            size_t length = index - segment;
            if (length == 0U ||
                (length == 1U && identity.data[segment] == '.') ||
                (length == 2U && identity.data[segment] == '.' &&
                 identity.data[segment + 1U] == '.')) {
                return false;
            }
            segment = index + 1U;
        }
    }
    return true;
}

static bool valid_shape(AetherPermissionInput input, bool include_arguments)
{
    if (!aether_permission_valid_view(input.scope.tool, 127U, true) ||
        !aether_permission_valid_view(input.scope.subtool, 127U, false) ||
        (include_arguments &&
         !aether_permission_valid_view(input.arguments, 2048U, false)) ||
        (input.level != AETHER_PERMISSION_LEVEL_READ &&
         input.level != AETHER_PERMISSION_LEVEL_WRITE &&
         input.level != AETHER_PERMISSION_LEVEL_EXEC) ||
        input.target_count > PERMISSION_TARGET_MAX_COUNT ||
        (input.target_count != 0U && input.targets == NULL)) {
        return false;
    }
    for (size_t index = 0U; index < input.target_count; ++index) {
        if (!valid_identity(input.targets[index].identity)) return false;
    }
    return true;
}

AetherStatus aether_permission_validate_input(
    const AetherPermissionRegistry* registry, AetherPermissionInput input
)
{
    AetherWorkspaceSnapshot workspace;
    AetherSessionSnapshot session;
    AetherRuntimeSnapshot runtime;
    AetherProviderSnapshot provider;
    AetherStatus status;

    if (registry == NULL || !aether_id_is_valid(input.request_id) ||
        !aether_id_is_valid(input.scope.workspace_id.id) ||
        !aether_id_is_valid(input.scope.session_id.id) ||
        !aether_id_is_valid(input.scope.runtime_id.id) ||
        !aether_id_is_valid(input.scope.provider_id.id) ||
        !aether_id_is_valid(input.scope.subject_id) ||
        !aether_id_is_valid(input.scope.executor_id) || input.runtime_generation == 0U ||
        !valid_shape(input, true)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_workspace_query(registry->workspaces, input.scope.workspace_id, &workspace);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_session_query(registry->sessions, input.scope.session_id, &session);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_runtime_query(registry->runtimes, input.scope.runtime_id, &runtime);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_runtime_provider_query(registry->runtimes, input.scope.provider_id, &provider);
    if (status != AETHER_STATUS_OK) return status;
    if (!aether_id_equal(session.workspace_id.id, input.scope.workspace_id.id) ||
        !aether_id_equal(session.runtime_id.id, input.scope.runtime_id.id) ||
        session.runtime_generation != input.runtime_generation ||
        runtime.generation != input.runtime_generation ||
        !aether_id_equal(runtime.provider_id.id, input.scope.provider_id.id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (workspace.state != AETHER_WORKSPACE_OPEN ||
        (session.state != AETHER_SESSION_IDLE && session.state != AETHER_SESSION_ACTIVE) ||
        runtime.state != AETHER_RUNTIME_READY) {
        return AETHER_STATUS_NOT_INITIALIZED;
    }
    if (!input.capability_supported) return AETHER_STATUS_NOT_SUPPORTED;
    return AETHER_STATUS_OK;
}

static bool add_bytes(size_t* total, size_t amount)
{
    if (amount > PERMISSION_INPUT_MAX_BYTES - *total) return false;
    *total += amount;
    return true;
}

size_t aether_permission_input_bytes(AetherPermissionInput input, bool include_arguments)
{
    size_t bytes;

    if (!valid_shape(input, include_arguments) ||
        input.target_count > PERMISSION_INPUT_MAX_BYTES / sizeof(AetherPermissionTarget)) {
        return 0U;
    }
    bytes = input.target_count * sizeof(AetherPermissionTarget);
    if (!add_bytes(&bytes, input.scope.tool.length + 1U) ||
        !add_bytes(&bytes, input.scope.subtool.length + 1U) ||
        (include_arguments && !add_bytes(&bytes, input.arguments.length + 1U))) {
        return 0U;
    }
    for (size_t index = 0U; index < input.target_count; ++index) {
        if (!add_bytes(&bytes, input.targets[index].identity.length + 1U)) return 0U;
    }
    return bytes;
}

static AetherStringView copy_view(AetherStringView source, char** cursor)
{
    AetherStringView result = {*cursor, source.length};
    if (source.length != 0U) (void)memcpy(*cursor, source.data, source.length);
    (*cursor)[source.length] = '\0';
    *cursor += source.length + 1U;
    return result;
}

void aether_permission_copy_into(AetherPermissionInput input, bool include_arguments,
                                 AetherPermissionInput* out_input, void* storage)
{
    AetherPermissionTarget* targets = storage;
    char* cursor = (char*)storage + input.target_count * sizeof(*targets);
    AetherPermissionInput copied = input;

    copied.targets = input.target_count == 0U ? NULL : targets;
    copied.scope.tool = copy_view(input.scope.tool, &cursor);
    copied.scope.subtool = copy_view(input.scope.subtool, &cursor);
    if (include_arguments) {
        copied.arguments = copy_view(input.arguments, &cursor);
    } else {
        copied.arguments = (AetherStringView){0};
        copied.request_id = (AetherId){0};
    }
    for (size_t index = 0U; index < input.target_count; ++index) {
        targets[index].identity = copy_view(input.targets[index].identity, &cursor);
    }
    *out_input = copied;
}

AetherStatus aether_permission_copy_input(
    AetherPermissionInput input, bool include_arguments,
    AetherPermissionInput* out_input, void** out_storage, size_t* out_size
)
{
    size_t bytes;
    void* storage;

    if (out_input != NULL) *out_input = (AetherPermissionInput){0};
    if (out_storage != NULL) *out_storage = NULL;
    if (out_size != NULL) *out_size = 0U;
    if (out_input == NULL || out_storage == NULL || out_size == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    bytes = aether_permission_input_bytes(input, include_arguments);
    if (bytes == 0U) return AETHER_STATUS_INVALID_ARGUMENT;
    storage = malloc(bytes);
    if (storage == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    aether_permission_copy_into(input, include_arguments, out_input, storage);
    *out_storage = storage;
    *out_size = bytes;
    return AETHER_STATUS_OK;
}
