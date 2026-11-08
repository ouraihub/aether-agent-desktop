#ifndef AETHER_CORE_MESSAGE_H
#define AETHER_CORE_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

#include "aether/core/session.h"
#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherMessageRegistry AetherMessageRegistry;
typedef struct AetherMessageId { AetherId id; } AetherMessageId;

typedef enum AetherMessageRole {
    AETHER_MESSAGE_ROLE_USER = 0,
    AETHER_MESSAGE_ROLE_ASSISTANT,
    AETHER_MESSAGE_ROLE_TOOL
} AetherMessageRole;

typedef enum AetherMessageState {
    AETHER_MESSAGE_RECEIVING = 0,
    AETHER_MESSAGE_COMPLETED,
    AETHER_MESSAGE_INTERRUPTED,
    AETHER_MESSAGE_FAILED
} AetherMessageState;

typedef struct AetherMessageSnapshot {
    AetherMessageId message_id;
    AetherSessionId session_id;
    AetherMessageRole role;
    AetherMessageState state;
    uint64_t revision;
    uint64_t cursor;
    char text[65536];
} AetherMessageSnapshot;

AetherStatus aether_message_registry_create(
    size_t capacity,
    AetherSessionRegistry* sessions,
    AetherMessageRegistry** out_registry
);
AetherStatus aether_message_registry_destroy(AetherMessageRegistry** registry);
AetherStatus aether_message_create(
    AetherMessageRegistry* registry,
    AetherSessionId session_id,
    AetherMessageRole role,
    AetherMessageId* out_id
);
AetherStatus aether_message_query(
    const AetherMessageRegistry* registry,
    AetherMessageId id,
    AetherMessageSnapshot* out_snapshot
);
AetherStatus aether_message_append_text(
    AetherMessageRegistry* registry,
    AetherMessageId id,
    uint64_t cursor,
    AetherStringView text
);
AetherStatus aether_message_replace_text(
    AetherMessageRegistry* registry,
    AetherMessageId id,
    uint64_t expected_revision,
    AetherStringView text
);
AetherStatus aether_message_finish(
    AetherMessageRegistry* registry,
    AetherMessageId id,
    AetherMessageState terminal_state
);

#ifdef __cplusplus
}
#endif

#endif
