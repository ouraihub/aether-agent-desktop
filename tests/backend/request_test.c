#include "aether/backend/backend.h"
#include "aether/backend/request.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, \
                #expression); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(void)
{
    AetherCore* core = NULL;
    AetherBackend* backend = NULL;
    AetherBackend* other = NULL;
    AetherBackendRequestSnapshot snapshot;
    AetherId ids[8];
    AetherId output = {99};

    CHECK(aether_core_create(&core) == AETHER_STATUS_OK);
    CHECK(aether_backend_create(core, &backend) == AETHER_STATUS_OK);
    CHECK(aether_backend_create(core, &other) == AETHER_STATUS_OK);
    CHECK(aether_backend_submit_request(backend, &output) ==
          AETHER_STATUS_NOT_INITIALIZED);
    CHECK(output.value == 0U);
    CHECK(aether_backend_initialize(backend) == AETHER_STATUS_OK);
    CHECK(aether_backend_start(backend) == AETHER_STATUS_OK);

    for (size_t index = 0U; index < 8U; ++index) {
        CHECK(aether_backend_submit_request(backend, &ids[index]) ==
              AETHER_STATUS_OK);
        for (size_t previous = 0U; previous < index; ++previous) {
            CHECK(!aether_id_equal(ids[index], ids[previous]));
        }
        CHECK(aether_backend_query_request(backend, ids[index], &snapshot) ==
              AETHER_STATUS_OK);
        CHECK(aether_id_equal(snapshot.request_id, ids[index]));
        CHECK(snapshot.state == AETHER_BACKEND_REQUEST_ACCEPTED);
    }
    CHECK(aether_backend_submit_request(backend, &output) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(output.value == 0U);
    CHECK(aether_backend_release_request(backend, ids[0]) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_backend_finish_request(backend, ids[0],
          AETHER_BACKEND_REQUEST_ACCEPTED, AETHER_STATUS_OK) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_backend_finish_request(backend, ids[0],
          AETHER_BACKEND_REQUEST_SUCCEEDED, AETHER_STATUS_INTERNAL_ERROR) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_backend_finish_request(backend, ids[0],
          AETHER_BACKEND_REQUEST_FAILED, AETHER_STATUS_OK) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_backend_finish_request(backend, ids[0],
          AETHER_BACKEND_REQUEST_CANCELLED, AETHER_STATUS_OK) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_backend_query_request(backend, ids[0], &snapshot) ==
          AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_BACKEND_REQUEST_ACCEPTED);

    CHECK(aether_backend_finish_request(backend, ids[0],
          AETHER_BACKEND_REQUEST_SUCCEEDED, AETHER_STATUS_OK) ==
          AETHER_STATUS_OK);
    CHECK(aether_backend_finish_request(backend, ids[0],
          AETHER_BACKEND_REQUEST_FAILED, AETHER_STATUS_INTERNAL_ERROR) ==
          AETHER_STATUS_ALREADY_EXISTS);
    CHECK(aether_backend_query_request(backend, ids[0], &snapshot) ==
          AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_BACKEND_REQUEST_SUCCEEDED);
    CHECK(snapshot.result == AETHER_STATUS_OK);
    snapshot.state = AETHER_BACKEND_REQUEST_FAILED;
    CHECK(aether_backend_query_request(backend, ids[0], &snapshot) ==
          AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_BACKEND_REQUEST_SUCCEEDED);
    CHECK(aether_backend_submit_request(backend, &output) ==
          AETHER_STATUS_OUT_OF_MEMORY);

    CHECK(aether_backend_query_request(other, ids[0], &snapshot) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.request_id.value == 0U);
    CHECK(aether_backend_release_request(backend, ids[0]) == AETHER_STATUS_OK);
    CHECK(aether_backend_release_request(backend, ids[0]) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(aether_backend_submit_request(backend, &output) == AETHER_STATUS_OK);
    CHECK(!aether_id_equal(output, ids[0]));
    CHECK(aether_backend_finish_request(backend, ids[0],
          AETHER_BACKEND_REQUEST_SUCCEEDED, AETHER_STATUS_OK) ==
          AETHER_STATUS_NOT_FOUND);

    CHECK(aether_backend_finish_request(backend, ids[1],
          AETHER_BACKEND_REQUEST_FAILED, AETHER_STATUS_INTERNAL_ERROR) ==
          AETHER_STATUS_OK);
    CHECK(aether_backend_stop(backend) == AETHER_STATUS_OK);
    CHECK(aether_backend_query_request(backend, ids[1], &snapshot) ==
          AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_BACKEND_REQUEST_FAILED);
    CHECK(snapshot.result == AETHER_STATUS_INTERNAL_ERROR);
    for (size_t index = 2U; index < 8U; ++index) {
        CHECK(aether_backend_query_request(backend, ids[index], &snapshot) ==
              AETHER_STATUS_OK);
        CHECK(snapshot.state == AETHER_BACKEND_REQUEST_CANCELLED);
        CHECK(snapshot.result == AETHER_STATUS_CANCELLED);
    }
    CHECK(aether_backend_query_request(backend, output, &snapshot) ==
          AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_BACKEND_REQUEST_CANCELLED);
    CHECK(aether_backend_close(backend) == AETHER_STATUS_OK);
    CHECK(aether_backend_release_request(backend, output) == AETHER_STATUS_OK);

    output.value = 99U;
    CHECK(aether_backend_submit_request(NULL, &output) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(output.value == 0U);
    snapshot.request_id.value = 99U;
    CHECK(aether_backend_query_request(NULL, ids[1], &snapshot) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(snapshot.request_id.value == 0U);
    CHECK(aether_backend_submit_request(backend, NULL) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_backend_query_request(backend, (AetherId){0}, &snapshot) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(snapshot.request_id.value == 0U);

    aether_backend_destroy(other);
    aether_backend_destroy(backend);
    aether_core_destroy(core);
    return EXIT_SUCCESS;
}
