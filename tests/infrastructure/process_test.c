#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "aether/infrastructure/process.h"
#include "aether/platform/platform.h"

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #expression); \
        return false; \
    } \
} while (0)
#define CAPACITY (257U * 1024U)
#define DEADLINE_NS UINT64_C(30000000000)

typedef struct Test {
    AetherProcessService* service;
    char executable[32768];
    char cwd[32768];
    unsigned char output[2][CAPACITY];
    size_t size[2];
    AetherProcessSnapshot snapshot;
} Test;

static AetherStringView view(const char* value)
{
    AetherStringView result = {value, strlen(value)};
    return result;
}

static AetherTraceContext trace(void)
{
    AetherTraceContext result = {0};
    result.trace_id.bytes[0] = 1U;
    result.span_id.bytes[0] = 2U;
    result.valid = true;
    return result;
}

static uint64_t now(void)
{
    AetherPlatformClock clock;
    if (aether_platform_clock_now(&clock) != AETHER_STATUS_OK) {
        fprintf(stderr, "Platform clock failed\n");
        exit(EXIT_FAILURE);
    }
    return clock.monotonic_ns;
}

static AetherPlatformProcessInput input(Test* test, const AetherStringView* arguments,
                                        size_t count)
{
    AetherPlatformProcessInput result = {0};
    result.executable = view(test->executable);
    result.working_directory = view(test->cwd);
    result.arguments = arguments;
    result.argument_count = count;
    return result;
}

static void reset_output(Test* test)
{
    test->size[0] = 0U;
    test->size[1] = 0U;
}

static bool poll_output(Test* test, AetherProcessRef process)
{
    size_t stream;
    for (stream = 0U; stream < 2U; ++stream) {
        unsigned char bytes[521];
        size_t size = 0U;
        AetherStatus status = aether_process_read(test->service, process,
            (AetherProcessStream)stream, bytes, sizeof(bytes), &size);
        CHECK(status == AETHER_STATUS_OK || status == AETHER_STATUS_NOT_FOUND);
        CHECK(test->size[stream] + size <= CAPACITY);
        memcpy(test->output[stream] + test->size[stream], bytes, size);
        test->size[stream] += size;
    }
    CHECK(aether_process_query(test->service, process, &test->snapshot) == AETHER_STATUS_OK);
    CHECK(test->snapshot.stdout_buffered <= 127U && test->snapshot.stderr_buffered <= 127U);
    CHECK(test->snapshot.io_status == AETHER_STATUS_OK);
    return true;
}

static bool drain(Test* test, AetherProcessRef process)
{
    uint64_t deadline = now() + DEADLINE_NS;
    do {
        CHECK(poll_output(test, process));
        if (test->snapshot.exited && test->snapshot.stdout_eof &&
            test->snapshot.stderr_eof && test->snapshot.stdout_buffered == 0U &&
            test->snapshot.stderr_buffered == 0U && test->snapshot.close_complete) {
            return true;
        }
    } while (now() < deadline);
    CHECK(false);
}

static bool spawn_mode(Test* test, const char* mode, AetherProcessRef* process)
{
    AetherStringView argument = view(mode);
    reset_output(test);
    CHECK(aether_process_spawn(test->service, input(test, &argument, 1U), trace(), process)
          == AETHER_STATUS_OK);
    return true;
}

static bool release(Test* test, AetherProcessRef process)
{
    AetherProcessSnapshot snapshot;
    CHECK(aether_process_close(test->service, process) == AETHER_STATUS_OK);
    CHECK(aether_process_release(test->service, process) == AETHER_STATUS_OK);
    memset(&snapshot, 0xA5, sizeof(snapshot));
    CHECK(aether_process_query(test->service, process, &snapshot) == AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.process.id.value == 0U && !snapshot.exited);
    return true;
}

static bool validation(Test* test)
{
    AetherStringView arguments[] = {
        {0}, {0}, {0}, {0}, {0}, {0}, {0}
    };
    AetherProcessEnvironment environment;
    AetherPlatformProcessInput launch;
    AetherProcessRef process;
    uint64_t deadline;
    arguments[0] = view("validate");
    arguments[1] = view("");
    arguments[2] = view("two words");
    arguments[3] = view("quote\"inside");
    arguments[4] = view("tail space\\");
    arguments[5] = view("\xCE\xA9\xE4\xB8\xAD");
    arguments[6] = view(test->cwd);
    environment.name = view("AETHER_PROCESS_ALLOWED");
    environment.value = view("present");
    launch = input(test, arguments, 7U);
    launch.environment = &environment;
    launch.environment_count = 1U;
    CHECK(SetEnvironmentVariableW(L"AETHER_PROCESS_AMBIENT", L"must-not-leak"));
    reset_output(test);
    CHECK(aether_process_spawn(test->service, launch, trace(), &process) == AETHER_STATUS_OK);
    CHECK(SetEnvironmentVariableW(L"AETHER_PROCESS_AMBIENT", NULL));
    deadline = now() + DEADLINE_NS;
    do {
        CHECK(aether_process_query(test->service, process, &test->snapshot) == AETHER_STATUS_OK);
        if (test->snapshot.exited) break;
    } while (now() < deadline);
    if (test->snapshot.exit_code != 0U) {
        fprintf(stderr, "validation child exit=%lu\n", (unsigned long)test->snapshot.exit_code);
    }
    CHECK(test->snapshot.exited && test->snapshot.exit_code == 0U);
    CHECK(test->snapshot.stdout_buffered != 0U || test->snapshot.stderr_buffered != 0U);
    CHECK(aether_process_release(test->service, process) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(drain(test, process));
    CHECK(test->size[0] == strlen("argv-env-cwd-ok\n"));
    CHECK(memcmp(test->output[0], "argv-env-cwd-ok\n", test->size[0]) == 0);
    CHECK(test->size[1] == strlen("validation-stderr\n"));
    CHECK(memcmp(test->output[1], "validation-stderr\n", test->size[1]) == 0);
    CHECK(release(test, process));
    return true;
}

static bool echo(Test* test)
{
    static unsigned char bytes[32768];
    AetherProcessRef process;
    AetherProcessRef rejected = {{123U}, 123U};
    AetherStringView argument = view("echo");
    AetherId write_id;
    AetherId rejected_write = {123U};
    AetherProcessWriteResult result;
    size_t i;
    for (i = 0U; i < sizeof(bytes); ++i) {
        bytes[i] = (unsigned char)(i % 256U);
    }
    CHECK(spawn_mode(test, "echo", &process));
    CHECK(aether_process_spawn(test->service, input(test, &argument, 1U), trace(), &rejected)
          == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.id.value == 0U && rejected.generation == 0U);
    CHECK(aether_process_write(test->service, process, bytes, sizeof(bytes), &write_id)
          == AETHER_STATUS_OK);
    memset(bytes, 0xA5, sizeof(bytes));
    CHECK(aether_process_write(test->service, process, bytes, 1U, &rejected_write)
          == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected_write.value == 0U);
    CHECK(aether_process_close_stdin(test->service, process) == AETHER_STATUS_OK);
    CHECK(aether_process_write(test->service, process, bytes, 1U, &rejected_write)
          == AETHER_STATUS_NOT_INITIALIZED);
    CHECK(rejected_write.value == 0U);
    CHECK(drain(test, process));
    CHECK(aether_process_release(test->service, process) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(aether_process_take_write(test->service, process, &result) == AETHER_STATUS_OK);
    CHECK(result.status == AETHER_STATUS_OK);
    CHECK(result.write_id.value == write_id.value && result.bytes_written == sizeof(bytes));
    CHECK(test->snapshot.exit_code == 23U);
    CHECK(test->size[0] == sizeof(bytes));
    for (i = 0U; i < sizeof(bytes); ++i) {
        bytes[i] = (unsigned char)(i % 256U);
    }
    CHECK(memcmp(test->output[0], bytes, sizeof(bytes)) == 0);
    CHECK(test->size[1] == strlen("stdin-eof\n"));
    CHECK(memcmp(test->output[1], "stdin-eof\n", test->size[1]) == 0);
    CHECK(aether_process_take_write(test->service, process, &result) == AETHER_STATUS_NOT_FOUND);
    CHECK(result.write_id.value == 0U && result.bytes_written == 0U);
    CHECK(release(test, process));
    return true;
}

static bool flood(Test* test)
{
    AetherProcessRef process;
    size_t i;
    CHECK(spawn_mode(test, "flood", &process));
    CHECK(aether_process_close_stdin(test->service, process) == AETHER_STATUS_OK);
    CHECK(drain(test, process));
    CHECK(test->snapshot.exit_code == 0U);
    CHECK(test->size[0] == CAPACITY && test->size[1] == CAPACITY);
    for (i = 0U; i < CAPACITY; ++i) {
        CHECK(test->output[0][i] == (unsigned char)(i % 251U));
        CHECK(test->output[1][i] == (unsigned char)((i * 3U + 7U) % 251U));
    }
    CHECK(release(test, process));
    return true;
}

static bool eof_before_exit(Test* test)
{
    AetherProcessRef process;
    uint64_t deadline;
    CHECK(spawn_mode(test, "eof", &process));
    deadline = now() + DEADLINE_NS;
    do {
        CHECK(poll_output(test, process));
        if (test->snapshot.stdout_eof && test->snapshot.stderr_eof &&
            test->snapshot.stdout_buffered == 0U && test->snapshot.stderr_buffered == 0U) {
            break;
        }
    } while (now() < deadline);
    CHECK(test->snapshot.stdout_eof && test->snapshot.stderr_eof);
    CHECK(!test->snapshot.exited && !test->snapshot.close_complete);
    CHECK(test->size[0] == strlen("pipes-closed\n") && test->size[1] == 0U);
    CHECK(memcmp(test->output[0], "pipes-closed\n", test->size[0]) == 0);
    CHECK(aether_process_close_stdin(test->service, process) == AETHER_STATUS_OK);
    CHECK(drain(test, process));
    CHECK(test->snapshot.exit_code == 17U);
    CHECK(release(test, process));
    return true;
}

static bool stop_hang(Test* test)
{
    AetherProcessRef process;
    uint64_t deadline;
    uint64_t started;
    CHECK(spawn_mode(test, "hang", &process));
    deadline = now() + DEADLINE_NS;
    do {
        CHECK(poll_output(test, process));
        if (test->size[0] == strlen("ready\n")) {
            break;
        }
    } while (now() < deadline);
    CHECK(test->size[0] == strlen("ready\n") && !test->snapshot.exited);
    CHECK(memcmp(test->output[0], "ready\n", test->size[0]) == 0);
    started = now();
    CHECK(aether_process_stop(test->service, process) == AETHER_STATUS_OK);
    CHECK(drain(test, process));
    CHECK(now() - started >= UINT64_C(1000000000));
    CHECK(test->snapshot.stop_requested && !test->snapshot.borrowed);
    CHECK(release(test, process));
    return true;
}

static bool failed_spawn_cleanup(Test* test)
{
    wchar_t directory[32768];
    wchar_t temporary[32768];
    wchar_t invalid[32768];
    char invalid_utf8[32768];
    AetherPlatformProcessInput launch = input(test, NULL, 0U);
    DWORD before;
    DWORD after;
    unsigned int i;
    DWORD length = GetTempPathW(32768U, directory);
    CHECK(length != 0U && length < 32768U);
    CHECK(GetTempFileNameW(directory, L"aep", 0U, temporary) != 0U);
    CHECK(wcscpy_s(invalid, 32768U, temporary) == 0);
    CHECK(wcscat_s(invalid, 32768U, L".exe") == 0);
    CHECK(MoveFileW(temporary, invalid));
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, invalid, -1,
                             invalid_utf8, (int)sizeof(invalid_utf8), NULL, NULL) != 0);
    launch.executable = view(invalid_utf8);
    /* Exercise one failed native image open before measuring repeated cleanup;
     * the loader may lazily initialize process-wide native support resources. */
    {
        AetherProcessRef process;
        CHECK(aether_process_spawn(test->service, launch, trace(), &process) ==
              AETHER_STATUS_INVALID_ARGUMENT);
        CHECK(process.id.value == 0U);
    }
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &before));
    for (i = 0U; i < 32U; ++i) {
        AetherProcessRef process = {{123U}, 123U};
        AetherStatus status = aether_process_spawn(test->service, launch, trace(), &process);
        CHECK(status == AETHER_STATUS_INVALID_ARGUMENT);
        CHECK(process.id.value == 0U && process.generation == 0U);
    }
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
    if (after != before) {
        fprintf(stderr, "failed spawn handles before=%lu after=%lu\n",
                (unsigned long)before, (unsigned long)after);
    }
    CHECK(after == before);
    CHECK(DeleteFileW(invalid));
    return true;
}

static bool borrowed(Test* test, const wchar_t* executable)
{
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    wchar_t command[32768];
    AetherProcessRef process;
    AetherId write_id = {123U};
    DWORD exit_code = 0U;
    uint64_t deadline;
    bool okay = false;
    if (swprintf_s(command, 32768U, L"\"%ls\" external", executable) < 0) {
        return false;
    }
    startup.cb = (DWORD)sizeof(startup);
    CHECK(CreateProcessW(executable, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                         NULL, NULL, &startup, &child));
    CloseHandle(child.hThread);
    if (aether_process_attach(test->service, child.dwProcessId, trace(), &process)
        == AETHER_STATUS_OK) {
        okay = aether_process_query(test->service, process, &test->snapshot) == AETHER_STATUS_OK;
        okay = okay && test->snapshot.borrowed && !test->snapshot.exited;
        okay = okay && test->snapshot.stdout_eof && test->snapshot.stderr_eof;
        okay = okay && aether_process_stop(test->service, process)
            == AETHER_STATUS_PERMISSION_DENIED;
        okay = okay && aether_process_write(test->service, process,
            (const unsigned char*)"x", 1U, &write_id) != AETHER_STATUS_OK;
        okay = okay && write_id.value == 0U;
        okay = okay && aether_process_close(test->service, process) == AETHER_STATUS_OK;
        deadline = now() + DEADLINE_NS;
        while (okay) {
            okay = aether_process_query(test->service, process, &test->snapshot)
                == AETHER_STATUS_OK;
            if (!okay || test->snapshot.close_complete || now() >= deadline) {
                break;
            }
        }
        okay = okay && test->snapshot.close_complete;
        okay = okay && aether_process_release(test->service, process) == AETHER_STATUS_OK;
        okay = okay && GetExitCodeProcess(child.hProcess, &exit_code) && exit_code == STILL_ACTIVE;
    }
    /* This external child belongs to the native test, never the Process Service. */
    if (!TerminateProcess(child.hProcess, 0U) ||
        WaitForSingleObject(child.hProcess, 30000U) != WAIT_OBJECT_0) {
        okay = false;
    }
    CloseHandle(child.hProcess);
    CHECK(okay);
    return true;
}

static bool owned_tree(Test* test)
{
    AetherProcessRef process;
    HANDLE descendant;
    char number[32];
    unsigned long pid;
    uint64_t deadline = now() + DEADLINE_NS;
    CHECK(spawn_mode(test, "tree", &process));
    do {
        CHECK(poll_output(test, process));
        if (test->snapshot.exited && test->size[0] != 0U) break;
    } while (now() < deadline);
    CHECK(test->snapshot.exited && test->snapshot.exit_code == 0U);
    CHECK(!test->snapshot.close_complete && test->size[0] < sizeof(number));
    memcpy(number, test->output[0], test->size[0]);
    number[test->size[0]] = '\0';
    pid = strtoul(number, NULL, 10);
    CHECK(pid != 0UL);
    descendant = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    CHECK(descendant != NULL && WaitForSingleObject(descendant, 0U) == WAIT_TIMEOUT);
    CHECK(aether_process_close(test->service, process) == AETHER_STATUS_OK);
    CHECK(drain(test, process));
    CHECK(WaitForSingleObject(descendant, 1000U) == WAIT_OBJECT_0);
    CHECK(CloseHandle(descendant));
    CHECK(release(test, process));
    return true;
}

static bool run(Test* test, const wchar_t* executable)
{
    AetherProcessLimits limits = {1U, 127U, 1U, 32768U};
    uint64_t deadline;
    CHECK(aether_process_service_create(limits, &test->service) == AETHER_STATUS_OK);
    CHECK(validation(test));
    CHECK(echo(test));
    CHECK(flood(test));
    CHECK(eof_before_exit(test));
    CHECK(stop_hang(test));
    CHECK(failed_spawn_cleanup(test));
    CHECK(borrowed(test, executable));
    CHECK(owned_tree(test));
    CHECK(aether_process_service_close(test->service) == AETHER_STATUS_OK);
    deadline = now() + DEADLINE_NS;
    while (!aether_process_service_is_closed(test->service) && now() < deadline) {
    }
    CHECK(aether_process_service_is_closed(test->service));
    CHECK(aether_process_service_destroy(&test->service) == AETHER_STATUS_OK);
    CHECK(test->service == NULL);
    return true;
}

int wmain(int argc, wchar_t** argv)
{
    static Test test;
    wchar_t executable[32768];
    wchar_t directory[32768];
    wchar_t* separator;
    DWORD length;
    if (argc != 2) {
        fprintf(stderr, "Expected absolute process_child executable argument\n");
        return EXIT_FAILURE;
    }
    length = GetFullPathNameW(argv[1], 32768U, executable, NULL);
    if (length == 0U || length >= 32768U) {
        return EXIT_FAILURE;
    }
    memcpy(directory, executable, ((size_t)length + 1U) * sizeof(wchar_t));
    separator = wcsrchr(directory, L'\\');
    if (separator == NULL) {
        return EXIT_FAILURE;
    }
    *separator = L'\0';
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, executable, -1,
                           test.executable, (int)sizeof(test.executable), NULL, NULL) == 0 ||
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, directory, -1,
                           test.cwd, (int)sizeof(test.cwd), NULL, NULL) == 0) {
        return EXIT_FAILURE;
    }
    return run(&test, executable) ? EXIT_SUCCESS : EXIT_FAILURE;
}
