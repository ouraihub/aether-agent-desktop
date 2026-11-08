#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wchar.h>
#include "aether/backend/process_gate.h"
#include "aether/platform/launch_guard.h"
#include "aether/platform/platform.h"
#include "permission_fixture.h"

#define PATH_CAPACITY 512U
#define WAIT_NS UINT64_C(30000000000)

typedef struct GateTest {
    PermissionFixture core;
    AetherProcessService* service;
    wchar_t original[32768];
    wchar_t root[PATH_CAPACITY];
    wchar_t cwd[PATH_CAPACITY];
    wchar_t outside[PATH_CAPACITY];
    wchar_t marker[PATH_CAPACITY];
    char executable[PATH_CAPACITY];
    char root_utf8[PATH_CAPACITY];
    char cwd_utf8[PATH_CAPACITY];
    char outside_utf8[PATH_CAPACITY];
    char marker_utf8[PATH_CAPACITY];
} GateTest;

static uint64_t now(void)
{
    AetherPlatformClock clock;
    OK(aether_platform_clock_now(&clock));
    return clock.monotonic_ns;
}

static AetherTraceContext trace(void)
{
    AetherTraceContext result = {0};
    result.trace_id.bytes[0] = 1U;
    result.span_id.bytes[0] = 2U;
    result.valid = true;
    return result;
}

static void utf8(const wchar_t* wide, char* bytes)
{
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1,
                             bytes, (int)PATH_CAPACITY, NULL, NULL) != 0);
}

static void absent(const GateTest* test)
{
    CHECK(GetFileAttributesW(test->marker) == INVALID_FILE_ATTRIBUTES);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
}

static void consume_marker(const GateTest* test, const char* expected)
{
    char bytes[256];
    DWORD size = 0U;
    HANDLE file = CreateFileW(test->marker, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    CHECK(ReadFile(file, bytes, (DWORD)sizeof(bytes), &size, NULL));
    CHECK((size_t)size == strlen(expected));
    CHECK(memcmp(bytes, expected, (size_t)size) == 0);
    CHECK(CloseHandle(file));
    CHECK(DeleteFileW(test->marker));
    absent(test);
}

static void setup_core(GateTest* test)
{
    PermissionFixture* f = &test->core;
    AetherWorkspaceId workspace;
    AetherProviderId provider;
    AetherRuntimeId runtime;
    AetherAgentConfigId agent;
    AetherSessionId session;
    OK(aether_workspace_registry_create(2U, 2U, &f->workspaces));
    OK(aether_workspace_create(f->workspaces,
        (AetherWorkspaceInput){text("Gate fixture"), text(test->root_utf8)}, &workspace));
    OK(aether_runtime_registry_create(2U, 2U, &f->runtimes));
    OK(aether_runtime_provider_register(f->runtimes, text("P"), text("1"), &provider));
    OK(aether_runtime_register(f->runtimes, provider, &runtime));
    OK(aether_runtime_update(f->runtimes, runtime, 1U, AETHER_RUNTIME_READY,
                             (AetherRuntimeCapabilities){0}));
    OK(aether_agent_config_create(f->workspaces,
        (AetherAgentConfigInput){text("A"), provider, text("M")}, &agent));
    OK(aether_session_registry_create(2U, f->workspaces, f->runtimes, &f->sessions));
    OK(aether_session_create(f->sessions, workspace, agent, runtime, 1U, &session));
    OK(aether_session_initialize(f->sessions, session));
    OK(aether_permission_registry_create(aether_permission_default_limits(),
        f->workspaces, f->sessions, f->runtimes, &f->permissions));
    f->input.request_id = aether_id_next();
    f->input.scope = (AetherPermissionScope){workspace, session, runtime, provider,
        aether_id_next(), aether_id_next(), text("untrusted-tool"), text("untrusted-subtool")};
    f->input.runtime_generation = 1U;
    f->input.level = AETHER_PERMISSION_LEVEL_READ;
    f->input.arguments = text("not-native-arguments");
    f->input.capability_supported = true;
    mode(f, AETHER_PERMISSION_MODE_ALWAYS_ASK);
}

static void setup(GateTest* test, const wchar_t* executable)
{
    wchar_t temp[MAX_PATH];
    wchar_t unique[MAX_PATH];
    wchar_t absolute[32768];
    DWORD size = GetCurrentDirectoryW(32768U, test->original);
    CHECK(size != 0U && size < 32768U);
    size = GetFullPathNameW(executable, 32768U, absolute, NULL);
    CHECK(size != 0U && size < 32768U);
    utf8(absolute, test->executable);
    size = GetTempPathW(MAX_PATH, temp);
    CHECK(size != 0U && size < MAX_PATH);
    CHECK(GetTempFileNameW(temp, L"agt", 0U, unique) != 0U);
    CHECK(DeleteFileW(unique));
    CHECK(swprintf(test->root, PATH_CAPACITY, L"%ls-\x03a9", unique) > 0);
    CHECK(swprintf(test->cwd, PATH_CAPACITY, L"%ls\\child-\x4e2d", test->root) > 0);
    CHECK(swprintf(test->outside, PATH_CAPACITY, L"%ls-outside", test->root) > 0);
    CHECK(swprintf(test->marker, PATH_CAPACITY, L"%ls\\effect.txt", test->cwd) > 0);
    CHECK(CreateDirectoryW(test->root, NULL));
    CHECK(CreateDirectoryW(test->cwd, NULL));
    CHECK(CreateDirectoryW(test->outside, NULL));
    CHECK(SetCurrentDirectoryW(test->cwd));
    utf8(test->root, test->root_utf8);
    utf8(test->cwd, test->cwd_utf8);
    utf8(test->outside, test->outside_utf8);
    utf8(test->marker, test->marker_utf8);
    setup_core(test);
    OK(aether_process_service_create((AetherProcessLimits){1U, 127U, 1U, 1024U},
                                     &test->service));
    absent(test);
}

static AetherPlatformProcessInput native_input(GateTest* test,
    const AetherStringView* argv, size_t argc, const AetherProcessEnvironment* env)
{
    AetherPlatformProcessInput input = {0};
    input.executable = text(test->executable);
    input.working_directory = text(test->cwd_utf8);
    input.arguments = argv;
    input.argument_count = argc;
    input.environment = env;
    input.environment_count = 1U;
    return input;
}

static AetherAuthorizedLaunch* prepare(GateTest* test, const char* payload)
{
    AetherStringView argv[] = {text("gate-marker"), text(test->marker_utf8), text(payload)};
    AetherProcessEnvironment env = {text("AETHER_GATE_ENV"), text("allowlisted")};
    AetherAuthorizedLaunch* plan = NULL;
    OK(aether_authorized_launch_prepare(test->core.workspaces,
        native_input(test, argv, 3U, &env), test->core.input, &plan));
    CHECK(plan != NULL);
    return plan;
}

static AetherPermissionId answer(GateTest* test, AetherAuthorizedLaunch* plan,
                                 AetherPermissionDecision decision)
{
    AetherPermissionInput actual = aether_authorized_launch_permission_input(plan);
    AetherPermissionResult result;
    AetherApprovalResult approval;
    uint64_t time = now();
    OK(aether_permission_begin(test->core.permissions, actual, time, time + WAIT_NS, &result));
    CHECK(result.decision == AETHER_PERMISSION_ASK && result.permission_id.id.value != 0U);
    OK(aether_permission_resolve(test->core.permissions, result.permission_id, actual, now(),
                                 decision, AETHER_APPROVAL_ONCE, &approval));
    CHECK(approval.approved == (decision == AETHER_PERMISSION_ALLOW));
    return result.permission_id;
}

static void rejected(GateTest* test, AetherAuthorizedLaunch* plan,
                      AetherPermissionId approval, AetherStatus expected)
{
    AetherProcessRef process = {{UINT64_C(123)}, UINT64_C(456)};
    CHECK(aether_authorized_launch_execute(plan, test->core.permissions, approval,
        test->service, trace(), &process) == expected);
    CHECK(process.id.value == 0U && process.generation == 0U);
    absent(test);
}

static void launched(GateTest* test, AetherAuthorizedLaunch* plan,
                      AetherPermissionId approval, const char* payload)
{
    AetherProcessRef process;
    AetherProcessSnapshot snapshot;
    uint64_t deadline = now() + WAIT_NS;
    OK(aether_authorized_launch_execute(plan, test->core.permissions, approval,
                                        test->service, trace(), &process));
    CHECK(process.id.value != 0U && process.generation != 0U);
    OK(aether_process_close_stdin(test->service, process));
    for (;;) {
        unsigned char bytes[128];
        size_t size;
        AetherStatus status = aether_process_read(test->service, process,
            AETHER_PROCESS_STDOUT, bytes, sizeof(bytes), &size);
        CHECK(status == AETHER_STATUS_OK || status == AETHER_STATUS_NOT_FOUND);
        CHECK(size == 0U);
        status = aether_process_read(test->service, process, AETHER_PROCESS_STDERR,
                                     bytes, sizeof(bytes), &size);
        CHECK(status == AETHER_STATUS_OK || status == AETHER_STATUS_NOT_FOUND);
        CHECK(size == 0U);
        OK(aether_process_query(test->service, process, &snapshot));
        CHECK(snapshot.io_status == AETHER_STATUS_OK);
        if (snapshot.close_complete) break;
        CHECK(now() < deadline);
        Sleep(1U);
    }
    CHECK(snapshot.exited && snapshot.exit_code == 0U);
    CHECK(snapshot.stdout_eof && snapshot.stderr_eof);
    OK(aether_process_close(test->service, process));
    OK(aether_process_release(test->service, process));
    consume_marker(test, payload);
}

static bool equal(AetherStringView left, AetherStringView right)
{
    return left.length == right.length &&
        (left.length == 0U || memcmp(left.data, right.data, left.length) == 0);
}

static void identity_and_copy(GateTest* test)
{
    char payload[] = "original-\xce\xa9\xe4\xb8\xad";
    char marker[PATH_CAPACITY];
    char env_value[] = "allowlisted";
    AetherStringView argv[] = {text("gate-marker"), {0}, text(payload)};
    AetherProcessEnvironment env = {text("AETHER_GATE_ENV"), text(env_value)};
    AetherAuthorizedLaunch* plan = NULL;
    AetherAuthorizedLaunch* same;
    AetherPlatformLaunchGuard* guard = NULL;
    AetherPermissionInput actual;
    AetherPermissionId approval;
    AetherPermissionTarget fake = {text("C:/fake/never-authoritative")};
    memcpy(marker, test->marker_utf8, strlen(test->marker_utf8) + 1U);
    argv[1] = text(marker);
    test->core.input.targets = &fake;
    test->core.input.target_count = 1U;
    OK(aether_authorized_launch_prepare(test->core.workspaces,
        native_input(test, argv, 3U, &env), test->core.input, &plan));
    test->core.input.targets = NULL;
    test->core.input.target_count = 0U;
    same = prepare(test, payload);
    actual = aether_authorized_launch_permission_input(plan);
    CHECK(equal(actual.arguments, aether_authorized_launch_permission_input(same).arguments));
    CHECK(equal(actual.scope.tool, text("process.spawn")) && actual.scope.subtool.length == 0U);
    CHECK(actual.level == AETHER_PERMISSION_LEVEL_EXEC && actual.target_count == 2U);
    CHECK(actual.request_id.value == test->core.input.request_id.value);
    CHECK(actual.scope.workspace_id.id.value == test->core.input.scope.workspace_id.id.value);
    CHECK(actual.scope.session_id.id.value == test->core.input.scope.session_id.id.value);
    CHECK(actual.scope.runtime_id.id.value == test->core.input.scope.runtime_id.id.value);
    CHECK(actual.scope.provider_id.id.value == test->core.input.scope.provider_id.id.value);
    CHECK(actual.scope.subject_id.value == test->core.input.scope.subject_id.value);
    CHECK(actual.scope.executor_id.value == test->core.input.scope.executor_id.value);
    CHECK(actual.runtime_generation == test->core.input.runtime_generation);
    OK(aether_platform_launch_guard_create(text(test->executable), text(test->cwd_utf8),
                                           text(test->root_utf8), &guard));
    CHECK(equal(actual.targets[0].identity, aether_platform_launch_guard_executable(guard)));
    CHECK(equal(actual.targets[1].identity, aether_platform_launch_guard_cwd(guard)));
    CHECK(memchr(actual.targets[0].identity.data, '\\', actual.targets[0].identity.length) == NULL);
    CHECK(memchr(actual.targets[1].identity.data, '\\', actual.targets[1].identity.length) == NULL);
    aether_platform_launch_guard_destroy(guard);
    approval = answer(test, plan, AETHER_PERMISSION_ALLOW);
    memset(payload, 'x', strlen(payload));
    memset(marker, 'x', strlen(marker));
    memset(env_value, 'x', strlen(env_value));
    launched(test, plan, approval, "original-\xce\xa9\xe4\xb8\xad");
    rejected(test, plan, approval, AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_release(test->core.permissions, approval));
    aether_authorized_launch_destroy(same);
    aether_authorized_launch_destroy(plan);
    puts("gate: canonical context/copies, actual Unicode marker, once replay rejected");
}

static void denied_and_changed(GateTest* test)
{
    AetherAuthorizedLaunch* first = prepare(test, "approved");
    AetherAuthorizedLaunch* changed = prepare(test, "changed");
    AetherPermissionId approval;
    AetherPermissionSnapshot snapshot;
    rejected(test, first, (AetherPermissionId){0}, AETHER_STATUS_PERMISSION_DENIED);
    rejected(test, first, (AetherPermissionId){aether_id_next()}, AETHER_STATUS_NOT_FOUND);
    approval = answer(test, first, AETHER_PERMISSION_DENY);
    rejected(test, first, approval, AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_release(test->core.permissions, approval));
    CHECK(!equal(aether_authorized_launch_permission_input(first).arguments,
                 aether_authorized_launch_permission_input(changed).arguments));
    approval = answer(test, first, AETHER_PERMISSION_ALLOW);
    rejected(test, changed, approval, AETHER_STATUS_INVALID_ARGUMENT);
    OK(aether_permission_query(test->core.permissions, approval, now(), &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_APPROVED);
    launched(test, first, approval, "approved");
    OK(aether_permission_release(test->core.permissions, approval));
    aether_authorized_launch_destroy(first);
    {
        AetherStringView argv[] = {text("gate-marker"), text(test->marker_utf8), text("env")};
        AetherProcessEnvironment original = {text("AETHER_GATE_ENV"), text("allowlisted")};
        AetherProcessEnvironment modified = {text("AETHER_GATE_ENV"), text("different")};
        AetherAuthorizedLaunch* original_plan = NULL;
        AetherAuthorizedLaunch* changed_plan = NULL;
        OK(aether_authorized_launch_prepare(test->core.workspaces,
            native_input(test, argv, 3U, &original), test->core.input, &original_plan));
        OK(aether_authorized_launch_prepare(test->core.workspaces,
            native_input(test, argv, 3U, &modified), test->core.input, &changed_plan));
        approval = answer(test, original_plan, AETHER_PERMISSION_ALLOW);
        rejected(test, changed_plan, approval, AETHER_STATUS_INVALID_ARGUMENT);
        OK(aether_permission_revoke(test->core.permissions, approval));
        OK(aether_permission_release(test->core.permissions, approval));
        aether_authorized_launch_destroy(changed_plan);
        aether_authorized_launch_destroy(original_plan);
    }
    aether_authorized_launch_destroy(changed);
    puts("gate: missing/rejected token and changed argv leave no file; original approval survives");
}

static void policy_and_scope(GateTest* test)
{
    AetherAuthorizedLaunch* plan = prepare(test, "new-deny");
    AetherPermissionId approval = answer(test, plan, AETHER_PERMISSION_ALLOW);
    AetherPermissionRule deny = rule(AETHER_PERMISSION_DENY);
    AetherStringView argv[] = {text("gate-marker"), text(test->marker_utf8), text("outside")};
    AetherProcessEnvironment env = {text("AETHER_GATE_ENV"), text("allowlisted")};
    AetherPlatformProcessInput input = native_input(test, argv, 3U, &env);
    AetherAuthorizedLaunch* outside = plan;
    deny.scope.workspace_id = test->core.input.scope.workspace_id;
    deny.scope.tool = text("process.spawn");
    OK(aether_permission_put_rule(test->core.permissions, deny));
    rejected(test, plan, approval, AETHER_STATUS_INVALID_ARGUMENT);
    rejected(test, plan, (AetherPermissionId){0}, AETHER_STATUS_PERMISSION_DENIED);
    OK(aether_permission_release(test->core.permissions, approval));
    OK(aether_permission_remove_rule(test->core.permissions, deny.rule_id));
    input.working_directory = text(test->outside_utf8);
    CHECK(aether_authorized_launch_prepare(test->core.workspaces, input, test->core.input,
        &outside) == AETHER_STATUS_PERMISSION_DENIED);
    CHECK(outside == NULL);
    absent(test);
    aether_authorized_launch_destroy(plan);
    test->core.input.capability_supported = false;
    plan = prepare(test, "no-capability");
    CHECK(!aether_authorized_launch_permission_input(plan).capability_supported);
    rejected(test, plan, (AetherPermissionId){0}, AETHER_STATUS_NOT_SUPPORTED);
    aether_authorized_launch_destroy(plan);
    test->core.input.capability_supported = true;
    test->core.input.hard_deny = true;
    test->core.input.noncacheable_ask = true;
    plan = prepare(test, "hard-deny");
    CHECK(aether_authorized_launch_permission_input(plan).hard_deny);
    CHECK(aether_authorized_launch_permission_input(plan).noncacheable_ask);
    rejected(test, plan, (AetherPermissionId){0}, AETHER_STATUS_PERMISSION_DENIED);
    aether_authorized_launch_destroy(plan);
    test->core.input.hard_deny = false;
    test->core.input.noncacheable_ask = false;
    puts("gate: fresh Deny, sibling-prefix cwd, capability and safety preserve no-file boundary");
}

static void expiry_revocation_and_pinning(GateTest* test)
{
    AetherAuthorizedLaunch* plan = prepare(test, "expired");
    AetherPermissionInput actual = aether_authorized_launch_permission_input(plan);
    AetherPermissionResult pending;
    AetherPermissionId approval;
    AetherApprovalResult granted;
    wchar_t renamed[PATH_CAPACITY];
    /* Explicit old monotonic interval: no sleeps are needed to prove expiry. */
    OK(aether_permission_begin(test->core.permissions, actual, 0U, 1U, &pending));
    OK(aether_permission_resolve(test->core.permissions, pending.permission_id,
        actual, 0U, AETHER_PERMISSION_ALLOW, AETHER_APPROVAL_ONCE, &granted));
    rejected(test, plan, pending.permission_id, AETHER_STATUS_TIMEOUT);
    OK(aether_permission_release(test->core.permissions, pending.permission_id));
    aether_authorized_launch_destroy(plan);
    plan = prepare(test, "revoked");
    approval = answer(test, plan, AETHER_PERMISSION_ALLOW);
    OK(aether_permission_revoke(test->core.permissions, approval));
    rejected(test, plan, approval, AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_release(test->core.permissions, approval));
    CHECK(SetCurrentDirectoryW(test->original));
    {
        wchar_t executable[PATH_CAPACITY];
        HANDLE writable;
        CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, test->executable,
            -1, executable, PATH_CAPACITY) != 0);
        writable = CreateFileW(executable, GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        CHECK(writable == INVALID_HANDLE_VALUE);
        CHECK(GetLastError() == ERROR_SHARING_VIOLATION || GetLastError() == ERROR_ACCESS_DENIED);
    }
    CHECK(swprintf(renamed, PATH_CAPACITY, L"%ls-moved", test->root) > 0);
    CHECK(!MoveFileW(test->root, renamed));
    CHECK(GetLastError() == ERROR_SHARING_VIOLATION || GetLastError() == ERROR_ACCESS_DENIED);
    aether_authorized_launch_destroy(plan);
    CHECK(MoveFileW(test->root, renamed));
    CHECK(MoveFileW(renamed, test->root));
    CHECK(SetCurrentDirectoryW(test->cwd));
    absent(test);
    puts("gate: expired/revoked approvals produce no file; actual root pinned until release");
}

static void direct_and_spawn_failure(GateTest* test)
{
    AetherAuthorizedLaunch* plan;
    AetherPermissionId approval;
    AetherPermissionSnapshot snapshot;
    uint64_t deadline;
    mode(&test->core, AETHER_PERMISSION_MODE_YOLO);
    plan = prepare(test, "direct");
    launched(test, plan, (AetherPermissionId){0}, "direct");
    rejected(test, plan, (AetherPermissionId){0}, AETHER_STATUS_ALREADY_EXISTS);
    aether_authorized_launch_destroy(plan);
    mode(&test->core, AETHER_PERMISSION_MODE_ALWAYS_ASK);
    plan = prepare(test, "closed-service");
    approval = answer(test, plan, AETHER_PERMISSION_ALLOW);
    OK(aether_process_service_close(test->service));
    deadline = now() + WAIT_NS;
    while (!aether_process_service_is_closed(test->service)) {
        CHECK(now() < deadline);
        Sleep(1U);
    }
    rejected(test, plan, approval, AETHER_STATUS_NOT_INITIALIZED);
    OK(aether_permission_query(test->core.permissions, approval, now(), &snapshot));
    CHECK(snapshot.state == AETHER_PERMISSION_CONSUMED);
    rejected(test, plan, approval, AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_permission_release(test->core.permissions, approval));
    aether_authorized_launch_destroy(plan);
    puts("gate: direct plan one-shot; spawn failure consumes approval without retry or file");
}

int wmain(int argc, wchar_t** argv)
{
    static GateTest test;
    CHECK(argc == 2);
    setup(&test, argv[1]);
    identity_and_copy(&test);
    denied_and_changed(&test);
    policy_and_scope(&test);
    expiry_revocation_and_pinning(&test);
    direct_and_spawn_failure(&test);
    OK(aether_process_service_destroy(&test.service));
    dispose(&test.core);
    CHECK(SetCurrentDirectoryW(test.original));
    CHECK(RemoveDirectoryW(test.cwd));
    CHECK(RemoveDirectoryW(test.outside));
    CHECK(RemoveDirectoryW(test.root));
    return EXIT_SUCCESS;
}
