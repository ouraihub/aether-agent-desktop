/* Allocator substitution applies only to Process Service sources in this target. */
#undef malloc
#undef calloc
#undef free
#include "aether/infrastructure/process.h"
#include "aether/platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "process allocation:%d\n", __LINE__); \
    exit(EXIT_FAILURE); } } while (0)
#define OK(x) CHECK((x) == AETHER_STATUS_OK)
static size_t calls;
static size_t failing;
static size_t live;
void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);
void* aether_test_malloc(size_t size)
{
    void* pointer;
    if (++calls == failing) return NULL;
    pointer = malloc(size);
    if (pointer != NULL) ++live;
    return pointer;
}
void* aether_test_calloc(size_t count, size_t size)
{
    void* pointer;
    if (size != 0U && count > SIZE_MAX / size) return NULL;
    pointer = aether_test_malloc(count * size);
    if (pointer != NULL) memset(pointer, 0, count * size);
    return pointer;
}
void aether_test_free(void* pointer)
{
    if (pointer != NULL) { CHECK(live != 0U); --live; }
    free(pointer);
}
int main(void)
{
    AetherProcessService* service = NULL;
    AetherProcessLimits limits = {1U, 127U, 1U, 1024U};
    AetherTraceContext trace = {0};
    AetherPlatformProcessInput input = {0};
    AetherProcessRef process;
    trace.valid = true;
    trace.trace_id.bytes[0] = 1U;
    trace.span_id.bytes[0] = 2U;
    for (size_t failure = 1U; failure <= 3U; ++failure) {
        calls = 0U;
        failing = failure;
        CHECK(aether_process_service_create(limits, &service) == AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(service == NULL && live == 0U);
    }
    failing = 0U;
    OK(aether_process_service_create(limits, &service));
    CHECK(live == 3U);
    /* Input is deliberately invalid; only pre-launch buffer failures are injected. */
    for (size_t failure = 1U; failure <= 1U; ++failure) {
        size_t baseline = calls;
        failing = baseline + failure;
        CHECK(aether_process_spawn(service, input, trace, &process) ==
              AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(process.id.value == 0U && live == 3U);
    }
    failing = 0U;
    CHECK(aether_process_spawn(service, input, trace, &process) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(process.id.value == 0U && live == 3U);
    OK(aether_process_service_close(service));
    while (!aether_process_service_is_closed(service)) { }
    OK(aether_process_service_destroy(&service));
    CHECK(live == 0U);
    return 0;
}
