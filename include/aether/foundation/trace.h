#ifndef AETHER_FOUNDATION_TRACE_H
#define AETHER_FOUNDATION_TRACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aether/foundation/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherTraceId {
    uint8_t bytes[16];
} AetherTraceId;

typedef struct AetherSpanId {
    uint8_t bytes[8];
} AetherSpanId;

typedef AetherStatus (*AetherTraceRandomFn)(
    void* context,
    uint8_t* output,
    size_t size
);

typedef struct AetherTraceRandomSource {
    AetherTraceRandomFn fill;
    void* context;
} AetherTraceRandomSource;

typedef struct AetherTraceContext {
    AetherTraceId trace_id;
    AetherSpanId span_id;
    AetherSpanId parent_span_id;
    bool has_parent;
    bool valid;
} AetherTraceContext;

typedef enum AetherOperationState {
    AETHER_OPERATION_ACTIVE = 0,
    AETHER_OPERATION_FINISHED,
    AETHER_OPERATION_UNFINISHED
} AetherOperationState;

typedef struct AetherOperation {
    AetherTraceContext trace;
    uint64_t started_ns;
    uint64_t ended_ns;
    uint64_t duration_ns;
    AetherStatus result;
    AetherOperationState state;
    bool clock_regressed;
} AetherOperation;

/*
 * Creates a root context from a caller-owned random source.
 * The source must fill exactly size bytes or return an error. No fallback
 * identity is generated on failure. The output is cleared before use.
 */
AetherStatus aether_trace_context_root(
    AetherTraceRandomSource source,
    AetherTraceContext* out_context
);

/* Creates a child span with the same trace ID and a new span ID. */
AetherStatus aether_trace_context_child(
    const AetherTraceContext* parent,
    AetherTraceRandomSource source,
    AetherTraceContext* out_context
);

/* Begins a non-allocating operation using a monotonic timestamp. */
AetherStatus aether_operation_begin(
    const AetherTraceContext* context,
    uint64_t started_ns,
    AetherOperation* out_operation
);

/* Finishes once. A repeated finish returns ALREADY_EXISTS. */
AetherStatus aether_operation_finish(
    AetherOperation* operation,
    uint64_t ended_ns,
    AetherStatus result
);

/* Marks an active operation unfinished during owner cleanup. */
void aether_operation_dispose(
    AetherOperation* operation,
    uint64_t ended_ns
);

#ifdef __cplusplus
}
#endif

#endif
