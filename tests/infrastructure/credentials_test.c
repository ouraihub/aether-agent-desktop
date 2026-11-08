#include "aether/infrastructure/credentials.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

static AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}

static void remove_database(const char* path)
{
    char sidecar[512];

    (void)remove(path);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-wal", path);
    (void)remove(sidecar);
    (void)snprintf(sidecar, sizeof(sidecar), "%s-shm", path);
    (void)remove(sidecar);
}

static void make_database_fixture(const char* path, const char* suffix)
{
    sqlite3* database = NULL;
    char* error = NULL;
    char sql[512];

    remove_database(path);
    assert(sqlite3_open(path, &database) == SQLITE_OK);
    assert(sqlite3_exec(database,
        "CREATE TABLE aether_schema(key TEXT PRIMARY KEY, value INTEGER);"
        "CREATE TABLE aether_credentials(provider TEXT, type INTEGER, "
        "payload TEXT, revision INTEGER, PRIMARY KEY(provider,type));",
        NULL, NULL, &error) == SQLITE_OK);
    assert(error == NULL);
    assert(snprintf(sql, sizeof(sql), "%s", suffix) > 0);
    assert(sqlite3_exec(database, sql, NULL, NULL, &error) == SQLITE_OK);
    assert(error == NULL);
    assert(sqlite3_close(database) == SQLITE_OK);
}

static void write_corrupt_file(const char* path)
{
    FILE* file = fopen(path, "wb");

    assert(file != NULL);
    assert(fwrite("not-a-sqlite-database", 1U, 21U, file) == 21U);
    assert(fclose(file) == 0);
}

static void assert_plaintext_payload(
    const char* path,
    AetherStringView expected
)
{
    sqlite3* database = NULL;
    sqlite3_stmt* statement = NULL;

    assert(sqlite3_open(path, &database) == SQLITE_OK);
    assert(sqlite3_prepare_v2(database,
        "SELECT payload FROM aether_credentials WHERE provider='openai'",
        -1, &statement, NULL) == SQLITE_OK);
    assert(sqlite3_step(statement) == SQLITE_ROW);
    assert(strcmp((const char*)sqlite3_column_text(statement, 0),
                  expected.data) == 0);
    assert(sqlite3_finalize(statement) == SQLITE_OK);
    assert(sqlite3_close(database) == SQLITE_OK);
}

int main(void)
{
    const char* path = "aether-credential-synthetic.db";
    AetherCredentialStore* store = NULL;
    AetherCredentialRecord record = {0};
    uint64_t revision = 0U;
    AetherCredentialInput input = {
        text("openai"), AETHER_CREDENTIAL_API_KEY,
        text("{\"key\":\"synthetic-api-key\"}")};
    AetherCredentialInput oauth = {
        text("example"), AETHER_CREDENTIAL_OAUTH,
        text("{\"access\":\"synthetic-access\",\"refresh\":\"synthetic-refresh\"}")};

    remove_database(path);
    assert(aether_credential_store_create(text(path), &store) ==
           AETHER_STATUS_OK);
    assert(aether_credential_store_put(
               store, (AetherCredentialInput){text("openai"),
                   AETHER_CREDENTIAL_API_KEY, text("not-json")},
               0U, &revision) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_credential_store_put(
               store, (AetherCredentialInput){text("openai"),
                   AETHER_CREDENTIAL_API_KEY, text("{}")},
               0U, &revision) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_credential_store_put(store, input, 0U, &revision) ==
           AETHER_STATUS_OK);
    assert(revision == 1U);
    assert(aether_credential_store_put(store, input, 0U, &revision) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_credential_store_get(
               store, input.provider, input.type, &record) == AETHER_STATUS_OK);
    assert(record.revision == 1U);
    assert(strcmp(record.payload_json, input.payload_json.data) == 0);
    aether_credential_record_dispose(&record);
    assert(aether_credential_store_put(store, input, 1U, &revision) ==
           AETHER_STATUS_OK);
    assert(revision == 2U);
    assert(aether_credential_store_put(store, oauth, 0U, &revision) ==
           AETHER_STATUS_OK);
    assert(revision == 1U);
    assert(aether_credential_store_delete(
               store, input.provider, input.type, 1U) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_credential_store_delete(
               store, input.provider, input.type, 2U) == AETHER_STATUS_OK);
    assert(aether_credential_store_get(
               store, input.provider, input.type, &record) ==
           AETHER_STATUS_NOT_FOUND);
    assert(aether_credential_store_destroy(&store) == AETHER_STATUS_OK);
    assert(store == NULL);

    assert(aether_credential_store_create(text(path), &store) ==
           AETHER_STATUS_OK);
    assert(aether_credential_store_get(
               store, oauth.provider, oauth.type, &record) == AETHER_STATUS_OK);
    assert(strcmp(record.payload_json, oauth.payload_json.data) == 0);
    aether_credential_record_dispose(&record);
    assert(aether_credential_store_destroy(&store) == AETHER_STATUS_OK);
    remove_database(path);

    assert(aether_credential_store_create(text(path), &store) ==
           AETHER_STATUS_OK);
    assert(aether_credential_store_put(store, input, 0U, &revision) ==
           AETHER_STATUS_OK);
    assert(aether_credential_store_destroy(&store) == AETHER_STATUS_OK);
    assert_plaintext_payload(path, input.payload_json);
    remove_database(path);

    make_database_fixture(path,
        "INSERT INTO aether_schema VALUES ('version',99);");
    assert(aether_credential_store_create(text(path), &store) ==
           AETHER_STATUS_NOT_SUPPORTED);
    assert(store == NULL);
    remove_database(path);

    make_database_fixture(path,
        "INSERT INTO aether_schema VALUES ('version',1);"
        "INSERT INTO aether_credentials VALUES ('bad',1,'not-json',1);");
    assert(aether_credential_store_create(text(path), &store) ==
           AETHER_STATUS_OK);
    assert(aether_credential_store_get(
               store, text("bad"), AETHER_CREDENTIAL_API_KEY, &record) ==
           AETHER_STATUS_IO_ERROR);
    assert(aether_credential_store_destroy(&store) == AETHER_STATUS_OK);
    remove_database(path);

    write_corrupt_file(path);
    assert(aether_credential_store_create(text(path), &store) ==
           AETHER_STATUS_PROTOCOL_ERROR);
    assert(store == NULL);
    remove_database(path);
    puts("credentials: CRUD, CAS, persistence, plaintext, deletion, and rejection passed");
    return EXIT_SUCCESS;
}
