#include "aether/platform/platform.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    AetherPlatformClock first_clock;
    AetherPlatformClock second_clock;
    AetherPlatformPath* path = NULL;
    AetherPlatformPath* current = NULL;
    const char unicode[] = "C:\\workspace\\\xE4\xB8\xAD\xE6\x96\x87";
    AetherStringView view;

    assert(aether_platform_clock_now(&first_clock) == AETHER_STATUS_OK);
    assert(aether_platform_clock_now(&second_clock) == AETHER_STATUS_OK);
    assert(second_clock.monotonic_ns >= first_clock.monotonic_ns);
    assert(second_clock.wall_clock_ms >= first_clock.wall_clock_ms);
    assert(aether_platform_clock_now(NULL) == AETHER_STATUS_INVALID_ARGUMENT);

    assert(aether_platform_path_from_view(
               (AetherStringView){unicode, sizeof(unicode) - 1U},
               &path) == AETHER_STATUS_OK);
    view = aether_platform_path_view(path);
    assert(view.length == sizeof(unicode) - 1U);
    assert(memcmp(view.data, unicode, view.length) == 0);
    aether_platform_path_destroy(path);
    aether_platform_path_destroy(NULL);

    assert(aether_platform_path_from_view(
               (AetherStringView){NULL, 1U}, &path) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_platform_path_current(&current) == AETHER_STATUS_OK);
    assert(aether_platform_path_view(current).length > 0U);
    aether_platform_path_destroy(current);
    assert(aether_platform_path_current(NULL) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    return 0;
}
