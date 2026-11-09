#ifndef AETHER_INFRASTRUCTURE_NETWORK_H
#define AETHER_INFRASTRUCTURE_NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherHttpRequest AetherHttpRequest;
typedef struct AetherHttpLimits {
    uint32_t timeout_ms;
    size_t max_response_bytes;
} AetherHttpLimits;
typedef struct AetherHttpRequestInput {
    AetherStringView url;
    AetherHttpLimits limits;
} AetherHttpRequestInput;
typedef struct AetherHttpResponse {
    uint32_t status_code;
    unsigned char* body;
    size_t body_length;
} AetherHttpResponse;

/* Windows-only libcurl GET. URL validation rejects invalid UTF-8, NUL/control,
 * backslash, fragment and all userinfo; only HTTP(S) is accepted. Redirects,
 * proxy/environment credentials, cookies and authentication are disabled.
 * TLS uses Schannel with peer/name verification and TLS 1.2 minimum. A single
 * monotonic deadline covers queueing, connect and body transfer. HTTP status
 * is returned as transport success, including 3xx/4xx/5xx. */
/* start copies the URL; accepted is not completed. Limits: timeout 1..600000ms,
 * body 1..64MiB. Failed outputs are cleared. Callers bound concurrent requests
 * and aggregate memory; this technical API does not perform authorization. */
AetherStatus aether_http_request_start(
    AetherHttpRequestInput input, AetherHttpRequest** out_request);
/* cancel may race wait, but callers keep request alive; neither may race
 * destroy. Cancellation cannot undo effects already observed by the server. */
AetherStatus aether_http_request_cancel(AetherHttpRequest* request);
/* Non-UI blocking join, exactly once, including failed requests. Success
 * transfers body ownership to out_response; failure returns no partial body.
 * Repeated wait returns ALREADY_EXISTS. */
AetherStatus aether_http_request_wait(
    AetherHttpRequest* request, AetherHttpResponse* out_response);
/* Before wait, returns NOT_INITIALIZED and preserves the live request.
 * After wait, releases the request and clears its pointer. NULL is safe. */
AetherStatus aether_http_request_destroy(AetherHttpRequest** request);
/* Frees the independently owned response body and clears the response. */
void aether_http_response_dispose(AetherHttpResponse* response);

#ifdef __cplusplus
}
#endif
#endif
