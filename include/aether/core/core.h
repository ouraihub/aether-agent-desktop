#ifndef AETHER_CORE_H
#define AETHER_CORE_H

#include "aether/foundation/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AetherCoreState {
    AETHER_CORE_STATE_CREATED = 0,
    AETHER_CORE_STATE_INITIALIZED,
    AETHER_CORE_STATE_RUNNING,
    AETHER_CORE_STATE_STOPPED,
    AETHER_CORE_STATE_CLOSED
} AetherCoreState;

typedef struct AetherCore AetherCore;

AetherStatus aether_core_create(AetherCore** out_core);
AetherStatus aether_core_initialize(AetherCore* core);
AetherStatus aether_core_start(AetherCore* core);
AetherStatus aether_core_stop(AetherCore* core);
AetherStatus aether_core_close(AetherCore* core);
AetherCoreState aether_core_get_state(const AetherCore* core);
AetherStatus aether_core_get_status(const AetherCore* core);
void aether_core_destroy(AetherCore* core);

#ifdef __cplusplus
}
#endif

#endif
