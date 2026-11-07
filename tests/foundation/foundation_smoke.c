#include "aether/foundation/error.h"
#include "aether/foundation/id.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    AetherError error;
    AetherId first;
    AetherId second;

    aether_error_clear(&error);
    assert(error.status == AETHER_STATUS_OK);
    assert(aether_error_set(NULL, AETHER_STATUS_OK, 0, "x") ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(
        aether_error_set(&error, AETHER_STATUS_INTERNAL_ERROR, 7, "failed") ==
        AETHER_STATUS_OK
    );
    assert(error.status == AETHER_STATUS_INTERNAL_ERROR);
    assert(error.code == 7);
    assert(error.message[0] == 'f');

    first = aether_id_next();
    second = aether_id_next();
    assert(aether_id_is_valid(first));
    assert(aether_id_is_valid(second));
    assert(!aether_id_equal(first, second));
    assert(!aether_id_is_valid((AetherId){0}));
    return 0;
}
