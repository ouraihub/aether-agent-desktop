#ifndef AETHER_BACKEND_QUERY_SERVICE_H
#define AETHER_BACKEND_QUERY_SERVICE_H
#include "aether/backend/query.h"
#include "aether/core/message.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Inproc composition-only binding; Frontend uses query.h, never this header.
 * Borrows public domain services, no state duplication or ownership transfer.
 * All dependencies outlive queries. create fails with cleared output; destroy
 * only releases this facade, never domain registries or independent DTOs.
 */
AetherStatus aether_backend_queries_create(
    AetherWorkspaceRegistry* workspaces, AetherRuntimeRegistry* runtimes,
    AetherSessionRegistry* sessions, AetherMessageRegistry* messages,
    AetherBackendQueries** out_queries
);
void aether_backend_queries_destroy(AetherBackendQueries* queries);
#ifdef __cplusplus
}
#endif
#endif
