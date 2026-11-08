#ifndef AETHER_BACKEND_QUERY_INTERNAL_H
#define AETHER_BACKEND_QUERY_INTERNAL_H
#include "aether/backend/query_service.h"

struct AetherBackendQueries {
    AetherWorkspaceRegistry* workspaces;
    AetherRuntimeRegistry* runtimes;
    AetherSessionRegistry* sessions;
    AetherMessageRegistry* messages;
};

AetherStatus aether_query_error(
    const AetherRequestContext* context, AetherStatus status,
    AetherQueryErrorCode code, AetherQueryError* out_error
);
AetherStatus aether_query_validate(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherQueryError* out_error
);
AetherStatus aether_query_authorize_workspace(
    const AetherRequestContext* context, AetherId workspace,
    AetherQueryError* out_error
);
AetherStatus aether_query_message_source(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherMessageSnapshot* out_snapshot, AetherQueryError* out_error
);
#endif
