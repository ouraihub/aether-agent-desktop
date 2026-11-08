#ifndef AETHER_INFRASTRUCTURE_PROCESS_H
#define AETHER_INFRASTRUCTURE_PROCESS_H
#include "aether/platform/process.h"
#include "aether/foundation/id.h"
#include "aether/foundation/trace.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherProcessService AetherProcessService;
typedef struct AetherProcessRef { AetherId id; uint64_t generation; } AetherProcessRef;
typedef struct AetherProcessWriteResult {
    AetherId write_id;
    AetherStatus status;
    size_t bytes_written;
} AetherProcessWriteResult;
typedef struct AetherProcessSnapshot {
    AetherProcessRef process;
    AetherTraceContext trace;
    bool borrowed;
    bool stop_requested;
    bool exited;
    uint32_t exit_code;
    bool stdout_eof;
    bool stderr_eof;
    bool close_complete;
    size_t stdout_buffered;
    size_t stderr_buffered;
    AetherStatus io_status;
} AetherProcessSnapshot;

/* Windows Process Service V1. Thread Safe except final destroy. Background
 * single pump owns native process operations; public functions only bounded
 * memory operations, no UI waits. spawn is synchronous OS creation and MUST
 * be invoked on a non-UI state/worker domain; caller policy must have passed
 * trusted actual executable/argv/cwd/environment validation immediately before.
 * This API is infrastructure, not a permission policy or filesystem sandbox.
 * max_processes1..4, per_stream_bytes1..128KiB, writes1..32/total<=1MiB,
 * max_write<=64KiB (fixed); limits only lower defaults. Native read scratch64KiB.
 * Input args/env copied into native launch before return, no caller retention.
 * Stream data raw privileged Adapter bytes, never auto-logged or UI-displayed.
 * Both stdout/stderr continuously pumped while capacity exists; full preserves
 * bytes and applies pipe backpressure, never silently drops protocol data.
 * Caller must drain for unblocked producer progress; buffers stay bounded.
 * write copies input and reserves queue/result bytes before acceptance; each
 * write result kept until take_write, actual byte count/error distinguishes
 * partial delivery. Queue full rejects, no inline write. close_stdin drains
 * accepted writes then sends EOF; no more writes accepted. Query exit does not
 * imply pipes drained. stop for owned process closes admission/requests stdin
 * EOF, after monotonic1s grace terminates owned Job, continues actual exit/pipe
 * cleanup. It is NOT cancellation of one shared Runtime request.
 * Borrowed PID observation has no IO/kill; stop PermissionDenied. close only
 * disconnects borrowed observation, never terminates external process.
 * close owned requests stop; close-complete only exited, both EOF and no pending
 * native write; buffered output remains drainable until release. release only
 * close-complete and write results consumed; data unread prevents release.
 * Service close requests all closes and pump exits only after cleanup; destroy
 * before is_closed AlreadyExists and retains resources, afterwards joins off UI.
 * Shutdown drops no pipe bytes: caller must keep draining output. No forced
 * free on deadlines, no retries of side effects. Handle identity/generation
 * match required, stale generation InvalidArgument, unknown NotFound.
 * All failures clear outputs. Non-Windows target is not built, no fake fallback.
 */
typedef struct AetherProcessLimits {
    size_t processes;
    size_t stream_bytes;
    size_t writes;
    size_t write_bytes;
} AetherProcessLimits;
AetherProcessLimits aether_process_default_limits(void);
AetherStatus aether_process_service_create(
    AetherProcessLimits limits, AetherProcessService** out_service
);
AetherStatus aether_process_spawn(
    AetherProcessService* service, AetherPlatformProcessInput input,
    AetherTraceContext trace, AetherProcessRef* out_process
);
AetherStatus aether_process_attach(
    AetherProcessService* service, uint32_t pid, AetherTraceContext trace,
    AetherProcessRef* out_process
);
AetherStatus aether_process_read(
    AetherProcessService* service, AetherProcessRef process, AetherProcessStream stream,
    unsigned char* bytes, size_t capacity, size_t* out_size
);
AetherStatus aether_process_write(
    AetherProcessService* service, AetherProcessRef process,
    const unsigned char* bytes, size_t size, AetherId* out_write
);
AetherStatus aether_process_take_write(
    AetherProcessService* service, AetherProcessRef process,
    AetherProcessWriteResult* out_result
);
AetherStatus aether_process_close_stdin(
    AetherProcessService* service, AetherProcessRef process
);
AetherStatus aether_process_stop(AetherProcessService* service, AetherProcessRef process);
AetherStatus aether_process_close(AetherProcessService* service, AetherProcessRef process);
AetherStatus aether_process_query(
    AetherProcessService* service, AetherProcessRef process, AetherProcessSnapshot* out_snapshot
);
AetherStatus aether_process_release(AetherProcessService* service, AetherProcessRef process);
AetherStatus aether_process_service_close(AetherProcessService* service);
bool aether_process_service_is_closed(AetherProcessService* service);
AetherStatus aether_process_service_destroy(AetherProcessService** service);
#ifdef __cplusplus
}
#endif
#endif
