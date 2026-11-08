#include "credentials_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

#define CREDENTIAL_SCHEMA_VERSION 1
#define CREDENTIAL_PROVIDER_LIMIT 256U
#define CREDENTIAL_JSON_LIMIT (64U * 1024U)

struct AetherCredentialStore {
    sqlite3* database;
};

static AetherStatus map_sqlite_status(int status)
{
    switch (status & 0xFF) {
    case SQLITE_NOMEM: return AETHER_STATUS_OUT_OF_MEMORY;
    case SQLITE_PERM:
    case SQLITE_AUTH:
        return AETHER_STATUS_PERMISSION_DENIED;
    case SQLITE_READONLY:
    case SQLITE_CANTOPEN:
        return AETHER_STATUS_PERMISSION_DENIED;
    case SQLITE_CORRUPT:
    case SQLITE_NOTADB:
        return AETHER_STATUS_PROTOCOL_ERROR;
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
        return AETHER_STATUS_TIMEOUT;
    case SQLITE_FULL:
    case SQLITE_IOERR:
        return AETHER_STATUS_IO_ERROR;
    default: return AETHER_STATUS_INTERNAL_ERROR;
    }
}

static AetherStatus prepare(
    sqlite3* database,
    const char* sql,
    sqlite3_stmt** out_statement
)
{
    int status;

    *out_statement = NULL;
    status = sqlite3_prepare_v2(database, sql, -1, out_statement, NULL);
    return status == SQLITE_OK ? AETHER_STATUS_OK : map_sqlite_status(status);
}

static AetherStatus bind_view(
    sqlite3_stmt* statement,
    int index,
    AetherStringView view
)
{
    int length;

    if (view.length > (size_t)INT_MAX) return AETHER_STATUS_INVALID_ARGUMENT;
    length = (int)view.length;
    return sqlite3_bind_text(statement, index, view.data, length,
                             SQLITE_TRANSIENT) == SQLITE_OK
        ? AETHER_STATUS_OK : AETHER_STATUS_OUT_OF_MEMORY;
}

static AetherStatus initialize_schema(sqlite3* database)
{
    static const char* sql =
        "PRAGMA journal_mode=WAL;"
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS aether_schema ("
        "key TEXT PRIMARY KEY, value INTEGER NOT NULL);"
        "INSERT OR IGNORE INTO aether_schema(key,value) VALUES ('version',1);"
        "CREATE TABLE IF NOT EXISTS aether_credentials ("
        "provider TEXT NOT NULL, type INTEGER NOT NULL, payload TEXT NOT NULL,"
        "revision INTEGER NOT NULL CHECK (revision > 0),"
        "PRIMARY KEY(provider,type));";
    int status = sqlite3_exec(database, sql, NULL, NULL, NULL);

    return status == SQLITE_OK ? AETHER_STATUS_OK : map_sqlite_status(status);
}

static AetherStatus check_schema(sqlite3* database)
{
    sqlite3_stmt* statement;
    AetherStatus status;
    int step;
    sqlite3_int64 version;

    status = prepare(database,
        "SELECT value FROM aether_schema WHERE key='version'", &statement);
    if (status != AETHER_STATUS_OK) return status;
    step = sqlite3_step(statement);
    if (step != SQLITE_ROW) {
        status = step == SQLITE_DONE ? AETHER_STATUS_PROTOCOL_ERROR
                                     : map_sqlite_status(step);
        (void)sqlite3_finalize(statement);
        return status;
    }
    version = sqlite3_column_int64(statement, 0);
    (void)sqlite3_finalize(statement);
    return version == CREDENTIAL_SCHEMA_VERSION
        ? AETHER_STATUS_OK : AETHER_STATUS_NOT_SUPPORTED;
}

AetherStatus aether_credential_store_create(
    AetherStringView database_path,
    AetherCredentialStore** out_store
)
{
    AetherCredentialStore* store;
    char* path;
    sqlite3* database = NULL;
    AetherStatus status;

    if (out_store == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_store = NULL;
    status = aether_string_view_validate(database_path);
    if (status != AETHER_STATUS_OK || database_path.length == 0U ||
        database_path.length > (size_t)INT_MAX) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    path = malloc(database_path.length + 1U);
    if (path == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    (void)memcpy(path, database_path.data, database_path.length);
    path[database_path.length] = '\0';
    status = sqlite3_open_v2(path, &database,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        NULL);
    free(path);
    if (status != SQLITE_OK) {
        if (database != NULL) (void)sqlite3_close(database);
        return map_sqlite_status(status);
    }
    store = malloc(sizeof(*store));
    if (store == NULL) {
        (void)sqlite3_close(database);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    store->database = database;
    status = initialize_schema(database);
    if (status == AETHER_STATUS_OK) status = check_schema(database);
    if (status != AETHER_STATUS_OK) {
        (void)sqlite3_close(database);
        free(store);
        return status;
    }
    *out_store = store;
    return AETHER_STATUS_OK;
}

AetherStatus aether_credential_store_destroy(AetherCredentialStore** store)
{
    int status;

    if (store == NULL || *store == NULL) return AETHER_STATUS_OK;
    status = sqlite3_close((*store)->database);
    if (status != SQLITE_OK) return map_sqlite_status(status);
    free(*store);
    *store = NULL;
    return AETHER_STATUS_OK;
}

static AetherStatus begin(sqlite3* database)
{
    int status = sqlite3_exec(database, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    return status == SQLITE_OK ? AETHER_STATUS_OK : map_sqlite_status(status);
}

static void rollback(sqlite3* database)
{
    (void)sqlite3_exec(database, "ROLLBACK", NULL, NULL, NULL);
}

AetherStatus aether_credential_store_put(
    AetherCredentialStore* store,
    AetherCredentialInput input,
    uint64_t expected_revision,
    uint64_t* out_revision
)
{
    sqlite3_stmt* statement = NULL;
    AetherStatus status;
    int step;
    sqlite3_int64 next_revision;

    if (out_revision == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_revision = 0U;
    if (store == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    status = aether_credential_validate_input(input);
    if (status != AETHER_STATUS_OK) return status;
    if (expected_revision == UINT64_MAX) return AETHER_STATUS_INVALID_ARGUMENT;
    status = begin(store->database);
    if (status != AETHER_STATUS_OK) return status;
    status = prepare(store->database,
        "SELECT revision FROM aether_credentials WHERE provider=?1 AND type=?2",
        &statement);
    if (status != AETHER_STATUS_OK) goto fail;
    status = bind_view(statement, 1, input.provider);
    if (status != AETHER_STATUS_OK) goto fail;
    if (sqlite3_bind_int(statement, 2, (int)input.type) != SQLITE_OK) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
        goto fail;
    }
    step = sqlite3_step(statement);
    if (step == SQLITE_ROW) {
        sqlite3_int64 current = sqlite3_column_int64(statement, 0);
        if (current < 1 || (uint64_t)current != expected_revision ||
            current == INT64_MAX) {
            status = AETHER_STATUS_INVALID_ARGUMENT;
            goto fail;
        }
        next_revision = current + 1;
    } else if (step == SQLITE_DONE) {
        if (expected_revision != 0U) {
            status = AETHER_STATUS_INVALID_ARGUMENT;
            goto fail;
        }
        next_revision = 1;
    } else {
        status = map_sqlite_status(step);
        goto fail;
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    status = prepare(store->database,
        "INSERT INTO aether_credentials(provider,type,payload,revision) "
        "VALUES(?1,?2,?3,?4) ON CONFLICT(provider,type) DO UPDATE SET "
        "payload=excluded.payload, revision=excluded.revision", &statement);
    if (status != AETHER_STATUS_OK) goto fail;
    status = bind_view(statement, 1, input.provider);
    if (status != AETHER_STATUS_OK) goto fail;
    if (sqlite3_bind_int(statement, 2, (int)input.type) != SQLITE_OK ||
        sqlite3_bind_text(statement, 3, input.payload_json.data,
                          (int)input.payload_json.length, SQLITE_TRANSIENT) !=
        SQLITE_OK || sqlite3_bind_int64(statement, 4, next_revision) !=
        SQLITE_OK) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
        goto fail;
    }
    step = sqlite3_step(statement);
    if (step != SQLITE_DONE) {
        status = map_sqlite_status(step);
        goto fail;
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    if (sqlite3_exec(store->database, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        status = map_sqlite_status(sqlite3_errcode(store->database));
        goto fail_no_statement;
    }
    *out_revision = (uint64_t)next_revision;
    return AETHER_STATUS_OK;

fail:
    (void)sqlite3_finalize(statement);
fail_no_statement:
    rollback(store->database);
    return status;
}

AetherStatus aether_credential_store_get(
    AetherCredentialStore* store,
    AetherStringView provider,
    AetherCredentialType type,
    AetherCredentialRecord* out_record
)
{
    sqlite3_stmt* statement = NULL;
    const unsigned char* provider_text;
    const unsigned char* payload;
    int provider_length;
    int payload_length;
    int step;
    AetherStatus status;

    if (out_record == NULL || store == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_record = (AetherCredentialRecord){0};
    status = aether_string_view_validate(provider);
    if (status != AETHER_STATUS_OK || provider.length == 0U ||
        provider.length > CREDENTIAL_PROVIDER_LIMIT ||
        (type != AETHER_CREDENTIAL_API_KEY && type != AETHER_CREDENTIAL_OAUTH)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = prepare(store->database,
        "SELECT provider,type,payload,revision FROM aether_credentials "
        "WHERE provider=?1 AND type=?2", &statement);
    if (status != AETHER_STATUS_OK) return status;
    status = bind_view(statement, 1, provider);
    if (status != AETHER_STATUS_OK) goto fail;
    if (sqlite3_bind_int(statement, 2, (int)type) != SQLITE_OK) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
        goto fail;
    }
    step = sqlite3_step(statement);
    if (step == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return AETHER_STATUS_NOT_FOUND;
    }
    if (step != SQLITE_ROW) {
        status = map_sqlite_status(step);
        goto fail;
    }
    provider_text = sqlite3_column_text(statement, 0);
    payload = sqlite3_column_text(statement, 2);
    provider_length = sqlite3_column_bytes(statement, 0);
    payload_length = sqlite3_column_bytes(statement, 2);
    if (provider_text == NULL || payload == NULL || provider_length <= 0 ||
        payload_length <= 0 || payload_length > (int)CREDENTIAL_JSON_LIMIT ||
        aether_credential_validate_stored(type, payload,
                                          (size_t)payload_length) !=
            AETHER_STATUS_OK) {
        status = AETHER_STATUS_IO_ERROR;
        goto fail;
    }
    out_record->provider = malloc((size_t)provider_length + 1U);
    out_record->payload_json = malloc((size_t)payload_length + 1U);
    if (out_record->provider == NULL || out_record->payload_json == NULL) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
        goto fail_output;
    }
    (void)memcpy(out_record->provider, provider_text, (size_t)provider_length);
    out_record->provider[provider_length] = '\0';
    (void)memcpy(out_record->payload_json, payload, (size_t)payload_length);
    out_record->payload_json[payload_length] = '\0';
    out_record->type = type;
    out_record->revision = (uint64_t)sqlite3_column_int64(statement, 3);
    (void)sqlite3_finalize(statement);
    return AETHER_STATUS_OK;

fail_output:
    aether_credential_record_dispose(out_record);
fail:
    (void)sqlite3_finalize(statement);
    return status;
}

AetherStatus aether_credential_store_delete(
    AetherCredentialStore* store,
    AetherStringView provider,
    AetherCredentialType type,
    uint64_t expected_revision
)
{
    sqlite3_stmt* statement = NULL;
    AetherStatus status;
    int step;

    if (store == NULL || expected_revision == 0U ||
        expected_revision == UINT64_MAX) return AETHER_STATUS_INVALID_ARGUMENT;
    status = aether_string_view_validate(provider);
    if (status != AETHER_STATUS_OK || provider.length == 0U ||
        provider.length > CREDENTIAL_PROVIDER_LIMIT ||
        (type != AETHER_CREDENTIAL_API_KEY && type != AETHER_CREDENTIAL_OAUTH)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = begin(store->database);
    if (status != AETHER_STATUS_OK) return status;
    status = prepare(store->database,
        "DELETE FROM aether_credentials WHERE provider=?1 AND type=?2 "
        "AND revision=?3", &statement);
    if (status != AETHER_STATUS_OK) goto fail;
    status = bind_view(statement, 1, provider);
    if (status != AETHER_STATUS_OK) goto fail;
    if (sqlite3_bind_int(statement, 2, (int)type) != SQLITE_OK ||
        sqlite3_bind_int64(statement, 3, (sqlite3_int64)expected_revision) !=
            SQLITE_OK) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
        goto fail;
    }
    step = sqlite3_step(statement);
    if (step != SQLITE_DONE) {
        status = map_sqlite_status(step);
        goto fail;
    }
    if (sqlite3_changes(store->database) != 1) {
        status = AETHER_STATUS_INVALID_ARGUMENT;
        goto fail;
    }
    (void)sqlite3_finalize(statement);
    statement = NULL;
    if (sqlite3_exec(store->database, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        status = map_sqlite_status(sqlite3_errcode(store->database));
        goto fail_no_statement;
    }
    return AETHER_STATUS_OK;

fail:
    (void)sqlite3_finalize(statement);
fail_no_statement:
    rollback(store->database);
    return status;
}
