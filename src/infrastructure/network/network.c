#include "aether/infrastructure/network.h"

#include <curl/curl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include "aether/foundation/error.h"
#include "aether/platform/platform.h"
#include "aether/platform/thread.h"

#define URL_LIMIT 8192U
#define RESPONSE_LIMIT (64U * 1024U * 1024U)
#define POLL_MS 50U

struct AetherHttpRequest {
    char* url;
    size_t max_response;
    uint64_t deadline;
    CURL* easy;
    AetherPlatformThread* thread;
    AetherPlatformMonitor* monitor;
    bool cancel_requested;
    bool global_acquired;
    AetherStatus status;
    AetherStatus callback_status;
    AetherHttpResponse response;
    AetherError failure;
};

static volatile long global_lock;
static size_t global_references;
static bool global_initialized;

static void lock_global(void)
{
#if defined(_MSC_VER)
    while (_InterlockedCompareExchange(&global_lock, 1L, 0L) != 0L)
#else
    while (__sync_lock_test_and_set(&global_lock, 1L) != 0L)
#endif
        aether_platform_thread_pause(0U);
}

static void unlock_global(void)
{
#if defined(_MSC_VER)
    (void)_InterlockedExchange(&global_lock, 0L);
#else
    __sync_lock_release(&global_lock);
#endif
}

static AetherStatus global_acquire(void)
{
    CURLcode result;
    lock_global();
    if (global_references == 0U) {
        const curl_version_info_data* version;
        result = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (result != CURLE_OK) {
            unlock_global();
            return AETHER_STATUS_IO_ERROR;
        }
        version = curl_version_info(CURLVERSION_NOW);
        if (version == NULL || (version->features & CURL_VERSION_THREADSAFE) == 0) {
            curl_global_cleanup();
            unlock_global();
            return AETHER_STATUS_NOT_SUPPORTED;
        }
        global_initialized = true;
    }
    ++global_references;
    unlock_global();
    return AETHER_STATUS_OK;
}

static void global_release(void)
{
    lock_global();
    if (global_references != 0U && --global_references == 0U && global_initialized) {
        curl_global_cleanup();
        global_initialized = false;
    }
    unlock_global();
}

static void fail(AetherHttpRequest* request, AetherStatus status, int32_t code,
                 const char* category)
{
    request->status = status;
    (void)aether_error_set(&request->failure, status, code, category);
}

static bool userinfo_present(const char* url)
{
    const char* p = strstr(url, "://");
    const char* end;
    if (p == NULL) return false;
    p += 3;
    end = p;
    while (*end != '\0' && *end != '/' && *end != '?') ++end;
    for (; p < end; ++p) if (*p == '@') return true;
    return false;
}

static bool ascii_equal(const char* a, const char* b)
{
    while (*a != '\0' && *b != '\0') {
        unsigned char x = (unsigned char)*a++;
        unsigned char y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x = (unsigned char)(x + 32U);
        if (y >= 'A' && y <= 'Z') y = (unsigned char)(y + 32U);
        if (x != y) return false;
    }
    return *a == '\0' && *b == '\0';
}

static AetherStatus copy_url(AetherStringView input, char** output)
{
    CURLU* handle;
    CURLUcode code;
    char* text;
    char* scheme = NULL;
    char* host = NULL;
    char* fragment = NULL;
    char* normalized = NULL;
    AetherStatus status;

    if (output == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *output = NULL;
    status = aether_string_view_validate(input);
    if (status != AETHER_STATUS_OK || input.data == NULL || input.length == 0U ||
        input.length > URL_LIMIT) return AETHER_STATUS_INVALID_ARGUMENT;
    text = malloc(input.length + 1U);
    if (text == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    (void)memcpy(text, input.data, input.length);
    text[input.length] = '\0';
    for (size_t i = 0U; i < input.length; ++i) {
        unsigned char byte = (unsigned char)text[i];
        if (byte == 0U || byte <= 0x20U || byte == 0x7FU || byte == '#' || byte == '\\') {
            free(text);
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
    }
    if (userinfo_present(text)) {
        free(text);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    handle = curl_url();
    if (handle == NULL) {
        free(text);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    code = curl_url_set(handle, CURLUPART_URL, text, CURLU_DISALLOW_USER);
    free(text);
    if (code != CURLUE_OK ||
        curl_url_get(handle, CURLUPART_SCHEME, &scheme, 0U) != CURLUE_OK ||
        curl_url_get(handle, CURLUPART_HOST, &host, 0U) != CURLUE_OK ||
        scheme == NULL || host == NULL || host[0] == '\0' ||
        (!ascii_equal(scheme, "http") && !ascii_equal(scheme, "https")) ||
        curl_url_get(handle, CURLUPART_FRAGMENT, &fragment, 0U) == CURLUE_OK) {
        if (scheme != NULL) curl_free(scheme);
        if (host != NULL) curl_free(host);
        if (fragment != NULL) curl_free(fragment);
        curl_url_cleanup(handle);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    curl_free(scheme);
    curl_free(host);
    code = curl_url_get(handle, CURLUPART_URL, &normalized, CURLU_PATH_AS_IS);
    curl_url_cleanup(handle);
    if (code != CURLUE_OK || normalized == NULL) {
        if (normalized != NULL) curl_free(normalized);
        return code == CURLUE_OUT_OF_MEMORY
            ? AETHER_STATUS_OUT_OF_MEMORY : AETHER_STATUS_INVALID_ARGUMENT;
    }
    *output = normalized;
    return AETHER_STATUS_OK;
}

static bool remaining(const AetherHttpRequest* request, uint64_t* milliseconds)
{
    AetherPlatformClock clock;
    uint64_t nanoseconds;
    if (aether_platform_clock_now(&clock) != AETHER_STATUS_OK ||
        clock.monotonic_ns >= request->deadline) {
        return false;
    }
    nanoseconds = request->deadline - clock.monotonic_ns;
    *milliseconds = (nanoseconds + 999999U) / 1000000U;
    if (*milliseconds == 0U) *milliseconds = 1U;
    return true;
}

static bool cancelled(AetherHttpRequest* request)
{
    bool value;
    aether_platform_monitor_lock(request->monitor);
    value = request->cancel_requested;
    aether_platform_monitor_unlock(request->monitor);
    return value;
}

static size_t receive(char* data, size_t size, size_t count, void* context)
{
    AetherHttpRequest* request = context;
    size_t bytes;
    size_t needed;
    size_t capacity;
    unsigned char* buffer;
    if (size != 0U && count > SIZE_MAX / size) {
        request->callback_status = AETHER_STATUS_OUT_OF_MEMORY;
        return 0U;
    }
    bytes = size * count;
    if (bytes > request->max_response - request->response.body_length) {
        request->callback_status = AETHER_STATUS_OUT_OF_MEMORY;
        return 0U;
    }
    needed = request->response.body_length + bytes;
    capacity = request->response.body_length == 0U ? 16384U : request->response.body_length;
    if (capacity > request->max_response) capacity = request->max_response;
    while (capacity < needed) {
        if (capacity > request->max_response / 2U) capacity = request->max_response;
        else capacity *= 2U;
    }
    if (needed > request->response.body_length) {
        buffer = realloc(request->response.body, capacity);
        if (buffer == NULL) {
            request->callback_status = AETHER_STATUS_OUT_OF_MEMORY;
            return 0U;
        }
        request->response.body = buffer;
    }
    if (bytes != 0U) memcpy(request->response.body + request->response.body_length, data, bytes);
    request->response.body_length = needed;
    return count;
}

static int progress(void* context, curl_off_t total, curl_off_t now,
                    curl_off_t upload_total, curl_off_t upload_now)
{
    AetherHttpRequest* request = context;
    uint64_t unused;
    (void)total;
    (void)now;
    (void)upload_total;
    (void)upload_now;
    return cancelled(request) || !remaining(request, &unused) ? 1 : 0;
}

static AetherStatus configure(AetherHttpRequest* request)
{
    CURLcode code;
    uint64_t milliseconds;
    long timeout;
    if (!remaining(request, &milliseconds)) return AETHER_STATUS_TIMEOUT;
    timeout = (long)milliseconds;
#define OPT(option, value) do { code = curl_easy_setopt(request->easy, option, value); \
    if (code != CURLE_OK) return AETHER_STATUS_IO_ERROR; } while (0)
    OPT(CURLOPT_URL, request->url);
    OPT(CURLOPT_HTTPGET, 1L);
    OPT(CURLOPT_FOLLOWLOCATION, 0L);
    OPT(CURLOPT_MAXREDIRS, 0L);
    OPT(CURLOPT_PROXY, "");
    OPT(CURLOPT_NOPROXY, "*");
    OPT(CURLOPT_NETRC, CURL_NETRC_IGNORED);
    OPT(CURLOPT_HTTPAUTH, CURLAUTH_NONE);
    OPT(CURLOPT_COOKIEFILE, NULL);
    OPT(CURLOPT_COOKIEJAR, NULL);
    OPT(CURLOPT_SSL_VERIFYPEER, 1L);
    OPT(CURLOPT_SSL_VERIFYHOST, 2L);
    OPT(CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    OPT(CURLOPT_TIMEOUT_MS, timeout);
    OPT(CURLOPT_WRITEFUNCTION, receive);
    OPT(CURLOPT_WRITEDATA, request);
    OPT(CURLOPT_XFERINFOFUNCTION, progress);
    OPT(CURLOPT_XFERINFODATA, request);
    OPT(CURLOPT_NOPROGRESS, 0L);
#undef OPT
    return AETHER_STATUS_OK;
}

static void worker(void* context)
{
    AetherHttpRequest* request = context;
    CURLM* multi = NULL;
    CURLMcode multi_code;
    CURLcode easy_code = CURLE_OK;
    CURLMsg* message;
    int running = 0;
    int messages;
    int poll_result;
    uint64_t milliseconds;

    request->status = configure(request);
    if (request->status == AETHER_STATUS_OK) {
        multi = curl_multi_init();
        if (multi == NULL) fail(request, AETHER_STATUS_OUT_OF_MEMORY, 0, "curl.multi");
        else if (curl_multi_add_handle(multi, request->easy) != CURLM_OK)
            fail(request, AETHER_STATUS_IO_ERROR, 0, "curl.multi.add");
        else {
            do {
                if (cancelled(request)) {
                    fail(request, AETHER_STATUS_CANCELLED,
                         (int32_t)CURLE_ABORTED_BY_CALLBACK,
                         "curl.cancelled");
                    break;
                }
                if (!remaining(request, &milliseconds)) {
                    fail(request, AETHER_STATUS_TIMEOUT,
                         (int32_t)CURLE_OPERATION_TIMEDOUT,
                         "curl.timeout");
                    break;
                }
                multi_code = curl_multi_perform(multi, &running);
                if (multi_code != CURLM_OK) {
                    fail(request, AETHER_STATUS_IO_ERROR,
                         (int32_t)multi_code, "curl.multi.perform");
                    break;
                }
                if (running == 0) break;
                if (milliseconds > POLL_MS) milliseconds = POLL_MS;
                multi_code = curl_multi_poll(multi, NULL, 0, (int)milliseconds, &poll_result);
                if (multi_code != CURLM_OK) {
                    fail(request, AETHER_STATUS_IO_ERROR, (int32_t)multi_code, "curl.multi.poll");
                    break;
                }
            } while (running != 0);
            messages = 0;
            while ((message = curl_multi_info_read(multi, &messages)) != NULL) {
                if (message->msg == CURLMSG_DONE) {
                    easy_code = message->data.result;
                }
            }
            multi_code = curl_multi_remove_handle(multi, request->easy);
            if (multi_code != CURLM_OK && request->status == AETHER_STATUS_OK) {
                fail(request, AETHER_STATUS_IO_ERROR,
                     (int32_t)multi_code, "curl.multi.remove");
            }
            if (request->status == AETHER_STATUS_OK) {
                if (request->callback_status != AETHER_STATUS_OK) {
                    fail(request, request->callback_status,
                         (int32_t)easy_code, "curl.response");
                } else if (easy_code != CURLE_OK) {
                    if (easy_code == CURLE_ABORTED_BY_CALLBACK ||
                        cancelled(request)) {
                        fail(request, AETHER_STATUS_CANCELLED,
                             (int32_t)easy_code, "curl.cancelled");
                    } else if (easy_code == CURLE_OPERATION_TIMEDOUT ||
                               !remaining(request, &milliseconds)) {
                        fail(request, AETHER_STATUS_TIMEOUT,
                             (int32_t)easy_code, "curl.timeout");
                    } else {
                        fail(request, AETHER_STATUS_IO_ERROR,
                             (int32_t)easy_code, "curl.transport");
                    }
                } else {
                    long status_code = 0L;
                    if (curl_easy_getinfo(request->easy,
                                          CURLINFO_RESPONSE_CODE,
                                          &status_code) != CURLE_OK ||
                        status_code < 100L || status_code > 599L) {
                        fail(request, AETHER_STATUS_PROTOCOL_ERROR, 0,
                             "curl.status");
                    } else {
                        request->response.status_code = (uint32_t)status_code;
                    }
                }
            }
        }
    }
    if (multi != NULL) {
        multi_code = curl_multi_cleanup(multi);
        if (multi_code != CURLM_OK && request->status == AETHER_STATUS_OK)
            fail(request, AETHER_STATUS_IO_ERROR, (int32_t)multi_code, "curl.multi.cleanup");
    }
    curl_easy_cleanup(request->easy);
    request->easy = NULL;
    if (request->status != AETHER_STATUS_OK) {
        free(request->response.body);
        request->response = (AetherHttpResponse){0};
    }
}

AetherStatus aether_http_request_start(AetherHttpRequestInput input, AetherHttpRequest** output)
{
    AetherHttpRequest* request;
    AetherPlatformClock clock;
    AetherStatus status;
    uint64_t timeout_ns;
    if (output == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *output = NULL;
    if (aether_platform_clock_now(&clock) != AETHER_STATUS_OK ||
        input.limits.timeout_ms == 0U ||
        input.limits.timeout_ms > 600000U ||
        input.limits.max_response_bytes == 0U ||
        input.limits.max_response_bytes > RESPONSE_LIMIT) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    request = calloc(1U, sizeof(*request));
    if (request == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    request->max_response = input.limits.max_response_bytes;
    timeout_ns = (uint64_t)input.limits.timeout_ms * 1000000U;
    request->deadline = clock.monotonic_ns > UINT64_MAX - timeout_ns
        ? UINT64_MAX : clock.monotonic_ns + timeout_ns;
    request->status = AETHER_STATUS_NOT_INITIALIZED;
    request->callback_status = AETHER_STATUS_OK;
    aether_error_clear(&request->failure);
    status = copy_url(input.url, &request->url);
    if (status != AETHER_STATUS_OK) goto fail_start;
    status = global_acquire();
    if (status != AETHER_STATUS_OK) goto fail_start;
    request->global_acquired = true;
    status = aether_platform_monitor_create(&request->monitor);
    if (status != AETHER_STATUS_OK) goto fail_start;
    request->easy = curl_easy_init();
    if (request->easy == NULL) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
        goto fail_start;
    }
    status = aether_platform_thread_create(worker, request, &request->thread);
    if (status != AETHER_STATUS_OK) goto fail_start;
    *output = request;
    return AETHER_STATUS_OK;
fail_start:
    if (request->easy != NULL) curl_easy_cleanup(request->easy);
    if (request->monitor != NULL) aether_platform_monitor_destroy(request->monitor);
    if (request->global_acquired) global_release();
    curl_free(request->url);
    free(request);
    return status;
}

AetherStatus aether_http_request_cancel(AetherHttpRequest* request)
{
    if (request == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(request->monitor);
    request->cancel_requested = true;
    aether_platform_monitor_wake_all(request->monitor);
    aether_platform_monitor_unlock(request->monitor);
    return AETHER_STATUS_OK;
}

AetherStatus aether_http_request_wait(AetherHttpRequest* request, AetherHttpResponse* output)
{
    AetherStatus status;
    if (request == NULL || output == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *output = (AetherHttpResponse){0};
    if (request->thread == NULL) return AETHER_STATUS_ALREADY_EXISTS;
    status = aether_platform_thread_join(&request->thread);
    if (status != AETHER_STATUS_OK) return status;
    if (request->status == AETHER_STATUS_OK) {
        *output = request->response;
        request->response = (AetherHttpResponse){0};
    }
    return request->status;
}

AetherStatus aether_http_request_destroy(AetherHttpRequest** request)
{
    AetherHttpRequest* owned;
    if (request == NULL || *request == NULL) return AETHER_STATUS_OK;
    owned = *request;
    if (owned->thread != NULL) return AETHER_STATUS_NOT_INITIALIZED;
    aether_platform_monitor_destroy(owned->monitor);
    curl_free(owned->url);
    free(owned->response.body);
    if (owned->global_acquired) global_release();
    free(owned);
    *request = NULL;
    return AETHER_STATUS_OK;
}

void aether_http_response_dispose(AetherHttpResponse* response)
{
    if (response == NULL) return;
    free(response->body);
    *response = (AetherHttpResponse){0};
}
