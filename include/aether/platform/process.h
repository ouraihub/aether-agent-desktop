#ifndef AETHER_PLATFORM_PROCESS_H
#define AETHER_PLATFORM_PROCESS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherPlatformProcess AetherPlatformProcess;
typedef struct AetherProcessEnvironment {
    AetherStringView name;
    AetherStringView value;
} AetherProcessEnvironment;
typedef struct AetherPlatformProcessInput {
    AetherStringView executable; /* absolute UTF-8 .exe, no Shell/search PATH */
    const AetherStringView* arguments; /* excludes argv[0], max64 incl empty */
    size_t argument_count;
    AetherStringView working_directory; /* explicit absolute path */
    const AetherProcessEnvironment* environment; /* exact allowlist, max64 */
    size_t environment_count;
} AetherPlatformProcessInput;

typedef enum AetherProcessStream {
    AETHER_PROCESS_STDOUT = 0,
    AETHER_PROCESS_STDERR
} AetherProcessStream;

typedef struct AetherPlatformProcessSnapshot {
    bool borrowed;
    bool exited;
    uint32_t exit_code;
    bool stdout_eof;
    bool stderr_eof;
    bool owned_tree_exited;
    bool stdin_closed;
    bool write_pending;
    uint32_t native_error; /* internal diagnostic, never directly UI/log */
} AetherPlatformProcessSnapshot;

/* Windows native V1. Thread Compatible: serialize same-process calls.
 * spawn may block in OS; only background/owner execution, not UI callbacks.
 * UTF-8 explicit lengths, no NUL, empty argv entries/values valid; exact argv
 * quoting follows Windows CRT rules (not cmd.exe). Env not inherited, names
 * case-insensitive unique, no '=' or empty name, environment block sorted.
 * Single allowlisted inherited handle list includes only child stdin/out/err.
 * Managed process launched suspended, assigned owned kill-on-close Job before
 * resume; descendants kept in job, not a sandbox. Failure rolls back child,
 * pipes/job/handles, no half process output. Attach PID only borrows observation
 * rights and has no pipe/termination capability; closing never kills it.
 * read is nonblocking: available bytes/EOF, NotFound for none yet; output clears.
 * Only one write_begin pending (<=64KiB): caller buffer must remain alive through
 * write_poll completion/cancel/close; implementation uses overlapped stdin pipe.
 * write_poll reports actual delivered bytes and status, no fake all-written.
 * close_stdin only after pending write; repeated close safe. terminate kills
 * owned Job only, borrowed PermissionDenied; query exit independent of EOF.
 * destroy closes own handles and kills owned surviving Job; call only after
 * owner confirmed managed cleanup, never use as silent successful stop. No
 * callbacks/worker threads hidden here; Process Service pumps IO asynchronously.
 * InvalidInput/NotFound/PermissionDenied/OOM/IO preserve real native reason in
 * snapshot where object exists. No arguments/environment/path in public errors.
 */
AetherStatus aether_platform_process_spawn(
    AetherPlatformProcessInput input, AetherPlatformProcess** out_process
);
AetherStatus aether_platform_process_attach(
    uint32_t pid, AetherPlatformProcess** out_process
);
AetherStatus aether_platform_process_read(
    AetherPlatformProcess* process, AetherProcessStream stream,
    unsigned char* bytes, size_t capacity, size_t* out_size
);
AetherStatus aether_platform_process_write_begin(
    AetherPlatformProcess* process, const unsigned char* bytes, size_t size
);
AetherStatus aether_platform_process_write_poll(
    AetherPlatformProcess* process, size_t* out_written
);
AetherStatus aether_platform_process_close_stdin(AetherPlatformProcess* process);
AetherStatus aether_platform_process_terminate(AetherPlatformProcess* process);
AetherStatus aether_platform_process_query(
    AetherPlatformProcess* process, AetherPlatformProcessSnapshot* out_snapshot
);
void aether_platform_process_destroy(AetherPlatformProcess* process);
#ifdef __cplusplus
}
#endif
#endif
