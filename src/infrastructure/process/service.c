#include "process_internal.h"
#include <stdlib.h>

AetherProcessLimits aether_process_default_limits(void)
{
    return (AetherProcessLimits){4U, 131072U, 32U, 1048576U};
}

void aether_process_record_free(AetherProcessService* service, ProcessRecord* record)
{
    ProcessRecord owned;
    if (!record->occupied) return;
    owned = *record;
    *record = (ProcessRecord){0};
    /* Move the ownership roots before freeing any possibly nested resource. */
    aether_platform_process_destroy(owned.native);
    if (owned.writes != NULL) {
        for (size_t index = 0U; index < service->limits.writes; ++index) {
            ProcessWrite* write = &owned.writes[index];
            if (write->occupied) free(write->bytes);
        }
    }
    free(owned.writes);
    free(owned.output[0].bytes);
    free(owned.output[1].bytes);
}

static void free_service(AetherProcessService* service)
{
    if (service->records != NULL) {
        for (size_t index = 0U; index < service->limits.processes; ++index) {
            aether_process_record_free(service, &service->records[index]);
        }
    }
    free(service->scratch);
    free(service->records);
    aether_platform_monitor_destroy(service->monitor);
    free(service);
}

AetherStatus aether_process_service_create(
    AetherProcessLimits limits, AetherProcessService** out_service
)
{
    AetherProcessService* service;
    AetherStatus status;
    if (out_service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_service = NULL;
    if (limits.processes == 0U || limits.processes > 4U || limits.stream_bytes == 0U ||
        limits.stream_bytes > 131072U || limits.writes == 0U || limits.writes > 32U ||
        limits.write_bytes == 0U || limits.write_bytes > 1048576U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    service = calloc(1U, sizeof(*service));
    if (service == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    service->limits = limits;
    service->records = calloc(limits.processes, sizeof(*service->records));
    if (service->records == NULL) { free(service); return AETHER_STATUS_OUT_OF_MEMORY; }
    service->scratch = malloc(65536U);
    if (service->scratch == NULL) { free_service(service); return AETHER_STATUS_OUT_OF_MEMORY; }
    status = aether_platform_monitor_create(&service->monitor);
    if (status == AETHER_STATUS_OK) {
        status = aether_platform_thread_create(aether_process_pump, service, &service->pump);
    }
    if (status != AETHER_STATUS_OK) { free_service(service); return status; }
    *out_service = service;
    return AETHER_STATUS_OK;
}

ProcessRecord* aether_process_find(AetherProcessService* service,
                                   AetherProcessRef ref, AetherStatus* status)
{
    if (!aether_id_is_valid(ref.id) || ref.generation == 0U) {
        *status = AETHER_STATUS_INVALID_ARGUMENT;
        return NULL;
    }
    for (size_t index = 0U; index < service->limits.processes; ++index) {
        ProcessRecord* record = &service->records[index];
        if (record->occupied && aether_id_equal(record->snapshot.process.id, ref.id)) {
            *status = record->snapshot.process.generation == ref.generation
                ? AETHER_STATUS_OK : AETHER_STATUS_INVALID_ARGUMENT;
            return *status == AETHER_STATUS_OK ? record : NULL;
        }
    }
    *status = AETHER_STATUS_NOT_FOUND;
    return NULL;
}

static AetherStatus prepare_record(AetherProcessService* service, ProcessRecord** out)
{
    *out = NULL;
    if (service->closing) return AETHER_STATUS_NOT_INITIALIZED;
    for (size_t index = 0U; index < service->limits.processes; ++index) {
        ProcessRecord* record = &service->records[index];
        if (record->occupied) continue;
        *record = (ProcessRecord){0};
        record->occupied = true;
        record->creating = true;
        record->output[0].bytes = malloc(service->limits.stream_bytes);
        record->output[1].bytes = malloc(service->limits.stream_bytes);
        record->writes = malloc(service->limits.writes * sizeof(*record->writes));
        if (record->output[0].bytes == NULL || record->output[1].bytes == NULL ||
            record->writes == NULL) {
            /* No native process has been created; rollback acquired buffers. */
            free(record->output[0].bytes);
            free(record->output[1].bytes);
            free(record->writes);
            *record = (ProcessRecord){0};
            return AETHER_STATUS_OUT_OF_MEMORY;
        }
        for (size_t slot = 0U; slot < service->limits.writes; ++slot) {
            record->writes[slot] = (ProcessWrite){0};
        }
        record->snapshot.process = (AetherProcessRef){aether_id_next(), 1U};
        if (!aether_id_is_valid(record->snapshot.process.id)) {
            aether_process_record_free(service, record);
            return AETHER_STATUS_INTERNAL_ERROR;
        }
        *out = record;
        return AETHER_STATUS_OK;
    }
    return AETHER_STATUS_OUT_OF_MEMORY;
}

AetherStatus aether_process_spawn(
    AetherProcessService* service, AetherPlatformProcessInput input,
    AetherTraceContext trace, AetherProcessRef* out_process
)
{
    ProcessRecord* record;
    AetherStatus status;
    if (out_process == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_process = (AetherProcessRef){0};
    if (service == NULL || !trace.valid) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    status = prepare_record(service, &record);
    if (status == AETHER_STATUS_OK) {
        AetherPlatformProcess* native = NULL;
        aether_platform_monitor_unlock(service->monitor);
        status = aether_platform_process_spawn(input, &native);
        aether_platform_monitor_lock(service->monitor);
        record->native = native;
        record->creating = false;
        if (status == AETHER_STATUS_OK) {
            record->snapshot.trace = trace;
            *out_process = record->snapshot.process;
        } else aether_process_record_free(service, record);
    }
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_attach(
    AetherProcessService* service, uint32_t pid, AetherTraceContext trace,
    AetherProcessRef* out_process
)
{
    ProcessRecord* record;
    AetherStatus status;
    if (out_process == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_process = (AetherProcessRef){0};
    if (service == NULL || !trace.valid || pid == 0U) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    status = prepare_record(service, &record);
    if (status == AETHER_STATUS_OK) {
        AetherPlatformProcess* native = NULL;
        aether_platform_monitor_unlock(service->monitor);
        status = aether_platform_process_attach(pid, &native);
        aether_platform_monitor_lock(service->monitor);
        record->native = native;
        record->creating = false;
        if (status == AETHER_STATUS_OK) {
            record->snapshot.trace = trace;
            record->snapshot.borrowed = true;
            record->snapshot.stdout_eof = true;
            record->snapshot.stderr_eof = true;
            *out_process = record->snapshot.process;
        } else aether_process_record_free(service, record);
    }
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_service_close(AetherProcessService* service)
{
    if (service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    service->closing = true;
    for (size_t index = 0U; index < service->limits.processes; ++index) {
        if (service->records[index].occupied) {
            aether_process_request_close_locked(&service->records[index]);
        }
    }
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return AETHER_STATUS_OK;
}

bool aether_process_service_is_closed(AetherProcessService* service)
{
    bool closed;
    if (service == NULL) return false;
    aether_platform_monitor_lock(service->monitor);
    closed = service->closing && service->exited;
    aether_platform_monitor_unlock(service->monitor);
    return closed;
}

AetherStatus aether_process_service_destroy(AetherProcessService** service)
{
    AetherStatus status;
    if (service == NULL || *service == NULL) return AETHER_STATUS_OK;
    if (!aether_process_service_is_closed(*service)) return AETHER_STATUS_ALREADY_EXISTS;
    status = aether_platform_thread_join(&(*service)->pump);
    if (status != AETHER_STATUS_OK) return status;
    free_service(*service);
    *service = NULL;
    return AETHER_STATUS_OK;
}
