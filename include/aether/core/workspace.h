#ifndef AETHER_CORE_WORKSPACE_H
#define AETHER_CORE_WORKSPACE_H

#include <stddef.h>
#include <stdint.h>

#include "aether/core/runtime_registry.h"
#include "aether/foundation/id.h"
#include "aether/foundation/status.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherWorkspaceRegistry AetherWorkspaceRegistry;
typedef struct AetherWorkspaceId { AetherId id; } AetherWorkspaceId;
typedef struct AetherAgentConfigId { AetherId id; } AetherAgentConfigId;

typedef enum AetherWorkspaceState {
    AETHER_WORKSPACE_OPEN = 0,
    AETHER_WORKSPACE_CLOSING,
    AETHER_WORKSPACE_CLOSED
} AetherWorkspaceState;

typedef struct AetherWorkspaceInput {
    AetherStringView display_name;
    AetherStringView root_path;
} AetherWorkspaceInput;

typedef struct AetherAgentConfigInput {
    AetherStringView display_name;
    AetherProviderId provider_id;
    AetherStringView model;
} AetherAgentConfigInput;

typedef struct AetherWorkspaceSnapshot {
    AetherWorkspaceId workspace_id;
    uint64_t revision;
    AetherWorkspaceState state;
    char display_name[128];
    char root_path[4096];
} AetherWorkspaceSnapshot;

typedef struct AetherAgentConfigSnapshot {
    AetherAgentConfigId agent_config_id;
    uint64_t revision;
    AetherProviderId provider_id;
    char display_name[128];
    char model[128];
} AetherAgentConfigSnapshot;

/* Synchronous in-memory domain registry. No IO, persistence, callbacks or UI. */
AetherStatus aether_workspace_registry_create(
    size_t workspace_capacity,
    size_t agent_capacity,
    AetherWorkspaceRegistry** out_registry
);
AetherStatus aether_workspace_registry_destroy(
    AetherWorkspaceRegistry** registry
);

AetherStatus aether_workspace_create(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceInput input,
    AetherWorkspaceId* out_id
);
AetherStatus aether_workspace_query(
    const AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id,
    AetherWorkspaceSnapshot* out_snapshot
);
AetherStatus aether_workspace_update_name(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id,
    uint64_t expected_revision,
    AetherStringView display_name
);
AetherStatus aether_workspace_begin_close(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id,
    uint64_t expected_revision
);
AetherStatus aether_workspace_finish_close(
    AetherWorkspaceRegistry* registry,
    AetherWorkspaceId id
);

AetherStatus aether_agent_config_create(
    AetherWorkspaceRegistry* registry,
    AetherAgentConfigInput input,
    AetherAgentConfigId* out_id
);
AetherStatus aether_agent_config_query(
    const AetherWorkspaceRegistry* registry,
    AetherAgentConfigId id,
    AetherAgentConfigSnapshot* out_snapshot
);
AetherStatus aether_agent_config_update_model(
    AetherWorkspaceRegistry* registry,
    AetherAgentConfigId id,
    uint64_t expected_revision,
    AetherStringView model
);
AetherStatus aether_agent_config_remove(
    AetherWorkspaceRegistry* registry,
    AetherAgentConfigId id
);

#ifdef __cplusplus
}
#endif

#endif
