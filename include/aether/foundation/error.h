#ifndef AETHER_ERROR_H
#define AETHER_ERROR_H
#include <stddef.h>
#include <stdint.h>

#include "aether/foundation/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AETHER_ERROR_MESSAGE_CAPACITY 160U

typedef struct AetherError {
    AetherStatus status;
    int32_t code;
    char message[AETHER_ERROR_MESSAGE_CAPACITY];
} AetherError;

void aether_error_clear(AetherError* error);
AetherStatus aether_error_set(
    AetherError* error,
    AetherStatus status,
    int32_t code,
    const char* message
);

#ifdef __cplusplus
}
#endif

#endif
