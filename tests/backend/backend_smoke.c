#include "aether/backend/backend.h"
#include "aether/backend/request.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    AetherBackend* backend = NULL;
    AetherBackendRequestSnapshot snapshot;
    AetherCore* core = NULL;
    AetherId request_id;

    assert(aether_backend_create(NULL, &backend) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_core_create(&core) == AETHER_STATUS_OK);
    assert(aether_backend_create(core, &backend) == AETHER_STATUS_OK);
    assert(aether_backend_submit_request(backend, &request_id) ==
           AETHER_STATUS_NOT_INITIALIZED);
    assert(aether_backend_initialize(backend) == AETHER_STATUS_OK);
    assert(aether_backend_start(backend) == AETHER_STATUS_OK);
    assert(aether_backend_submit_request(backend, &request_id) ==
           AETHER_STATUS_OK);
    assert(aether_backend_query_request(backend, request_id, &snapshot) ==
           AETHER_STATUS_OK);
    assert(snapshot.state == AETHER_BACKEND_REQUEST_ACCEPTED);
    assert(aether_backend_finish_request(
               backend,
               request_id,
               AETHER_BACKEND_REQUEST_SUCCEEDED,
               AETHER_STATUS_OK
           ) == AETHER_STATUS_OK);
    assert(aether_backend_finish_request(
               backend,
               request_id,
               AETHER_BACKEND_REQUEST_FAILED,
               AETHER_STATUS_INTERNAL_ERROR
           ) == AETHER_STATUS_ALREADY_EXISTS);
    assert(aether_backend_query_request(backend, request_id, &snapshot) ==
           AETHER_STATUS_OK);
    assert(snapshot.state == AETHER_BACKEND_REQUEST_SUCCEEDED);
    assert(aether_backend_query_request(
               backend,
               (AetherId){0},
               &snapshot
           ) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_backend_stop(backend) == AETHER_STATUS_OK);
    assert(aether_backend_close(backend) == AETHER_STATUS_OK);
    aether_backend_destroy(backend);
    aether_core_destroy(core);
    return 0;
}
