#ifndef AETHER_BACKEND_VIEW_H
#define AETHER_BACKEND_VIEW_H
#include "aether/backend/query.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AetherViewKind {
    AETHER_VIEW_WORKSPACE = 0,
    AETHER_VIEW_SESSION
} AetherViewKind;

typedef struct AetherViewSnapshot {
    AetherId subscription_id;
    AetherViewKind kind;
    uint64_t base_revision;
    uint64_t sequence; /* Always zero on OpenView. */
    union { AetherWorkspaceDto workspace; AetherSessionDto session; } value;
} AetherViewSnapshot;

typedef struct AetherViewEvent {
    uint32_t schema_version;
    AetherId event_id;
    AetherId subscription_id;
    AetherViewKind kind;
    AetherId object_id;
    AetherId workspace_id;
    AetherId session_id; /* Zero for Workspace change. */
    AetherId runtime_id; /* Zero when not applicable. */
    uint64_t sequence;
    uint64_t revision;
    uint64_t timestamp_ms; /* Unix display time, never ordering. */
    AetherTraceContext trace;
    union { AetherWorkspaceDto workspace; AetherSessionDto session; } value;
} AetherViewEvent;

typedef enum AetherViewHealth {
    AETHER_VIEW_HEALTHY = 0,
    AETHER_VIEW_RESYNC_REQUIRED,
    AETHER_VIEW_CLOSING,
    AETHER_VIEW_CLOSED
} AetherViewHealth;

typedef void (*AetherViewEventFn)(void* context, const AetherViewEvent* event);
typedef void (*AetherViewClosedFn)(void* context, AetherId subscription_id);

typedef struct AetherViewReceiver {
    AetherViewEventFn event;
    AetherViewClosedFn closed;
    void* context;
} AetherViewReceiver;

/* Actual local Workspace/Session snapshots and state-change events, Schema V1.
 * Not Message/Runtime event coverage or the full APP-08 aggregate. No wire
 * structures or Core pointers cross the boundary. Input scope authorized by
 * the same Query contract; immutable snapshot and subscription established in
 * one serialized owner operation, then sequence1.. for later changes.
 * Event callback may precede OpenView result: receiver must already be alive,
 * buffer bounded events until snapshot installed and validate subscription ID.
 * Callbacks serial on a dedicated non-UI delivery domain, no state/Client lock
 * held. Payload borrowed until return; UI queue must copy value and check page
 * receiver generation. Callback may submit work or unsubscribe but MUST NOT
 * wait for itself, join/destroy Client or synchronously call state-owner APIs.
 * Only actual mutations publish; same-revision no-op does not emit an event.
 * Per-view queue configurable1..256, actual bytes <=2MiB; at most16 views.
 * Overflow/fact creation failure makes matching views ResyncRequired, drops
 * queued data, never rolls back source mutation or pretends stream complete.
 * Health query is independent of queue. Reopen takes a new baseline/identity;
 * does not replay commands. Polling health is not a business event substitute.
 * unsubscribe closes callback-entry gate, drops queued data; entered callback
 * may finish. closed callback exactly once after no event callback remains;
 * receiver lives through its return. Health CLOSED only after closed callback
 * returns; release then reclaims the bounded slot. Before CLOSED AlreadyExists.
 * Unknown/released identities NotFound; repeated unsubscribe is idempotent.
 * Client close unsubscribes all and is_closed waits for callbacks too, no early
 * free on timeout. Caller excludes callback contexts from destruction until
 * close-complete and its own UI queue references are released.
 */
#ifdef __cplusplus
}
#endif
#endif
