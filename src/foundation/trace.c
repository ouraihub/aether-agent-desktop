#include "aether/foundation/trace.h"

#include <string.h>

static void aether_trace_clear(AetherTraceContext* context)
{
    if (context != NULL) {
        (void)memset(context, 0, sizeof(*context));
    }
}

static bool aether_trace_nonzero(const uint8_t* bytes, size_t size)
{
    size_t index;

    for (index = 0U; index < size; ++index) {
        if (bytes[index] != 0U) {
            return true;
        }
    }
    return false;
}

static AetherStatus aether_trace_random(
    AetherTraceRandomSource source,
    uint8_t* output,
    size_t size
)
{
    if (source.fill == NULL || output == NULL || size == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (source.fill(source.context, output, size) != AETHER_STATUS_OK) {
        return AETHER_STATUS_IO_ERROR;
    }
    if (!aether_trace_nonzero(output, size)) {
        return AETHER_STATUS_IO_ERROR;
    }
    return AETHER_STATUS_OK;
}

AetherStatus aether_trace_context_root(
    AetherTraceRandomSource source,
    AetherTraceContext* out_context
)
{
    AetherStatus status;

    if (out_context == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_trace_clear(out_context);
    status = aether_trace_random(source, out_context->trace_id.bytes, 16U);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    status = aether_trace_random(source, out_context->span_id.bytes, 8U);
    if (status != AETHER_STATUS_OK) {
        aether_trace_clear(out_context);
        return status;
    }
    out_context->has_parent = false;
    out_context->valid = true;
    return AETHER_STATUS_OK;
}

AetherStatus aether_trace_context_child(
    const AetherTraceContext* parent,
    AetherTraceRandomSource source,
    AetherTraceContext* out_context
)
{
    AetherStatus status;

    if (parent == NULL || out_context == NULL || !parent->valid) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_trace_clear(out_context);
    out_context->trace_id = parent->trace_id;
    out_context->parent_span_id = parent->span_id;
    status = aether_trace_random(source, out_context->span_id.bytes, 8U);
    if (status != AETHER_STATUS_OK) {
        aether_trace_clear(out_context);
        return status;
    }
    out_context->has_parent = true;
    out_context->valid = true;
    return AETHER_STATUS_OK;
}

AetherStatus aether_operation_begin(
    const AetherTraceContext* context,
    uint64_t started_ns,
    AetherOperation* out_operation
)
{
    if (context == NULL || out_operation == NULL || !context->valid) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_operation, 0, sizeof(*out_operation));
    out_operation->trace = *context;
    out_operation->started_ns = started_ns;
    out_operation->state = AETHER_OPERATION_ACTIVE;
    out_operation->result = AETHER_STATUS_OK;
    return AETHER_STATUS_OK;
}

AetherStatus aether_operation_finish(
    AetherOperation* operation,
    uint64_t ended_ns,
    AetherStatus result
)
{
    if (operation == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (operation->state != AETHER_OPERATION_ACTIVE) {
        return AETHER_STATUS_ALREADY_EXISTS;
    }
    operation->ended_ns = ended_ns;
    operation->clock_regressed = ended_ns < operation->started_ns;
    operation->duration_ns = operation->clock_regressed
        ? 0U
        : ended_ns - operation->started_ns;
    operation->result = result;
    operation->state = AETHER_OPERATION_FINISHED;
    return AETHER_STATUS_OK;
}

void aether_operation_dispose(AetherOperation* operation, uint64_t ended_ns)
{
    if (operation == NULL || operation->state != AETHER_OPERATION_ACTIVE) {
        return;
    }
    operation->ended_ns = ended_ns;
    operation->clock_regressed = ended_ns < operation->started_ns;
    operation->duration_ns = operation->clock_regressed
        ? 0U
        : ended_ns - operation->started_ns;
    operation->state = AETHER_OPERATION_UNFINISHED;
}
