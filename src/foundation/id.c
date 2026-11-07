#include "aether/foundation/id.h"

#if defined(_MSC_VER)
#include <intrin.h>

static volatile __int64 next_id = 0;
#else
#include <stdatomic.h>

static atomic_uint_least64_t next_id = 0;
#endif

AetherId aether_id_next(void)
{
    AetherId id;

#if defined(_MSC_VER)
    id.value = (uint64_t)_InterlockedIncrement64(&next_id);
#else
    id.value = atomic_fetch_add_explicit(
        &next_id,
        1U,
        memory_order_relaxed
    ) + 1U;
#endif
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
