#include "aether/platform/platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

AetherStatus aether_platform_clock_now(AetherPlatformClock* out_clock)
{
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    FILETIME file_time;
    ULARGE_INTEGER wall;
    const uint64_t billion = UINT64_C(1000000000);
    const uint64_t unix_epoch = UINT64_C(116444736000000000);
    uint64_t ticks;
    uint64_t rate;
    uint64_t seconds;
    uint64_t fraction;

    if (out_clock == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_clock = (AetherPlatformClock){0};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 ||
        !QueryPerformanceCounter(&counter) || counter.QuadPart < 0) {
        return AETHER_STATUS_IO_ERROR;
    }
    ticks = (uint64_t)counter.QuadPart;
    rate = (uint64_t)frequency.QuadPart;
    seconds = ticks / rate;
    /* Divide before scaling the uptime; scale only the sub-second residue. */
    fraction = (uint64_t)((long double)(ticks % rate) *
                          (long double)billion / (long double)rate);
    if (fraction >= billion) {
        fraction = billion - 1U;
    }
    if (seconds > (UINT64_MAX - fraction) / billion) {
        return AETHER_STATUS_IO_ERROR;
    }
    GetSystemTimeAsFileTime(&file_time);
    wall.LowPart = file_time.dwLowDateTime;
    wall.HighPart = file_time.dwHighDateTime;
    if (wall.QuadPart < unix_epoch) {
        return AETHER_STATUS_IO_ERROR;
    }
    out_clock->monotonic_ns = seconds * billion + fraction;
    out_clock->wall_clock_ms = (wall.QuadPart - unix_epoch) / 10000U;
    return AETHER_STATUS_OK;
}
