#include "aether/backend/client.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    AetherCore* core = NULL;
    AetherBackend* backend = NULL;
    AetherBackendClient* client = NULL;
    AetherBackendClientRequest request;
    AetherId request_id;

    assert(aether_core_create(&core) == AETHER_STATUS_OK);
    assert(aether_backend_create(core, &backend) == AETHER_STATUS_OK);
    assert(aether_backend_client_create(backend, &client) == AETHER_STATUS_OK);
    assert(aether_backend_client_submit(client, &request_id) ==
           AETHER_STATUS_NOT_INITIALIZED);
    assert(aether_backend_client_initialize(client) == AETHER_STATUS_OK);
    assert(aether_backend_client_start(client) == AETHER_STATUS_OK);
    assert(aether_backend_client_submit(client, &request_id) ==
           AETHER_STATUS_OK);
    assert(aether_backend_client_query(client, request_id, &request) ==
           AETHER_STATUS_OK);
    assert(request.request_id.value == request_id.value);
    assert(request.state == AETHER_CLIENT_REQUEST_ACCEPTED);
    assert(aether_backend_client_finish(
               client, request_id, AETHER_CLIENT_REQUEST_SUCCEEDED,
               AETHER_STATUS_OK) == AETHER_STATUS_OK);
    assert(aether_backend_client_query(client, request_id, &request) ==
           AETHER_STATUS_OK);
    assert(request.state == AETHER_CLIENT_REQUEST_SUCCEEDED);
    assert(request.result == AETHER_STATUS_OK);
    assert(aether_backend_client_finish(
               client, request_id, AETHER_CLIENT_REQUEST_FAILED,
               AETHER_STATUS_INTERNAL_ERROR) == AETHER_STATUS_ALREADY_EXISTS);
    assert(aether_backend_client_release(client, request_id) ==
           AETHER_STATUS_OK);
    assert(aether_backend_client_query(client, request_id, &request) ==
           AETHER_STATUS_NOT_FOUND);
    assert(aether_backend_client_stop(client) == AETHER_STATUS_OK);
    assert(aether_backend_client_close(client) == AETHER_STATUS_OK);
    aether_backend_client_destroy(client);
    aether_backend_destroy(backend);
    aether_core_destroy(core);
    return 0;
}
