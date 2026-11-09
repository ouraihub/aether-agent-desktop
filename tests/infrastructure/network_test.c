#include "network_fixture.h"
#include "aether/infrastructure/network.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "network: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        goto cleanup; \
    } \
} while (0)

typedef struct NetworkCase {
    NetworkFixtureMode mode;
    const char* target;
    AetherStatus expected;
    uint32_t http_status;
    uint32_t timeout_ms;
    size_t limit;
    bool cancel_after_receive;
} NetworkCase;

static AetherHttpRequestInput input_for(const char* url, uint32_t timeout, size_t limit)
{
    return (AetherHttpRequestInput){
        {url, strlen(url)}, {timeout, limit}};
}

static bool empty_response(const AetherHttpResponse* response)
{
    return response->body == NULL && response->body_length == 0U &&
           response->status_code == 0U;
}

static bool run_case(const NetworkCase* test)
{
    NetworkFixture fixture;
    AetherHttpRequest* pending = NULL;
    AetherHttpResponse response = {0};
    AetherStatus status;
    char url[512];
    int length;
    bool ok = false;
    bool waited = false;
    bool fixture_ok;
    ULONGLONG started;
    ULONGLONG elapsed;

    if (!network_fixture_start(&fixture, test->mode)) {
        (void)fprintf(stderr, "network: fixture initialization failed: %d\n", fixture.error);
        return false;
    }
    length = snprintf(url, sizeof(url), "http://127.0.0.1:%u%s",
                      (unsigned int)fixture.port, test->target);
    CHECK(length > 0 && (size_t)length < sizeof(url));
    started = GetTickCount64();
    CHECK(aether_http_request_start(
        input_for(url, test->timeout_ms, test->limit), &pending) == AETHER_STATUS_OK);
    CHECK(pending != NULL);
    {
        AetherHttpRequest* retained = pending;
        CHECK(aether_http_request_destroy(&pending) == AETHER_STATUS_NOT_INITIALIZED);
        CHECK(pending == retained);
    }
    /* The accepted request must not keep borrowing caller-owned URL bytes. */
    (void)memset(url, 'x', (size_t)length);
    if (test->cancel_after_receive) {
        CHECK(network_fixture_received(&fixture));
        started = GetTickCount64();
        CHECK(aether_http_request_cancel(pending) == AETHER_STATUS_OK);
    }
    status = aether_http_request_wait(pending, &response);
    waited = true;
    elapsed = GetTickCount64() - started;
    if (status != test->expected) {
        (void)fprintf(stderr, "network: %s expected status=%d, actual=%d\n",
                      test->target, (int)test->expected, (int)status);
    }
    CHECK(status == test->expected);
    if (test->expected == AETHER_STATUS_OK) {
        CHECK(response.status_code == test->http_status);
        CHECK(response.body != NULL);
        CHECK(response.body_length == sizeof(network_fixture_body));
        CHECK(memcmp(response.body, network_fixture_body, sizeof(network_fixture_body)) == 0);
    } else {
        CHECK(empty_response(&response));
    }
    if (test->expected == AETHER_STATUS_TIMEOUT) {
        CHECK(elapsed >= 350U && elapsed <= 1800U);
    }
    if (test->cancel_after_receive) CHECK(elapsed <= 1200U);
    {
        AetherHttpResponse repeated = {0};
        CHECK(aether_http_request_wait(pending, &repeated) == AETHER_STATUS_ALREADY_EXISTS);
        CHECK(empty_response(&repeated));
    }
    CHECK(aether_http_request_destroy(&pending) == AETHER_STATUS_OK);
    CHECK(pending == NULL);
    ok = true;

cleanup:
    if (pending != NULL) {
        if (!waited) {
            (void)aether_http_request_cancel(pending);
            (void)aether_http_request_wait(pending, &response);
        }
        if (aether_http_request_destroy(&pending) != AETHER_STATUS_OK) ok = false;
    }
    aether_http_response_dispose(&response);
    if (!empty_response(&response)) ok = false;
    fixture_ok = network_fixture_stop(&fixture);
    if (!fixture_ok) {
        (void)fprintf(stderr, "network: fixture cleanup failed: %d\n", fixture.error);
    }
    if (ok && (fixture.connections != 1U || strcmp(fixture.target, test->target) != 0)) {
        (void)fprintf(stderr, "network: target=%s expected=%s connections=%u\n",
                      fixture.target, test->target, fixture.connections);
        ok = false;
    }
    return ok && fixture_ok;
}

static bool reject_url(AetherStringView url)
{
    AetherHttpRequest* pending = NULL;
    AetherHttpResponse response = {0};
    AetherHttpRequestInput input = {url, {1000U, 1024U}};
    AetherStatus status = aether_http_request_start(input, &pending);
    bool ok = status == AETHER_STATUS_INVALID_ARGUMENT && pending == NULL;

    if (!ok) {
        (void)fprintf(stderr, "network: invalid URL start status=%d, request=%p\n",
                      (int)status, (void*)pending);
    }
    if (pending != NULL) {
        (void)aether_http_request_cancel(pending);
        (void)aether_http_request_wait(pending, &response);
        aether_http_response_dispose(&response);
        if (aether_http_request_destroy(&pending) != AETHER_STATUS_OK) ok = false;
    }
    return ok;
}

static bool resource_cycles(const NetworkCase* cases)
{
    DWORD baseline = 0U;
    DWORD remaining = 0U;
    unsigned int cycle;

    /* Functional cases have already initialized transport/OS runtime state. */
    Sleep(100U);
    if (!GetProcessHandleCount(GetCurrentProcess(), &baseline)) {
        (void)fprintf(stderr, "network: cannot count baseline process handles\n");
        return false;
    }
    for (cycle = 0U; cycle < 3U; ++cycle) {
        if (!run_case(&cases[0]) || !run_case(&cases[4]) || !run_case(&cases[5])) {
            return false;
        }
    }
    Sleep(100U);
    if (!GetProcessHandleCount(GetCurrentProcess(), &remaining)) {
        (void)fprintf(stderr, "network: cannot count remaining process handles\n");
        return false;
    }
    if (remaining > baseline) {
        (void)fprintf(stderr, "network: handle accumulation: before=%lu after=%lu\n",
                      (unsigned long)baseline, (unsigned long)remaining);
        return false;
    }
    (void)printf("network: resource cycles before=%lu after=%lu\n",
                 (unsigned long)baseline, (unsigned long)remaining);
    return true;
}

int network_test_run(void)
{
    static const NetworkCase cases[] = {
        {NETWORK_FIXTURE_BODY, "/exact/path%20value?first=1&second=%2F",
         AETHER_STATUS_OK, 200U, 3000U, sizeof(network_fixture_body), false},
        {NETWORK_FIXTURE_BODY, "/limit", AETHER_STATUS_OUT_OF_MEMORY,
         0U, 3000U, 1U, false},
        {NETWORK_FIXTURE_NOT_FOUND, "/missing?query=kept", AETHER_STATUS_OK,
         404U, 3000U, 1024U, false},
        {NETWORK_FIXTURE_REDIRECT, "/redirect", AETHER_STATUS_OK,
         302U, 3000U, 1024U, false},
        {NETWORK_FIXTURE_DELAY_HEADERS, "/delayed", AETHER_STATUS_TIMEOUT,
         0U, 600U, 1024U, false},
        {NETWORK_FIXTURE_DELAY_HEADERS, "/cancel-after-receive", AETHER_STATUS_CANCELLED,
         0U, 4000U, 1024U, true},
        {NETWORK_FIXTURE_DRIP, "/total-deadline", AETHER_STATUS_TIMEOUT,
         0U, 600U, 1024U, false}
    };
    static const char nul_url[] = "http://127.0.0.1:1/safe\0ignored";
    static const char invalid_url[] = "not-a-url";
    size_t i;

    if (!reject_url((AetherStringView){invalid_url, sizeof(invalid_url) - 1U}) ||
        !reject_url((AetherStringView){nul_url, sizeof(nul_url) - 1U})) return 1;
    {
        static const char* const rejected[] = {
            "http://user:password@127.0.0.1/", "http://@127.0.0.1/",
            "http://127.0.0.1/#fragment", "http://127.0.0.1/\r\nInjected: 1",
            "file:///not-an-http-endpoint"
        };
        for (i = 0U; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
            if (!reject_url((AetherStringView){rejected[i], strlen(rejected[i])})) return 1;
        }
    }
    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!run_case(&cases[i])) return 1;
    }
    if (!resource_cycles(cases)) return 1;
    if (!network_test_concurrency()) return 1;
    (void)puts("network: self-contained GET, bounds, status, timeout and cancellation passed");
    return 0;
}

int network_test_fixture_failure(void)
{
    NetworkFixture fixture;

    if (network_fixture_start(&fixture, NETWORK_FIXTURE_FAIL_INIT)) {
        (void)network_fixture_stop(&fixture);
        (void)fprintf(stderr, "network: diagnostic unexpectedly initialized fixture\n");
        return 2;
    }
    (void)fprintf(stderr, "network: intentional fixture initialization failure (%d)\n",
                  fixture.error);
    return 1;
}

int network_test_tls_untrusted(const char* url)
{
    AetherHttpRequest* pending = NULL;
    AetherHttpResponse response = {0};
    AetherStatus status;
    bool waited = false;
    int result = 1;

    CHECK(aether_http_request_start(input_for(url, 3000U, 1024U), &pending)
          == AETHER_STATUS_OK);
    status = aether_http_request_wait(pending, &response);
    waited = true;
    CHECK(status == AETHER_STATUS_IO_ERROR);
    CHECK(empty_response(&response));
    CHECK(aether_http_request_destroy(&pending) == AETHER_STATUS_OK);
    CHECK(pending == NULL);
    result = 0;

cleanup:
    if (pending != NULL) {
        if (!waited) {
            (void)aether_http_request_cancel(pending);
            (void)aether_http_request_wait(pending, &response);
        }
        if (aether_http_request_destroy(&pending) != AETHER_STATUS_OK) result = 1;
    }
    aether_http_response_dispose(&response);
    return result;
}

int main(void)
{
    return network_test_command();
}
