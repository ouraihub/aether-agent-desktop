#include "process_windows_internal.h"
#include <stdlib.h>

static AetherStatus failed(AetherPlatformProcess* process, DWORD error)
{
    process->snapshot.native_error = error;
    return aether_windows_process_error(error);
}

AetherStatus aether_platform_process_read(
    AetherPlatformProcess* process, AetherProcessStream stream,
    unsigned char* bytes, size_t capacity, size_t* out_size
)
{
    HANDLE pipe;
    DWORD available;
    DWORD read;
    bool* eof;
    if (out_size == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_size = 0U;
    if (process == NULL || bytes == NULL || capacity == 0U || capacity > 65536U ||
        (stream != AETHER_PROCESS_STDOUT && stream != AETHER_PROCESS_STDERR)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (process->snapshot.borrowed) return AETHER_STATUS_NOT_SUPPORTED;
    pipe = stream == AETHER_PROCESS_STDOUT ? process->stdout_read : process->stderr_read;
    eof = stream == AETHER_PROCESS_STDOUT ? &process->snapshot.stdout_eof
                                         : &process->snapshot.stderr_eof;
    if (*eof) return AETHER_STATUS_OK;
    if (!PeekNamedPipe(pipe, NULL, 0U, NULL, &available, NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            *eof = true;
            return AETHER_STATUS_OK;
        }
        return failed(process, error);
    }
    if (available == 0U) return AETHER_STATUS_NOT_FOUND;
    if ((size_t)available > capacity) available = (DWORD)capacity;
    if (!ReadFile(pipe, bytes, available, &read, NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            *eof = true;
            return AETHER_STATUS_OK;
        }
        return failed(process, error);
    }
    *out_size = (size_t)read;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_process_write_begin(
    AetherPlatformProcess* process, const unsigned char* bytes, size_t size
)
{
    DWORD written = 0U;
    if (process == NULL || bytes == NULL || size == 0U || size > 65536U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (process->snapshot.borrowed) return AETHER_STATUS_PERMISSION_DENIED;
    if (process->snapshot.stdin_closed) return AETHER_STATUS_NOT_INITIALIZED;
    if (process->write_pending || process->write_ready) return AETHER_STATUS_ALREADY_EXISTS;
    if (!ResetEvent(process->write_operation.hEvent)) return failed(process, GetLastError());
    if (WriteFile(process->stdin_write, bytes, (DWORD)size, &written,
                   &process->write_operation)) {
        process->written = (size_t)written;
        process->write_status = AETHER_STATUS_OK;
        process->write_ready = true;
        return AETHER_STATUS_OK;
    }
    if (GetLastError() != ERROR_IO_PENDING) return failed(process, GetLastError());
    process->write_pending = true;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_process_write_poll(
    AetherPlatformProcess* process, size_t* out_written
)
{
    DWORD written = 0U;
    AetherStatus status;
    if (out_written == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_written = 0U;
    if (process == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    if (process->write_ready) {
        process->write_ready = false;
        *out_written = process->written;
        return process->write_status;
    }
    if (!process->write_pending) return AETHER_STATUS_NOT_FOUND;
    if (!GetOverlappedResult(process->stdin_write, &process->write_operation,
                              &written, FALSE)) {
        DWORD error = GetLastError();
        if (error == ERROR_IO_INCOMPLETE) return AETHER_STATUS_NOT_FOUND;
        status = failed(process, error);
    } else {
        status = AETHER_STATUS_OK;
    }
    process->write_pending = false;
    *out_written = (size_t)written;
    return status;
}

AetherStatus aether_platform_process_close_stdin(AetherPlatformProcess* process)
{
    if (process == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    if (process->snapshot.borrowed) return AETHER_STATUS_PERMISSION_DENIED;
    if (process->snapshot.stdin_closed) return AETHER_STATUS_OK;
    if (process->write_pending || process->write_ready) return AETHER_STATUS_ALREADY_EXISTS;
    if (!CloseHandle(process->stdin_write)) return failed(process, GetLastError());
    process->stdin_write = NULL;
    process->snapshot.stdin_closed = true;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_process_terminate(AetherPlatformProcess* process)
{
    if (process == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    if (process->snapshot.borrowed) return AETHER_STATUS_PERMISSION_DENIED;
    if (!TerminateJobObject(process->job, 1U)) return failed(process, GetLastError());
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_process_query(
    AetherPlatformProcess* process, AetherPlatformProcessSnapshot* out_snapshot
)
{
    DWORD wait;
    if (out_snapshot == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_snapshot = (AetherPlatformProcessSnapshot){0};
    if (process == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    wait = WaitForSingleObject(process->process, 0U);
    if (wait == WAIT_OBJECT_0) {
        DWORD code;
        if (!GetExitCodeProcess(process->process, &code)) return failed(process, GetLastError());
        process->snapshot.exited = true;
        process->snapshot.exit_code = code;
    } else if (wait != WAIT_TIMEOUT) {
        return failed(process, GetLastError());
    }
    if (process->job != NULL) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting;
        if (!QueryInformationJobObject(process->job, JobObjectBasicAccountingInformation,
                                        &accounting, sizeof(accounting), NULL)) {
            return failed(process, GetLastError());
        }
        process->snapshot.owned_tree_exited = accounting.ActiveProcesses == 0U;
    }
    process->snapshot.write_pending = process->write_pending || process->write_ready;
    *out_snapshot = process->snapshot;
    return AETHER_STATUS_OK;
}

void aether_platform_process_destroy(AetherPlatformProcess* process)
{
    if (process == NULL) return;
    if (process->write_pending) {
        DWORD written;
        /* Owner-only final destruction; the buffer must remain live until the
         * OS no longer references its pending overlapped write. */
        (void)CancelIoEx(process->stdin_write, &process->write_operation);
        (void)GetOverlappedResult(process->stdin_write, &process->write_operation,
                                  &written, TRUE);
    }
    if (process->job != NULL) (void)CloseHandle(process->job);
    if (process->stdin_write != NULL) (void)CloseHandle(process->stdin_write);
    if (process->stdout_read != NULL) (void)CloseHandle(process->stdout_read);
    if (process->stderr_read != NULL) (void)CloseHandle(process->stderr_read);
    if (process->write_operation.hEvent != NULL) {
        (void)CloseHandle(process->write_operation.hEvent);
    }
    if (process->process != NULL) (void)CloseHandle(process->process);
    free(process);
}
