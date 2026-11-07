#include "aether/backend/backend.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    AetherBackend* backend = NULL;
    AetherCore* core = NULL;

    assert(aether_backend_create(NULL, &backend) ==
           AETHER_STATUS_INVALID_ARGUMENT);
    assert(aether_core_create(&core) == AETHER_STATUS_OK);
    assert(aether_backend_create(core, &backend) == AETHER_STATUS_OK);
    assert(aether_backend_initialize(backend) == AETHER_STATUS_OK);
    assert(aether_backend_start(backend) == AETHER_STATUS_OK);
    assert(aether_backend_get_core_state(backend) ==
           AETHER_CORE_STATE_RUNNING);
    assert(aether_backend_stop(backend) == AETHER_STATUS_OK);
    assert(aether_backend_close(backend) == AETHER_STATUS_OK);
    aether_backend_destroy(backend);
    aether_core_destroy(core);
    return 0;
}
