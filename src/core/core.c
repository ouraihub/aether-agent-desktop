#include "aether/core/core.h"

#include <stdbool.h>
#include <stdlib.h>
struct AetherCore {
    AetherCoreState state;
    AetherStatus status;
};

static bool aether_core_state_can_initialize(AetherCoreState state)
{
    return state == AETHER_CORE_STATE_CREATED;
}

static bool aether_core_state_can_start(AetherCoreState state)
{
    return state == AETHER_CORE_STATE_INITIALIZED ||
           state == AETHER_CORE_STATE_STOPPED;
}

static bool aether_core_state_can_stop(AetherCoreState state)
{
    return state == AETHER_CORE_STATE_RUNNING;
}

static bool aether_core_state_can_close(AetherCoreState state)
{
    return state == AETHER_CORE_STATE_INITIALIZED ||
           state == AETHER_CORE_STATE_STOPPED;
}

AetherStatus aether_core_create(AetherCore** out_core)
{
    AetherCore* core;

    if (out_core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    *out_core = NULL;
    core = (AetherCore*)malloc(sizeof(*core));
    if (core == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }

    core->state = AETHER_CORE_STATE_CREATED;
    core->status = AETHER_STATUS_OK;
    *out_core = core;
    return AETHER_STATUS_OK;
}

AetherStatus aether_core_initialize(AetherCore* core)
{
    if (core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (!aether_core_state_can_initialize(core->state)) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }

    core->state = AETHER_CORE_STATE_INITIALIZED;
    return AETHER_STATUS_OK;
}

AetherStatus aether_core_start(AetherCore* core)
{
    if (core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (!aether_core_state_can_start(core->state)) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }

    core->state = AETHER_CORE_STATE_RUNNING;
    return AETHER_STATUS_OK;
}

AetherStatus aether_core_stop(AetherCore* core)
{
    if (core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (!aether_core_state_can_stop(core->state)) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }

    core->state = AETHER_CORE_STATE_STOPPED;
    return AETHER_STATUS_OK;
}

AetherStatus aether_core_close(AetherCore* core)
{
    if (core == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (!aether_core_state_can_close(core->state)) {
        return AETHER_STATUS_INTERNAL_ERROR;
    }

    core->state = AETHER_CORE_STATE_CLOSED;
    return AETHER_STATUS_OK;
}

AetherCoreState aether_core_get_state(const AetherCore* core)
{
    if (core == NULL) {
        return AETHER_CORE_STATE_CLOSED;
    }

    return core->state;
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
