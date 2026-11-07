#include "aether/core/core.h"

#include <assert.h>
#include <stddef.h>
int main(void)
{
    AetherCore* core = NULL;

    assert(aether_core_create(NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_core_create(&core) == AETHER_STATUS_OK);
    assert(core != NULL);
    assert(aether_core_get_status(core) == AETHER_STATUS_OK);
    assert(aether_core_get_status(NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    aether_core_destroy(core);
    aether_core_destroy(NULL);
    return 0;
}
