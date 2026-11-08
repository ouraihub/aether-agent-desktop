#include "credentials_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "yyjson.h"

#define CREDENTIAL_JSON_LIMIT (64U * 1024U)
#define CREDENTIAL_KEY_LIMIT (4096U)

static bool is_nonempty_string(const yyjson_val* value, size_t limit)
{
    size_t length;

    if (!yyjson_is_str(value)) return false;
    length = yyjson_get_len(value);
    return length != 0U && length <= limit;
}

static bool has_oauth_secret(const yyjson_val* root)
{
    return is_nonempty_string(yyjson_obj_get(root, "access"),
                              CREDENTIAL_KEY_LIMIT) ||
           is_nonempty_string(yyjson_obj_get(root, "refresh"),
                              CREDENTIAL_KEY_LIMIT);
}

AetherStatus aether_credential_validate_input(AetherCredentialInput input)
{
    yyjson_doc* document;
    yyjson_val* root;
    AetherStatus status;

    status = aether_string_view_validate(input.provider);
    if (status != AETHER_STATUS_OK || input.provider.length == 0U ||
        input.provider.length > 256U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = aether_string_view_validate(input.payload_json);
    if (status != AETHER_STATUS_OK || input.payload_json.length == 0U ||
        input.payload_json.length > CREDENTIAL_JSON_LIMIT) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (input.type != AETHER_CREDENTIAL_API_KEY &&
        input.type != AETHER_CREDENTIAL_OAUTH) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }

    document = yyjson_read(input.payload_json.data, input.payload_json.length, 0U);
    if (document == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(document);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (input.type == AETHER_CREDENTIAL_API_KEY &&
        !is_nonempty_string(yyjson_obj_get(root, "key"), CREDENTIAL_KEY_LIMIT)) {
        yyjson_doc_free(document);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (input.type == AETHER_CREDENTIAL_OAUTH && !has_oauth_secret(root)) {
        yyjson_doc_free(document);
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    yyjson_doc_free(document);
    return AETHER_STATUS_OK;
}

AetherStatus aether_credential_validate_stored(
    AetherCredentialType type,
    const unsigned char* data,
    size_t length
)
{
    AetherCredentialInput input;
    yyjson_doc* document;
    yyjson_val* root;

    if (data == NULL || length == 0U || length > CREDENTIAL_JSON_LIMIT) {
        return AETHER_STATUS_IO_ERROR;
    }
    input = (AetherCredentialInput){
        {"stored", 6U}, type, {(const char*)data, length}
    };
    document = yyjson_read((const char*)data, length, 0U);
    if (document == NULL) return AETHER_STATUS_IO_ERROR;
    root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root) ||
        (type == AETHER_CREDENTIAL_API_KEY &&
         !is_nonempty_string(yyjson_obj_get(root, "key"),
                              CREDENTIAL_KEY_LIMIT)) ||
        (type == AETHER_CREDENTIAL_OAUTH && !has_oauth_secret(root))) {
        yyjson_doc_free(document);
        return AETHER_STATUS_IO_ERROR;
    }
    yyjson_doc_free(document);
    return aether_credential_validate_input(input) == AETHER_STATUS_OK
        ? AETHER_STATUS_OK : AETHER_STATUS_IO_ERROR;
}
