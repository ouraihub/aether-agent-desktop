#ifndef AETHER_BACKEND_CLIENT_H
#define AETHER_BACKEND_CLIENT_H

#include "aether/backend/backend.h"
#include "aether/backend/request.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherBackendClient AetherBackendClient;

typedef enum AetherBackendClientRequestState {
    AETHER_CLIENT_REQUEST_ACCEPTED = 0,
    AETHER_CLIENT_REQUEST_SUCCEEDED,
    AETHER_CLIENT_REQUEST_FAILED,
    AETHER_CLIENT_REQUEST_CANCELLED
} AetherBackendClientRequestState;

typedef struct AetherBackendClientRequest {
    AetherId request_id;
    AetherBackendClientRequestState state;
    AetherStatus result;
} AetherBackendClientRequest;

/*
 * V1 in-process client for the currently published synchronous Backend
 * lifecycle and local request-record contract. The client borrows backend;
 * the caller must serialize calls and destroy the client before backend.
 * No payload is retained after a call returns. This is not a Runtime client.
 */
AetherStatus aether_backend_client_create(
    AetherBackend* backend,
    AetherBackendClient** out_client
);
void aether_backend_client_destroy(AetherBackendClient* client);

AetherStatus aether_backend_client_initialize(AetherBackendClient* client);
AetherStatus aether_backend_client_start(AetherBackendClient* client);
AetherStatus aether_backend_client_stop(AetherBackendClient* client);
AetherStatus aether_backend_client_close(AetherBackendClient* client);
AetherCoreState aether_backend_client_core_state(
    const AetherBackendClient* client
);

AetherStatus aether_backend_client_submit(
    AetherBackendClient* client,
    AetherId* out_request_id
);
AetherStatus aether_backend_client_query(
    const AetherBackendClient* client,
    AetherId request_id,
    AetherBackendClientRequest* out_request
);
AetherStatus aether_backend_client_finish(
    AetherBackendClient* client,
    AetherId request_id,
    AetherBackendClientRequestState state,
    AetherStatus result
);
AetherStatus aether_backend_client_release(
    AetherBackendClient* client,
    AetherId request_id
);

#ifdef __cplusplus
}
#endif

#endif
