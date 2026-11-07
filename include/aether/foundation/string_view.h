#ifndef AETHER_STRING_VIEW_H
#define AETHER_STRING_VIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "aether/foundation/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherStringView {
    const char* data;
    size_t length;
} AetherStringView;

AetherStatus aether_string_view_validate(AetherStringView view);
AetherStatus aether_string_view_copy(
    AetherStringView view,
    char* destination,
    size_t capacity
);
bool aether_string_view_equal(
    AetherStringView left,
    AetherStringView right
);

#ifdef __cplusplus
}
#endif

#endif
