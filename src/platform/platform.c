#include "aether/platform/platform.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <sys/time.h>
#include <unistd.h>
#endif

struct AetherPlatformPath {
    char* data;
    size_t length;
};

static AetherStatus copy_path(
    AetherStringView input,
    AetherPlatformPath** out_path
)
{
    AetherPlatformPath* path;
    AetherStatus status;

    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    status = aether_string_view_validate(input);
    if (status != AETHER_STATUS_OK || input.length == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    path = (AetherPlatformPath*)malloc(sizeof(*path));
    if (path == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    path->data = (char*)malloc(input.length + 1U);
    if (path->data == NULL) {
        free(path);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    (void)memcpy(path->data, input.data, input.length);
    path->data[input.length] = '\0';
    path->length = input.length;
    *out_path = path;
    return AETHER_STATUS_OK;
}

#if defined(_WIN32)
static AetherStatus monotonic_ns(uint64_t* out_value)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if (out_value == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (QueryPerformanceFrequency(&frequency) == 0 ||
        QueryPerformanceCounter(&counter) == 0) {
        return AETHER_STATUS_IO_ERROR;
    }
    {
        uint64_t ticks = (uint64_t)counter.QuadPart;
        uint64_t ticks_per_second = (uint64_t)frequency.QuadPart;
        uint64_t whole_seconds = ticks / ticks_per_second;
        uint64_t remainder = ticks % ticks_per_second;

        *out_value = whole_seconds * 1000000000ULL +
                     (remainder * 1000000000ULL) / ticks_per_second;
    }
    return AETHER_STATUS_OK;
}
#else
static AetherStatus monotonic_ns(uint64_t* out_value)
{
    struct timespec time_value;

    if (out_value == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &time_value) != 0) {
        return AETHER_STATUS_IO_ERROR;
    }
    *out_value = (uint64_t)time_value.tv_sec * 1000000000ULL +
                 (uint64_t)time_value.tv_nsec;
    return AETHER_STATUS_OK;
}
#endif

#if defined(_WIN32)
static AetherStatus wall_clock_ms(uint64_t* out_value)
{
    FILETIME file_time;
    ULARGE_INTEGER value;

    if (out_value == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    GetSystemTimeAsFileTime(&file_time);
    value.LowPart = file_time.dwLowDateTime;
    value.HighPart = file_time.dwHighDateTime;
    *out_value = (uint64_t)(value.QuadPart / 10000ULL);
    return AETHER_STATUS_OK;
}
#else
static AetherStatus wall_clock_ms(uint64_t* out_value)
{
    struct timeval time_value;

    if (out_value == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (gettimeofday(&time_value, NULL) != 0) {
        return AETHER_STATUS_IO_ERROR;
    }
    *out_value = (uint64_t)time_value.tv_sec * 1000ULL +
                 (uint64_t)time_value.tv_usec / 1000ULL;
    return AETHER_STATUS_OK;
}
#endif

AetherStatus aether_platform_clock_now(AetherPlatformClock* out_clock)
{
    AetherStatus status;

    if (out_clock == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_clock = (AetherPlatformClock){0};
    status = monotonic_ns(&out_clock->monotonic_ns);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    return wall_clock_ms(&out_clock->wall_clock_ms);
}

AetherStatus aether_platform_path_from_view(
    AetherStringView input,
    AetherPlatformPath** out_path
)
{
    return copy_path(input, out_path);
}

AetherStatus aether_platform_path_current(AetherPlatformPath** out_path)
{
    char buffer[4096];
    size_t length;

#if defined(_WIN32)
    DWORD result = GetCurrentDirectoryA((DWORD)sizeof(buffer), buffer);
    if (result == 0U || result >= sizeof(buffer)) {
        if (out_path != NULL) {
            *out_path = NULL;
        }
        return result == 0U
                   ? AETHER_STATUS_IO_ERROR
                   : AETHER_STATUS_OUT_OF_MEMORY;
    }
    length = (size_t)result;
#else
    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (getcwd(buffer, sizeof(buffer)) == NULL) {
        *out_path = NULL;
        return AETHER_STATUS_IO_ERROR;
    }
    length = strlen(buffer);
#endif
    return copy_path((AetherStringView){buffer, length}, out_path);
}

AetherStringView aether_platform_path_view(const AetherPlatformPath* path)
{
    if (path == NULL) {
        return (AetherStringView){NULL, 0U};
    }
    return (AetherStringView){path->data, path->length};
}

void aether_platform_path_destroy(AetherPlatformPath* path)
{
    if (path == NULL) {
        return;
    }
    free(path->data);
    free(path);
}
