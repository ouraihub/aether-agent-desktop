#define _POSIX_C_SOURCE 200809L
#include "aether/platform/platform.h"

#include <time.h>

AetherStatus aether_platform_clock_now(AetherPlatformClock* out_clock)
{
    struct timespec monotonic;
    struct timespec wall;
    const uint64_t billion = UINT64_C(1000000000);

    if (out_clock == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_clock = (AetherPlatformClock){0};
    if (clock_gettime(CLOCK_MONOTONIC, &monotonic) != 0 ||
        clock_gettime(CLOCK_REALTIME, &wall) != 0 ||
        monotonic.tv_sec < 0 || wall.tv_sec < 0 ||
        monotonic.tv_nsec < 0 || monotonic.tv_nsec >= 1000000000L ||
        wall.tv_nsec < 0 || wall.tv_nsec >= 1000000000L ||
        (uint64_t)monotonic.tv_sec >
            (UINT64_MAX - (uint64_t)monotonic.tv_nsec) / billion ||
        (uint64_t)wall.tv_sec >
            (UINT64_MAX - (uint64_t)wall.tv_nsec / 1000000U) / 1000U) {
        return AETHER_STATUS_IO_ERROR;
    }
    out_clock->monotonic_ns = (uint64_t)monotonic.tv_sec * billion +
                              (uint64_t)monotonic.tv_nsec;
    out_clock->wall_clock_ms = (uint64_t)wall.tv_sec * 1000U +
                               (uint64_t)wall.tv_nsec / 1000000U;
    return AETHER_STATUS_OK;
}
