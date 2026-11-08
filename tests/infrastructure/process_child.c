#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include "process_tree_helper.h"

static int write_all(HANDLE handle, const unsigned char* bytes, DWORD size)
{
    while (size != 0U) {
        DWORD written = 0U;
        if (!WriteFile(handle, bytes, size, &written, NULL) || written == 0U) {
            return 0;
        }
        bytes += written;
        size -= written;
    }
    return 1;
}

static int text(DWORD stream, const char* value)
{
    return write_all(GetStdHandle(stream), (const unsigned char*)value,
                     (DWORD)strlen(value));
}

static int validation(int argc, wchar_t** argv)
{
    wchar_t cwd[32768];
    wchar_t value[64];
    wchar_t* block;
    wchar_t* entry;
    size_t count = 0U;
    DWORD length;
    int valid;
    if (argc != 8 || wcscmp(argv[2], L"") != 0 ||
        wcscmp(argv[3], L"two words") != 0 ||
        wcscmp(argv[4], L"quote\"inside") != 0 ||
        wcscmp(argv[5], L"tail space\\") != 0 ||
        wcscmp(argv[6], L"\x03a9\x4e2d") != 0) {
        return 31;
    }
    length = GetCurrentDirectoryW(32768U, cwd);
    if (length == 0U || length >= 32768U || wcscmp(cwd, argv[7]) != 0) {
        return 32;
    }
    length = GetEnvironmentVariableW(L"AETHER_PROCESS_ALLOWED", value, 64U);
    if (length != 7U || wcscmp(value, L"present") != 0) {
        return 33;
    }
    SetLastError(ERROR_SUCCESS);
    if (GetEnvironmentVariableW(L"AETHER_PROCESS_AMBIENT", value, 64U) != 0U ||
        GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
        return 34;
    }
    block = GetEnvironmentStringsW();
    if (block == NULL) {
        return 35;
    }
    valid = 1;
    for (entry = block; *entry != L'\0'; entry += wcslen(entry) + 1U) {
        ++count;
        if (wcscmp(entry, L"AETHER_PROCESS_ALLOWED=present") != 0) {
            valid = 0;
        }
    }
    FreeEnvironmentStringsW(block);
    if (!valid || count != 1U) {
        return 36;
    }
    return text(STD_OUTPUT_HANDLE, "argv-env-cwd-ok\n") &&
        text(STD_ERROR_HANDLE, "validation-stderr\n") ? 0 : 37;
}

static int echo_input(int echo)
{
    unsigned char bytes[4096];
    for (;;) {
        DWORD size = 0U;
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), bytes, sizeof(bytes), &size, NULL)) {
            if (GetLastError() != ERROR_BROKEN_PIPE) {
                return 41;
            }
            break;
        }
        if (size == 0U) {
            break;
        }
        if (echo && !write_all(GetStdHandle(STD_OUTPUT_HANDLE), bytes, size)) {
            return 42;
        }
    }
    if (echo && !text(STD_ERROR_HANDLE, "stdin-eof\n")) {
        return 43;
    }
    return echo ? 23 : 17;
}

static int flood(void)
{
    unsigned char output[257];
    unsigned char error[257];
    size_t offset;
    for (offset = 0U; offset < 257U * 1024U; offset += sizeof(output)) {
        size_t i;
        for (i = 0U; i < sizeof(output); ++i) {
            output[i] = (unsigned char)((offset + i) % 251U);
            error[i] = (unsigned char)(((offset + i) * 3U + 7U) % 251U);
        }
        if (!write_all(GetStdHandle(STD_OUTPUT_HANDLE), output, sizeof(output)) ||
            !write_all(GetStdHandle(STD_ERROR_HANDLE), error, sizeof(error))) {
            return 51;
        }
    }
    return 0;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) {
        return 61;
    }
    if (wcscmp(argv[1], L"tree") == 0) return launch_descendant();
    if (wcscmp(argv[1], L"validate") == 0) {
        return validation(argc, argv);
    }
    if (wcscmp(argv[1], L"echo") == 0) {
        return echo_input(1);
    }
    if (wcscmp(argv[1], L"flood") == 0) {
        return flood();
    }
    if (wcscmp(argv[1], L"eof") == 0) {
        if (!text(STD_OUTPUT_HANDLE, "pipes-closed\n")) {
            return 62;
        }
        CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
        CloseHandle(GetStdHandle(STD_ERROR_HANDLE));
        return echo_input(0);
    }
    if (wcscmp(argv[1], L"hang") == 0) {
        if (!text(STD_OUTPUT_HANDLE, "ready\n")) {
            return 63;
        }
        (void)WaitForSingleObject(GetCurrentProcess(), INFINITE);
        return 64;
    }
    if (wcscmp(argv[1], L"external") == 0) {
        (void)WaitForSingleObject(GetCurrentProcess(), INFINITE);
        return 66;
    }
    return 65;
}
