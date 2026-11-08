#include "aether/platform/platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int matches(const AetherPlatformPath* path, const wchar_t* expected)
{
    wchar_t decoded[32768];
    AetherStringView view = aether_platform_path_view(path);
    int units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, view.data,
                                    (int)view.length, decoded, 32767);
    if (units == 0) {
        return 0;
    }
    decoded[units] = L'\0';
    return wcscmp(decoded, expected) == 0;
}

int main(void)
{
    wchar_t original[32768];
    wchar_t temp[32768];
    wchar_t file[MAX_PATH];
    wchar_t directory[MAX_PATH];
    wchar_t* local_app_data = NULL;
    AetherPlatformPath* path = NULL;
    int success = 0;
    int created = 0;
    int changed = 0;
    DWORD length = GetCurrentDirectoryW(32768U, original);

    if (length == 0U || length >= 32768U) {
        return 1;
    }
    length = GetTempPathW(32768U, temp);
    if (length == 0U || length >= MAX_PATH - 32U) {
        return 1;
    }
    if (aether_platform_path_system(AETHER_PLATFORM_DIRECTORY_TEMP, &path) !=
        AETHER_STATUS_OK || !matches(path, temp)) {
        goto cleanup;
    }
    aether_platform_path_destroy(path);
    path = NULL;
    if (FAILED(SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL,
                                   &local_app_data))) {
        goto cleanup;
    }
    if (aether_platform_path_system(AETHER_PLATFORM_DIRECTORY_USER_DATA,
                                    &path) != AETHER_STATUS_OK ||
        !matches(path, local_app_data)) {
        goto cleanup;
    }
    aether_platform_path_destroy(path);
    path = NULL;
    if (!GetTempFileNameW(temp, L"aet", 0, file)) {
        goto cleanup;
    }
    int written = swprintf(directory, MAX_PATH,
                            L"%ls-\x4E2D\x6587-\xD83D\xDE80", file);
    if (!DeleteFileW(file) || written < 0 || written >= MAX_PATH ||
        !CreateDirectoryW(directory, NULL)) {
        goto cleanup;
    }
    created = 1;
    if (!SetCurrentDirectoryW(directory)) {
        goto cleanup;
    }
    changed = 1;
    if (aether_platform_path_current(&path) != AETHER_STATUS_OK ||
        !matches(path, directory)) {
        goto cleanup;
    }
    success = 1;
cleanup:
    aether_platform_path_destroy(path);
    CoTaskMemFree(local_app_data);
    if (changed && !SetCurrentDirectoryW(original)) {
        success = 0;
    }
    if (created && !RemoveDirectoryW(directory)) {
        success = 0;
    }
    if (!success) {
        fprintf(stderr, "Windows Unicode/system directory regression failed\n");
    }
    return success ? 0 : 1;
}
