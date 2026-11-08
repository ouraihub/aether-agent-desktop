/* Private substitution in view_hub.c; tests actual delivery and rollback. */
#undef malloc
#undef calloc
#undef free
#include "async_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "view allocation:%d\n", __LINE__); \
    exit(EXIT_FAILURE); } } while (0)
#define OK(x) CHECK((x) == AETHER_STATUS_OK)

static size_t calls;
static size_t fail_at;
static size_t live;
static AetherPlatformMonitor* gate;
static size_t closures;
void* aether_test_malloc(size_t size);
void* aether_test_calloc(size_t count, size_t size);
void aether_test_free(void* pointer);

void* aether_test_malloc(size_t size)
{
    void* pointer;
    if (++calls == fail_at) return NULL;
    pointer = malloc(size);
    if (pointer != NULL) ++live;
    return pointer;
}
void* aether_test_calloc(size_t count, size_t size)
{
    void* pointer;
    if (size != 0U && count > SIZE_MAX / size) return NULL;
    pointer = aether_test_malloc(count * size);
    if (pointer != NULL) (void)memset(pointer, 0, count * size);
    return pointer;
}
void aether_test_free(void* pointer)
{
    if (pointer != NULL) { CHECK(live != 0U); --live; }
    free(pointer);
}
static void event_received(void* context, const AetherViewEvent* event)
{
    (void)context;
    (void)event;
    CHECK(false); /* No publication is part of this rollback scenario. */
}
static void closed(void* context, AetherId id)
{
    (void)context;
    CHECK(aether_id_is_valid(id));
    aether_platform_monitor_lock(gate);
    ++closures;
    aether_platform_monitor_unlock(gate);
}
int main(void)
{
    AetherViewHub* hub = NULL;
    AetherViewSnapshot source = {0};
    AetherViewSnapshot opened;
    AetherId object = aether_id_next();
    AetherViewReceiver receiver = {event_received, closed, NULL};
    fail_at = 1U;
    CHECK(aether_view_hub_create(&hub) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(hub == NULL && live == 0U);
    fail_at = 0U;
    OK(aether_platform_monitor_create(&gate));
    OK(aether_view_hub_create(&hub));
    source.kind = AETHER_VIEW_WORKSPACE;
    source.value.workspace.id = object;
    source.value.workspace.revision = 1U;
    fail_at = calls + 1U;
    CHECK(aether_view_open(hub, source, object, receiver, 1U, &opened) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(opened.subscription_id.value == 0U && live == 1U);
    fail_at = 0U;
    for (size_t index = 0U; index < 16U; ++index) {
        OK(aether_view_open(hub, source, object, receiver, 1U, &opened));
        CHECK(opened.sequence == 0U && opened.base_revision == 1U);
    }
    CHECK(aether_view_open(hub, source, object, receiver, 1U, &opened) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(opened.subscription_id.value == 0U);
    aether_view_hub_close(hub);
    while (!aether_view_hub_closed(hub)) { }
    aether_platform_monitor_lock(gate);
    CHECK(closures == 16U); /* Failed admission never retained callback context. */
    aether_platform_monitor_unlock(gate);
    OK(aether_view_hub_destroy(&hub));
    CHECK(live == 0U && hub == NULL);
    aether_platform_monitor_destroy(gate);
    return 0;
}
