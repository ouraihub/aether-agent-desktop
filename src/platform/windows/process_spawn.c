#include "process_windows_internal.h"
#include "aether/foundation/id.h"

#include <stdlib.h>
#include <wchar.h>

#define STDIN_PIPE_BYTES 65536U
#define CHILD_HANDLE_COUNT 3U

static void close_handle(HANDLE* handle)
{
    if (*handle != NULL && *handle != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(*handle);
    }
    *handle = NULL;
}

static DWORD create_output_pipe(HANDLE* parent, HANDLE* child)
{
    /* Neither endpoint starts inheritable; only the child's writer is enabled. */
    if (!CreatePipe(parent, child, NULL, 0U)) {
        return GetLastError();
    }
    if (!SetHandleInformation(*child, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
        return GetLastError();
    }
    return ERROR_SUCCESS;
}

static DWORD create_input_pipe(HANDLE* parent, HANDLE* child)
{
    wchar_t name[128];
    AetherId id = aether_id_next();
    SECURITY_ATTRIBUTES security = {0};
    OVERLAPPED connection = {0};
    HANDLE server = NULL;
    HANDLE reader = NULL;
    DWORD error = ERROR_SUCCESS;
    DWORD transferred = 0U;
    bool pending = false;
    int length;

    if (!aether_id_is_valid(id)) {
        return ERROR_NO_SYSTEM_RESOURCES;
    }
    length = swprintf(name, sizeof(name) / sizeof(name[0]),
                      L"\\\\.\\pipe\\aether-process-%lu-%llu",
                      (unsigned long)GetCurrentProcessId(), (unsigned long long)id.value);
    if (length < 0 || (size_t)length >= sizeof(name) / sizeof(name[0])) {
        return ERROR_INSUFFICIENT_BUFFER;
    }
    server = CreateNamedPipeW(name,
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1U, STDIN_PIPE_BYTES, STDIN_PIPE_BYTES, 0U, NULL);
    if (server == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }
    connection.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (connection.hEvent == NULL) {
        error = GetLastError();
        goto done;
    }
    if (!ConnectNamedPipe(server, &connection)) {
        error = GetLastError();
        if (error == ERROR_IO_PENDING) {
            pending = true;
        } else if (error != ERROR_PIPE_CONNECTED) {
            goto done;
        }
    }
    security.nLength = (DWORD)sizeof(security);
    security.bInheritHandle = TRUE;
    reader = CreateFileW(name, GENERIC_READ, 0U, &security, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (reader == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        goto done;
    }
    if (pending) {
        if (!GetOverlappedResult(server, &connection, &transferred, TRUE)) {
            error = GetLastError();
            pending = false;
            goto done;
        }
        pending = false;
    }
    *parent = server;
    *child = reader;
    server = NULL;
    reader = NULL;
    error = ERROR_SUCCESS;

done:
    if (pending) {
        /* The stack OVERLAPPED and event must outlive cancellation completion. */
        (void)CancelIoEx(server, &connection);
        (void)GetOverlappedResult(server, &connection, &transferred, TRUE);
    }
    close_handle(&reader);
    close_handle(&connection.hEvent);
    close_handle(&server);
    return error;
}

static DWORD create_attributes(STARTUPINFOEXW* startup, HANDLE* inherited)
{
    SIZE_T size = 0U;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes;
    DWORD error;

    if (InitializeProcThreadAttributeList(NULL, 1U, 0U, &size)) {
        return ERROR_GEN_FAILURE;
    }
    error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER) {
        return error;
    }
    if (size == 0U) {
        return ERROR_GEN_FAILURE;
    }
    attributes = malloc(size);
    if (attributes == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    if (!InitializeProcThreadAttributeList(attributes, 1U, 0U, &size)) {
        error = GetLastError();
        free(attributes);
        return error;
    }
    startup->lpAttributeList = attributes;
    if (!UpdateProcThreadAttribute(attributes, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                    inherited, CHILD_HANDLE_COUNT * sizeof(*inherited),
                                    NULL, NULL)) {
        return GetLastError();
    }
    return ERROR_SUCCESS;
}

static DWORD create_job(AetherPlatformProcess* process)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};

    process->job = CreateJobObjectW(NULL, NULL);
    if (process->job == NULL) {
        return GetLastError();
    }
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(process->job, JobObjectExtendedLimitInformation,
                                  &limits, (DWORD)sizeof(limits))) {
        return GetLastError();
    }
    return ERROR_SUCCESS;
}

static DWORD create_child(
    AetherWindowsLaunch* launch, AetherPlatformProcess* process,
    PROCESS_INFORMATION* child
)
{
    STARTUPINFOEXW startup = {0};
    HANDLE inherited[CHILD_HANDLE_COUNT] = {0};
    DWORD error;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT |
                  CREATE_NO_WINDOW | CREATE_SUSPENDED;

    error = create_job(process);
    if (error != ERROR_SUCCESS) {
        goto done;
    }
    error = create_output_pipe(&process->stdout_read, &inherited[1]);
    if (error != ERROR_SUCCESS) {
        goto done;
    }
    error = create_output_pipe(&process->stderr_read, &inherited[2]);
    if (error != ERROR_SUCCESS) {
        goto done;
    }
    error = create_input_pipe(&process->stdin_write, &inherited[0]);
    if (error != ERROR_SUCCESS) {
        goto done;
    }
    process->write_operation.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (process->write_operation.hEvent == NULL) {
        error = GetLastError();
        goto done;
    }
    error = create_attributes(&startup, inherited);
    if (error != ERROR_SUCCESS) {
        goto done;
    }
    startup.StartupInfo.cb = (DWORD)sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = inherited[0];
    startup.StartupInfo.hStdOutput = inherited[1];
    startup.StartupInfo.hStdError = inherited[2];
    if (!CreateProcessW(launch->executable, launch->command_line, NULL, NULL, TRUE,
                         flags, launch->environment, launch->cwd,
                         &startup.StartupInfo, child)) {
        error = GetLastError();
    }

done:
    if (startup.lpAttributeList != NULL) {
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        free(startup.lpAttributeList);
    }
    close_handle(&inherited[0]);
    close_handle(&inherited[1]);
    close_handle(&inherited[2]);
    return error;
}

static void discard_owned_process(AetherPlatformProcess* process)
{
    if (process->process != NULL) {
        /* This handle exists only for our suspended CreateProcessW result. */
        (void)TerminateProcess(process->process, ERROR_PROCESS_ABORTED);
        close_handle(&process->job);
        (void)WaitForSingleObject(process->process, INFINITE);
    }
    close_handle(&process->job);
    close_handle(&process->process);
    close_handle(&process->stdout_read);
    close_handle(&process->stderr_read);
    close_handle(&process->stdin_write);
    close_handle(&process->write_operation.hEvent);
    free(process);
}

AetherStatus aether_platform_process_spawn(
    AetherPlatformProcessInput input, AetherPlatformProcess** out_process
)
{
    AetherWindowsLaunch launch = {0};
    AetherPlatformProcess* process;
    PROCESS_INFORMATION child = {0};
    AetherStatus status;
    DWORD error;

    if (out_process == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_process = NULL;
    /* Validation and launch materialization precede native side effects or IDs. */
    status = aether_windows_launch_build(input, &launch);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    process = calloc(1U, sizeof(*process));
    if (process == NULL) {
        aether_windows_launch_dispose(&launch);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    error = create_child(&launch, process, &child);
    aether_windows_launch_dispose(&launch);
    if (error != ERROR_SUCCESS) {
        discard_owned_process(process);
        return aether_windows_process_error(error);
    }
    process->process = child.hProcess;
    if (process->process == NULL || child.hThread == NULL) {
        close_handle(&child.hThread);
        discard_owned_process(process);
        return AETHER_STATUS_IO_ERROR;
    }
    if (!AssignProcessToJobObject(process->job, process->process)) {
        error = GetLastError();
    } else if (ResumeThread(child.hThread) == (DWORD)-1) {
        error = GetLastError();
    }
    close_handle(&child.hThread);
    if (error != ERROR_SUCCESS) {
        discard_owned_process(process);
        return aether_windows_process_error(error);
    }
    *out_process = process;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_process_attach(
    uint32_t pid, AetherPlatformProcess** out_process
)
{
    AetherPlatformProcess* process;

    if (out_process == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_process = NULL;
    if (pid == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    process = calloc(1U, sizeof(*process));
    if (process == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    process->process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, (DWORD)pid);
    if (process->process == NULL) {
        DWORD error = GetLastError();
        free(process);
        return aether_windows_process_error(error);
    }
    process->snapshot.borrowed = true;
    process->snapshot.stdout_eof = true;
    process->snapshot.stderr_eof = true;
    process->snapshot.stdin_closed = true;
    *out_process = process;
    return AETHER_STATUS_OK;
}
