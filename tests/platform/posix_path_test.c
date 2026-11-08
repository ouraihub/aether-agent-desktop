#define _POSIX_C_SOURCE 200809L
#include "aether/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int expect_directory(AetherPlatformDirectory kind, const char* expected)
{
    AetherPlatformPath* path = NULL;
    AetherStatus status = aether_platform_path_system(kind, &path);
    AetherStringView view = aether_platform_path_view(path);
    int matches = status == AETHER_STATUS_OK &&
                  view.length == strlen(expected) &&
                  memcmp(view.data, expected, view.length) == 0;
    aether_platform_path_destroy(path);
    return matches;
}

int main(void)
{
    AetherPlatformPath* original = NULL;
    AetherPlatformPath* current = NULL;
    char directory[] = "/tmp/aether-\xE4\xB8\xAD\xE6\x96\x87-XXXXXX";
    int success = 0;

    if (setenv("HOME", "/home/synthetic", 1) != 0 ||
        setenv("TMPDIR", "/tmp/synthetic", 1) != 0 ||
        !expect_directory(AETHER_PLATFORM_DIRECTORY_TEMP, "/tmp/synthetic")) {
        return 1;
    }
#if defined(__APPLE__)
    if (!expect_directory(AETHER_PLATFORM_DIRECTORY_USER_DATA,
                          "/home/synthetic/Library/Application Support")) {
        return 1;
    }
#else
    if (setenv("XDG_DATA_HOME", "/data/synthetic", 1) != 0 ||
        !expect_directory(AETHER_PLATFORM_DIRECTORY_USER_DATA,
                          "/data/synthetic") ||
        setenv("XDG_DATA_HOME", "relative-ignored", 1) != 0 ||
        !expect_directory(AETHER_PLATFORM_DIRECTORY_USER_DATA,
                          "/home/synthetic/.local/share")) {
        return 1;
    }
#endif
    if (aether_platform_path_current(&original) != AETHER_STATUS_OK) {
        return 1;
    }
    if (mkdtemp(directory) == NULL) {
        aether_platform_path_destroy(original);
        return 1;
    }
    if (chdir(directory) == 0) {
        if (aether_platform_path_current(&current) == AETHER_STATUS_OK) {
            AetherStringView view = aether_platform_path_view(current);
            success = view.length == strlen(directory) &&
                      memcmp(view.data, directory, view.length) == 0;
        }
        aether_platform_path_destroy(current);
        current = NULL;
        if (rmdir(directory) != 0 ||
            aether_platform_path_current(&current) != AETHER_STATUS_NOT_FOUND ||
            current != NULL) {
            success = 0;
        }
    }
    if (chdir(aether_platform_path_view(original).data) != 0) {
        success = 0;
    }
    aether_platform_path_destroy(original);
    if (!success) {
        fprintf(stderr, "POSIX directory/error regression failed\n");
    }
    return success ? 0 : 1;
}
