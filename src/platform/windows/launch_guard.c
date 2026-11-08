#include "aether/platform/launch_guard.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define INPUT_UNITS 32768U
#define IDENTITY_BYTES 512U
#define FINAL_UNITS (IDENTITY_BYTES + 4U)
/* Three paths, each at most 511 bytes: at most 256 ancestor directories each. */
#define ANCESTOR_HANDLES (3U * 256U)
#define RESOURCE_COUNT 3U

typedef struct GuardResource {
    HANDLE handle;
    wchar_t path[FINAL_UNITS];
    size_t length;
    char identity[IDENTITY_BYTES];
    size_t identity_length;
} GuardResource;

struct AetherPlatformLaunchGuard {
    GuardResource resources[RESOURCE_COUNT];
    HANDLE ancestors[ANCESTOR_HANDLES];
    size_t ancestor_count;
    wchar_t scratch[INPUT_UNITS];
};

static AetherStatus native_error(DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_NOT_FOUND:
        return AETHER_STATUS_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return AETHER_STATUS_PERMISSION_DENIED;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return AETHER_STATUS_OUT_OF_MEMORY;
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_NO_UNICODE_TRANSLATION:
    case ERROR_INSUFFICIENT_BUFFER:
    case ERROR_DIRECTORY:
        return AETHER_STATUS_INVALID_ARGUMENT;
    default:
        return AETHER_STATUS_IO_ERROR;
    }
}

static bool drive_absolute(const wchar_t* path, size_t length)
{
    return length >= 3U &&
        ((path[0] >= L'A' && path[0] <= L'Z') ||
         (path[0] >= L'a' && path[0] <= L'z')) &&
        path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
}

static AetherStatus input_path(AetherStringView input, wchar_t* output)
{
    int length;
    size_t start = 0U;

    if (input.data == NULL || input.length == 0U ||
        input.length > (INPUT_UNITS - 1U) * 4U ||
        memchr(input.data, '\0', input.length) != NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        input.data, (int)input.length, output, (int)INPUT_UNITS - 1);
    if (length == 0) {
        return native_error(GetLastError());
    }
    output[length] = L'\0';
    if (length >= 4 && wmemcmp(output, L"\\\\?\\", 4U) == 0) {
        start = 4U;
    }
    if (!drive_absolute(output + start, (size_t)length - start)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = start; i < (size_t)length; ++i) {
        wchar_t value = output[i];
        if (value < L' ' || value == L'"' || value == L'<' || value == L'>' ||
            value == L'|' || value == L'?' || value == L'*' ||
            (value == L':' && i != start + 1U)) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        if (value == L'/') {
            output[i] = L'\\';
        }
    }
    return AETHER_STATUS_OK;
}

/* Stripping the extended prefix must not turn a real filename into a device. */
static bool reserved_component(const wchar_t* text, size_t length)
{
    wchar_t name[4];
    size_t base = 0U;

    while (base < length && text[base] != L'.') {
        ++base;
    }
    if (base != 3U && base != 4U) {
        return false;
    }
    for (size_t i = 0U; i < base; ++i) {
        wchar_t value = text[i];
        if (value >= L'a' && value <= L'z') {
            value = (wchar_t)(value - L'a' + L'A');
        }
        name[i] = value;
    }
    if (base == 3U) {
        return wmemcmp(name, L"CON", 3U) == 0 ||
            wmemcmp(name, L"PRN", 3U) == 0 ||
            wmemcmp(name, L"AUX", 3U) == 0 ||
            wmemcmp(name, L"NUL", 3U) == 0;
    }
    return (wmemcmp(name, L"COM", 3U) == 0 || wmemcmp(name, L"LPT", 3U) == 0) &&
        ((name[3] >= L'1' && name[3] <= L'9') || name[3] == L'\u00b9' ||
         name[3] == L'\u00b2' || name[3] == L'\u00b3');
}

static bool plain_dos_safe(const wchar_t* path, size_t length)
{
    size_t component = 7U;

    for (size_t i = component; i <= length; ++i) {
        if (i != length && path[i] != L'\\') {
            if (path[i] < L' ' || path[i] == L':' || path[i] == L'/' ||
                path[i] == L'"' || path[i] == L'<' || path[i] == L'>' ||
                path[i] == L'|' || path[i] == L'?' || path[i] == L'*') {
                return false;
            }
            continue;
        }
        if (i != component &&
            (path[i - 1U] == L'.' || path[i - 1U] == L' ' ||
             reserved_component(path + component, i - component))) {
            return false;
        }
        if (i == component && i != length) {
            return false;
        }
        component = i + 1U;
    }
    return true;
}

static AetherStatus final_path(HANDLE handle, wchar_t* output, size_t* out_length)
{
    DWORD length = GetFinalPathNameByHandleW(handle, output, (DWORD)FINAL_UNITS,
        VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
    wchar_t root[4];
    UINT drive_type;

    if (length == 0U) {
        return native_error(GetLastError());
    }
    if (length >= FINAL_UNITS || length < 7U ||
        wmemcmp(output, L"\\\\?\\", 4U) != 0 ||
        !drive_absolute(output + 4U, (size_t)length - 4U) || output[6] != L'\\') {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    while (length > 7U && output[length - 1U] == L'\\') {
        output[--length] = L'\0';
    }
    if (!plain_dos_safe(output, (size_t)length)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    memcpy(root, output + 4U, 3U * sizeof(*root));
    root[3] = L'\0';
    drive_type = GetDriveTypeW(root);
    if (drive_type != DRIVE_FIXED && drive_type != DRIVE_REMOVABLE &&
        drive_type != DRIVE_CDROM && drive_type != DRIVE_RAMDISK) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_length = (size_t)length;
    return AETHER_STATUS_OK;
}

static AetherStatus identity_copy(GuardResource* resource)
{
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        resource->path + 4U, (int)(resource->length - 4U), resource->identity,
        (int)IDENTITY_BYTES - 1, NULL, NULL);

    if (length == 0) {
        return native_error(GetLastError());
    }
    resource->identity[length] = '\0';
    resource->identity_length = (size_t)length;
    for (size_t i = 0U; i < resource->identity_length; ++i) {
        if (resource->identity[i] == '\\') {
            resource->identity[i] = '/';
        }
    }
    return AETHER_STATUS_OK;
}

static AetherStatus open_resource(
    AetherPlatformLaunchGuard* guard, AetherStringView input, size_t index
)
{
    GuardResource* resource = &guard->resources[index];
    BY_HANDLE_FILE_INFORMATION information;
    bool directory = index != 0U;
    AetherStatus status = input_path(input, guard->scratch);

    if (status != AETHER_STATUS_OK) {
        return status;
    }
    resource->handle = CreateFileW(guard->scratch,
        directory ? FILE_READ_ATTRIBUTES : GENERIC_READ,
        directory ? FILE_SHARE_READ | FILE_SHARE_WRITE : FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (resource->handle == INVALID_HANDLE_VALUE) {
        return native_error(GetLastError());
    }
    if (!GetFileInformationByHandle(resource->handle, &information)) {
        return native_error(GetLastError());
    }
    if (((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) !=
        directory) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = final_path(resource->handle, resource->path, &resource->length);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    return identity_copy(resource);
}

static AetherStatus pin_ancestor(AetherPlatformLaunchGuard* guard, size_t length)
{
    HANDLE handle;
    BY_HANDLE_FILE_INFORMATION information;
    wchar_t actual[FINAL_UNITS];
    size_t actual_length;
    AetherStatus status;

    if (guard->ancestor_count == ANCESTOR_HANDLES) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    handle = CreateFileW(guard->scratch, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return native_error(GetLastError());
    }
    guard->ancestors[guard->ancestor_count++] = handle;
    if (!GetFileInformationByHandle(handle, &information)) {
        return native_error(GetLastError());
    }
    if ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = final_path(handle, actual, &actual_length);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    /* Exact spelling is conservative on case-sensitive Windows directories. */
    if (actual_length != length || wmemcmp(actual, guard->scratch, length) != 0) {
        return AETHER_STATUS_PERMISSION_DENIED;
    }
    return AETHER_STATUS_OK;
}

/* Pin final DOS ancestors root-first, then recheck the original handles. Holding
 * only leaf handles would still permit a parent directory to be renamed.
 * These directory handles do not lock DOS drive mappings or sandbox the child. */
static AetherStatus pin_ancestors(AetherPlatformLaunchGuard* guard, size_t index)
{
    const GuardResource* resource = &guard->resources[index];

    memcpy(guard->scratch, resource->path,
        (resource->length + 1U) * sizeof(*resource->path));
    for (size_t i = 7U; i < resource->length; ++i) {
        wchar_t saved;
        AetherStatus status;
        if (i != 7U && resource->path[i] != L'\\') {
            continue;
        }
        saved = guard->scratch[i];
        guard->scratch[i] = L'\0';
        status = pin_ancestor(guard, i);
        guard->scratch[i] = saved;
        if (status != AETHER_STATUS_OK) {
            return status;
        }
    }
    return AETHER_STATUS_OK;
}

static AetherStatus revalidate_resource(AetherPlatformLaunchGuard* guard, size_t index)
{
    const GuardResource* resource = &guard->resources[index];
    size_t length;
    AetherStatus status = final_path(resource->handle, guard->scratch, &length);

    if (status != AETHER_STATUS_OK) {
        return status;
    }
    if (length != resource->length ||
        wmemcmp(guard->scratch, resource->path, length) != 0) {
        return AETHER_STATUS_PERMISSION_DENIED;
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_launch_guard_create(
    AetherStringView executable, AetherStringView cwd, AetherStringView workspace,
    AetherPlatformLaunchGuard** out_guard
)
{
    AetherPlatformLaunchGuard* guard;
    const GuardResource* root;
    const GuardResource* directory;
    AetherStatus status;

    if (out_guard == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_guard = NULL;
    guard = calloc(1U, sizeof(*guard));
    if (guard == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    status = open_resource(guard, executable, 0U);
    if (status == AETHER_STATUS_OK) {
        status = open_resource(guard, cwd, 1U);
    }
    if (status == AETHER_STATUS_OK) {
        status = open_resource(guard, workspace, 2U);
    }
    for (size_t i = 0U; i < RESOURCE_COUNT && status == AETHER_STATUS_OK; ++i) {
        status = pin_ancestors(guard, i);
    }
    for (size_t i = 0U; i < RESOURCE_COUNT && status == AETHER_STATUS_OK; ++i) {
        status = revalidate_resource(guard, i);
    }
    root = &guard->resources[2];
    directory = &guard->resources[1];
    if (status == AETHER_STATUS_OK &&
        (directory->length < root->length ||
         wmemcmp(directory->path, root->path, root->length) != 0 ||
         (directory->length > root->length && root->length != 7U &&
          directory->path[root->length] != L'\\'))) {
        status = AETHER_STATUS_PERMISSION_DENIED;
    }
    if (status != AETHER_STATUS_OK) {
        aether_platform_launch_guard_destroy(guard);
        return status;
    }
    *out_guard = guard;
    return AETHER_STATUS_OK;
}

AetherStringView aether_platform_launch_guard_executable(
    const AetherPlatformLaunchGuard* guard
)
{
    AetherStringView result = {NULL, 0U};
    if (guard != NULL) {
        result.data = guard->resources[0].identity;
        result.length = guard->resources[0].identity_length;
    }
    return result;
}

AetherStringView aether_platform_launch_guard_cwd(const AetherPlatformLaunchGuard* guard)
{
    AetherStringView result = {NULL, 0U};
    if (guard != NULL) {
        result.data = guard->resources[1].identity;
        result.length = guard->resources[1].identity_length;
    }
    return result;
}

static void close_guard_handle(HANDLE handle)
{
    if (handle != NULL && handle != INVALID_HANDLE_VALUE) (void)CloseHandle(handle);
}

void aether_platform_launch_guard_destroy(AetherPlatformLaunchGuard* guard)
{
    if (guard == NULL) {
        return;
    }
    for (size_t i = 0U; i < RESOURCE_COUNT; ++i) {
        close_guard_handle(guard->resources[i].handle);
    }
    for (size_t i = guard->ancestor_count; i > 0U; --i) {
        close_guard_handle(guard->ancestors[i - 1U]);
    }
    free(guard);
}
