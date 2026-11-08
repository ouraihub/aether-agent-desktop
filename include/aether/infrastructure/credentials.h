#ifndef AETHER_INFRASTRUCTURE_CREDENTIALS_H
#define AETHER_INFRASTRUCTURE_CREDENTIALS_H

#include <stdint.h>

#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherCredentialStore AetherCredentialStore;

typedef enum AetherCredentialType {
    AETHER_CREDENTIAL_API_KEY = 1,
    AETHER_CREDENTIAL_OAUTH = 2
} AetherCredentialType;

typedef struct AetherCredentialInput {
    AetherStringView provider;
    AetherCredentialType type;
    AetherStringView payload_json;
} AetherCredentialInput;

typedef struct AetherCredentialRecord {
    char* provider;
    AetherCredentialType type;
    char* payload_json;
    uint64_t revision;
} AetherCredentialRecord;

/*
 * SQLite-backed OMP-style credential storage. Payload JSON is plaintext in the
 * database and its WAL/journal/backup files; this module provides no at-rest
 * encryption. The caller owns and serializes one store instance at a time.
 * The path is copied by SQLite during open and is not retained by the caller.
 * Creation opens or creates the database and validates schema version 1.
 */
AetherStatus aether_credential_store_create(
    AetherStringView database_path,
    AetherCredentialStore** out_store
);

/* Closes the SQLite connection and releases the store. NULL is accepted. */
AetherStatus aether_credential_store_destroy(AetherCredentialStore** store);

/* Inserts or replaces one provider/type row when expected_revision matches. */
AetherStatus aether_credential_store_put(
    AetherCredentialStore* store,
    AetherCredentialInput input,
    uint64_t expected_revision,
    uint64_t* out_revision
);

/* Reads an independent, owned copy of one credential row. */
AetherStatus aether_credential_store_get(
    AetherCredentialStore* store,
    AetherStringView provider,
    AetherCredentialType type,
    AetherCredentialRecord* out_record
);

/* Deletes one row when expected_revision matches. */
AetherStatus aether_credential_store_delete(
    AetherCredentialStore* store,
    AetherStringView provider,
    AetherCredentialType type,
    uint64_t expected_revision
);

/* Releases all strings in a record and clears its fields. */
void aether_credential_record_dispose(AetherCredentialRecord* record);

#ifdef __cplusplus
}
#endif

#endif
