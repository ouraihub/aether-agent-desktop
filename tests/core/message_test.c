#include "message_fixture.h"

static void test_cursors_cas_and_completion(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageId id = new_message(&f, AETHER_MESSAGE_ROLE_ASSISTANT);
    AetherMessageFact create = block_fact(&f, id, 0U, AETHER_BLOCK_TEXT, "hello", "");
    char delta[] = {' ', 'w', 'o', 'r', 'l', 'd'};
    AetherMessageFact edit = create;
    AetherMessageSnapshot before;
    AetherBlockSnapshot* block;
    edit.cursor = 2U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_PROTOCOL_ERROR);
    OK(aether_message_apply(f.messages, create));
    before = metadata(&f, id);
    OK(aether_message_apply(f.messages, create));
    CHECK(metadata(&f, id).revision == before.revision);
    edit = create;
    edit.payload.data = text("different");
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_PROTOCOL_ERROR);
    edit = create;
    edit.expected_revision = 1U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_PROTOCOL_ERROR);
    edit = create;
    edit.terminal_state = AETHER_MESSAGE_FAILED;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    edit = create;
    edit.ordinal = 1U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    edit = create;
    edit.operation = AETHER_MESSAGE_APPEND_BLOCK;
    edit.payload.data = text(" world");
    edit.expected_revision = 1U;
    edit.cursor = 3U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_PROTOCOL_ERROR);
    edit.cursor = 2U;
    edit.expected_revision = 2U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    edit.expected_revision = 1U;
    edit.payload.data = (AetherStringView){delta, sizeof(delta)};
    OK(aether_message_apply(f.messages, edit));
    memset(delta, 'x', sizeof(delta));
    edit.payload.data = text(" world");
    block = block_snapshot(&f, id, 0U);
    check_text(block->payload.data, "hello world");
    aether_message_block_snapshot_destroy(block);
    before = metadata(&f, id);
    OK(aether_message_apply(f.messages, edit));
    CHECK(metadata(&f, id).revision == before.revision);
    CHECK(aether_message_apply(f.messages, create) == AETHER_STATUS_INVALID_ARGUMENT);
    edit.payload.data = text("conflict");
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_PROTOCOL_ERROR);
    edit.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    edit.payload.data = text("replacement");
    edit.cursor = 3U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    edit.expected_revision = 2U;
    OK(aether_message_apply(f.messages, edit));
    block = block_snapshot(&f, id, 0U);
    CHECK(block->revision == 3U && block->cursor == 3U && !block->finalized);
    check_text(block->payload.data, "replacement");
    aether_message_block_snapshot_destroy(block);
    CHECK(aether_message_apply(f.messages, end_fact(&f, id, AETHER_MESSAGE_COMPLETED)) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_message_release(f.messages, id) == AETHER_STATUS_INVALID_ARGUMENT);
    finalize(&f, edit);
    CHECK(metadata(&f, id).state == AETHER_MESSAGE_RECEIVING);
    block = block_snapshot(&f, id, 0U);
    CHECK(block->finalized && block->revision == 4U && block->cursor == 4U);
    check_text(block->payload.data, "replacement");
    edit.operation = AETHER_MESSAGE_FINALIZE_BLOCK;
    edit.expected_revision = 3U;
    edit.cursor = 4U;
    before = metadata(&f, id);
    OK(aether_message_apply(f.messages, edit));
    CHECK(metadata(&f, id).revision == before.revision);
    edit.operation = AETHER_MESSAGE_APPEND_BLOCK;
    edit.expected_revision = 4U;
    edit.cursor = 5U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    aether_message_block_snapshot_destroy(block);
    end_message(&f, id, AETHER_MESSAGE_COMPLETED);
    before = metadata(&f, id);
    CHECK(before.state == AETHER_MESSAGE_COMPLETED && before.block_count == 1U);
    OK(aether_message_apply(f.messages, end_fact(&f, id, AETHER_MESSAGE_COMPLETED)));
    CHECK(metadata(&f, id).revision == before.revision);
    CHECK(aether_message_apply(f.messages, end_fact(&f, id, AETHER_MESSAGE_FAILED)) ==
        AETHER_STATUS_ALREADY_EXISTS);
    edit = block_fact(&f, id, 1U, AETHER_BLOCK_TEXT, "late", "");
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    OK(aether_message_release(f.messages, id));
    CHECK(aether_message_query(f.messages, id, &before) == AETHER_STATUS_NOT_FOUND);
    CHECK(before.message_id.id.value == 0U && before.block_count == 0U);
    CHECK(aether_message_release(f.messages, id) == AETHER_STATUS_NOT_FOUND);
    dispose(&f);
}

static void test_owned_inputs_and_snapshots(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageId id = new_message(&f, AETHER_MESSAGE_ROLE_SYSTEM);
    char data[] = {'a', 'b', 'c'};
    char label[] = {'c', 's', 'v'};
    AetherMessageFact fact = block_fact(&f, id, 0U, AETHER_BLOCK_STRUCTURED, "", "");
    AetherBlockSnapshot* first;
    AetherBlockSnapshot* second;
    AetherMessageSnapshot before;
    fact.payload.data = (AetherStringView){data, sizeof(data)};
    fact.payload.label = (AetherStringView){label, sizeof(label)};
    OK(aether_message_apply(f.messages, fact));
    memset(data, 'x', sizeof(data));
    memset(label, 'x', sizeof(label));
    first = block_snapshot(&f, id, 0U);
    second = block_snapshot(&f, id, 0U);
    CHECK(first != second && first->payload.data.data != second->payload.data.data);
    CHECK(first->payload.label.data != second->payload.label.data);
    check_text(first->payload.data, "abc");
    check_text(first->payload.label, "csv");
    CHECK(first->payload.data.data[3] == '\0' && first->payload.label.data[3] == '\0');
    fact.payload = first->payload;
    before = metadata(&f, id);
    OK(aether_message_apply(f.messages, fact));
    fact.payload.label = text("tsv");
    CHECK(aether_message_apply(f.messages, fact) == AETHER_STATUS_PROTOCOL_ERROR);
    fact.payload = first->payload;
    CHECK(metadata(&f, id).revision == before.revision);
    fact.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    fact.cursor = 2U;
    fact.expected_revision = 1U;
    fact.payload.data = text("new");
    OK(aether_message_apply(f.messages, fact));
    check_text(first->payload.data, "abc");
    check_text(second->payload.data, "abc");
    end_message(&f, id, AETHER_MESSAGE_INTERRUPTED);
    OK(aether_message_release(f.messages, id));
    dispose(&f);
    check_text(first->payload.data, "abc");
    check_text(second->payload.label, "csv");
    aether_message_block_snapshot_destroy(first);
    aether_message_block_snapshot_destroy(second);
    aether_message_block_snapshot_destroy(NULL);
}

static void test_binding_and_interruption(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageId id = new_message(&f, AETHER_MESSAGE_ROLE_USER);
    AetherMessageId rejected = id;
    AetherMessageFact fact = block_fact(&f, id, 0U, AETHER_BLOCK_TEXT, "partial", "");
    AetherMessageFact wrong = fact;
    AetherMessageSnapshot before = metadata(&f, id);
    AetherBlockSnapshot* block;
    wrong.session_id = f.other_session;
    CHECK(aether_message_apply(f.messages, wrong) == AETHER_STATUS_INVALID_ARGUMENT);
    wrong = fact;
    wrong.runtime_generation = 2U;
    CHECK(aether_message_apply(f.messages, wrong) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_message_create(f.messages, f.session, 2U, AETHER_MESSAGE_ROLE_USER,
        &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(rejected.id.value == 0U && metadata(&f, id).revision == before.revision);
    OK(aether_message_apply(f.messages, fact));
    OK(aether_runtime_reconnect(f.runtimes, f.runtime, 1U));
    OK(aether_runtime_update(f.runtimes, f.runtime, 2U, AETHER_RUNTIME_READY,
        (AetherRuntimeCapabilities){AETHER_RUNTIME_CAP_SESSION, AETHER_RUNTIME_CAP_SESSION}));
    CHECK(aether_message_create(f.messages, f.session, 1U, AETHER_MESSAGE_ROLE_USER,
        &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(rejected.id.value == 0U);
    CHECK(aether_message_create(f.messages, f.session, 2U, AETHER_MESSAGE_ROLE_USER,
        &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(rejected.id.value == 0U);
    fact.operation = AETHER_MESSAGE_APPEND_BLOCK;
    fact.expected_revision = 1U;
    fact.cursor = 2U;
    fact.payload.data = text(" rejected");
    CHECK(aether_message_apply(f.messages, fact) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_message_apply(f.messages, end_fact(&f, id, AETHER_MESSAGE_COMPLETED)) ==
        AETHER_STATUS_INVALID_ARGUMENT);
    OK(aether_session_mark_runtime_unavailable(f.sessions, f.session));
    end_message(&f, id, AETHER_MESSAGE_INTERRUPTED);
    CHECK(metadata(&f, id).state == AETHER_MESSAGE_INTERRUPTED);
    block = block_snapshot(&f, id, 0U);
    CHECK(!block->finalized && block->cursor == 1U && block->revision == 1U);
    check_text(block->payload.data, "partial");
    aether_message_block_snapshot_destroy(block);
    OK(aether_message_release(f.messages, id));
    dispose(&f);
}

static void test_cross_message_and_closing(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageId first = new_message(&f, AETHER_MESSAGE_ROLE_USER);
    AetherMessageId second = new_message(&f, AETHER_MESSAGE_ROLE_ASSISTANT);
    AetherMessageFact fact = block_fact(&f, first, 0U,
                                        AETHER_BLOCK_TEXT, "retained", "");
    AetherMessageFact wrong;
    AetherBlockSnapshot* snapshot;
    OK(aether_message_apply(f.messages, fact));
    wrong = fact;
    wrong.message_id = second;
    CHECK(aether_message_apply(f.messages, wrong) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(metadata(&f, second).block_count == 0U);
    OK(aether_session_begin_close(f.sessions, f.session));
    wrong = fact;
    wrong.operation = AETHER_MESSAGE_APPEND_BLOCK;
    wrong.expected_revision = 1U;
    wrong.cursor = 2U;
    wrong.payload.data = text("late");
    CHECK(aether_message_apply(f.messages, wrong) == AETHER_STATUS_INVALID_ARGUMENT);
    end_message(&f, first, AETHER_MESSAGE_INTERRUPTED);
    snapshot = block_snapshot(&f, first, 0U);
    check_text(snapshot->payload.data, "retained");
    CHECK(!snapshot->finalized && snapshot->revision == 1U);
    aether_message_block_snapshot_destroy(snapshot);
    dispose(&f);
}

static void test_limits_and_atomic_capacity(void)
{
    AetherMessageLimits limits = aether_message_default_limits();
    MessageFixture f = fixture(limits);
    AetherMessageId id = new_message(&f, AETHER_MESSAGE_ROLE_USER);
    AetherMessageFact fact = block_fact(&f, id, 0U, AETHER_BLOCK_TEXT, "seed", "");
    size_t base = metadata(&f, id).stored_bytes;
    size_t full;
    AetherMessageSnapshot before;
    AetherMessageId other;
    AetherBlockSnapshot* block;
    OK(aether_message_apply(f.messages, fact));
    full = metadata(&f, id).stored_bytes;
    CHECK(full > base);
    OK(aether_message_registry_destroy(&f.messages));
    /* An update must reserve its complete replacement before releasing the old block. */
    for (unsigned mode = 0U; mode < 2U; ++mode) {
        limits = aether_message_default_limits();
        if (mode == 0U) {
            limits.message_bytes = full;
        } else {
            limits.total_bytes = full;
        }
        OK(aether_message_registry_create(limits, f.sessions, f.runtimes, &f.messages));
        id = new_message(&f, AETHER_MESSAGE_ROLE_USER);
        fact = block_fact(&f, id, 0U, AETHER_BLOCK_TEXT, "seed", "");
        OK(aether_message_apply(f.messages, fact));
        before = metadata(&f, id);
        fact.operation = AETHER_MESSAGE_REPLACE_BLOCK;
        fact.expected_revision = 1U;
        fact.cursor = 2U;
        fact.payload.data = text("x");
        CHECK(aether_message_apply(f.messages, fact) == AETHER_STATUS_OUT_OF_MEMORY);
        fact.operation = AETHER_MESSAGE_APPEND_BLOCK;
        CHECK(aether_message_apply(f.messages, fact) == AETHER_STATUS_OUT_OF_MEMORY);
        CHECK(metadata(&f, id).revision == before.revision);
        CHECK(metadata(&f, id).stored_bytes == before.stored_bytes);
        CHECK(metadata(&f, id).block_count == 1U);
        block = block_snapshot(&f, id, 0U);
        CHECK(block->revision == 1U && block->cursor == 1U && !block->finalized);
        check_text(block->payload.data, "seed");
        aether_message_block_snapshot_destroy(block);
        fact.operation = AETHER_MESSAGE_CREATE_BLOCK;
        fact.expected_revision = 0U;
        fact.cursor = 1U;
        fact.payload.data = text("seed");
        OK(aether_message_apply(f.messages, fact));
        end_message(&f, id, AETHER_MESSAGE_FAILED);
        OK(aether_message_release(f.messages, id));
        id = new_message(&f, AETHER_MESSAGE_ROLE_USER);
        fact.message_id = id;
        OK(aether_message_apply(f.messages, fact));
        OK(aether_message_registry_destroy(&f.messages));
    }
    limits = aether_message_default_limits();
    limits.messages = 1U;
    limits.blocks_per_message = 1U;
    OK(aether_message_registry_create(limits, f.sessions, f.runtimes, &f.messages));
    id = new_message(&f, AETHER_MESSAGE_ROLE_USER);
    other = id;
    CHECK(aether_message_create(f.messages, f.session, 1U, AETHER_MESSAGE_ROLE_USER,
        &other) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(other.id.value == 0U);
    fact = block_fact(&f, id, 0U, AETHER_BLOCK_TEXT, "", "");
    OK(aether_message_apply(f.messages, fact));
    before = metadata(&f, id);
    fact.block_id = aether_id_next();
    fact.ordinal = 1U;
    CHECK(aether_message_apply(f.messages, fact) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(metadata(&f, id).block_count == 1U && metadata(&f, id).revision == before.revision);
    dispose(&f);
}

static void test_invalid_limits_and_outputs(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageLimits defaults = aether_message_default_limits();
    AetherMessageLimits limits;
    AetherMessageRegistry* output;
    AetherMessageId missing = {{UINT64_MAX}};
    AetherBlockSnapshot* block = (AetherBlockSnapshot*)&f;
    AetherMessageSnapshot snapshot;
    for (unsigned field = 0U; field < 5U; ++field) {
        for (unsigned excessive = 0U; excessive < 2U; ++excessive) {
            limits = defaults;
            switch (field) {
            case 0U: limits.messages = excessive ? defaults.messages + 1U : 0U; break;
            case 1U: limits.blocks_per_message = excessive ? 257U : 0U; break;
            case 2U: limits.message_bytes = excessive ? defaults.message_bytes + 1U : 0U; break;
            case 3U: limits.total_bytes = excessive ? defaults.total_bytes + 1U : 0U; break;
            default: limits.tool_argument_bytes = excessive ? 262145U : 0U; break;
            }
            output = f.messages;
            CHECK(aether_message_registry_create(limits, f.sessions, f.runtimes, &output) ==
                AETHER_STATUS_INVALID_ARGUMENT);
            CHECK(output == NULL);
        }
    }
    memset(&snapshot, 0xff, sizeof(snapshot));
    CHECK(aether_message_query(f.messages, missing, &snapshot) == AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.message_id.id.value == 0U && snapshot.stored_bytes == 0U);
    CHECK(aether_message_query_block(f.messages, missing, 0U, &block) == AETHER_STATUS_NOT_FOUND);
    CHECK(block == NULL);
    OK(aether_message_registry_destroy(NULL));
    dispose(&f);
    OK(aether_message_registry_destroy(&f.messages));
}

int main(void)
{
    test_cursors_cas_and_completion();
    test_owned_inputs_and_snapshots();
    test_binding_and_interruption();
    test_cross_message_and_closing();
    test_limits_and_atomic_capacity();
    test_invalid_limits_and_outputs();
    return 0;
}
