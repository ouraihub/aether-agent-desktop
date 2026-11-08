#ifndef AETHER_CORE_MESSAGE_H
#define AETHER_CORE_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "aether/core/session.h"
#include "aether/foundation/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AETHER_MESSAGE_API_VERSION 2U

typedef struct AetherMessageRegistry AetherMessageRegistry;
typedef struct AetherMessageId { AetherId id; } AetherMessageId;

typedef enum AetherMessageRole {
    AETHER_MESSAGE_ROLE_USER = 0,
    AETHER_MESSAGE_ROLE_ASSISTANT,
    AETHER_MESSAGE_ROLE_TOOL,
    AETHER_MESSAGE_ROLE_SYSTEM
} AetherMessageRole;

typedef enum AetherMessageState {
    AETHER_MESSAGE_RECEIVING = 0,
    AETHER_MESSAGE_COMPLETED,
    AETHER_MESSAGE_INTERRUPTED,
    AETHER_MESSAGE_FAILED
} AetherMessageState;

typedef enum AetherBlockKind {
    AETHER_BLOCK_TEXT = 0,
    AETHER_BLOCK_IMAGE,
    AETHER_BLOCK_FILE,
    AETHER_BLOCK_TOOL_CALL,
    AETHER_BLOCK_TOOL_RESULT,
    AETHER_BLOCK_STRUCTURED
} AetherBlockKind;

typedef struct AetherBlockPayload {
    AetherBlockKind kind;
    /* Text, normalized tool arguments/result, or structured UTF-8 data.
     * Structured/tool data is not implicitly JSON or executable code. */
    AetherStringView data;
    /* Tool name, resource MIME, or structured format label; empty for Text. */
    AetherStringView label;
    /* Image/File: opaque authorized resource identity, never an OS path. */
    AetherId resource_id;
    /* ToolCall/ToolResult: explicit call identity and invocation generation. */
    AetherId tool_call_id;
    uint64_t tool_generation;
} AetherBlockPayload;

typedef enum AetherMessageOperation {
    AETHER_MESSAGE_CREATE_BLOCK = 0,
    AETHER_MESSAGE_APPEND_BLOCK,
    AETHER_MESSAGE_REPLACE_BLOCK,
    AETHER_MESSAGE_FINALIZE_BLOCK,
    AETHER_MESSAGE_END
} AetherMessageOperation;

typedef struct AetherMessageFact {
    AetherSessionId session_id;
    AetherMessageId message_id;
    uint64_t runtime_generation;
    AetherMessageOperation operation;
    AetherId block_id;
    uint32_t ordinal;
    uint64_t cursor;
    uint64_t expected_revision;
    AetherBlockPayload payload;
    /* END only: Completed requires all blocks finalized; interruption/failure
     * retains partial blocks. No fake block ID or cursor is used for END. */
    AetherMessageState terminal_state;
} AetherMessageFact;

typedef struct AetherMessageLimits {
    size_t messages;
    size_t blocks_per_message;
    size_t message_bytes;
    size_t total_bytes;
    size_t tool_argument_bytes;
} AetherMessageLimits;

typedef struct AetherMessageSnapshot {
    AetherMessageId message_id;
    AetherSessionId session_id;
    uint64_t runtime_generation;
    AetherMessageRole role;
    AetherMessageState state;
    uint64_t revision;
    size_t block_count;
    size_t stored_bytes;
} AetherMessageSnapshot;

typedef struct AetherBlockSnapshot {
    AetherId block_id;
    uint32_t ordinal;
    uint64_t revision;
    uint64_t cursor;
    bool finalized;
    AetherBlockPayload payload;
} AetherBlockSnapshot;

/* V2 replaces the Text-only development API with no compatibility shim.
 * Synchronous, Thread Compatible: serialize calls and borrowed registries on
 * the same state owner. Session/Runtime registries must outlive this registry.
 * No IO, callbacks, persistence, timeouts or automatic cancellation. Role,
 * state, kind and operation must be named enum values; unknown kind Unsupported.
 * All input views borrowed for the call, valid UTF-8 without embedded NUL;
 * no NUL terminator required. Accepted data/labels are copied atomically.
 * Default maxima: 16 retained messages, 256 blocks, 4MiB/message, 32MiB total,
 * 256KiB/tool arguments. Limits may only lower maxima, must all be nonzero.
 * Metadata, labels, content and exact last-fact replay copy count against
 * message/global bytes. Temporary replacement allocation reserves extra bytes
 * too: it may reject before final size limit; failure changes no prior facts.
 * create accepts Idle/Active Session and current generation; content edits
 * require the same open Session generation. END Interrupted/Failed is allowed
 * during teardown to retain content; Completed requires Idle/Active Session.
 * Block create uses caller-unique nonzero block ID, ordinal [0,block_limit),
 * cursor 1. Further cursors contiguous; only Text accepts append.
 * APPEND/REPLACE/FINALIZE require current block revision; CREATE requires zero.
 * FINALIZE repeats the full current payload, closes a block NOT the message.
 * Block terminal_state is Receiving; END uses empty payload and zero
 * block/ordinal/cursor/expected_revision.
 * Same cursor and exact same last fact is idempotent (no revision change);
 * conflicting duplicate/gap ProtocolError, older cursor InvalidArgument.
 * New facts after terminal/finalized are rejected; exact replay is permitted.
 * Tool results require a finalized ToolCall in the same Session/runtime
 * generation, same invocation ID/generation; one result per call, no guesses.
 * Duplicate call/result identity or occupied ordinal returns AlreadyExists;
 * out-of-range ordinal is InvalidArgument. Labels are at most 127 bytes.
 * release is terminal-only; ToolCall messages referenced by retained results
 * cannot release until results release. Unknown IDs NotFound; association/
 * state/revision errors InvalidArgument; capacity/OOM OutOfMemory. Failure
 * outputs clear. Query metadata by value, query_block returns an owned snapshot
 * (one allocation), freed via block_snapshot_destroy; it never aliases Core.
 * Large snapshot work belongs off UI. destroy(NULL)/cleared handles safe.
 */
AetherMessageLimits aether_message_default_limits(void);
AetherStatus aether_message_registry_create(
    AetherMessageLimits limits, AetherSessionRegistry* sessions,
    AetherRuntimeRegistry* runtimes, AetherMessageRegistry** out_registry
);
AetherStatus aether_message_registry_destroy(AetherMessageRegistry** registry);
AetherStatus aether_message_create(
    AetherMessageRegistry* registry, AetherSessionId session_id,
    uint64_t runtime_generation, AetherMessageRole role, AetherMessageId* out_id
);
AetherStatus aether_message_query(
    const AetherMessageRegistry* registry, AetherMessageId id,
    AetherMessageSnapshot* out_snapshot
);
AetherStatus aether_message_query_block(
    const AetherMessageRegistry* registry, AetherMessageId message_id,
    uint32_t ordinal, AetherBlockSnapshot** out_snapshot
);
void aether_message_block_snapshot_destroy(AetherBlockSnapshot* snapshot);
AetherStatus aether_message_apply(
    AetherMessageRegistry* registry, AetherMessageFact fact
);
AetherStatus aether_message_release(
    AetherMessageRegistry* registry, AetherMessageId id
);

#ifdef __cplusplus
}
#endif
#endif
