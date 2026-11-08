/* Private substitution in Windows process implementation sources only. */
#undef malloc
#undef calloc
#undef free
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "aether/platform/process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "native process allocation:%d\n", __LINE__); \
    exit(EXIT_FAILURE); } } while (0)
static size_t calls;
static size_t failing;
static size_t live;
void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);
void* aether_test_malloc(size_t size)
{
    void* pointer;
    if (++calls == failing) return NULL;
    pointer = malloc(size);
    if (pointer != NULL) ++live;
    return pointer;
}
void* aether_test_calloc(size_t count, size_t size)
{
    void* pointer;
    if (size != 0U && count > SIZE_MAX / size) return NULL;
    pointer = aether_test_malloc(count * size);
    if (pointer != NULL) memset(pointer, 0, count * size);
    return pointer;
}
void aether_test_free(void* pointer)
{
    if (pointer != NULL) { CHECK(live != 0U); --live; }
    free(pointer);
}
static AetherStringView view(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}
int main(void)
{
    AetherPlatformProcessInput input = {0};
    AetherPlatformProcess* process = NULL;
    AetherStringView arguments[] = { {"", 0U}, {"two words", 9U}, {"quote\"", 6U} };
    AetherProcessEnvironment environment[] = {
        {{"Z_TEST", 6U}, {"last", 4U}}, {{"A_TEST", 6U}, {"", 0U}}
    };
    wchar_t directory[MAX_PATH];
    wchar_t temporary[MAX_PATH];
    wchar_t invalid[MAX_PATH];
    char file_utf8[1024];
    char cwd_utf8[1024];
    DWORD before;
    DWORD after;
    size_t allocations;
    CHECK(GetTempPathW(MAX_PATH, directory) != 0U);
    CHECK(GetTempFileNameW(directory, L"aen", 0U, temporary) != 0U);
    CHECK(swprintf(invalid, MAX_PATH, L"%ls.exe", temporary) > 0);
    CHECK(MoveFileW(temporary, invalid));
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, invalid, -1,
        file_utf8, sizeof(file_utf8), NULL, NULL) != 0);
    CHECK(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, directory, -1,
        cwd_utf8, sizeof(cwd_utf8), NULL, NULL) != 0);
    input.executable = view(file_utf8);
    input.working_directory = view(cwd_utf8);
    input.arguments = arguments;
    input.argument_count = sizeof(arguments) / sizeof(arguments[0]);
    input.environment = environment;
    input.environment_count = sizeof(environment) / sizeof(environment[0]);
    CHECK(aether_platform_process_spawn(input, &process) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(process == NULL && live == 0U);
    allocations = calls;
    CHECK(allocations > 0U);
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &before));
    for (size_t failure = 1U; failure <= allocations; ++failure) {
        calls = 0U;
        failing = failure;
        CHECK(aether_platform_process_spawn(input, &process) == AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(process == NULL && live == 0U);
        CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
        CHECK(before == after);
    }
    CHECK(DeleteFileW(invalid));
    printf("Native process allocation failures: %zu rollback paths, no live allocations\n",
           allocations);
    return 0;
}
