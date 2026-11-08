#include "aether/backend/client.h"

#include <stdlib.h>

struct AetherBackendClient {
    AetherBackend* backend;
};

static AetherBackendRequestState aether_client_to_backend_state(
    AetherBackendClientRequestState state,
    AetherStatus result,
    AetherStatus* out_status
)
{
    if (out_status == NULL) {
        return AETHER_BACKEND_REQUEST_FAILED;
    }
    *out_status = AETHER_STATUS_OK;
    switch (state) {
    case AETHER_CLIENT_REQUEST_SUCCEEDED:
        if (result != AETHER_STATUS_OK) {
            *out_status = AETHER_STATUS_INVALID_ARGUMENT;
        }
        return AETHER_BACKEND_REQUEST_SUCCEEDED;
    case AETHER_CLIENT_REQUEST_FAILED:
        if (result == AETHER_STATUS_OK || result == AETHER_STATUS_CANCELLED) {
            *out_status = AETHER_STATUS_INVALID_ARGUMENT;
        }
        return AETHER_BACKEND_REQUEST_FAILED;
    case AETHER_CLIENT_REQUEST_CANCELLED:
        if (result != AETHER_STATUS_CANCELLED) {
            *out_status = AETHER_STATUS_INVALID_ARGUMENT;
        }
        return AETHER_BACKEND_REQUEST_CANCELLED;
    case AETHER_CLIENT_REQUEST_ACCEPTED:
    default:
        *out_status = AETHER_STATUS_INVALID_ARGUMENT;
        return AETHER_BACKEND_REQUEST_FAILED;
    }
}

static AetherBackendClientRequestState aether_client_from_backend_state(
    AetherBackendRequestState state
)
{
    switch (state) {
    case AETHER_BACKEND_REQUEST_ACCEPTED:
        return AETHER_CLIENT_REQUEST_ACCEPTED;
    case AETHER_BACKEND_REQUEST_SUCCEEDED:
        return AETHER_CLIENT_REQUEST_SUCCEEDED;
    case AETHER_BACKEND_REQUEST_FAILED:
        return AETHER_CLIENT_REQUEST_FAILED;
    case AETHER_BACKEND_REQUEST_CANCELLED:
        return AETHER_CLIENT_REQUEST_CANCELLED;
    default:
        return AETHER_CLIENT_REQUEST_FAILED;
    }
}

AetherStatus aether_backend_client_create(
    AetherBackend* backend,
    AetherBackendClient** out_client
)
{
    AetherBackendClient* client;

    if (out_client == NULL || backend == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_client = NULL;
    client = (AetherBackendClient*)malloc(sizeof(*client));
    if (client == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    client->backend = backend;
    *out_client = client;
    return AETHER_STATUS_OK;
}

void aether_backend_client_destroy(AetherBackendClient* client)
{
    free(client);
}

AetherStatus aether_backend_client_initialize(AetherBackendClient* client)
{
    if (client == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return aether_backend_initialize(client->backend);
}

AetherStatus aether_backend_client_start(AetherBackendClient* client)
{
    if (client == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return aether_backend_start(client->backend);
}

AetherStatus aether_backend_client_stop(AetherBackendClient* client)
{
    if (client == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return aether_backend_stop(client->backend);
}

AetherStatus aether_backend_client_close(AetherBackendClient* client)
{
    if (client == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return aether_backend_close(client->backend);
}

AetherCoreState aether_backend_client_core_state(
    const AetherBackendClient* client
)
{
    if (client == NULL) {
        return AETHER_CORE_STATE_CLOSED;
    }
    return aether_backend_get_core_state(client->backend);
}

AetherStatus aether_backend_client_submit(
    AetherBackendClient* client,
    AetherId* out_request_id
)
{
    if (client == NULL) {
        if (out_request_id != NULL) {
            *out_request_id = (AetherId){0};
        }
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return aether_backend_submit_request(client->backend, out_request_id);
}

AetherStatus aether_backend_client_query(
    const AetherBackendClient* client,
    AetherId request_id,
    AetherBackendClientRequest* out_request
)
{
    AetherBackendRequestSnapshot snapshot;
    AetherStatus status;

    if (out_request == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_request = (AetherBackendClientRequest){0};
    if (client == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_backend_query_request(
        client->backend, request_id, &snapshot);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    out_request->request_id = snapshot.request_id;
    out_request->state = aether_client_from_backend_state(snapshot.state);
    out_request->result = snapshot.result;
    return AETHER_STATUS_OK;
}

AetherStatus aether_backend_client_finish(
    AetherBackendClient* client,
    AetherId request_id,
    AetherBackendClientRequestState state,
    AetherStatus result
)
{
    AetherBackendRequestState backend_state;
    AetherStatus mapping_status;

    if (client == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    backend_state = aether_client_to_backend_state(
        state, result, &mapping_status);
    if (mapping_status != AETHER_STATUS_OK) {
        return mapping_status;
    }
    return aether_backend_finish_request(
        client->backend, request_id, backend_state, result);
}

AetherStatus aether_backend_client_release(
    AetherBackendClient* client,
    AetherId request_id
)
{
    if (client == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    return aether_backend_release_request(client->backend, request_id);
}
