#include "network_fixture.h"

#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static bool tls_loopback_url(const char* url)
{
    static const char prefix[] = "https://127.0.0.1:";
    const char* cursor;
    unsigned int port = 0U;
    size_t digits = 0U;

    if (strncmp(url, prefix, sizeof(prefix) - 1U) != 0) return false;
    cursor = url + sizeof(prefix) - 1U;
    while (*cursor >= '0' && *cursor <= '9') {
        if (++digits > 5U) return false;
        port = port * 10U + (unsigned int)(*cursor++ - '0');
    }
    return digits != 0U && port != 0U && port <= 65535U && strcmp(cursor, "/") == 0;
}

int network_test_command(void)
{
    int count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    char url[128];
    int result = 1;

    if (arguments == NULL) {
        (void)fprintf(stderr, "network: cannot decode process command line\n");
        return 1;
    }
    if (count == 1) {
        result = network_test_run();
    } else if (count == 2 && wcscmp(arguments[1], L"--fixture-fail") == 0) {
        result = network_test_fixture_failure();
    } else if (count == 3 && wcscmp(arguments[1], L"--tls-untrusted") == 0 &&
               WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, arguments[2], -1,
                                   url, (int)sizeof(url), NULL, NULL) > 0 &&
               tls_loopback_url(url)) {
        result = network_test_tls_untrusted(url);
    } else {
        (void)fprintf(stderr, "network: expected no arguments, --fixture-fail, or "
                      "--tls-untrusted https://127.0.0.1:PORT/\n");
    }
    (void)LocalFree(arguments);
    return result;
}
