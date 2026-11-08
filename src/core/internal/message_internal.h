#ifndef AETHER_MESSAGE_INTERNAL_H
#define AETHER_MESSAGE_INTERNAL_H
#include "aether/core/message.h"

typedef struct MessageBlock {
    AetherBlockSnapshot snapshot;
    AetherMessageFact last;
    char* storage;
    size_t storage_bytes;
    size_t charge;
} MessageBlock;

typedef struct MessageRecord {
    bool occupied;
    AetherMessageSnapshot snapshot;
    MessageBlock** blocks;
} MessageRecord;

struct AetherMessageRegistry {
    AetherMessageLimits limits;
    AetherSessionRegistry* sessions;
    AetherRuntimeRegistry* runtimes;
    MessageRecord* records;
    size_t stored_bytes;
};

MessageRecord* aether_message_find(
    const AetherMessageRegistry* registry, AetherMessageId id
);
AetherStatus aether_message_check_binding(
    const AetherMessageRegistry* registry, const MessageRecord* record,
    AetherMessageFact fact, bool require_live
);
/* Checked payload validation/ownership. Strings are copied in one allocation.
 * charge includes MessageBlock and its byte storage, including replay input.
 * build is transactional; no registry state changes before the caller commits.
 */
AetherStatus aether_message_payload_validate(
    AetherBlockPayload payload, size_t tool_argument_bytes
);
bool aether_message_fact_equal(
    AetherMessageFact left, AetherMessageFact right
);
AetherStatus aether_message_build_block(
    AetherMessageRegistry* registry, MessageRecord* record,
    const MessageBlock* previous, AetherMessageFact fact,
    MessageBlock** out_block
);
void aether_message_free_block(MessageBlock* block);
void aether_message_clear_record(
    AetherMessageRegistry* registry, MessageRecord* record
);
AetherStatus aether_message_check_tool(
    const AetherMessageRegistry* registry, const MessageRecord* owner,
    AetherMessageFact fact, const MessageBlock* previous
);
#endif
