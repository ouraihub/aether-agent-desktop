#ifndef AETHER_BACKEND_INTERNAL_H
#define AETHER_BACKEND_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "aether/backend/backend.h"
#include "aether/backend/request.h"
#define AETHER_BACKEND_REQUEST_CAPACITY 8U

typedef struct AetherBackendRequestRecord {
    AetherBackendRequestSnapshot snapshot;
    bool occupied;
} AetherBackendRequestRecord;

struct AetherBackend {
    AetherCore* core;
    bool started;
    AetherBackendRequestRecord requests[AETHER_BACKEND_REQUEST_CAPACITY];
};

#endif
