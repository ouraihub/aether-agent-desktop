/* Private allocator substitution in launch_guard/process_gate only. */
#undef malloc
#undef calloc
#undef free
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "aether/backend/process_gate.h"
#include "permission_fixture.h"

static size_t calls;
static size_t failing;
static size_t live;
void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);

void* aether_test_malloc(size_t size)
{
    void* result;
    if (++calls == failing) return NULL;
    result = malloc(size);
    if (result != NULL) ++live;
    return result;
}
void* aether_test_calloc(size_t count, size_t size)
{
    void* result;
    if (size != 0U && count > SIZE_MAX / size) return NULL;
    result = aether_test_malloc(count * size);
    if (result != NULL) memset(result, 0, count * size);
    return result;
}
void aether_test_free(void* pointer)
{
    if (pointer != NULL) { CHECK(live != 0U); --live; }
    free(pointer);
}

int wmain(int argc, wchar_t** argv)
{
    wchar_t directory[512];
    char cwd[2048];
    char executable[2048];
    DWORD handles;
    DWORD after;
    AetherWorkspaceRegistry* workspaces = NULL;
    AetherWorkspaceId workspace;
    AetherPermissionInput context = {0};
    AetherPlatformProcessInput native = {0};
    AetherAuthorizedLaunch* launch = NULL;
    size_t allocations;
    CHECK(argc == 2);
    CHECK(GetCurrentDirectoryW(512U, directory) > 0U);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, directory, -1,
        cwd, sizeof(cwd), NULL, NULL) != 0);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[1], -1,
        executable, sizeof(executable), NULL, NULL) != 0);
    OK(aether_workspace_registry_create(1U, 1U, &workspaces));
    OK(aether_workspace_create(workspaces,
        (AetherWorkspaceInput){text("isolated gate"), text(cwd)}, &workspace));
    context.scope.workspace_id = workspace;
    native.executable = text(executable);
    native.working_directory = text(cwd);
    OK(aether_authorized_launch_prepare(workspaces, native, context, &launch));
    allocations = calls;
    aether_authorized_launch_destroy(launch);
    CHECK(live == 0U);
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &handles));
    for (size_t failure = 1U; failure <= allocations; ++failure) {
        calls = 0U;
        failing = failure;
        launch = NULL;
        CHECK(aether_authorized_launch_prepare(workspaces, native, context, &launch) ==
              AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(launch == NULL && live == 0U);
        CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
        CHECK(handles == after);
    }
    OK(aether_workspace_registry_destroy(&workspaces));
    printf("gate allocation: %zu rollback paths, no retained handles or allocations\n",
           allocations);
    return 0;
}
