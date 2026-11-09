#include "network_fixture.h"

#include <limits.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

const unsigned char network_fixture_body[6] = {'A', 0U, 'B', 0xFFU, '\r', '\n'};

static bool stopped(NetworkFixture* fixture)
{
    return WaitForSingleObject(fixture->stop, 0U) == WAIT_OBJECT_0;
}

static bool socket_ready(NetworkFixture* fixture, SOCKET socket_value, bool writing,
                         ULONGLONG deadline)
{
    while (!stopped(fixture) && GetTickCount64() < deadline) {
        fd_set sockets;
        struct timeval timeout = {0L, 100000L};
        int result;

        FD_ZERO(&sockets);
        FD_SET(socket_value, &sockets);
        result = select(0, writing ? NULL : &sockets,
                        writing ? &sockets : NULL, NULL, &timeout);
        if (result > 0) return true;
        if (result == SOCKET_ERROR) {
            fixture->error = WSAGetLastError();
            return false;
        }
    }
    return false;
}

static bool send_bytes(NetworkFixture* fixture, SOCKET client, const void* bytes,
                       size_t length, ULONGLONG deadline)
{
    const char* cursor = bytes;

    while (length != 0U && socket_ready(fixture, client, true, deadline)) {
        int sent = send(client, cursor, (int)length, 0);

        if (sent == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            return false;
        }
        if (sent == 0) return false;
        cursor += (size_t)sent;
        length -= (size_t)sent;
    }
    return length == 0U;
}

static bool receive_target(NetworkFixture* fixture, SOCKET client,
                           ULONGLONG deadline)
{
    char headers[4096];
    size_t used = 0U;
    char* end;
    size_t length;

    headers[0] = '\0';
    while (used < sizeof(headers) - 1U) {
        int count;

        if (!socket_ready(fixture, client, false, deadline)) return false;
        count = recv(client, headers + used, (int)(sizeof(headers) - used - 1U), 0);
        if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
        if (count <= 0) return false;
        used += (size_t)count;
        headers[used] = '\0';
        if (strstr(headers, "\r\n\r\n") != NULL) break;
    }
    if (strstr(headers, "\r\n\r\n") == NULL || strncmp(headers, "GET ", 4U) != 0) {
        fixture->error = WSAEINVAL;
        return false;
    }
    end = strstr(headers + 4U, " HTTP/1.1\r\n");
    if (end == NULL) {
        fixture->error = WSAEINVAL;
        return false;
    }
    length = (size_t)(end - (headers + 4U));
    if (length >= sizeof(fixture->target)) {
        fixture->error = WSAEMSGSIZE;
        return false;
    }
    if (fixture->connections == 1U) {
        (void)memcpy(fixture->target, headers + 4U, length);
        fixture->target[length] = '\0';
    }
    (void)SetEvent(fixture->received);
    return true;
}

static void respond(NetworkFixture* fixture, SOCKET client, ULONGLONG deadline)
{
    char header[256];
    int length;
    unsigned int i;

    if (fixture->mode == NETWORK_FIXTURE_DELAY_HEADERS &&
        WaitForSingleObject(fixture->stop, 3000U) == WAIT_OBJECT_0) return;
    if (fixture->mode == NETWORK_FIXTURE_REDIRECT && fixture->connections == 1U) {
        length = snprintf(header, sizeof(header),
            "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:%u/redirected\r\n"
            "Content-Length: 6\r\nConnection: close\r\n\r\n",
            (unsigned int)fixture->port);
    } else {
        const char* status = fixture->mode == NETWORK_FIXTURE_NOT_FOUND
            ? "404 Not Found" : "200 OK";
        unsigned int body_length = fixture->mode == NETWORK_FIXTURE_DRIP ? 32U : 6U;

        length = snprintf(header, sizeof(header),
            "HTTP/1.1 %s\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
            status, body_length);
    }
    if (length <= 0 || (size_t)length >= sizeof(header)) {
        fixture->error = WSAEMSGSIZE;
        return;
    }
    if (!send_bytes(fixture, client, header, (size_t)length, deadline)) return;
    if (fixture->mode != NETWORK_FIXTURE_DRIP) {
        (void)send_bytes(fixture, client, network_fixture_body,
                         sizeof(network_fixture_body), deadline);
        return;
    }
    for (i = 0U; i < 32U; ++i) {
        const unsigned char byte = (unsigned char)('a' + i % 26U);

        if (!send_bytes(fixture, client, &byte, 1U, deadline)) return;
        if (WaitForSingleObject(fixture->stop, 120U) == WAIT_OBJECT_0) return;
    }
}

static unsigned int __stdcall serve(void* context)
{
    NetworkFixture* fixture = context;
    ULONGLONG deadline = GetTickCount64() + NETWORK_FIXTURE_WAIT_MS;

    while (socket_ready(fixture, fixture->listener, false, deadline)) {
        SOCKET client = accept(fixture->listener, NULL, NULL);
        u_long nonblocking = 1UL;

        if (client == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            fixture->error = WSAGetLastError();
            break;
        }
        ++fixture->connections;
        if (ioctlsocket(client, FIONBIO, &nonblocking) != 0) {
            fixture->error = WSAGetLastError();
        } else if (receive_target(fixture, client, deadline)) {
            respond(fixture, client, deadline);
        }
        (void)shutdown(client, SD_BOTH);
        (void)closesocket(client);
        if (fixture->mode != NETWORK_FIXTURE_REDIRECT) break;
        if (fixture->connections >= 2U) break;
    }
    return 0U;
}

bool network_fixture_stop(NetworkFixture* fixture)
{
    bool ok = true;

    if (fixture->stop != NULL && !SetEvent(fixture->stop)) ok = false;
    if (fixture->thread != NULL) {
        /* Worker select/send/receive loops are bounded and observe stop every 100ms. */
        if (WaitForSingleObject(fixture->thread, INFINITE) != WAIT_OBJECT_0) ok = false;
        if (!CloseHandle(fixture->thread)) ok = false;
        fixture->thread = NULL;
    }
    if (fixture->listener != INVALID_SOCKET) {
        if (closesocket(fixture->listener) != 0) ok = false;
        fixture->listener = INVALID_SOCKET;
    }
    if (fixture->received != NULL) {
        if (!CloseHandle(fixture->received)) ok = false;
        fixture->received = NULL;
    }
    if (fixture->stop != NULL) {
        if (!CloseHandle(fixture->stop)) ok = false;
        fixture->stop = NULL;
    }
    if (fixture->winsock_started) {
        if (WSACleanup() != 0) ok = false;
        fixture->winsock_started = false;
    }
    return ok && fixture->error == 0;
}

bool network_fixture_start(NetworkFixture* fixture, NetworkFixtureMode mode)
{
    WSADATA data;
    struct sockaddr_in address;
    int address_size = (int)sizeof(address);
    u_long nonblocking = 1UL;
    int result;

    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->listener = INVALID_SOCKET;
    fixture->mode = mode;
    if (mode == NETWORK_FIXTURE_FAIL_INIT) {
        fixture->error = WSAEINVAL;
        (void)network_fixture_stop(fixture);
        return false;
    }
    result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        fixture->error = result;
        return false;
    }
    fixture->winsock_started = true;
    fixture->stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    fixture->received = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (fixture->stop == NULL || fixture->received == NULL) goto failed;
    fixture->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fixture->listener == INVALID_SOCKET) goto failed;
    (void)memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0U;
    if (bind(fixture->listener, (const struct sockaddr*)&address, address_size) != 0 ||
        getsockname(fixture->listener, (struct sockaddr*)&address, &address_size) != 0 ||
        listen(fixture->listener, 2) != 0 ||
        ioctlsocket(fixture->listener, FIONBIO, &nonblocking) != 0) goto failed;
    fixture->port = ntohs(address.sin_port);
    fixture->thread = (HANDLE)_beginthreadex(NULL, 1024U * 1024U, serve, fixture,
        STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (fixture->thread == NULL) goto failed;
    return true;

failed:
    fixture->error = WSAGetLastError();
    if (fixture->error == 0) fixture->error = WSAEINVAL;
    (void)network_fixture_stop(fixture);
    return false;
}

bool network_fixture_received(NetworkFixture* fixture)
{
    return WaitForSingleObject(fixture->received, NETWORK_FIXTURE_WAIT_MS) == WAIT_OBJECT_0;
}
