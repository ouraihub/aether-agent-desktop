/* Allocator substitution is private to this test target, never production. */
#undef malloc
#undef calloc
#undef free
#include "aether/core/runtime_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t allocations;
static size_t failing;
static size_t live;

void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);

void* aether_test_malloc(size_t size)
{
    ++allocations;
    if (allocations == failing) {
        return NULL;
    }
    void* pointer = malloc(size);
    if (pointer != NULL) {
        ++live;
    }
    return pointer;
}

void* aether_test_calloc(size_t count, size_t size)
{
    if (count > SIZE_MAX / size) {
        return NULL;
    }
    void* pointer = aether_test_malloc(count * size);
    if (pointer != NULL) {
        memset(pointer, 0, count * size);
    }
    return pointer;
}

void aether_test_free(void* pointer)
{
    if (pointer != NULL) {
        --live;
    }
    free(pointer);
}

static int exercise(size_t failure)
{
    AetherRuntimeRegistry* registry = NULL;
    AetherProviderId provider;
    AetherRuntimeId runtime;
    AetherRuntimeRef* reference = NULL;
    AetherStringView name = {"test-only-provider", 18U};
    AetherStringView version = {"1", 1U};
    allocations = 0U;
    failing = failure;
    AetherStatus status = aether_runtime_registry_create(1U, 1U, &registry);
    if (failure <= 3U) {
        return status == AETHER_STATUS_OUT_OF_MEMORY &&
               registry == NULL && live == 0U;
    }
    if (status != AETHER_STATUS_OK ||
        aether_runtime_provider_register(registry, name, version, &provider) !=
            AETHER_STATUS_OK ||
        aether_runtime_register(registry, provider, &runtime) !=
            AETHER_STATUS_OK ||
        aether_runtime_update(registry, runtime, 1U, AETHER_RUNTIME_READY,
                              (AetherRuntimeCapabilities){0}) !=
            AETHER_STATUS_OK) {
        return 0;
    }
    status = aether_runtime_acquire(registry, runtime, 1U, &reference);
    if (failure == 4U &&
        (status != AETHER_STATUS_OUT_OF_MEMORY || reference != NULL)) {
        return 0;
    }
    if (failure > 4U && status != AETHER_STATUS_OK) {
        return 0;
    }
    aether_runtime_ref_release(&reference);
    if (aether_runtime_begin_unregister(registry, runtime) != AETHER_STATUS_OK ||
        aether_runtime_finish_unregister(registry, runtime) != AETHER_STATUS_OK ||
        aether_runtime_registry_destroy(&registry) != AETHER_STATUS_OK) {
        return 0;
    }
    return registry == NULL && reference == NULL && live == 0U;
}

int main(void)
{
    for (size_t failure = 1U; failure <= 5U; ++failure) {
        if (!exercise(failure)) {
            fprintf(stderr, "Registry allocation rollback failed: %zu\n",
                    failure);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
