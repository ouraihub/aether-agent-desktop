#include "message_fixture.h"

static void test_all_block_kinds(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageId id = new_message(&f, AETHER_MESSAGE_ROLE_ASSISTANT);
    AetherMessageId result_id;
    AetherMessageFact facts[5];
    AetherMessageFact result;
    AetherBlockSnapshot* snapshot;
    const AetherBlockKind kinds[] = {AETHER_BLOCK_TEXT, AETHER_BLOCK_IMAGE,
        AETHER_BLOCK_FILE, AETHER_BLOCK_TOOL_CALL, AETHER_BLOCK_STRUCTURED};
    const char* data[] = {"", "", "", "not JSON: normalized arguments", "a,b\n1,2"};
    const char* labels[] = {"", "image/png", "application/pdf", "lookup", "csv"};
    AetherId call_id = aether_id_next();
    for (uint32_t i = 0U; i < 5U; ++i) {
        facts[i] = block_fact(&f, id, i, kinds[i], data[i], labels[i]);
        if (kinds[i] == AETHER_BLOCK_IMAGE || kinds[i] == AETHER_BLOCK_FILE) {
            facts[i].payload.resource_id = aether_id_next();
        } else if (kinds[i] == AETHER_BLOCK_TOOL_CALL) {
            facts[i].payload.tool_call_id = call_id;
            facts[i].payload.tool_generation = 7U;
        }
        OK(aether_message_apply(f.messages, facts[i]));
        snapshot = block_snapshot(&f, id, i);
        CHECK(snapshot->payload.kind == kinds[i] && snapshot->ordinal == i);
        CHECK(snapshot->block_id.value == facts[i].block_id.value);
        CHECK(snapshot->payload.resource_id.value == facts[i].payload.resource_id.value);
        CHECK(snapshot->payload.tool_call_id.value == facts[i].payload.tool_call_id.value);
        CHECK(snapshot->payload.tool_generation == facts[i].payload.tool_generation);
        check_text(snapshot->payload.data, data[i]);
        check_text(snapshot->payload.label, labels[i]);
        aether_message_block_snapshot_destroy(snapshot);
        if (i == 4U) {
            CHECK(aether_message_apply(f.messages, end_fact(&f, id, AETHER_MESSAGE_COMPLETED)) ==
                AETHER_STATUS_INVALID_ARGUMENT);
            CHECK(metadata(&f, id).state == AETHER_MESSAGE_RECEIVING);
        }
        finalize(&f, facts[i]);
    }
    CHECK(metadata(&f, id).block_count == 5U);
    CHECK(metadata(&f, id).state == AETHER_MESSAGE_RECEIVING);
    end_message(&f, id, AETHER_MESSAGE_COMPLETED);
    result_id = new_message(&f, AETHER_MESSAGE_ROLE_TOOL);
    result = block_fact(&f, result_id, 0U, AETHER_BLOCK_TOOL_RESULT, "plain result", "");
    result.payload.tool_call_id = call_id;
    result.payload.tool_generation = 7U;
    OK(aether_message_apply(f.messages, result));
    snapshot = block_snapshot(&f, result_id, 0U);
    CHECK(snapshot->payload.kind == AETHER_BLOCK_TOOL_RESULT);
    CHECK(snapshot->payload.tool_call_id.value == call_id.value);
    CHECK(snapshot->payload.tool_generation == 7U);
    check_text(snapshot->payload.data, "plain result");
    aether_message_block_snapshot_destroy(snapshot);
    finalize(&f, result);
    end_message(&f, result_id, AETHER_MESSAGE_COMPLETED);
    CHECK(aether_message_release(f.messages, id) == AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_message_release(f.messages, result_id));
    OK(aether_message_release(f.messages, id));
    dispose(&f);
}

static void test_tool_associations_and_release(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageId calls = new_message(&f, AETHER_MESSAGE_ROLE_ASSISTANT);
    AetherMessageId results = new_message(&f, AETHER_MESSAGE_ROLE_TOOL);
    AetherMessageId foreign;
    AetherMessageId duplicate_results = new_message(&f, AETHER_MESSAGE_ROLE_TOOL);
    AetherMessageFact call = block_fact(&f, calls, 0U, AETHER_BLOCK_TOOL_CALL, "a=1", "run");
    AetherMessageFact result = block_fact(&f, results, 0U, AETHER_BLOCK_TOOL_RESULT, "ok", "");
    AetherMessageFact invalid;
    AetherMessageSnapshot before;
    call.payload.tool_call_id = aether_id_next();
    call.payload.tool_generation = 9U;
    result.payload.tool_call_id = call.payload.tool_call_id;
    result.payload.tool_generation = 9U;
    before = metadata(&f, results);
    /* A result cannot infer a call that has not arrived. */
    CHECK(aether_message_apply(f.messages, result) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(metadata(&f, results).revision == before.revision);
    OK(aether_message_apply(f.messages, call));
    CHECK(aether_message_apply(f.messages, result) == AETHER_STATUS_INVALID_ARGUMENT);
    finalize(&f, call);
    invalid = result;
    invalid.payload.tool_generation = 8U;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid = result;
    invalid.payload.tool_call_id = aether_id_next();
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid = result;
    invalid.runtime_generation = 2U;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    OK(aether_message_create(f.messages, f.other_session, 1U, AETHER_MESSAGE_ROLE_TOOL, &foreign));
    invalid = result;
    invalid.session_id = f.other_session;
    invalid.message_id = foreign;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(metadata(&f, foreign).block_count == 0U);
    invalid = call;
    invalid.block_id = aether_id_next();
    invalid.ordinal = 1U;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(metadata(&f, calls).block_count == 1U);
    OK(aether_message_apply(f.messages, result));
    before = metadata(&f, results);
    OK(aether_message_apply(f.messages, result));
    CHECK(metadata(&f, results).revision == before.revision);
    invalid = result;
    invalid.block_id = aether_id_next();
    invalid.ordinal = 1U;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_ALREADY_EXISTS);
    invalid.message_id = duplicate_results;
    invalid.ordinal = 0U;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(metadata(&f, duplicate_results).block_count == 0U);
    invalid.message_id = foreign;
    invalid.session_id = f.other_session;
    invalid.ordinal = 0U;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    end_message(&f, calls, AETHER_MESSAGE_COMPLETED);
    CHECK(aether_message_release(f.messages, calls) == AETHER_STATUS_ALREADY_EXISTS);
    /* Interrupted result blocks still hold their source call until release. */
    end_message(&f, results, AETHER_MESSAGE_INTERRUPTED);
    CHECK(aether_message_release(f.messages, calls) == AETHER_STATUS_ALREADY_EXISTS);
    OK(aether_message_release(f.messages, results));
    OK(aether_message_release(f.messages, calls));
    invalid = end_fact(&f, foreign, AETHER_MESSAGE_FAILED);
    invalid.session_id = f.other_session;
    OK(aether_message_apply(f.messages, invalid));
    OK(aether_message_release(f.messages, foreign));
    end_message(&f, duplicate_results, AETHER_MESSAGE_FAILED);
    OK(aether_message_release(f.messages, duplicate_results));
    dispose(&f);
}

static void test_nontext_updates_and_immutable_identity(void)
{
    MessageFixture f = fixture(aether_message_default_limits());
    AetherMessageId id = new_message(&f, AETHER_MESSAGE_ROLE_ASSISTANT);
    AetherMessageFact create = block_fact(&f, id, 0U, AETHER_BLOCK_STRUCTURED, "a", "csv");
    AetherMessageFact edit;
    AetherMessageSnapshot before;
    AetherBlockSnapshot* snapshot;
    OK(aether_message_apply(f.messages, create));
    before = metadata(&f, id);
    edit = create;
    edit.operation = AETHER_MESSAGE_APPEND_BLOCK;
    edit.expected_revision = 1U;
    edit.cursor = 2U;
    edit.payload.data = text("b");
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_NOT_SUPPORTED);
    edit.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    edit.payload.label = text("json");
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    edit.payload.label = text("csv");
    edit.payload.kind = AETHER_BLOCK_TEXT;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(metadata(&f, id).revision == before.revision);
    edit.payload.kind = AETHER_BLOCK_STRUCTURED;
    OK(aether_message_apply(f.messages, edit));
    snapshot = block_snapshot(&f, id, 0U);
    check_text(snapshot->payload.data, "b");
    aether_message_block_snapshot_destroy(snapshot);
    create = block_fact(&f, id, 1U, AETHER_BLOCK_IMAGE, "", "image/png");
    create.payload.resource_id = aether_id_next();
    OK(aether_message_apply(f.messages, create));
    edit = create;
    edit.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    edit.expected_revision = 1U;
    edit.cursor = 2U;
    edit.payload.resource_id = aether_id_next();
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    create = block_fact(&f, id, 2U, AETHER_BLOCK_TOOL_CALL, "x", "tool");
    create.payload.tool_call_id = aether_id_next();
    create.payload.tool_generation = 1U;
    OK(aether_message_apply(f.messages, create));
    edit = create;
    edit.operation = AETHER_MESSAGE_REPLACE_BLOCK;
    edit.expected_revision = 1U;
    edit.cursor = 2U;
    edit.payload.tool_call_id = aether_id_next();
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    edit.payload.tool_call_id = create.payload.tool_call_id;
    edit.payload.tool_generation = 2U;
    CHECK(aether_message_apply(f.messages, edit) == AETHER_STATUS_INVALID_ARGUMENT);
    end_message(&f, id, AETHER_MESSAGE_INTERRUPTED);
    dispose(&f);
}

static void test_payload_validation_is_atomic(void)
{
    AetherMessageLimits limits = aether_message_default_limits();
    MessageFixture f;
    AetherMessageId id;
    AetherMessageFact valid;
    AetherMessageFact invalid;
    AetherMessageSnapshot before;
    char long_label[128];
    const char embedded_nul[] = {'a', '\0', 'b'};
    const char bad_utf8[] = "\xC0\xAF";
    limits.tool_argument_bytes = 4U;
    f = fixture(limits);
    id = new_message(&f, AETHER_MESSAGE_ROLE_ASSISTANT);
    valid = block_fact(&f, id, 0U, AETHER_BLOCK_TEXT, "text", "");
    before = metadata(&f, id);
    invalid = valid;
    invalid.payload.kind = (AetherBlockKind)99;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_NOT_SUPPORTED);
    invalid = valid;
    invalid.payload.data = (AetherStringView){embedded_nul, sizeof(embedded_nul)};
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid.payload.data = (AetherStringView){bad_utf8, sizeof(bad_utf8)};
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid.payload.data = (AetherStringView){NULL, 1U};
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid = valid;
    invalid.payload.label = text("not text metadata");
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid = valid;
    invalid.payload.resource_id = aether_id_next();
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    for (unsigned kind = AETHER_BLOCK_IMAGE; kind <= AETHER_BLOCK_FILE; ++kind) {
        invalid = valid;
        invalid.payload.kind = (AetherBlockKind)kind;
        invalid.payload.data = text("");
        invalid.payload.label = text("image/png");
        CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
        invalid.payload.resource_id = aether_id_next();
        invalid.payload.label = text("");
        CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
        invalid.payload.label = text("image/png");
        invalid.payload.data = text("not a resource path");
        CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    }
    invalid = block_fact(&f, id, 0U, AETHER_BLOCK_TOOL_CALL, "1234", "tool");
    invalid.payload.tool_call_id = aether_id_next();
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid.payload.tool_generation = 1U;
    invalid.payload.tool_call_id.value = 0U;
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid.payload.tool_call_id = aether_id_next();
    invalid.payload.label = text("");
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    invalid.payload.label = text("tool");
    invalid.payload.data = text("12345");
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_OUT_OF_MEMORY);
    invalid = block_fact(&f, id, 0U, AETHER_BLOCK_STRUCTURED, "data", "");
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    memset(long_label, 'x', sizeof(long_label));
    invalid.payload.label = (AetherStringView){long_label, sizeof(long_label)};
    CHECK(aether_message_apply(f.messages, invalid) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(metadata(&f, id).block_count == 0U);
    CHECK(metadata(&f, id).revision == before.revision);
    CHECK(metadata(&f, id).stored_bytes == before.stored_bytes);
    OK(aether_message_apply(f.messages, valid));
    valid = block_fact(&f, id, 1U, AETHER_BLOCK_TOOL_CALL, "1234", "tool");
    valid.payload.tool_call_id = aether_id_next();
    valid.payload.tool_generation = 1U;
    OK(aether_message_apply(f.messages, valid));
    dispose(&f);
}

int main(void)
{
    test_all_block_kinds();
    test_tool_associations_and_release();
    test_nontext_updates_and_immutable_identity();
    test_payload_validation_is_atomic();
    return 0;
}
