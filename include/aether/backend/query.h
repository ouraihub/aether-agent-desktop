#ifndef AETHER_BACKEND_QUERY_H
#define AETHER_BACKEND_QUERY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aether/foundation/id.h"
#include "aether/foundation/trace.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif
#define AETHER_BACKEND_QUERY_VERSION 1U

typedef struct AetherBackendQueries AetherBackendQueries;

typedef struct AetherRequestContext {
    uint32_t api_version;
    AetherTraceContext trace;
    /* Trusted inproc composition verifies identity/rights before constructing.
     * This is not an authentication protocol; external input cannot set rights. */
    AetherId caller_id;
    bool read_all_workspaces;
    AetherId workspace_scope;
} AetherRequestContext;

typedef enum AetherQueryErrorCode {
    AETHER_QUERY_ERROR_NONE = 0,
    AETHER_QUERY_ERROR_INPUT,
    AETHER_QUERY_ERROR_VERSION,
    AETHER_QUERY_ERROR_FORBIDDEN,
    AETHER_QUERY_ERROR_MISSING,
    AETHER_QUERY_ERROR_CAPACITY,
    AETHER_QUERY_ERROR_STATE,
    AETHER_QUERY_ERROR_INTERNAL,
    AETHER_QUERY_ERROR_REVISION_CONFLICT
} AetherQueryErrorCode;

typedef struct AetherQueryError {
    AetherStatus status;
    AetherQueryErrorCode code;
    bool diagnostic_available;
    AetherTraceId diagnostic_id;
    char message[160];
} AetherQueryError;

typedef enum AetherWorkspaceDtoState {
    AETHER_WORKSPACE_DTO_OPEN = 0, AETHER_WORKSPACE_DTO_CLOSING,
    AETHER_WORKSPACE_DTO_CLOSED
} AetherWorkspaceDtoState;
typedef struct AetherWorkspaceDto {
    AetherId id;
    uint64_t revision;
    AetherWorkspaceDtoState state;
    char display_name[128];
    /* No root path or environment/permission secret fields exported. */
} AetherWorkspaceDto;

typedef enum AetherSessionDtoState {
    AETHER_SESSION_DTO_CREATED = 0, AETHER_SESSION_DTO_INITIALIZING,
    AETHER_SESSION_DTO_IDLE, AETHER_SESSION_DTO_ACTIVE,
    AETHER_SESSION_DTO_INTERRUPTED, AETHER_SESSION_DTO_RUNTIME_UNAVAILABLE,
    AETHER_SESSION_DTO_CLOSING, AETHER_SESSION_DTO_CLOSED,
    AETHER_SESSION_DTO_FAILED
} AetherSessionDtoState;
typedef struct AetherSessionDto {
    AetherId id;
    AetherId workspace_id;
    AetherId agent_id;
    AetherId runtime_id;
    uint64_t runtime_generation;
    uint64_t revision;
    uint32_t active_turns;
    AetherSessionDtoState state;
} AetherSessionDto;

typedef enum AetherRuntimeDtoState {
    AETHER_RUNTIME_DTO_REGISTERED = 0, AETHER_RUNTIME_DTO_READY,
    AETHER_RUNTIME_DTO_UNAVAILABLE, AETHER_RUNTIME_DTO_CLOSING
} AetherRuntimeDtoState;
typedef struct AetherRuntimeDto {
    AetherId id;
    AetherId provider_id;
    uint64_t generation;
    uint64_t revision;
    AetherRuntimeDtoState state;
    uint64_t capabilities_known;
    uint64_t capabilities_supported;
} AetherRuntimeDto;

typedef enum AetherMessageDtoRole {
    AETHER_MESSAGE_DTO_USER = 0, AETHER_MESSAGE_DTO_ASSISTANT,
    AETHER_MESSAGE_DTO_TOOL, AETHER_MESSAGE_DTO_SYSTEM
} AetherMessageDtoRole;
typedef enum AetherMessageDtoState {
    AETHER_MESSAGE_DTO_RECEIVING = 0, AETHER_MESSAGE_DTO_COMPLETED,
    AETHER_MESSAGE_DTO_INTERRUPTED, AETHER_MESSAGE_DTO_FAILED
} AetherMessageDtoState;
typedef struct AetherMessageDto {
    AetherId id;
    AetherId session_id;
    uint64_t runtime_generation;
    uint64_t revision;
    size_t block_count;
    AetherMessageDtoRole role;
    AetherMessageDtoState state;
} AetherMessageDto;

typedef enum AetherBlockDtoKind {
    AETHER_BLOCK_DTO_TEXT = 0, AETHER_BLOCK_DTO_IMAGE, AETHER_BLOCK_DTO_FILE,
    AETHER_BLOCK_DTO_TOOL_CALL, AETHER_BLOCK_DTO_TOOL_RESULT,
    AETHER_BLOCK_DTO_STRUCTURED
} AetherBlockDtoKind;
typedef struct AetherBlockDto {
    AetherId id;
    uint32_t ordinal;
    uint64_t revision;
    uint64_t cursor;
    bool finalized;
    AetherBlockDtoKind kind;
    AetherStringView data;
    AetherStringView label;
    AetherId resource_id;
    AetherId tool_call_id;
    uint64_t tool_generation;
} AetherBlockDto;

/* V1 read-only state-owner Query endpoints, not UI-thread asynchronous calls.
 * Thread Compatible: serialize with Core owners; no callbacks/IO/deadline/cancel.
 * Context is separate from business IDs, borrowed for call, never retained.
 * Existing queries return independent values; query_block allocates one owned
 * DTO with terminated UTF-8 payload bytes, released with block_destroy(NULL-safe).
 * Snapshots remain valid after source change or service/Core destruction.
 * Block data+label+DTO <=1MiB and caller max_bytes (1..1MiB); no truncation.
 * Workspace-scope caller sees only that Workspace and descendant objects;
 * Runtime query requires readable Session bound to that Runtime (not global
 * lookup by guessed ID). Scope/caller/trace supplied only by trusted inproc host.
 * Unsupported API version NotSupported; missing scope/caller/trace/ID or limit
 * InvalidArgument; forbidden PermissionDenied; missing NotFound; OOM/size
 * OutOfMemory. Failure DTO clears, safe error code/message + input trace ID
 * copied when valid. error must be non-NULL; error never contains paths, args,
 * provider raw errors or secrets. No request ID/permission is inferred by trace.
 */
AetherStatus aether_backend_query_workspace(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherWorkspaceDto* out_dto, AetherQueryError* out_error
);
AetherStatus aether_backend_query_session(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherSessionDto* out_dto, AetherQueryError* out_error
);
AetherStatus aether_backend_query_runtime(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId session_id, AetherId runtime_id,
    AetherRuntimeDto* out_dto, AetherQueryError* out_error
);
AetherStatus aether_backend_query_message(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId id, AetherMessageDto* out_dto, AetherQueryError* out_error
);
AetherStatus aether_backend_query_block(
    AetherBackendQueries* queries, const AetherRequestContext* context,
    AetherId message_id, uint32_t ordinal, size_t max_bytes,
    AetherBlockDto** out_dto, AetherQueryError* out_error
);
void aether_backend_block_destroy(AetherBlockDto* dto);

#ifdef __cplusplus
}
#endif
#endif
