#include "aether/foundation/id.h"

/* Private test targets may start near exhaustion without exposing a reset API. */
#ifndef AETHER_ID_INITIAL_VALUE
#define AETHER_ID_INITIAL_VALUE 0
#endif

#if defined(_MSC_VER)
#include <intrin.h>

static volatile __int64 next_id = AETHER_ID_INITIAL_VALUE;
#else
#include <stdatomic.h>

static atomic_uint_least64_t next_id = AETHER_ID_INITIAL_VALUE;
#endif

AetherId aether_id_next(void)
{
#if defined(_MSC_VER)
    __int64 current = _InterlockedCompareExchange64(&next_id, 0, 0);
    for (;;) {
        /* Interpret unsigned identity bits without signed-overflow arithmetic. */
        __int64 desired = current == INT64_MAX ? INT64_MIN : current + 1;
        __int64 observed;
        if (current == -1) return (AetherId){0};
        observed = _InterlockedCompareExchange64(&next_id, desired, current);
        if (observed == current) return (AetherId){(uint64_t)desired};
        current = observed;
    }
#else
    uint_least64_t current = atomic_load_explicit(&next_id, memory_order_relaxed);
    for (;;) {
        if (current >= UINT64_MAX) return (AetherId){0};
        if (atomic_compare_exchange_weak_explicit(&next_id, &current, current + 1U,
                memory_order_relaxed, memory_order_relaxed)) {
            return (AetherId){current + 1U};
        }
    }
#endif
}

bool aether_id_is_valid(AetherId id)
{
    return id.value != 0U;
}

bool aether_id_equal(AetherId left, AetherId right)
{
    return left.value == right.value;
}
