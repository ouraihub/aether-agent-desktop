#ifndef AETHER_CORE_H
#define AETHER_CORE_H

#include "aether/foundation/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherCore AetherCore;

AetherStatus aether_core_create(AetherCore** out_core);
AetherStatus aether_core_get_status(const AetherCore* core);
void aether_core_destroy(AetherCore* core);

#ifdef __cplusplus
}
#endif

#endif
