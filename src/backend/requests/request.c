#include "backend_internal.h"

static size_t find_request(const AetherBackend* backend, AetherId request_id)
{
    for (size_t index = 0U; index < AETHER_BACKEND_REQUEST_CAPACITY; ++index) {
        const AetherBackendRequestRecord* record = &backend->requests[index];
        if (record->occupied &&
            aether_id_equal(record->snapshot.request_id, request_id)) {
            return index;
        }
    }
    return AETHER_BACKEND_REQUEST_CAPACITY;
}

AetherStatus aether_backend_submit_request(
    AetherBackend* backend,
    AetherId* out_request_id
)
{
    if (out_request_id == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_request_id = (AetherId){0};
    if (backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (!backend->started) {
        return AETHER_STATUS_NOT_INITIALIZED;
    }

    for (size_t index = 0U; index < AETHER_BACKEND_REQUEST_CAPACITY; ++index) {
        AetherBackendRequestRecord* record = &backend->requests[index];
        if (record->occupied) {
            continue;
        }
        AetherId id = aether_id_next();
        if (!aether_id_is_valid(id)) {
            return AETHER_STATUS_INTERNAL_ERROR;
        }
        record->snapshot = (AetherBackendRequestSnapshot){
            id, AETHER_BACKEND_REQUEST_ACCEPTED, AETHER_STATUS_OK
        };
        record->occupied = true;
        *out_request_id = id;
        return AETHER_STATUS_OK;
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_backend_query_request(
    const AetherBackend* backend,
    AetherId request_id,
    AetherBackendRequestSnapshot* out_snapshot
)
{
    if (out_snapshot == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_snapshot = (AetherBackendRequestSnapshot){0};
    if (backend == NULL || !aether_id_is_valid(request_id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    size_t index = find_request(backend, request_id);
    if (index == AETHER_BACKEND_REQUEST_CAPACITY) {
        return AETHER_STATUS_NOT_FOUND;
    }
    *out_snapshot = backend->requests[index].snapshot;
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_finish_request(
    AetherBackend* backend,
    AetherId request_id,
    AetherBackendRequestState terminal_state,
    AetherStatus result
)
{
    if (backend == NULL || !aether_id_is_valid(request_id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if ((terminal_state != AETHER_BACKEND_REQUEST_SUCCEEDED &&
         terminal_state != AETHER_BACKEND_REQUEST_FAILED &&
         terminal_state != AETHER_BACKEND_REQUEST_CANCELLED) ||
        (terminal_state == AETHER_BACKEND_REQUEST_SUCCEEDED &&
         result != AETHER_STATUS_OK) ||
        (terminal_state == AETHER_BACKEND_REQUEST_FAILED &&
         (result == AETHER_STATUS_OK || result == AETHER_STATUS_CANCELLED)) ||
        (terminal_state == AETHER_BACKEND_REQUEST_CANCELLED &&
         result != AETHER_STATUS_CANCELLED)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    size_t index = find_request(backend, request_id);
    if (index == AETHER_BACKEND_REQUEST_CAPACITY) {
        return AETHER_STATUS_NOT_FOUND;
    }
    AetherBackendRequestSnapshot* snapshot =
        &backend->requests[index].snapshot;
    if (snapshot->state != AETHER_BACKEND_REQUEST_ACCEPTED) {
        return AETHER_STATUS_ALREADY_EXISTS;
    }
    snapshot->state = terminal_state;
    snapshot->result = result;
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_release_request(
    AetherBackend* backend,
    AetherId request_id
)
{
    if (backend == NULL || !aether_id_is_valid(request_id)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    size_t index = find_request(backend, request_id);
    if (index == AETHER_BACKEND_REQUEST_CAPACITY) {
        return AETHER_STATUS_NOT_FOUND;
    }
    AetherBackendRequestRecord* record = &backend->requests[index];
    if (record->snapshot.state == AETHER_BACKEND_REQUEST_ACCEPTED) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *record = (AetherBackendRequestRecord){0};
    return AETHER_STATUS_OK;
}
