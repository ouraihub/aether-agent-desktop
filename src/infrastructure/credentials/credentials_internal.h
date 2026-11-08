#ifndef AETHER_CREDENTIALS_INTERNAL_H
#define AETHER_CREDENTIALS_INTERNAL_H

#include <stddef.h>

#include "aether/infrastructure/credentials.h"

AetherStatus aether_credential_validate_input(AetherCredentialInput input);
AetherStatus aether_credential_validate_stored(
    AetherCredentialType type,
    const unsigned char* data,
    size_t length
);

#endif
