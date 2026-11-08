#include "aether/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, \
                #expression); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(int argc, char** argv)
{
    AetherPlatformClock first;
    AetherPlatformClock second;
    AetherPlatformPath* path = NULL;
    AetherStringView view;
    char input[] = "C:/\xE4\xB8\xAD\xE6\x96\x87";
    const char invalid[] = "\xED\xA0\x80";
    const char embedded_nul[] = { 'a', '\0', 'b' };
    time_t before = time(NULL);

    CHECK(aether_platform_clock_now(&first) == AETHER_STATUS_OK);
    CHECK(aether_platform_clock_now(&second) == AETHER_STATUS_OK);
    time_t after = time(NULL);
    CHECK(before >= 0 && after >= before);
    CHECK(first.wall_clock_ms >= (uint64_t)before * 1000U);
    CHECK(first.wall_clock_ms < ((uint64_t)after + 1U) * 1000U);
    CHECK(second.monotonic_ns >= first.monotonic_ns);
    CHECK(aether_platform_clock_now(NULL) == AETHER_STATUS_INVALID_ARGUMENT);

    CHECK(aether_platform_path_from_view(
        (AetherStringView){input, sizeof(input) - 1U}, &path) ==
        AETHER_STATUS_OK);
    view = aether_platform_path_view(path);
    input[0] = 'D';
    CHECK(view.data[0] == 'C');
    CHECK(view.data[view.length] == '\0');
    CHECK(aether_string_view_validate(view) == AETHER_STATUS_OK);
    aether_platform_path_destroy(path);
    path = NULL;
    aether_platform_path_destroy(NULL);

    CHECK(aether_platform_path_from_view(
        (AetherStringView){embedded_nul, sizeof(embedded_nul)}, &path) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(path == NULL);
    CHECK(aether_platform_path_from_view(
        (AetherStringView){invalid, sizeof(invalid) - 1U}, &path) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(path == NULL);
    CHECK(aether_platform_path_from_view(
        (AetherStringView){input, SIZE_MAX}, &path) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(path == NULL);
    CHECK(aether_platform_path_from_view(
        (AetherStringView){NULL, 0U}, &path) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_platform_path_current(NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_platform_path_system((AetherPlatformDirectory)99, &path) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(path == NULL);
    CHECK(aether_platform_path_system(AETHER_PLATFORM_DIRECTORY_TEMP, NULL) ==
          AETHER_STATUS_INVALID_ARGUMENT);

    CHECK(aether_platform_path_current(&path) == AETHER_STATUS_OK);
    view = aether_platform_path_view(path);
    CHECK(aether_string_view_validate(view) == AETHER_STATUS_OK);
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) {
        printf("cwd=%.*s\n", (int)view.length, view.data);
        printf("wall_clock_ms=%llu unix_ms=%llu monotonic_delta_ns=%llu\n",
               (unsigned long long)first.wall_clock_ms,
               (unsigned long long)before * 1000ULL,
               (unsigned long long)(second.monotonic_ns - first.monotonic_ns));
    }
    aether_platform_path_destroy(path);
    return EXIT_SUCCESS;
}
