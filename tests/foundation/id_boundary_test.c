#include "aether/foundation/id.h"
#include "aether/platform/thread.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "id boundary:%d\n", __LINE__); \
    exit(EXIT_FAILURE); } } while (0)

typedef struct Batch {
    AetherId ids[64];
} Batch;

static void generate(void* context)
{
    Batch* batch = context;
    for (size_t index = 0U; index < 64U; ++index) batch->ids[index] = aether_id_next();
}

int main(void)
{
    AetherPlatformThread* threads[4] = {0};
    Batch batches[4] = {0};
    uint64_t values[256];
    size_t count = 0U;
    for (size_t index = 0U; index < 4U; ++index) {
        CHECK(aether_platform_thread_create(generate, &batches[index],
                                            &threads[index]) == AETHER_STATUS_OK);
    }
    for (size_t index = 0U; index < 4U; ++index) {
        CHECK(aether_platform_thread_join(&threads[index]) == AETHER_STATUS_OK);
        for (size_t offset = 0U; offset < 64U; ++offset) {
            uint64_t value = batches[index].ids[offset].value;
            if (value == 0U) continue;
            for (size_t earlier = 0U; earlier < count; ++earlier) {
                CHECK(values[earlier] != value);
            }
            values[count++] = value;
        }
    }
#ifdef AETHER_TEST_ID_EXHAUSTION
    CHECK(count == 31U);
    for (size_t index = 0U; index < count; ++index) {
        CHECK(values[index] >= UINT64_MAX - 30U && values[index] <= UINT64_MAX);
    }
    for (size_t index = 0U; index < 100U; ++index) CHECK(aether_id_next().value == 0U);
    puts("Concurrent exhaustion: 31 unique final IDs, all later requests invalid");
#else
    CHECK(count == 256U);
    bool crossed = false;
    for (size_t index = 0U; index < count; ++index) {
        CHECK(values[index] >= (uint64_t)INT64_MAX - 30U);
        if (values[index] > (uint64_t)INT64_MAX) crossed = true;
    }
    CHECK(crossed);
    puts("Concurrent signed boundary: 256 unique IDs, unsigned order preserved");
#endif
    return 0;
}
