#ifndef AETHER_BACKEND_STATE_COMMAND_H
#define AETHER_BACKEND_STATE_COMMAND_H
#include "aether/backend/async_query.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Local in-memory state commands, not durable product business Commands.
 * They run on the same serialized owner as Queries, with explicit write rights
 * from the trusted inproc host. No paths, IO, Runtime action or implicit saving.
 * They preserve Core revision/state rules. Session request-close only enters
 * Closing, never claims Runtime stopped or resources close-complete. Actual
 * Session/permission/Runtime cleanup remains APP-09/10/12 orchestration.
 */
typedef struct AetherStateCommandContext {
    AetherRequestContext request;
    bool write_allowed;
} AetherStateCommandContext;

typedef struct AetherWorkspaceRenameInput {
    AetherId workspace_id;
    uint64_t expected_revision;
    AetherStringView display_name;
} AetherWorkspaceRenameInput;

typedef struct AetherSessionCloseInput {
    AetherId session_id;
    uint64_t expected_revision;
} AetherSessionCloseInput;

/* Direct forms only on the state owner (not UI callbacks); output is a real
 * updated snapshot, or cleared on failure. Wrong expected revision yields
 * InvalidArgument/REVISION_CONFLICT. Read permission never implies write.
 */
AetherStatus aether_backend_rename_workspace(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherWorkspaceRenameInput input, AetherWorkspaceDto* out_dto,
    AetherQueryError* out_error
);
AetherStatus aether_backend_request_session_close(
    AetherBackendQueries* queries, const AetherStateCommandContext* context,
    AetherSessionCloseInput input, AetherSessionDto* out_dto,
    AetherQueryError* out_error
);

/* Async forms enqueue on the existing Query Client's exclusive state owner.
 * They copy the name and context before returning. Results use typed command
 * kinds and the matching Workspace/Session DTO in the common result channel.
 * Local request records/deadlines/completion are defined in command_request.h;
 * no persistence or Runtime execution promise. Cancellation before dispatch
 * prevents mutation; after dispatch actual execution facts remain even when
 * the request deadline wins. Closing stops new/queued commands, never rolls
 * back already applied mutations. No implicit retries.
 */
AetherStatus aether_backend_async_rename_workspace(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherWorkspaceRenameInput input, uint64_t receiver_generation,
    AetherId* out_command
);
AetherStatus aether_backend_async_request_session_close(
    AetherAsyncQueries* client, AetherStateCommandContext context,
    AetherSessionCloseInput input, uint64_t receiver_generation,
    AetherId* out_command
);
#ifdef __cplusplus
}
#endif
#endif
