#ifndef AETHER_TEST_NETWORK_FIXTURE_H
#define AETHER_TEST_NETWORK_FIXTURE_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include <stdbool.h>

#define NETWORK_FIXTURE_WAIT_MS 5000U

typedef enum NetworkFixtureMode {
    NETWORK_FIXTURE_FAIL_INIT,
    NETWORK_FIXTURE_BODY,
    NETWORK_FIXTURE_NOT_FOUND,
    NETWORK_FIXTURE_REDIRECT,
    NETWORK_FIXTURE_DELAY_HEADERS,
    NETWORK_FIXTURE_DRIP
} NetworkFixtureMode;

typedef struct NetworkFixture {
    SOCKET listener;
    HANDLE thread;
    HANDLE stop;
    HANDLE received;
    NetworkFixtureMode mode;
    unsigned short port;
    unsigned int connections;
    int error;
    bool winsock_started;
    char target[256];
} NetworkFixture;

/* Binary body deliberately contains NUL and a non-ASCII byte. */
extern const unsigned char network_fixture_body[6];

bool network_fixture_start(NetworkFixture* fixture, NetworkFixtureMode mode);
bool network_fixture_received(NetworkFixture* fixture);
bool network_fixture_stop(NetworkFixture* fixture);

/* Shared by the no-argument suite and the explicit diagnostic executable. */
int network_test_run(void);
int network_test_fixture_failure(void);
int network_test_command(void);
int network_test_tls_untrusted(const char* url);
bool network_test_concurrency(void);

#endif
