#ifndef AETHER_ID_H
#define AETHER_ID_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherId {
    uint64_t value;
} AetherId;

/* Thread-safe process-local monotonically increasing identity. No allocation.
 * UINT64_MAX is issued once; exhaustion permanently returns invalid zero,
 * never wraps/reuses an earlier ID. IDs are not persistent or authorization.
 */
AetherId aether_id_next(void);
bool aether_id_is_valid(AetherId id);
bool aether_id_equal(AetherId left, AetherId right);

#ifdef __cplusplus
}
#endif

#endif
