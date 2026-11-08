#include "process_internal.h"

void aether_process_request_close_locked(ProcessRecord* record)
{
    record->close_requested = true;
    record->stdin_requested = true;
    if (!record->snapshot.borrowed) record->snapshot.stop_requested = true;
}

AetherStatus aether_process_stop(AetherProcessService* service, AetherProcessRef process)
{
    AetherStatus status;
    ProcessRecord* record;
    if (service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record != NULL) {
        if (record->snapshot.borrowed) status = AETHER_STATUS_PERMISSION_DENIED;
        else {
            record->snapshot.stop_requested = true;
            record->stdin_requested = true;
        }
    }
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_close(AetherProcessService* service, AetherProcessRef process)
{
    AetherStatus status;
    ProcessRecord* record;
    if (service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record != NULL) aether_process_request_close_locked(record);
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_query(
    AetherProcessService* service, AetherProcessRef process, AetherProcessSnapshot* out_snapshot
)
{
    AetherStatus status;
    ProcessRecord* record;
    if (out_snapshot == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_snapshot = (AetherProcessSnapshot){0};
    if (service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record != NULL) {
        record->snapshot.stdout_buffered = record->output[0].size;
        record->snapshot.stderr_buffered = record->output[1].size;
        *out_snapshot = record->snapshot;
    }
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_release(AetherProcessService* service, AetherProcessRef process)
{
    AetherStatus status;
    ProcessRecord* record;
    if (service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record != NULL) {
        if (!record->snapshot.close_complete || record->output[0].size != 0U ||
            record->output[1].size != 0U || record->write_bytes != 0U) {
            status = AETHER_STATUS_ALREADY_EXISTS;
        } else aether_process_record_free(service, record);
    }
    aether_platform_monitor_unlock(service->monitor);
    return status;
}
