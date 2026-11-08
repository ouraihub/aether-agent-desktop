#ifndef AETHER_BACKEND_ASYNC_QUERY_H
#define AETHER_BACKEND_ASYNC_QUERY_H
#include "aether/backend/query.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherAsyncQueries AetherAsyncQueries;
typedef enum AetherAsyncQueryKind {
    AETHER_ASYNC_QUERY_WORKSPACE = 0,
    AETHER_ASYNC_QUERY_SESSION,
    AETHER_ASYNC_QUERY_RUNTIME,
    AETHER_ASYNC_QUERY_MESSAGE,
    AETHER_ASYNC_QUERY_BLOCK,
    AETHER_ASYNC_COMMAND_RENAME_WORKSPACE,
    AETHER_ASYNC_COMMAND_CLOSE_SESSION
} AetherAsyncQueryKind;

typedef struct AetherAsyncQueryInput {
    AetherAsyncQueryKind kind;
    AetherRequestContext context;
    AetherId id;
    AetherId session_id; /* Runtime query only */
    uint32_t ordinal;   /* Block query only */
    size_t max_bytes;   /* Block query only, 1..1MiB */
    uint64_t receiver_generation;
} AetherAsyncQueryInput;

typedef struct AetherAsyncQueryResult {
    AetherId query_id;
    uint64_t receiver_generation;
    AetherAsyncQueryKind kind;
    AetherStatus status;
    AetherQueryError error;
    union {
        AetherWorkspaceDto workspace;
        AetherSessionDto session;
        AetherRuntimeDto runtime;
        AetherMessageDto message;
        AetherBlockDto* block;
    } value;
} AetherAsyncQueryResult;

/* Query Client V1 with explicit local-state command extensions (state_command.h).
 * Not a durable business Command/RequestRecord or subscription.
 * create/destroy are composition-root/non-UI operations. Borrows queries and
 * all its registries EXCLUSIVELY while live: single background worker becomes
 * their state owner, no concurrent direct domain mutations/queries permitted.
 * That restriction is explicit; this does not retrofit Core thread safety.
 * Frontend uses only submit/cancel/take/request_close/is_closed on this handle.
 * Thread Safe except destroy, caller excludes concurrent calls when destroying.
 * submit copies fixed input/context, returns a unique query ID on acceptance,
 * no user callbacks, no blocking IO or inline query; entry errors no result.
 * At most capacity (1..32) retained in-flight/results. result_bytes (1..4MiB)
 * reserves exact fixed result + maximum block response per accepted query;
 * full count/bytes rejects OutOfMemory, no drop, eviction or invisible retries.
 * take transfers one result; release with result_dispose, copies of an owning
 * block result must not be disposed twice. Result stays valid after Client and
 * Core destruction. Failing take clears output. NotFound means none ready.
 * Cancel matches ID+receiver_generation; old generation InvalidArgument,
 * unknown NotFound, completed AlreadyExists. Before-start cancellation yields
 * one Cancelled result without running query. In-progress query may finish;
 * cancellation is checked before/after mapping, but completion may win a race.
 * A page must still discard mismatched receiver generations and dispose payload.
 * This wait cancellation does not cancel any underlying business command.
 * request_close stops admission and requests cancellation without waiting;
 * is_closed means no worker callback remains, completed results may be taken.
 * destroy before close-complete AlreadyExists, no free; afterwards join on
 * non-UI owner and discard unclaimed results. Does not destroy borrowed queries.
 * No implicit timeout/retry; caller can stop its wait with cancel. API/version/
 * authorization errors may be asynchronously delivered after acceptance; input
 * shape, budget and valid trace checked synchronously. Same query IDs never reused.
 */
AetherStatus aether_backend_async_queries_create(
    AetherBackendQueries* queries, size_t capacity, size_t result_bytes,
    AetherAsyncQueries** out_client
);
AetherStatus aether_backend_async_query_submit(
    AetherAsyncQueries* client, AetherAsyncQueryInput input, AetherId* out_query
);
AetherStatus aether_backend_async_query_cancel(
    AetherAsyncQueries* client, AetherId query, uint64_t receiver_generation
);
AetherStatus aether_backend_async_query_take(
    AetherAsyncQueries* client, AetherAsyncQueryResult* out_result
);
void aether_backend_async_query_result_dispose(AetherAsyncQueryResult* result);
AetherStatus aether_backend_async_queries_request_close(AetherAsyncQueries* client);
bool aether_backend_async_queries_is_closed(AetherAsyncQueries* client);
AetherStatus aether_backend_async_queries_destroy(AetherAsyncQueries** client);

#ifdef __cplusplus
}
#endif
#endif
