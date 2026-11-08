#include "process_internal.h"
#include <stdlib.h>
#include <string.h>

AetherStatus aether_process_read(
    AetherProcessService* service, AetherProcessRef process, AetherProcessStream stream,
    unsigned char* bytes, size_t capacity, size_t* out_size
)
{
    AetherStatus status;
    ProcessRecord* record;
    ProcessBuffer* buffer;
    size_t count;
    if (out_size == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_size = 0U;
    if (service == NULL || bytes == NULL || capacity == 0U ||
        (stream != AETHER_PROCESS_STDOUT && stream != AETHER_PROCESS_STDERR)) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record == NULL) { aether_platform_monitor_unlock(service->monitor); return status; }
    if (record->snapshot.borrowed) {
        aether_platform_monitor_unlock(service->monitor);
        return AETHER_STATUS_NOT_SUPPORTED;
    }
    buffer = &record->output[stream == AETHER_PROCESS_STDOUT ? 0U : 1U];
    count = capacity < buffer->size ? capacity : buffer->size;
    for (size_t index = 0U; index < count; ++index) {
        bytes[index] = buffer->bytes[(buffer->head + index) % service->limits.stream_bytes];
    }
    buffer->head = (buffer->head + count) % service->limits.stream_bytes;
    buffer->size -= count;
    *out_size = count;
    status = count != 0U || (stream == AETHER_PROCESS_STDOUT
        ? record->snapshot.stdout_eof : record->snapshot.stderr_eof)
        ? AETHER_STATUS_OK : AETHER_STATUS_NOT_FOUND;
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_write(
    AetherProcessService* service, AetherProcessRef process,
    const unsigned char* bytes, size_t size, AetherId* out_write
)
{
    AetherStatus status;
    ProcessRecord* record;
    ProcessWrite* slot = NULL;
    if (out_write == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_write = (AetherId){0};
    if (service == NULL || bytes == NULL || size == 0U || size > 65536U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record == NULL) { aether_platform_monitor_unlock(service->monitor); return status; }
    if (record->snapshot.borrowed) status = AETHER_STATUS_PERMISSION_DENIED;
    else if (service->closing || record->stdin_requested || record->snapshot.exited ||
             record->close_requested) status = AETHER_STATUS_NOT_INITIALIZED;
    else if (size > service->limits.write_bytes - record->write_bytes) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
    } else {
        for (size_t index = 0U; index < service->limits.writes; ++index) {
            if (!record->writes[index].occupied) { slot = &record->writes[index]; break; }
        }
        if (slot == NULL) status = AETHER_STATUS_OUT_OF_MEMORY;
        else {
            unsigned char* copy = malloc(size);
            if (copy == NULL) status = AETHER_STATUS_OUT_OF_MEMORY;
            else {
                AetherId id = aether_id_next();
                if (!aether_id_is_valid(id)) {
                    free(copy);
                    status = AETHER_STATUS_INTERNAL_ERROR;
                } else {
                    (void)memcpy(copy, bytes, size);
                    *slot = (ProcessWrite){true, false, false, id, copy, size,
                                           0U, AETHER_STATUS_OK};
                    record->write_bytes += size;
                    *out_write = id;
                    status = AETHER_STATUS_OK;
                }
            }
        }
    }
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_take_write(
    AetherProcessService* service, AetherProcessRef process,
    AetherProcessWriteResult* out_result
)
{
    AetherStatus status;
    ProcessRecord* record;
    if (out_result == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    *out_result = (AetherProcessWriteResult){0};
    if (service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record == NULL) { aether_platform_monitor_unlock(service->monitor); return status; }
    status = AETHER_STATUS_NOT_FOUND;
    for (size_t index = 0U; index < service->limits.writes; ++index) {
        ProcessWrite* write = &record->writes[index];
        if (!write->occupied || !write->done) continue;
        *out_result = (AetherProcessWriteResult){write->id, write->status, write->delivered};
        record->write_bytes -= write->size;
        free(write->bytes);
        *write = (ProcessWrite){0};
        status = AETHER_STATUS_OK;
        break;
    }
    aether_platform_monitor_unlock(service->monitor);
    return status;
}

AetherStatus aether_process_close_stdin(
    AetherProcessService* service, AetherProcessRef process
)
{
    AetherStatus status;
    ProcessRecord* record;
    if (service == NULL) return AETHER_STATUS_INVALID_ARGUMENT;
    aether_platform_monitor_lock(service->monitor);
    record = aether_process_find(service, process, &status);
    if (record != NULL) {
        if (record->snapshot.borrowed) status = AETHER_STATUS_PERMISSION_DENIED;
        else record->stdin_requested = true;
    }
    aether_platform_monitor_wake_all(service->monitor);
    aether_platform_monitor_unlock(service->monitor);
    return status;
}
