#ifndef AETHER_PROCESS_WINDOWS_INTERNAL_H
#define AETHER_PROCESS_WINDOWS_INTERNAL_H
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "aether/platform/process.h"

typedef struct AetherWindowsLaunch {
    wchar_t* executable;
    wchar_t* command_line;
    wchar_t* cwd;
    wchar_t* environment;
} AetherWindowsLaunch;

struct AetherPlatformProcess {
    HANDLE process;
    HANDLE job;
    HANDLE stdout_read;
    HANDLE stderr_read;
    HANDLE stdin_write;
    OVERLAPPED write_operation;
    bool write_pending;
    bool write_ready;
    size_t written;
    AetherStatus write_status;
    AetherPlatformProcessSnapshot snapshot;
};

AetherStatus aether_windows_launch_build(
    AetherPlatformProcessInput input, AetherWindowsLaunch* out_launch
);
void aether_windows_launch_dispose(AetherWindowsLaunch* launch);
AetherStatus aether_windows_process_error(DWORD error);
#endif
