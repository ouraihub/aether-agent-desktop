#include "aether/foundation/trace.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct TraceFixture {
    uint8_t next;
    bool fail;
    bool zero;
} TraceFixture;

static AetherStatus trace_fill(
    void* context,
    uint8_t* output,
    size_t size
)
{
    TraceFixture* fixture = context;
    size_t index;

    if (fixture->fail) {
        return AETHER_STATUS_IO_ERROR;
    }
    for (index = 0U; index < size; ++index) {
        output[index] = fixture->zero ? 0U : fixture->next++;
    }
    return AETHER_STATUS_OK;
}

int main(void)
{
    TraceFixture fixture = {1U, false, false};
    AetherTraceRandomSource source = {trace_fill, &fixture};
    AetherTraceContext root;
    AetherTraceContext child;
    AetherOperation operation;

    assert(aether_trace_context_root(source, &root) == AETHER_STATUS_OK);
    assert(root.valid);
    assert(!root.has_parent);
    assert(aether_trace_context_child(&root, source, &child) == AETHER_STATUS_OK);
    assert(child.valid);
    assert(child.has_parent);
    assert(memcmp(root.trace_id.bytes, child.trace_id.bytes, 16U) == 0);
    assert(memcmp(root.span_id.bytes, child.parent_span_id.bytes, 8U) == 0);
    assert(memcmp(root.span_id.bytes, child.span_id.bytes, 8U) != 0);

    assert(aether_operation_begin(&child, 100U, &operation) == AETHER_STATUS_OK);
    assert(aether_operation_finish(
               &operation, 145U, AETHER_STATUS_CANCELLED) == AETHER_STATUS_OK);
    assert(operation.state == AETHER_OPERATION_FINISHED);
    assert(operation.duration_ns == 45U);
    assert(!operation.clock_regressed);
    assert(aether_operation_finish(
               &operation, 200U, AETHER_STATUS_OK) ==
           AETHER_STATUS_ALREADY_EXISTS);

    assert(aether_operation_begin(&root, 200U, &operation) == AETHER_STATUS_OK);
    aether_operation_dispose(&operation, 190U);
    assert(operation.state == AETHER_OPERATION_UNFINISHED);
    assert(operation.duration_ns == 0U);
    assert(operation.clock_regressed);

    fixture.zero = true;
    assert(aether_trace_context_root(source, &root) == AETHER_STATUS_IO_ERROR);
    assert(!root.valid);
    fixture.zero = false;
    fixture.fail = true;
    assert(aether_trace_context_root(source, &root) == AETHER_STATUS_IO_ERROR);
    assert(!root.valid);
    assert(aether_trace_context_child(&root, source, &child) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    return 0;
}
