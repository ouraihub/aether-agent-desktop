#include "../internal/async_internal.h"

#include <stdlib.h>
#include <string.h>

#define VIEW_LIMIT 16U
#define VIEW_QUEUE_LIMIT 256U
#define VIEW_QUEUE_BYTES (2U * 1024U * 1024U)

typedef struct ViewSlot {
    bool occupied;
    bool active;
    AetherId id;
    AetherId object;
    AetherViewKind kind;
    AetherViewHealth health;
    AetherViewReceiver receiver;
    AetherViewEvent* queue;
    size_t capacity;
    size_t head;
    size_t count;
    uint64_t revision;
    uint64_t sequence;
} ViewSlot;

struct AetherViewHub {
    AetherPlatformMonitor* monitor;
    AetherPlatformThread* thread;
    ViewSlot slots[VIEW_LIMIT];
    size_t next_slot;
    bool closing;
    bool exited;
};

static ViewSlot* find_slot(AetherViewHub* hub, AetherId subscription)
{
    size_t index;
    for (index = 0U; index < VIEW_LIMIT; ++index) {
        ViewSlot* slot = &hub->slots[index];
        if (slot->occupied && aether_id_equal(slot->id, subscription)) return slot;
    }
    return NULL;
}

static void close_slot(ViewSlot* slot)
{
    if (slot->health != AETHER_VIEW_CLOSED) {
        slot->health = AETHER_VIEW_CLOSING;
        slot->head = 0U;
        slot->count = 0U;
    }
}

static ViewSlot* next_delivery(AetherViewHub* hub)
{
    size_t offset;
    for (offset = 0U; offset < VIEW_LIMIT; ++offset) {
        size_t index = (hub->next_slot + offset) % VIEW_LIMIT;
        ViewSlot* slot = &hub->slots[index];
        if (slot->occupied && !slot->active &&
            (slot->health == AETHER_VIEW_CLOSING ||
             (slot->health == AETHER_VIEW_HEALTHY && slot->count != 0U))) {
            hub->next_slot = (index + 1U) % VIEW_LIMIT;
            return slot;
        }
    }
    return NULL;
}

static void deliver(AetherViewHub* hub, ViewSlot* slot)
{
    AetherViewReceiver receiver = slot->receiver;
    AetherId subscription = slot->id;
    bool closing = slot->health == AETHER_VIEW_CLOSING;
    AetherViewEvent event;
    if (!closing) {
        event = slot->queue[slot->head];
        slot->head = (slot->head + 1U) % slot->capacity;
        --slot->count;
    }
    /* Entry gate: unsubscribe/resync may now allow only this entered callback. */
    slot->active = true;
    aether_platform_monitor_unlock(hub->monitor);
    if (closing) receiver.closed(receiver.context, subscription);
    else receiver.event(receiver.context, &event);
    aether_platform_monitor_lock(hub->monitor);
    slot->active = false;
    if (closing) {
        slot->health = AETHER_VIEW_CLOSED;
        memset(&slot->receiver, 0, sizeof(slot->receiver));
    }
    aether_platform_monitor_wake_all(hub->monitor);
}

static void delivery_worker(void* context)
{
    AetherViewHub* hub = context;
    aether_platform_monitor_lock(hub->monitor);
    for (;;) {
        ViewSlot* slot = next_delivery(hub);
        if (slot != NULL) {
            deliver(hub, slot);
        } else if (hub->closing) {
            hub->exited = true;
            aether_platform_monitor_wake_all(hub->monitor);
            aether_platform_monitor_unlock(hub->monitor);
            return;
        } else {
            aether_platform_monitor_wait(hub->monitor);
        }
    }
}

AetherStatus aether_view_hub_create(AetherViewHub** out_hub)
{
    AetherViewHub* hub;
    AetherStatus status;
    if (out_hub == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_hub = NULL;
    hub = calloc(1U, sizeof(*hub));
    if (hub == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    status = aether_platform_monitor_create(&hub->monitor);
    if (status == AETHER_STATUS_OK) {
        status = aether_platform_thread_create(delivery_worker, hub, &hub->thread);
    }
    if (status != AETHER_STATUS_OK) {
        aether_platform_monitor_destroy(hub->monitor);
        free(hub);
        return status;
    }
    *out_hub = hub;
    return AETHER_STATUS_OK;
}

void aether_view_hub_close(AetherViewHub* hub)
{
    size_t index;
    if (hub == NULL) return;
    aether_platform_monitor_lock(hub->monitor);
    hub->closing = true;
    for (index = 0U; index < VIEW_LIMIT; ++index) {
        if (hub->slots[index].occupied) close_slot(&hub->slots[index]);
    }
    aether_platform_monitor_wake_all(hub->monitor);
    aether_platform_monitor_unlock(hub->monitor);
}

bool aether_view_hub_closed(AetherViewHub* hub)
{
    bool closed;
    if (hub == NULL) return false;
    aether_platform_monitor_lock(hub->monitor);
    closed = hub->closing && hub->exited;
    aether_platform_monitor_unlock(hub->monitor);
    return closed;
}

AetherStatus aether_view_hub_destroy(AetherViewHub** hub)
{
    AetherStatus status;
    size_t index;
    if (hub == NULL || *hub == NULL) return AETHER_STATUS_OK;
    if (!aether_view_hub_closed(*hub)) return AETHER_STATUS_ALREADY_EXISTS;
    status = aether_platform_thread_join(&(*hub)->thread);
    if (status != AETHER_STATUS_OK) return status;
    for (index = 0U; index < VIEW_LIMIT; ++index) free((*hub)->slots[index].queue);
    aether_platform_monitor_destroy((*hub)->monitor);
    free(*hub);
    *hub = NULL;
    return AETHER_STATUS_OK;
}

AetherStatus aether_view_open(
    AetherViewHub* hub, AetherViewSnapshot snapshot, AetherId object,
    AetherViewReceiver receiver, size_t capacity, AetherViewSnapshot* out_snapshot
)
{
    ViewSlot* slot = NULL;
    AetherViewEvent* queue;
    AetherId subscription;
    size_t index;
    if (out_snapshot == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (hub == NULL || !aether_id_is_valid(object) || receiver.event == NULL ||
        receiver.closed == NULL || capacity == 0U || capacity > VIEW_QUEUE_LIMIT ||
        capacity > VIEW_QUEUE_BYTES / sizeof(AetherViewEvent)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (snapshot.kind == AETHER_VIEW_WORKSPACE) {
        if (!aether_id_equal(snapshot.value.workspace.id, object)) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        snapshot.base_revision = snapshot.value.workspace.revision;
    } else if (snapshot.kind == AETHER_VIEW_SESSION) {
        if (!aether_id_equal(snapshot.value.session.id, object)) {
            return AETHER_STATUS_INVALID_ARGUMENT;
        }
        snapshot.base_revision = snapshot.value.session.revision;
    } else {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_platform_monitor_lock(hub->monitor);
    if (hub->closing) {
        aether_platform_monitor_unlock(hub->monitor);
        return AETHER_STATUS_CANCELLED;
    }
    for (index = 0U; index < VIEW_LIMIT; ++index) {
        if (!hub->slots[index].occupied) {
            slot = &hub->slots[index];
            break;
        }
    }
    if (slot == NULL) {
        aether_platform_monitor_unlock(hub->monitor);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    queue = malloc(capacity * sizeof(*queue));
    if (queue == NULL) {
        aether_platform_monitor_unlock(hub->monitor);
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    subscription = aether_id_next();
    if (!aether_id_is_valid(subscription)) {
        free(queue);
        aether_platform_monitor_unlock(hub->monitor);
        return AETHER_STATUS_INTERNAL_ERROR;
    }
    *slot = (ViewSlot){0};
    slot->occupied = true;
    slot->id = subscription;
    slot->object = object;
    slot->kind = snapshot.kind;
    slot->health = AETHER_VIEW_HEALTHY;
    slot->receiver = receiver;
    slot->queue = queue;
    slot->capacity = capacity;
    slot->revision = snapshot.base_revision;
    snapshot.subscription_id = subscription;
    snapshot.sequence = 0U;
    *out_snapshot = snapshot;
    aether_platform_monitor_unlock(hub->monitor);
    return AETHER_STATUS_OK;
}

void aether_view_publish(AetherViewHub* hub, AetherViewEvent event)
{
    size_t index;
    if (hub == NULL) return;
    aether_platform_monitor_lock(hub->monitor);
    for (index = 0U; index < VIEW_LIMIT; ++index) {
        ViewSlot* slot = &hub->slots[index];
        size_t position;
        if (!slot->occupied || slot->health != AETHER_VIEW_HEALTHY ||
            slot->kind != event.kind || !aether_id_equal(slot->object, event.object_id) ||
            event.revision <= slot->revision) continue;
        if (!aether_id_is_valid(event.event_id) || event.schema_version != 1U ||
            slot->sequence == UINT64_MAX || slot->count == slot->capacity) {
            slot->health = AETHER_VIEW_RESYNC_REQUIRED;
            slot->head = 0U;
            slot->count = 0U;
            continue;
        }
        position = (slot->head + slot->count) % slot->capacity;
        slot->queue[position] = event;
        slot->queue[position].subscription_id = slot->id;
        slot->queue[position].sequence = ++slot->sequence;
        slot->revision = event.revision;
        ++slot->count;
    }
    aether_platform_monitor_wake_all(hub->monitor);
    aether_platform_monitor_unlock(hub->monitor);
}

void aether_view_close_id(AetherViewHub* hub, AetherId subscription)
{
    ViewSlot* slot;
    if (hub == NULL) return;
    aether_platform_monitor_lock(hub->monitor);
    slot = find_slot(hub, subscription);
    if (slot != NULL) {
        close_slot(slot);
        aether_platform_monitor_wake_all(hub->monitor);
    }
    aether_platform_monitor_unlock(hub->monitor);
}

AetherStatus aether_backend_view_health(
    AetherAsyncQueries* client, AetherId subscription, AetherViewHealth* out_health
)
{
    AetherViewHub* hub;
    ViewSlot* slot;
    if (out_health == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_health = AETHER_VIEW_HEALTHY;
    if (client == NULL || client->views == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    hub = client->views;
    aether_platform_monitor_lock(hub->monitor);
    slot = find_slot(hub, subscription);
    if (slot != NULL) *out_health = slot->health;
    aether_platform_monitor_unlock(hub->monitor);
    return slot != NULL ? AETHER_STATUS_OK : AETHER_STATUS_NOT_FOUND;
}

AetherStatus aether_backend_unsubscribe(AetherAsyncQueries* client, AetherId subscription)
{
    AetherViewHub* hub;
    ViewSlot* slot;
    if (client == NULL || client->views == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    hub = client->views;
    aether_platform_monitor_lock(hub->monitor);
    slot = find_slot(hub, subscription);
    if (slot != NULL) {
        close_slot(slot);
        aether_platform_monitor_wake_all(hub->monitor);
    }
    aether_platform_monitor_unlock(hub->monitor);
    return slot != NULL ? AETHER_STATUS_OK : AETHER_STATUS_NOT_FOUND;
}

AetherStatus aether_backend_view_release(AetherAsyncQueries* client, AetherId subscription)
{
    AetherViewHub* hub;
    ViewSlot* slot;
    AetherStatus status = AETHER_STATUS_OK;
    if (client == NULL || client->views == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    hub = client->views;
    aether_platform_monitor_lock(hub->monitor);
    slot = find_slot(hub, subscription);
    if (slot == NULL) status = AETHER_STATUS_NOT_FOUND;
    else if (slot->health != AETHER_VIEW_CLOSED || slot->active) {
        status = AETHER_STATUS_ALREADY_EXISTS;
    } else {
        free(slot->queue);
        *slot = (ViewSlot){0};
    }
    aether_platform_monitor_unlock(hub->monitor);
    return status;
}
