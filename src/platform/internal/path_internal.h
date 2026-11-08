#ifndef AETHER_PLATFORM_PATH_INTERNAL_H
#define AETHER_PLATFORM_PATH_INTERNAL_H

#include "aether/platform/platform.h"

struct AetherPlatformPath {
    size_t length;
    char* data;
};

/* Private allocation; fill length bytes before publishing to the caller. */
AetherStatus aether_platform_path_allocate(
    size_t length,
    AetherPlatformPath** out_path
);

#endif
