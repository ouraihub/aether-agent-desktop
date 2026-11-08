#include "aether/infrastructure/credentials.h"

#include <stdlib.h>

void aether_credential_record_dispose(AetherCredentialRecord* record)
{
    if (record == NULL) return;
    free(record->provider);
    free(record->payload_json);
    *record = (AetherCredentialRecord){0};
}
