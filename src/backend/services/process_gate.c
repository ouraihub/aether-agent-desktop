#include "aether/backend/process_gate.h"
#include "aether/platform/launch_guard.h"
#include "aether/platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AetherAuthorizedLaunch {
    AetherPlatformLaunchGuard* guard;
    AetherPlatformProcessInput native;
    AetherPermissionInput permission;
    AetherPermissionTarget targets[2];
    AetherStringView arguments[64];
    AetherProcessEnvironment environment[64];
    char canonical[2049];
    size_t canonical_size;
    char* strings;
    bool attempted;
};

static bool append(AetherAuthorizedLaunch* launch, const char* bytes, size_t size)
{
    if (size > sizeof(launch->canonical) - 1U - launch->canonical_size) return false;
    if (size != 0U) memcpy(launch->canonical + launch->canonical_size, bytes, size);
    launch->canonical_size += size;
    launch->canonical[launch->canonical_size] = '\0';
    return true;
}

static bool append_field(AetherAuthorizedLaunch* launch, AetherStringView value)
{
    char length[32];
    int count;
    if (value.length > 2048U ||
        aether_string_view_validate(value) != AETHER_STATUS_OK ||
        (value.length != 0U && memchr(value.data, '\0', value.length) != NULL)) return false;
    count = snprintf(length, sizeof(length), "%zu:", value.length);
    return count >= 0 && (size_t)count < sizeof(length) &&
        append(launch, length, (size_t)count) && append(launch, value.data, value.length);
}

static AetherStringView copy_string(AetherStringView value, char** cursor)
{
    AetherStringView copied = {*cursor, value.length};
    if (value.length != 0U) memcpy(*cursor, value.data, value.length);
    (*cursor)[value.length] = '\0';
    *cursor += value.length + 1U;
    return copied;
}

AetherStatus aether_authorized_launch_prepare(
    AetherWorkspaceRegistry* workspaces, AetherPlatformProcessInput input,
    AetherPermissionInput trusted_context, AetherAuthorizedLaunch** out_launch
)
{
    AetherAuthorizedLaunch* launch;
    AetherWorkspaceSnapshot workspace;
    AetherStatus status;
    size_t bytes = 0U;
    char* cursor;
    if (out_launch == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_launch = NULL;
    if (workspaces == NULL || input.argument_count > 64U || input.environment_count > 64U ||
        (input.argument_count != 0U && input.arguments == NULL) ||
        (input.environment_count != 0U && input.environment == NULL)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_workspace_query(workspaces, trusted_context.scope.workspace_id, &workspace);
    if (status != AETHER_STATUS_OK) return status;
    if (workspace.state != AETHER_WORKSPACE_OPEN) return AETHER_STATUS_NOT_INITIALIZED;
    launch = calloc(1U, sizeof(*launch));
    if (launch == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    status = aether_platform_launch_guard_create(input.executable, input.working_directory,
        (AetherStringView){workspace.root_path, strlen(workspace.root_path)}, &launch->guard);
    if (status != AETHER_STATUS_OK) { free(launch); return status; }
    launch->native = input;
    launch->native.executable = aether_platform_launch_guard_executable(launch->guard);
    launch->native.working_directory = aether_platform_launch_guard_cwd(launch->guard);
    launch->permission = trusted_context;
    launch->permission.scope.tool = (AetherStringView){"process.spawn", 13U};
    launch->permission.scope.subtool = (AetherStringView){0};
    launch->permission.level = AETHER_PERMISSION_LEVEL_EXEC;
    launch->targets[0].identity = launch->native.executable;
    launch->targets[1].identity = launch->native.working_directory;
    launch->permission.targets = launch->targets;
    launch->permission.target_count = 2U;
    if (!append(launch, "A", 1U)) status = AETHER_STATUS_INVALID_ARGUMENT;
    for (size_t index = 0U; status == AETHER_STATUS_OK && index < input.argument_count; ++index) {
        if (!append_field(launch, input.arguments[index])) status = AETHER_STATUS_INVALID_ARGUMENT;
        else bytes += input.arguments[index].length + 1U;
    }
    if (status == AETHER_STATUS_OK && !append(launch, "E", 1U)) {
        status = AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (size_t index = 0U; status == AETHER_STATUS_OK && index < input.environment_count;
         ++index) {
        AetherProcessEnvironment entry = input.environment[index];
        if (!append_field(launch, entry.name) || !append_field(launch, entry.value) ||
            entry.name.length == 0U || memchr(entry.name.data, '=', entry.name.length) != NULL) {
            status = AETHER_STATUS_INVALID_ARGUMENT;
        } else bytes += entry.name.length + entry.value.length + 2U;
    }
    if (status != AETHER_STATUS_OK) { aether_authorized_launch_destroy(launch); return status; }
    launch->strings = malloc(bytes == 0U ? 1U : bytes);
    if (launch->strings == NULL) {
        aether_authorized_launch_destroy(launch);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    cursor = launch->strings;
    for (size_t index = 0U; index < input.argument_count; ++index) {
        launch->arguments[index] = copy_string(input.arguments[index], &cursor);
    }
    for (size_t index = 0U; index < input.environment_count; ++index) {
        launch->environment[index].name = copy_string(input.environment[index].name, &cursor);
        launch->environment[index].value = copy_string(input.environment[index].value, &cursor);
    }
    launch->native.arguments = launch->arguments;
    launch->native.environment = launch->environment;
    launch->permission.arguments = (AetherStringView){launch->canonical, launch->canonical_size};
    *out_launch = launch;
    return AETHER_STATUS_OK;
}

AetherPermissionInput aether_authorized_launch_permission_input(
    const AetherAuthorizedLaunch* launch
)
{
    return launch != NULL ? launch->permission : (AetherPermissionInput){0};
}

AetherStatus aether_authorized_launch_execute(
    AetherAuthorizedLaunch* launch, AetherPermissionRegistry* permissions,
    AetherPermissionId approval, AetherProcessService* service,
    AetherTraceContext trace, AetherProcessRef* out_process
)
{
    AetherPlatformClock now;
    AetherStatus status;
    if (out_process == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_process = (AetherProcessRef){0};
    if (launch == NULL || permissions == NULL || service == NULL || !trace.valid) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (launch->attempted) return AETHER_STATUS_ALREADY_EXISTS;
    status = aether_platform_clock_now(&now);
    if (status != AETHER_STATUS_OK) return status;
    status = aether_permission_consume(permissions, approval, launch->permission,
                                       now.monotonic_ns);
    if (status != AETHER_STATUS_OK) return status;
    launch->attempted = true;
    return aether_process_spawn(service, launch->native, trace, out_process);
}

void aether_authorized_launch_destroy(AetherAuthorizedLaunch* launch)
{
    if (launch == NULL) return;
    aether_platform_launch_guard_destroy(launch->guard);
    free(launch->strings);
    free(launch);
}
