#include "aether/foundation/error.h"

#include <stdio.h>
#include <string.h>

void aether_error_clear(AetherError* error)
{
    if (error == NULL) {
        return;
    }

    error->status = AETHER_STATUS_OK;
    error->code = 0;
    error->message[0] = '\0';
}

AetherStatus aether_error_set(
    AetherError* error,
    AetherStatus status,
    int32_t code,
    const char* message
)
{
    int written;

    if (error == NULL || message == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    aether_error_clear(error);
    written = snprintf(
        error->message,
        sizeof(error->message),
        "%s",
        message
    );
    if (written < 0 || (size_t)written >= sizeof(error->message)) {
        aether_error_clear(error);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    error->status = status;
    error->code = code;
    return AETHER_STATUS_OK;
}

void aether_error_copy(AetherError* destination, const AetherError* source)
{
    if (destination == NULL || source == NULL || destination == source) {
        return;
    }

    (void)memcpy(destination, source, sizeof(*destination));
}
