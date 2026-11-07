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

AetherId aether_id_next(void);
bool aether_id_is_valid(AetherId id);
bool aether_id_equal(AetherId left, AetherId right);

#ifdef __cplusplus
}
#endif

#endif
