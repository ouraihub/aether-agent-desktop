#include "backend_internal.h"

#include <stdlib.h>
#include <string.h>

AetherStatus aether_backend_create(
    AetherCore* core,
    AetherBackend** out_backend
)
{
    AetherBackend* backend;

    if (out_backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_backend = NULL;
    if (core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    backend = (AetherBackend*)malloc(sizeof(*backend));
    if (backend == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }

    backend->core = core;
    backend->started = false;
    (void)memset(backend->requests, 0, sizeof(backend->requests));
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
        for (size_t index = 0U;
             index < AETHER_BACKEND_REQUEST_CAPACITY; ++index) {
            AetherBackendRequestSnapshot* snapshot =
                &backend->requests[index].snapshot;
            if (backend->requests[index].occupied &&
                snapshot->state == AETHER_BACKEND_REQUEST_ACCEPTED) {
                snapshot->state = AETHER_BACKEND_REQUEST_CANCELLED;
                snapshot->result = AETHER_STATUS_CANCELLED;
            }
        }
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

