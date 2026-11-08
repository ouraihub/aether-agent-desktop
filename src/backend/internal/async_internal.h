#ifndef AETHER_BACKEND_ASYNC_INTERNAL_H
#define AETHER_BACKEND_ASYNC_INTERNAL_H
#include "aether/backend/async_query.h"
#include "aether/backend/state_command.h"
#include "aether/backend/command_request.h"
#include "aether/infrastructure/executor.h"
#include "aether/platform/thread.h"

typedef struct AetherViewHub AetherViewHub;
typedef struct CommandRecord {
    bool occupied;
    bool completion_taken;
    bool work_pending;
    AetherRequestContext context;
    AetherId workspace;
    AetherCommandRequestSnapshot snapshot;
} CommandRecord;
_Static_assert(sizeof(CommandRecord) * 128U <= 1024U * 1024U,
               "Command request metadata exceeds configured budget");

typedef struct AsyncQuerySlot {
    bool occupied;
    AetherId work_id;
    size_t charge;
    AetherAsyncQueries* owner;
    AetherAsyncQueryInput input;
    bool write_allowed;
    uint64_t expected_revision;
    size_t name_length;
    char name[128];
    AetherViewReceiver receiver;
    size_t queue_capacity;
    AetherAsyncQueryResult result;
    CommandRecord* request;
} AsyncQuerySlot;

struct AetherAsyncQueries {
    AetherBackendQueries* queries;
    AetherExecutor* executor;
    AetherPlatformMonitor* monitor;
    AsyncQuerySlot* slots;
    size_t capacity;
    size_t result_bytes;
    size_t reserved_bytes;
    bool closing;
    AetherViewHub* views;
    CommandRecord requests[128];
};

AetherStatus aether_async_submit_work(
    AetherAsyncQueries* client, AetherAsyncQueryInput input,
    const AetherStateCommandContext* command, uint64_t expected_revision,
    AetherStringView name, const AetherViewReceiver* receiver,
    size_t queue_capacity, uint64_t timeout_ns, AetherId* out_query
);
AetherStatus aether_view_hub_create(AetherViewHub** out_hub);
AetherStatus aether_view_hub_destroy(AetherViewHub** hub);
void aether_view_hub_close(AetherViewHub* hub);
bool aether_view_hub_closed(AetherViewHub* hub);
AetherStatus aether_view_open(
    AetherViewHub* hub, AetherViewSnapshot snapshot, AetherId object,
    AetherViewReceiver receiver, size_t capacity, AetherViewSnapshot* out_snapshot
);
void aether_view_publish(
    AetherViewHub* hub, AetherViewEvent event
);
void aether_view_close_id(AetherViewHub* hub, AetherId subscription);
void aether_async_publish(AsyncQuerySlot* slot);
AetherStatus aether_async_run_open(AsyncQuerySlot* slot);
/* Caller holds Client monitor for reserve/accept; hooks lock internally. */
CommandRecord* aether_command_reserve(AetherAsyncQueries* client, uint64_t now_ns);
void aether_command_accept(CommandRecord* record, AsyncQuerySlot* slot,
    AetherId id, uint64_t now_ns, uint64_t timeout_ns);
AetherStatus aether_command_start(AsyncQuerySlot* slot);
AetherStatus aether_command_finish(AsyncQuerySlot* slot);
void aether_command_close_locked(AetherAsyncQueries* client, uint64_t now_ns);
void aether_command_cancelled_locked(AsyncQuerySlot* slot, uint64_t now_ns);
void aether_command_clock_failed_locked(AsyncQuerySlot* slot, AetherStatus status);
#endif
