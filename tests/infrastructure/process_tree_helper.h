#ifndef AETHER_TEST_PROCESS_TREE_HELPER_H
#define AETHER_TEST_PROCESS_TREE_HELPER_H

static int launch_descendant(void)
{
    wchar_t executable[32768];
    wchar_t command[32768];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD length = GetModuleFileNameW(NULL, executable, 32768U);
    DWORD written;
    char pid[32];
    int count;
    if (length == 0U || length >= 32768U) return 71;
    if (swprintf(command, 32768U, L"\"%ls\" external", executable) < 0) return 72;
    startup.cb = sizeof(startup);
    if (!CreateProcessW(executable, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                          NULL, NULL, &startup, &child)) return 73;
    CloseHandle(child.hThread);
    count = snprintf(pid, sizeof(pid), "%lu\n", (unsigned long)child.dwProcessId);
    if (count < 0 || (size_t)count >= sizeof(pid)) { CloseHandle(child.hProcess); return 74; }
    if (!WriteFile(output, pid, (DWORD)count, &written, NULL) || written != (DWORD)count) {
        CloseHandle(child.hProcess);
        return 75;
    }
    CloseHandle(child.hProcess);
    return 0; /* Descendant remains in inherited managed Job; parent exits first. */
}
#endif
