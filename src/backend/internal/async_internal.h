#ifndef AETHER_BACKEND_ASYNC_INTERNAL_H
#define AETHER_BACKEND_ASYNC_INTERNAL_H
#include "aether/backend/async_query.h"
#include "aether/backend/state_command.h"
#include "aether/infrastructure/executor.h"
#include "aether/platform/thread.h"

typedef struct AetherViewHub AetherViewHub;
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
};

AetherStatus aether_async_submit_work(
    AetherAsyncQueries* client, AetherAsyncQueryInput input,
    const AetherStateCommandContext* command, uint64_t expected_revision,
    AetherStringView name, const AetherViewReceiver* receiver,
    size_t queue_capacity, AetherId* out_query
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
#endif
