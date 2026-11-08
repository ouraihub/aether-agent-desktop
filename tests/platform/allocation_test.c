/* This target alone substitutes the allocator in platform path sources. */
#undef malloc
#undef free
#include "aether/platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t calls;
static size_t fail_at;
static size_t outstanding;

void* aether_test_malloc(size_t size);
void aether_test_free(void* pointer);

void* aether_test_malloc(size_t size)
{
    ++calls;
    if (calls == fail_at) {
        return NULL;
    }
    void* pointer = malloc(size);
    if (pointer != NULL) {
        ++outstanding;
    }
    return pointer;
}

void aether_test_free(void* pointer)
{
    if (pointer != NULL) {
        --outstanding;
    }
    free(pointer);
}

static int exercise(int current)
{
    AetherPlatformPath* path = NULL;
    AetherStatus status;

    calls = 0U;
    if (current) {
        status = aether_platform_path_current(&path);
    } else {
        const char input[] = "work/\xE4\xB8\xAD\xE6\x96\x87";
        status = aether_platform_path_from_view(
            (AetherStringView){input, sizeof(input) - 1U}, &path);
    }
    if (status == AETHER_STATUS_OK) {
        AetherStringView view = aether_platform_path_view(path);
        if (aether_string_view_validate(view) != AETHER_STATUS_OK) {
            aether_platform_path_destroy(path);
            return 0;
        }
        aether_platform_path_destroy(path);
    } else if (status != AETHER_STATUS_OUT_OF_MEMORY || path != NULL) {
        return 0;
    }
    return outstanding == 0U &&
           ((fail_at <= calls) == (status == AETHER_STATUS_OUT_OF_MEMORY));
}

int main(void)
{
    for (int current = 0; current <= 1; ++current) {
        for (size_t failure = 1U; failure <= 3U; ++failure) {
            fail_at = failure;
            if (!exercise(current)) {
                fprintf(stderr, "allocation rollback failed: %d/%zu\n",
                        current, failure);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
