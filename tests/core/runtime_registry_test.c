#include "aether/core/runtime_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, \
                #expression); \
        return EXIT_FAILURE; \
    } \
} while (0)

static AetherStringView text(const char* value)
{
    return (AetherStringView){value, strlen(value)};
}

int main(int argc, char** argv)
{
    AetherRuntimeRegistry* registry = NULL;
    AetherRuntimeRegistry* other = NULL;
    AetherProviderId provider;
    AetherProviderId rejected;
    AetherProviderSnapshot provider_snapshot;
    AetherRuntimeId first;
    AetherRuntimeId second;
    AetherRuntimeId overflow;
    AetherRuntimeRef* left = NULL;
    AetherRuntimeRef* right = NULL;
    AetherRuntimeRef* invalid = NULL;
    AetherRuntimeSnapshot snapshot;
    AetherRuntimeCapabilities capabilities = {
        AETHER_RUNTIME_CAP_STREAM | AETHER_RUNTIME_CAP_CANCEL,
        AETHER_RUNTIME_CAP_STREAM
    };
    char name[] = "controlled-provider";

    CHECK(aether_runtime_registry_create(0U, 2U, &registry) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(registry == NULL);
    CHECK(aether_runtime_registry_create(SIZE_MAX, 2U, &registry) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(registry == NULL);
    CHECK(aether_runtime_registry_create(1U, 2U, &registry) == AETHER_STATUS_OK);
    CHECK(aether_runtime_registry_create(1U, 1U, &other) == AETHER_STATUS_OK);
    CHECK(aether_runtime_provider_register(registry, text(""), text("1"),
              &rejected) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(rejected.id.value == 0U);
    CHECK(aether_runtime_provider_register(registry,
              (AetherStringView){"bad\0name", 8U}, text("1"), &rejected) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_runtime_provider_register(registry,
              (AetherStringView){"\xED\xA0\x80", 3U}, text("1"), &rejected) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_runtime_provider_register(registry, text(name), text("1"),
                                            &provider) == AETHER_STATUS_OK);
    name[0] = 'X';
    CHECK(aether_runtime_provider_query(registry, provider, &provider_snapshot)
          == AETHER_STATUS_OK);
    CHECK(strcmp(provider_snapshot.name, "controlled-provider") == 0);
    CHECK(aether_runtime_provider_register(registry, text("controlled-provider"),
              text("2"), &rejected) == AETHER_STATUS_ALREADY_EXISTS);
    CHECK(rejected.id.value == 0U);
    CHECK(aether_runtime_provider_register(registry, text("different"),
              text("1"), &rejected) == AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(rejected.id.value == 0U);
    CHECK(aether_runtime_provider_query(other, provider, &provider_snapshot) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(provider_snapshot.provider_id.id.value == 0U);
    CHECK(aether_runtime_register(registry, provider, &first) == AETHER_STATUS_OK);
    CHECK(aether_runtime_register(registry, provider, &second) == AETHER_STATUS_OK);
    CHECK(aether_runtime_register(registry, provider, &overflow) ==
          AETHER_STATUS_OUT_OF_MEMORY);
    CHECK(overflow.id.value == 0U);
    CHECK(aether_runtime_query(registry, first, &snapshot) == AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_RUNTIME_REGISTERED);
    CHECK(snapshot.capabilities.known == 0U);
    CHECK(aether_runtime_acquire(registry, first, 1U, &invalid) ==
          AETHER_STATUS_NOT_INITIALIZED);
    CHECK(invalid == NULL);
    CHECK(aether_runtime_update(registry, first, 1U, AETHER_RUNTIME_READY,
                                capabilities) == AETHER_STATUS_OK);
    CHECK(aether_runtime_update(registry, second, 1U, AETHER_RUNTIME_READY,
                                capabilities) == AETHER_STATUS_OK);
    CHECK(aether_runtime_acquire(registry, first, 1U, &left) == AETHER_STATUS_OK);
    CHECK(aether_runtime_acquire(registry, first, 1U, &right) == AETHER_STATUS_OK);
    CHECK(aether_runtime_registry_destroy(&registry) ==
          AETHER_STATUS_ALREADY_EXISTS);
    CHECK(registry != NULL);
    CHECK(aether_runtime_provider_remove(registry, provider) ==
          AETHER_STATUS_ALREADY_EXISTS);

    CHECK(aether_runtime_reconnect(registry, first, 1U) == AETHER_STATUS_OK);
    CHECK(aether_runtime_ref_query(left, &snapshot) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(snapshot.runtime_id.id.value == 0U);
    CHECK(aether_runtime_update(registry, first, 1U, AETHER_RUNTIME_READY,
                                capabilities) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_runtime_query(registry, first, &snapshot) == AETHER_STATUS_OK);
    CHECK(snapshot.generation == 2U && snapshot.revision == 3U);
    CHECK(snapshot.state == AETHER_RUNTIME_REGISTERED);
    CHECK(snapshot.capabilities.known == 0U);
    CHECK(aether_runtime_update(registry, first, 2U, AETHER_RUNTIME_READY,
                                capabilities) == AETHER_STATUS_OK);
    CHECK(aether_runtime_update(registry, first, 2U, AETHER_RUNTIME_READY,
              (AetherRuntimeCapabilities){0U, 1U}) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_runtime_query(registry, first, &snapshot) == AETHER_STATUS_OK);
    CHECK(snapshot.revision == 4U);
    CHECK(snapshot.capabilities.supported == AETHER_RUNTIME_CAP_STREAM);
    snapshot.capabilities.supported = 0U;
    CHECK(aether_runtime_update(registry, first, 2U, AETHER_RUNTIME_READY,
                                capabilities) == AETHER_STATUS_OK);
    CHECK(aether_runtime_query(registry, first, &snapshot) == AETHER_STATUS_OK);
    CHECK(snapshot.revision == 4U);
    CHECK(snapshot.capabilities.supported == AETHER_RUNTIME_CAP_STREAM);
    CHECK(aether_runtime_update(registry, first, 2U, AETHER_RUNTIME_UNAVAILABLE,
                                capabilities) == AETHER_STATUS_OK);
    CHECK(aether_runtime_acquire(registry, first, 2U, &invalid) ==
          AETHER_STATUS_NOT_INITIALIZED);
    CHECK(invalid == NULL);

    CHECK(aether_runtime_begin_unregister(registry, first) == AETHER_STATUS_OK);
    CHECK(aether_runtime_begin_unregister(registry, first) == AETHER_STATUS_OK);
    CHECK(aether_runtime_acquire(registry, first, 2U, &invalid) ==
          AETHER_STATUS_NOT_INITIALIZED);
    CHECK(aether_runtime_update(registry, first, 2U, AETHER_RUNTIME_READY,
                                capabilities) == AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_runtime_reconnect(registry, first, 2U) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_runtime_finish_unregister(registry, first) ==
          AETHER_STATUS_ALREADY_EXISTS);
    aether_runtime_ref_release(&left);
    aether_runtime_ref_release(&left);
    CHECK(aether_runtime_finish_unregister(registry, first) ==
          AETHER_STATUS_ALREADY_EXISTS);
    aether_runtime_ref_release(&right);
    CHECK(aether_runtime_finish_unregister(registry, first) == AETHER_STATUS_OK);
    CHECK(aether_runtime_query(registry, first, &snapshot) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(snapshot.runtime_id.id.value == 0U);
    CHECK(aether_runtime_query(registry, second, &snapshot) == AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_RUNTIME_READY && snapshot.generation == 1U);
    CHECK(aether_runtime_register(registry, provider, &overflow) ==
          AETHER_STATUS_OK);
    CHECK(!aether_id_equal(first.id, overflow.id));
    CHECK(aether_runtime_query(registry, first, &snapshot) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(aether_runtime_query(other, second, &snapshot) ==
          AETHER_STATUS_NOT_FOUND);
    CHECK(aether_runtime_begin_unregister(registry, overflow) ==
          AETHER_STATUS_OK);
    CHECK(aether_runtime_finish_unregister(registry, overflow) ==
          AETHER_STATUS_OK);

    CHECK(aether_runtime_finish_unregister(registry, second) ==
          AETHER_STATUS_INVALID_ARGUMENT);
    CHECK(aether_runtime_acquire(registry, second, 1U, &left) == AETHER_STATUS_OK);
    CHECK(aether_runtime_begin_unregister(registry, second) == AETHER_STATUS_OK);
    CHECK(aether_runtime_ref_query(left, &snapshot) == AETHER_STATUS_OK);
    CHECK(snapshot.state == AETHER_RUNTIME_CLOSING);
    CHECK(aether_runtime_finish_unregister(registry, second) ==
          AETHER_STATUS_ALREADY_EXISTS);
    aether_runtime_ref_release(&left);
    CHECK(aether_runtime_finish_unregister(registry, second) == AETHER_STATUS_OK);
    CHECK(aether_runtime_provider_remove(registry, provider) == AETHER_STATUS_OK);
    CHECK(aether_runtime_registry_destroy(&registry) == AETHER_STATUS_OK);
    CHECK(aether_runtime_registry_destroy(&registry) == AETHER_STATUS_OK);
    CHECK(aether_runtime_registry_destroy(&other) == AETHER_STATUS_OK);
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) {
        puts("Registered provider and two runtimes; shared holds block removal.");
        puts("Reconnect rejects old facts/refs; unregister waits for both holds.");
        puts("Slot reuse preserves identity; other runtime remains Ready.");
        puts("Provider removed, registries destroyed, all owning refs cleared.");
    }
    return EXIT_SUCCESS;
}
