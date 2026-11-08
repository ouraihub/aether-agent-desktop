#include "path_internal.h"

#include <stdlib.h>
#include <string.h>

AetherStatus aether_platform_path_allocate(
    size_t length,
    AetherPlatformPath** out_path
)
{
    AetherPlatformPath* path;

    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    if (length == 0U || length > SIZE_MAX - sizeof(*path) - 1U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    path = malloc(sizeof(*path) + length + 1U);
    if (path == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    path->data = (char*)(path + 1);
    path->length = length;
    path->data[length] = '\0';
    *out_path = path;
    return AETHER_STATUS_OK;
}

AetherStatus aether_platform_path_from_view(
    AetherStringView input,
    AetherPlatformPath** out_path
)
{
    AetherPlatformPath* path;
    AetherStatus status;

    if (out_path == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    if (input.length == 0U || input.data == NULL ||
        input.length > SIZE_MAX - sizeof(*path) - 1U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (aether_string_view_validate(input) != AETHER_STATUS_OK ||
        memchr(input.data, '\0', input.length) != NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_platform_path_allocate(input.length, &path);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    memcpy(path->data, input.data, input.length);
    *out_path = path;
    return AETHER_STATUS_OK;
}

AetherStringView aether_platform_path_view(const AetherPlatformPath* path)
{
    if (path == NULL) {
        return (AetherStringView){NULL, 0U};
    }
    return (AetherStringView){path->data, path->length};
}

void aether_platform_path_destroy(AetherPlatformPath* path)
{
    free(path);
}
