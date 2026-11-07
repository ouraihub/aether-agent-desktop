#include "aether/core/core.h"

#include <assert.h>
#include <stddef.h>
int main(void)
{
    AetherCore* core = NULL;

    assert(aether_core_create(NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_core_create(&core) == AETHER_STATUS_OK);
    assert(core != NULL);
    assert(aether_core_get_state(core) == AETHER_CORE_STATE_CREATED);
    assert(aether_core_start(core) == AETHER_STATUS_INTERNAL_ERROR);
    assert(aether_core_initialize(core) == AETHER_STATUS_OK);
    assert(aether_core_initialize(core) == AETHER_STATUS_INTERNAL_ERROR);
    assert(aether_core_start(core) == AETHER_STATUS_OK);
    assert(aether_core_stop(core) == AETHER_STATUS_OK);
    assert(aether_core_stop(core) == AETHER_STATUS_INTERNAL_ERROR);
    assert(aether_core_close(core) == AETHER_STATUS_OK);
    assert(aether_core_get_state(core) == AETHER_CORE_STATE_CLOSED);
    assert(aether_core_close(core) == AETHER_STATUS_INTERNAL_ERROR);
    assert(aether_core_get_status(NULL) == AETHER_STATUS_INVALID_ARGUMENT);
    aether_core_destroy(core);
    aether_core_destroy(NULL);
    return 0;
}
