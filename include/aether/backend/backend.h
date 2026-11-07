#ifndef AETHER_BACKEND_BACKEND_H
#define AETHER_BACKEND_BACKEND_H

#include "aether/core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherBackend AetherBackend;

AetherStatus aether_backend_create(
    AetherCore* core,
    AetherBackend** out_backend
);
AetherStatus aether_backend_initialize(AetherBackend* backend);
AetherStatus aether_backend_start(AetherBackend* backend);
AetherStatus aether_backend_stop(AetherBackend* backend);
AetherStatus aether_backend_close(AetherBackend* backend);
AetherCoreState aether_backend_get_core_state(
    const AetherBackend* backend
);
void aether_backend_destroy(AetherBackend* backend);

#ifdef __cplusplus
}
#endif

#endif
