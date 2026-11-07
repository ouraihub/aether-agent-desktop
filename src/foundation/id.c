#include "aether/foundation/id.h"

#include <stdatomic.h>

static atomic_uint_least64_t next_id = 0;

AetherId aether_id_next(void)
{
    AetherId id;

    id.value = atomic_fetch_add_explicit(
        &next_id,
        1U,
        memory_order_relaxed
    ) + 1U;
    return id;
}

bool aether_id_is_valid(AetherId id)
{
    return id.value != 0U;
}

bool aether_id_equal(AetherId left, AetherId right)
{
    return left.value == right.value;
}
