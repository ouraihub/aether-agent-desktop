#include "aether/core/core.h"

#include <stdio.h>

int main(void)
{
    AetherCore* core = NULL;
    AetherStatus status;

    status = aether_core_create(&core);
    if (status != AETHER_STATUS_OK) {
        fprintf(stderr, "aether_core_create failed: %d\n", (int)status);
        return 1;
    }

    status = aether_core_get_status(core);
    aether_core_destroy(core);
    if (status != AETHER_STATUS_OK) {
        fprintf(stderr, "aether_core_get_status failed: %d\n", (int)status);
        return 1;
    }

    puts("Aether desktop skeleton ready.");
    return 0;
}
