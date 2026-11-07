#include "aether/foundation/string_view.h"

#include <string.h>

static bool is_continuation(unsigned char value)
{
    return (value & 0xC0U) == 0x80U;
}

AetherStatus aether_string_view_validate(AetherStringView view)
{
    size_t index = 0U;

    if (view.length != 0U && view.data == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    while (index < view.length) {
        unsigned char first = (unsigned char)view.data[index];
        size_t continuation_count;
        unsigned char minimum;
        unsigned char maximum;

        if (first <= 0x7FU) {
            ++index;
            continue;
        }
        if (first >= 0xC2U && first <= 0xDFU) {
            continuation_count = 1U;
            minimum = 0x80U;
            maximum = 0xBFU;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            continuation_count = 2U;
            minimum = 0x80U;
            maximum = 0xBFU;
            if (first == 0xE0U) {
                minimum = 0xA0U;
            } else if (first == 0xEDU) {
                maximum = 0x9FU;
            }
        } else if (first >= 0xF0U && first <= 0xF4U) {
            continuation_count = 3U;
            minimum = 0x80U;
            maximum = 0xBFU;
            if (first == 0xF0U) {
                minimum = 0x90U;
            } else if (first == 0xF4U) {
                maximum = 0x8FU;
            }
        } else {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }

        if (view.length - index <= continuation_count) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        if ((unsigned char)view.data[index + 1U] < minimum ||
            (unsigned char)view.data[index + 1U] > maximum) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        for (size_t offset = 2U; offset <= continuation_count; ++offset) {
            if (!is_continuation((unsigned char)view.data[index + offset])) {
                return AETHER_STATUS_INVALID_ARGUMENT;
            }
        }
        index += continuation_count + 1U;
    }

    return AETHER_STATUS_OK;
}

AetherStatus aether_string_view_copy(
    AetherStringView view,
    char* destination,
    size_t capacity
)
{
    AetherStatus status = aether_string_view_validate(view);

    if (status != AETHER_STATUS_OK || destination == NULL || capacity == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (view.length >= capacity) {
        destination[0] = '\0';
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    if (view.length != 0U) {
        (void)memcpy(destination, view.data, view.length);
    }
    destination[view.length] = '\0';
    return AETHER_STATUS_OK;
}

bool aether_string_view_equal(
    AetherStringView left,
    AetherStringView right
)
{
    if (aether_string_view_validate(left) != AETHER_STATUS_OK ||
        aether_string_view_validate(right) != AETHER_STATUS_OK ||
        left.length != right.length) {
        return false;
    }
    return left.length == 0U || memcmp(left.data, right.data, left.length) == 0;
}
