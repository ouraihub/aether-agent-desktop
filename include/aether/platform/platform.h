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

typedef enum AetherPlatformDirectory {
    AETHER_PLATFORM_DIRECTORY_TEMP = 0,
    AETHER_PLATFORM_DIRECTORY_USER_DATA
} AetherPlatformDirectory;

/* V1 synchronous platform values; no callbacks, cancellation or deadline.
 * Clock reads are thread safe. Fields are sampled separately, not atomically.
 * monotonic_ns: arbitrary local origin, only for elapsed time/deadlines.
 * wall_clock_ms: milliseconds since 1970-01-01 UTC; may move backwards.
 * Clock failure clears both fields; invalid output returns INVALID_ARGUMENT.
 * Native clock failures/range errors return IO_ERROR.
 *
 * Path constructors return independent owned objects; failures clear output.
 * Input is borrowed for the call: nonempty UTF-8, explicit byte length,
 * no embedded NUL; invalid encoding/length returns INVALID_ARGUMENT.
 * Allocation failure returns OUT_OF_MEMORY. Paths are values, not access
 * grants: no existence, canonicalization, reserved-name or sandbox check.
 * view borrows immutable NUL-terminated bytes until destroy; NULL gives empty.
 * destroy(NULL) is safe; never destroy an already freed non-NULL pointer.
 * Independent paths/readers are thread safe; caller synchronizes destroy.
 * Current/system directory queries may block in the OS; not for UI callbacks.
 * Caller must serialize mutations of process cwd/environment against queries.
 * Queries return NOT_FOUND/PERMISSION_DENIED/IO_ERROR for native failures,
 * INVALID_ARGUMENT for invalid directory kinds or non-UTF-8 POSIX paths.
 * TEMP uses the OS temp location; USER_DATA is LocalAppData on Windows,
 * ~/Library/Application Support on macOS, XDG_DATA_HOME or ~/.local/share
 * on Linux. Queries do not create directories or append a product name.
 */

AetherStatus aether_platform_clock_now(AetherPlatformClock* out_clock);
AetherStatus aether_platform_path_from_view(
    AetherStringView input,
    AetherPlatformPath** out_path
);
AetherStatus aether_platform_path_current(
    AetherPlatformPath** out_path
);
AetherStatus aether_platform_path_system(
    AetherPlatformDirectory directory,
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
