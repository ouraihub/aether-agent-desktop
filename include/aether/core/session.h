#ifndef AETHER_CORE_SESSION_H
#define AETHER_CORE_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "aether/core/runtime_registry.h"
#include "aether/core/workspace.h"
#include "aether/foundation/id.h"
#include "aether/foundation/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherSessionRegistry AetherSessionRegistry;
typedef struct AetherSessionId { AetherId id; } AetherSessionId;

typedef enum AetherSessionState {
    AETHER_SESSION_CREATED = 0,
    AETHER_SESSION_INITIALIZING,
    AETHER_SESSION_IDLE,
    AETHER_SESSION_ACTIVE,
    AETHER_SESSION_INTERRUPTED,
    AETHER_SESSION_RUNTIME_UNAVAILABLE,
    AETHER_SESSION_CLOSING,
    AETHER_SESSION_CLOSED,
    AETHER_SESSION_FAILED
} AetherSessionState;

typedef struct AetherSessionSnapshot {
    AetherSessionId session_id;
    AetherWorkspaceId workspace_id;
    AetherAgentConfigId agent_config_id;
    AetherRuntimeId runtime_id;
    uint64_t runtime_generation;
    uint64_t agent_revision;
    uint64_t revision;
    uint32_t active_turns;
    AetherSessionState state;
} AetherSessionSnapshot;

/* Borrows the Workspace and Runtime registries for the registry lifetime. */
AetherStatus aether_session_registry_create(
    size_t capacity,
    AetherWorkspaceRegistry* workspaces,
    AetherRuntimeRegistry* runtimes,
    AetherSessionRegistry** out_registry
);
AetherStatus aether_session_registry_destroy(
    AetherSessionRegistry** registry
);

AetherStatus aether_session_create(
    AetherSessionRegistry* registry,
    AetherWorkspaceId workspace_id,
    AetherAgentConfigId agent_config_id,
    AetherRuntimeId runtime_id,
    uint64_t runtime_generation,
    AetherSessionId* out_id
);
AetherStatus aether_session_initialize(
    AetherSessionRegistry* registry,
    AetherSessionId id
);
AetherStatus aether_session_start_turn(
    AetherSessionRegistry* registry,
    AetherSessionId id
);
AetherStatus aether_session_finish_turn(
    AetherSessionRegistry* registry,
    AetherSessionId id
);
AetherStatus aether_session_mark_runtime_unavailable(
    AetherSessionRegistry* registry,
    AetherSessionId id
);
AetherStatus aether_session_mark_interrupted(
    AetherSessionRegistry* registry,
    AetherSessionId id
);
AetherStatus aether_session_begin_close(
    AetherSessionRegistry* registry,
    AetherSessionId id
);
AetherStatus aether_session_finish_close(
    AetherSessionRegistry* registry,
    AetherSessionId id
);
AetherStatus aether_session_query(
    const AetherSessionRegistry* registry,
    AetherSessionId id,
    AetherSessionSnapshot* out_snapshot
);

#ifdef __cplusplus
}
#endif

#endif
