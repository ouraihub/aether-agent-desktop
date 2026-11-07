#include "aether/backend/backend.h"

#include <stdbool.h>
#include <stdlib.h>
struct AetherBackend {
    AetherCore* core;
    bool owns_core;
    bool started;
};

AetherStatus aether_backend_create(
    AetherCore* core,
    AetherBackend** out_backend
)
{
    AetherBackend* backend;

    if (core == NULL || out_backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    *out_backend = NULL;
    backend = (AetherBackend*)malloc(sizeof(*backend));
    if (backend == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }

    backend->core = core;
    backend->owns_core = false;
    backend->started = false;
    *out_backend = backend;
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_initialize(AetherBackend* backend)
{
    if (backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    return aether_core_initialize(backend->core);
}

AetherStatus aether_backend_start(AetherBackend* backend)
{
    AetherStatus status;

    if (backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (backend->started) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }

    status = aether_core_start(backend->core);
    if (status == AETHER_STATUS_OK) {
        backend->started = true;
    }
    return status;
}

AetherStatus aether_backend_stop(AetherBackend* backend)
{
    AetherStatus status;

    if (backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (!backend->started) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }

    status = aether_core_stop(backend->core);
    if (status == AETHER_STATUS_OK) {
        backend->started = false;
    }
    return status;
}

AetherStatus aether_backend_close(AetherBackend* backend)
{
    if (backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (backend->started) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }

    return aether_core_close(backend->core);
}

AetherCoreState aether_backend_get_core_state(
    const AetherBackend* backend
)
{
    if (backend == NULL) {
        return AETHER_CORE_STATE_CLOSED;
    }

    return aether_core_get_state(backend->core);
}

void aether_backend_destroy(AetherBackend* backend)
{
    if (backend == NULL) {
        return;
    }

    free(backend);
}
