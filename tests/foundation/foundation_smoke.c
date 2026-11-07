#include "aether/foundation/error.h"
#include "aether/foundation/id.h"
#include "aether/foundation/string_view.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    AetherError error;
    AetherError copied;
    AetherId first;
    AetherId second;
    char buffer[8];
    const char valid_text[] = "hello";
    const char utf8_text[] = "\xE4\xB8\xAD";
    const char invalid_text[] = "\xE4\x28\xA1";

    aether_error_clear(&error);
    assert(error.status == AETHER_STATUS_OK);
    assert(aether_error_set(NULL, AETHER_STATUS_OK, 0, "x") ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_error_set(&error, AETHER_STATUS_INTERNAL_ERROR, 7,
                            "failed") == AETHER_STATUS_OK);
    aether_error_copy(&copied, &error);
    assert(copied.status == AETHER_STATUS_INTERNAL_ERROR);
    assert(copied.code == 7);
    assert(copied.message[0] == 'f');

    assert(aether_string_view_validate((AetherStringView){
               valid_text, sizeof(valid_text) - 1U}) == AETHER_STATUS_OK);
    assert(aether_string_view_validate((AetherStringView){
               utf8_text, sizeof(utf8_text) - 1U}) == AETHER_STATUS_OK);
    assert(aether_string_view_validate((AetherStringView){
               invalid_text, sizeof(invalid_text) - 1U}) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_string_view_copy((AetherStringView){
               valid_text, sizeof(valid_text) - 1U}, buffer, sizeof(buffer)) ==
           AETHER_STATUS_OK);
    assert(aether_string_view_equal((AetherStringView){
               valid_text, sizeof(valid_text) - 1U},
               (AetherStringView){buffer, sizeof(valid_text) - 1U}));
    assert(aether_string_view_copy((AetherStringView){
               valid_text, sizeof(valid_text) - 1U}, buffer, 5U) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_string_view_validate((AetherStringView){NULL, 1U}) ==
           AETHER_STATUS_INVALID_ARGUMENT);

    first = aether_id_next();
    second = aether_id_next();
    assert(aether_id_is_valid(first));
    assert(aether_id_is_valid(second));
    assert(!aether_id_equal(first, second));
    assert(!aether_id_is_valid((AetherId){0}));
    return 0;
}
