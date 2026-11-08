#ifndef AETHER_PLATFORM_H
#define AETHER_PLATFORM_H

#include <stdint.h>

#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherPlatformPath AetherPlatformPath;

typedef struct AetherPlatformClock {
    uint64_t monotonic_ns;
    uint64_t wall_clock_ms;
} AetherPlatformClock;

AetherStatus aether_platform_clock_now(AetherPlatformClock* out_clock);
AetherStatus aether_platform_path_from_view(
    AetherStringView input,
    AetherPlatformPath** out_path
);
AetherStatus aether_platform_path_current(
    AetherPlatformPath** out_path
);
AetherStringView aether_platform_path_view(
    const AetherPlatformPath* path
);
void aether_platform_path_destroy(AetherPlatformPath* path);

#ifdef __cplusplus
}
#endif

#endif
