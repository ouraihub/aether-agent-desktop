#include "path_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

/* Windows directory query APIs return at most 32,767 UTF-16 units. */
#define PATH_UNITS 32768U

static AetherStatus native_error(DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return AETHER_STATUS_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
        return AETHER_STATUS_PERMISSION_DENIED;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return AETHER_STATUS_OUT_OF_MEMORY;
    default:
        return AETHER_STATUS_IO_ERROR;
    }
}

static AetherStatus from_wide(
    const wchar_t* text,
    size_t length,
    AetherPlatformPath** out_path
)
{
    AetherPlatformPath* path;
    AetherStatus status;
    int bytes;

    if (length == 0U || length > INT_MAX) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text,
                               (int)length, NULL, 0, NULL, NULL);
    if (bytes == 0) {
        return native_error(GetLastError());
    }
    status = aether_platform_path_allocate((size_t)bytes, &path);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text,
                           (int)length, path->data, bytes, NULL, NULL) == 0) {
        DWORD error = GetLastError();
        aether_platform_path_destroy(path);
        return native_error(error);
    }
    *out_path = path;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_path_current(AetherPlatformPath** out_path)
{
    wchar_t* buffer;
    DWORD length;
    AetherStatus status;

    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    buffer = malloc(PATH_UNITS * sizeof(*buffer));
    if (buffer == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    length = GetCurrentDirectoryW(PATH_UNITS, buffer);
    if (length == 0U || length >= PATH_UNITS) {
        status = native_error(length == 0U ? GetLastError()
                                          : ERROR_FILENAME_EXCED_RANGE);
    } else {
        status = from_wide(buffer, (size_t)length, out_path);
    }
    free(buffer);
    return status;
}

AetherStatus aether_platform_path_system(
    AetherPlatformDirectory directory,
    AetherPlatformPath** out_path
)
{
    AetherStatus status;
    wchar_t* buffer;

    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    if (directory == AETHER_PLATFORM_DIRECTORY_USER_DATA) {
        HRESULT result = SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0,
                                              NULL, &buffer);
        if (FAILED(result)) {
            return native_error(HRESULT_FACILITY(result) == FACILITY_WIN32
                                    ? HRESULT_CODE(result) : ERROR_GEN_FAILURE);
        }
        status = from_wide(buffer, wcslen(buffer), out_path);
        CoTaskMemFree(buffer);
        return status;
    }
    if (directory != AETHER_PLATFORM_DIRECTORY_TEMP) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    buffer = malloc(PATH_UNITS * sizeof(*buffer));
    if (buffer == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    DWORD length = GetTempPathW(PATH_UNITS, buffer);
    if (length == 0U || length >= PATH_UNITS) {
        status = native_error(length == 0U ? GetLastError()
                                          : ERROR_FILENAME_EXCED_RANGE);
    } else {
        status = from_wide(buffer, (size_t)length, out_path);
    }
    free(buffer);
    return status;
}
