#include "network_fixture.h"
#include "aether/infrastructure/network.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "network concurrency:%d: %s\n", __LINE__, #condition); \
        goto cleanup; \
    } \
} while (0)

bool network_test_concurrency(void)
{
    NetworkFixture slow = {0};
    NetworkFixture fast = {0};
    AetherHttpRequest* pending[2] = {NULL, NULL};
    AetherHttpResponse response[2] = {{0}, {0}};
    bool waited[2] = {false, false};
    bool opened[2] = {false, false};
    char url[2][128];
    bool ok = false;
    int length;

    CHECK(network_fixture_start(&slow, NETWORK_FIXTURE_DELAY_HEADERS));
    opened[0] = true;
    length = snprintf(url[0], sizeof(url[0]), "http://127.0.0.1:%u/slow",
                      (unsigned int)slow.port);
    CHECK(length > 0 && (size_t)length < sizeof(url[0]));
    CHECK(aether_http_request_start((AetherHttpRequestInput){
        {url[0], (size_t)length}, {4000U, 64U}}, &pending[0]) == AETHER_STATUS_OK);
    CHECK(network_fixture_received(&slow));
    CHECK(network_fixture_start(&fast, NETWORK_FIXTURE_BODY));
    opened[1] = true;
    length = snprintf(url[1], sizeof(url[1]), "http://127.0.0.1:%u/fast",
                      (unsigned int)fast.port);
    CHECK(length > 0 && (size_t)length < sizeof(url[1]));
    CHECK(aether_http_request_start((AetherHttpRequestInput){
        {url[1], (size_t)length}, {3000U, 64U}}, &pending[1]) == AETHER_STATUS_OK);
    {
        AetherStatus status = aether_http_request_wait(pending[1], &response[1]);
        waited[1] = true;
        CHECK(status == AETHER_STATUS_OK);
    }
    CHECK(response[1].status_code == 200U);
    CHECK(response[1].body_length == sizeof(network_fixture_body));
    CHECK(memcmp(response[1].body, network_fixture_body, sizeof(network_fixture_body)) == 0);
    CHECK(aether_http_request_destroy(&pending[1]) == AETHER_STATUS_OK);
    CHECK(aether_http_request_cancel(pending[0]) == AETHER_STATUS_OK);
    {
        AetherStatus status = aether_http_request_wait(pending[0], &response[0]);
        waited[0] = true;
        CHECK(status == AETHER_STATUS_CANCELLED);
    }
    CHECK(response[0].body == NULL && response[0].body_length == 0U);
    CHECK(aether_http_request_destroy(&pending[0]) == AETHER_STATUS_OK);
    ok = true;

cleanup:
    for (size_t index = 0U; index < 2U; ++index) {
        if (pending[index] != NULL) {
            if (!waited[index]) {
                (void)aether_http_request_cancel(pending[index]);
                (void)aether_http_request_wait(pending[index], &response[index]);
            }
            if (aether_http_request_destroy(&pending[index]) != AETHER_STATUS_OK) ok = false;
        }
        aether_http_response_dispose(&response[index]);
    }
    if (opened[1] && !network_fixture_stop(&fast)) ok = false;
    if (opened[0] && !network_fixture_stop(&slow)) ok = false;
    return ok;
}
