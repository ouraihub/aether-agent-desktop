#include "aether/core/core.h"

#include <stdlib.h>

struct AetherCore {
    AetherStatus status;
};

AetherStatus aether_core_create(AetherCore** out_core)
{
    AetherCore* core;

    if (out_core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    *out_core = NULL;
    core = (AetherCore*)malloc(sizeof(*core));
    if (core == NULL) {
        return AETHER_STATUS_NOT_INITIALIZED;
    }

    core->status = AETHER_STATUS_OK;
    *out_core = core;
    return AETHER_STATUS_OK;
}

AetherStatus aether_core_get_status(const AetherCore* core)
{
    if (core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    return core->status;
}

void aether_core_destroy(AetherCore* core)
{
    free(core);
}
