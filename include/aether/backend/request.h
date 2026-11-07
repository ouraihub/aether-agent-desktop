#ifndef AETHER_BACKEND_REQUEST_H
#define AETHER_BACKEND_REQUEST_H

#include "aether/foundation/id.h"
#include "aether/foundation/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherBackend AetherBackend;

typedef enum AetherBackendRequestState {
    AETHER_BACKEND_REQUEST_ACCEPTED = 0,
    AETHER_BACKEND_REQUEST_SUCCEEDED,
    AETHER_BACKEND_REQUEST_FAILED,
    AETHER_BACKEND_REQUEST_CANCELLED
} AetherBackendRequestState;

typedef struct AetherBackendRequestSnapshot {
    AetherId request_id;
    AetherBackendRequestState state;
    AetherStatus result;
} AetherBackendRequestSnapshot;

/* V1 local request records, not a Runtime command execution API.
 * All calls on one Backend must be serialized by the caller.
 * Synchronous, no callbacks, no timeout; no borrowed payload is retained.
 * Submit requires a started Backend. Eight records are retained until
 * explicit release or Backend destruction, including terminal records.
 * Request IDs are process-local and valid only for their owning Backend.
 * Query copies a snapshot; an invalid ID in output means no result.
 * Valid output parameters are cleared even when a call fails.
 * Stop cancels Accepted records; query/release remain valid until destroy.
 */

AetherStatus aether_backend_submit_request(
    AetherBackend* backend,
    AetherId* out_request_id
);
AetherStatus aether_backend_query_request(
    const AetherBackend* backend,
    AetherId request_id,
    AetherBackendRequestSnapshot* out_snapshot
);
AetherStatus aether_backend_finish_request(
    AetherBackend* backend,
    AetherId request_id,
    AetherBackendRequestState terminal_state,
    AetherStatus result
);

/* Release terminal records only; Accepted returns INVALID_ARGUMENT.
 * Released/unknown IDs return NOT_FOUND. No slot is silently evicted.
 */
AetherStatus aether_backend_release_request(
    AetherBackend* backend,
    AetherId request_id
);

#ifdef __cplusplus
}
#endif

#endif
