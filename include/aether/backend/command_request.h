#ifndef AETHER_BACKEND_COMMAND_REQUEST_H
#define AETHER_BACKEND_COMMAND_REQUEST_H
#include "aether/backend/state_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AetherCommandRequestState {
    AETHER_COMMAND_ACCEPTED = 0,
    AETHER_COMMAND_RUNNING,
    AETHER_COMMAND_SUCCEEDED,
    AETHER_COMMAND_FAILED,
    AETHER_COMMAND_CANCELLED,
    AETHER_COMMAND_TIMED_OUT
} AetherCommandRequestState;

typedef struct AetherCommandRequestSnapshot {
    AetherId request_id;
    AetherAsyncQueryKind kind;
    AetherCommandRequestState state;
    uint64_t receiver_generation;
    uint64_t deadline_ns;
    uint64_t retain_until_ns;
    uint64_t terminal_revision; /* 0 until first terminal commit, then1 forever. */
    bool cancel_requested;
    bool execution_started;
    bool execution_complete;
    AetherStatus execution_status;
    AetherStatus result;
    AetherQueryError error;
    AetherTraceContext trace;
    union { AetherWorkspaceDto workspace; AetherSessionDto session; } value;
} AetherCommandRequestSnapshot;

/* Local state-command request lifecycle V1, still no Store/Runtime side effect.
 * Existing async rename/Session request-close create retained records too;
 * work ID equals their local request ID but is never a Session/Trace identity.
 * Default deadline10min from acceptance; typed variants below accept relative
 * timeout_ns (nonzero, <=10min) for deterministic short-deadline consumers.
 * Up to128 records, <1MiB fixed metadata; accepted command input remains copied
 * in bounded ingress. Taking the common work result does NOT release history.
 * Terminal kept10min or explicit release; expiry only reclaims when execution
 * complete and its one completion fact has been taken. Querying a record after
 * deadline commits TimedOut even if work is still executing; no early free.
 * Dispatch/finish recheck monotonic now, now>=deadline wins over success, but
 * execution_started/complete/status and typed actual result remain separately
 * visible. Late results never change terminal_revision/state or emit twice.
 * cancel_request requires its own trusted write context and target scope/caller;
 * it records intent, not success of stopping. Before-dispatch cancellation
 * prevents mutation; already-running mutation can succeed. Unknown NotFound;
 * read-only caller PermissionDenied. Query/release/completion validate the
 * original caller identity and readable workspace; trace grants no authority.
 * Completion channel is a bounded authoritative RequestCompleted fact, polled
 * with take_completed: one snapshot per terminal request (not a UI callback).
 * Releasing terminal before completion consumed or execution returned is
 * AlreadyExists. Returned snapshots are independent values. No implicit retries.
 * This implements the local state slice, not APP-07 Runtime/permission/persist
 * orchestration. Platform clock failure explicitly fails admission/operation.
 */
AetherStatus aether_backend_submit_rename_request(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherWorkspaceRenameInput input, uint64_t receiver_generation,
    uint64_t timeout_ns, AetherId* out_request
);
AetherStatus aether_backend_submit_close_request(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherSessionCloseInput input, uint64_t receiver_generation,
    uint64_t timeout_ns, AetherId* out_request
);
AetherStatus aether_backend_query_command_request(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherId request, AetherCommandRequestSnapshot* out_snapshot,
    AetherQueryError* out_error
);
AetherStatus aether_backend_cancel_command_request(
    AetherAsyncQueries* client, const AetherStateCommandContext* context,
    AetherId request, AetherCommandRequestSnapshot* out_snapshot,
    AetherQueryError* out_error
);
AetherStatus aether_backend_take_command_completed(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherCommandRequestSnapshot* out_snapshot, AetherQueryError* out_error
);
AetherStatus aether_backend_release_command_request(
    AetherAsyncQueries* client, const AetherRequestContext* context,
    AetherId request, AetherQueryError* out_error
);
#ifdef __cplusplus
}
#endif
#endif
