#ifndef AETHER_TEST_PROCESS_GATE_CHILD_H
#define AETHER_TEST_PROCESS_GATE_CHILD_H
#include <windows.h>
#include <wchar.h>

/* Dispatch gate-marker from process_child's wmain; no ambient path or data. */
static int process_gate_marker(int argc, wchar_t** argv)
{
    wchar_t environment[32];
    char payload[256];
    HANDLE file;
    DWORD written = 0U;
    int size;
    int success;
    if (GetEnvironmentVariableW(L"AETHER_GATE_ENV", environment, 32U) != 11U ||
        wcscmp(environment, L"allowlisted") != 0) return 75;
    if (argc != 4) return 71;
    size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[3], -1,
                              payload, (int)sizeof(payload), NULL, NULL);
    if (size <= 0) return 72;
    file = CreateFileW(argv[2], GENERIC_WRITE, 0U, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 73;
    success = WriteFile(file, payload, (DWORD)(size - 1), &written, NULL) &&
        written == (DWORD)(size - 1);
    if (!CloseHandle(file)) success = 0;
    return success ? 0 : 74;
}
#endif
