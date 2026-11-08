#define _POSIX_C_SOURCE 200809L
#include "path_internal.h"

#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static AetherStatus native_error(int error)
{
    switch (error) {
    case ENOENT:
    case ENOTDIR:
        return AETHER_STATUS_NOT_FOUND;
    case EACCES:
    case EPERM:
        return AETHER_STATUS_PERMISSION_DENIED;
    case ENOMEM:
        return AETHER_STATUS_OUT_OF_MEMORY;
    default:
        return AETHER_STATUS_IO_ERROR;
    }
}

AetherStatus aether_platform_path_current(AetherPlatformPath** out_path)
{
    size_t capacity = 256U;

    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    for (;;) {
        char* buffer = malloc(capacity);
        if (buffer == NULL) {
            return AETHER_STATUS_OUT_OF_MEMORY;
        }
        if (getcwd(buffer, capacity) != NULL) {
            AetherStatus status = aether_platform_path_from_view(
                (AetherStringView){buffer, strlen(buffer)}, out_path);
            free(buffer);
            return status;
        }
        int error = errno;
        free(buffer);
        if (error != ERANGE || capacity > SIZE_MAX / 2U) {
            return native_error(error);
        }
        capacity *= 2U;
    }
}

static AetherStatus user_data(AetherPlatformPath** out_path)
{
#if defined(__APPLE__)
    const char suffix[] = "/Library/Application Support";
#else
    const char suffix[] = "/.local/share";
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg != NULL && xdg[0] == '/') {
        return aether_platform_path_from_view(
            (AetherStringView){xdg, strlen(xdg)}, out_path);
    }
#endif
    const char* home = getenv("HOME");
    char* storage = NULL;
    size_t capacity = 1024U;
    struct passwd entry;
    struct passwd* found = NULL;

    if (home == NULL || home[0] != '/') {
        for (;;) {
            storage = malloc(capacity);
            if (storage == NULL) {
                return AETHER_STATUS_OUT_OF_MEMORY;
            }
            int error = getpwuid_r(getuid(), &entry, storage, capacity, &found);
            if (error == 0 && found != NULL) {
                home = found->pw_dir;
                break;
            }
            free(storage);
            if (error != ERANGE || capacity > SIZE_MAX / 2U) {
                return error == 0 ? AETHER_STATUS_NOT_FOUND
                                  : native_error(error);
            }
            capacity *= 2U;
        }
    }
    size_t length = strlen(home);
    AetherPlatformPath* path;
    AetherStatus status;
    if (length > SIZE_MAX - sizeof(suffix)) {
        free(storage);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_platform_path_allocate(length + sizeof(suffix) - 1U,
                                           &path);
    if (status == AETHER_STATUS_OK) {
        memcpy(path->data, home, length);
        memcpy(path->data + length, suffix, sizeof(suffix));
        status = aether_string_view_validate(aether_platform_path_view(path));
        if (status == AETHER_STATUS_OK) {
            *out_path = path;
        } else {
            aether_platform_path_destroy(path);
        }
    }
    free(storage);
    return status;
}

AetherStatus aether_platform_path_system(
    AetherPlatformDirectory directory,
    AetherPlatformPath** out_path
)
{
    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    if (directory == AETHER_PLATFORM_DIRECTORY_USER_DATA) {
        return user_data(out_path);
    }
    if (directory == AETHER_PLATFORM_DIRECTORY_TEMP) {
        const char* temp = getenv("TMPDIR");
        if (temp == NULL || temp[0] != '/') {
            temp = "/tmp";
        }
        return aether_platform_path_from_view(
            (AetherStringView){temp, strlen(temp)}, out_path);
    }
    return AETHER_STATUS_INVALID_ARGUMENT;
}
