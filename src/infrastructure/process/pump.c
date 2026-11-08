#include "process_internal.h"
#include <string.h>

static void pump_read(AetherProcessService* service, ProcessRecord* record,
                       size_t stream, unsigned char* scratch)
{
    ProcessBuffer* buffer = &record->output[stream];
    size_t available = service->limits.stream_bytes - buffer->size;
    size_t count = 0U;
    AetherStatus status;
    if (available == 0U) return;
    if (available > 65536U) available = 65536U;
    status = aether_platform_process_read(record->native,
        stream == 0U ? AETHER_PROCESS_STDOUT : AETHER_PROCESS_STDERR,
        scratch, available, &count);
    if (status != AETHER_STATUS_OK && status != AETHER_STATUS_NOT_FOUND) {
        record->snapshot.io_status = status;
        record->snapshot.stop_requested = true;
        record->close_requested = true;
        return;
    }
    for (size_t index = 0U; index < count; ++index) {
        buffer->bytes[(buffer->head + buffer->size + index) % service->limits.stream_bytes] =
            scratch[index];
    }
    buffer->size += count;
}

static ProcessWrite* next_write(AetherProcessService* service, ProcessRecord* record)
{
    ProcessWrite* selected = NULL;
    for (size_t index = 0U; index < service->limits.writes; ++index) {
        ProcessWrite* write = &record->writes[index];
        if (!write->occupied || write->done) continue;
        if (selected == NULL || write->id.value < selected->id.value) selected = write;
    }
    return selected;
}

static void pump_write(AetherProcessService* service, ProcessRecord* record)
{
    ProcessWrite* write = next_write(service, record);
    if (write != NULL) {
        AetherStatus status;
        if (!write->started) {
            if (record->snapshot.exited || record->terminated) {
                write->status = AETHER_STATUS_CANCELLED;
                write->done = true;
                return;
            }
            status = aether_platform_process_write_begin(record->native,
                write->bytes + write->delivered, write->size - write->delivered);
            if (status != AETHER_STATUS_OK) {
                write->status = status;
                write->done = true;
                record->snapshot.io_status = status;
                return;
            }
            write->started = true;
        }
        {
            size_t written = 0U;
            status = aether_platform_process_write_poll(record->native, &written);
            if (status == AETHER_STATUS_NOT_FOUND) return;
            write->delivered += written;
            if (status != AETHER_STATUS_OK) {
                write->status = status;
                write->done = true;
                record->snapshot.io_status = status;
            } else if (write->delivered == write->size) write->done = true;
            else if (written == 0U) {
                write->status = AETHER_STATUS_IO_ERROR;
                write->done = true;
                record->snapshot.io_status = AETHER_STATUS_IO_ERROR;
            } else write->started = false;
        }
    } else if (record->stdin_requested) {
        AetherStatus status = aether_platform_process_close_stdin(record->native);
        if (status != AETHER_STATUS_OK && status != AETHER_STATUS_ALREADY_EXISTS) {
            record->snapshot.io_status = status;
        }
    }
}

static void pump_record(AetherProcessService* service, ProcessRecord* record,
                         unsigned char* scratch)
{
    AetherPlatformProcessSnapshot native;
    AetherPlatformClock clock;
    AetherStatus status;
    if (record->snapshot.borrowed && record->close_requested) {
        record->snapshot.close_complete = true;
        return;
    }
    if (record->snapshot.close_complete) return;
    status = aether_platform_process_query(record->native, &native);
    if (status != AETHER_STATUS_OK) { record->snapshot.io_status = status; return; }
    record->snapshot.exited = native.exited;
    record->snapshot.exit_code = native.exit_code;
    if (record->snapshot.borrowed) return;
    pump_read(service, record, 0U, scratch);
    pump_read(service, record, 1U, scratch);
    if (record->snapshot.stop_requested && !native.owned_tree_exited) {
        status = aether_platform_clock_now(&clock);
        if (status != AETHER_STATUS_OK) {
            record->snapshot.io_status = status;
        } else {
            if (record->stop_deadline == 0U) {
                record->stop_deadline = clock.monotonic_ns > UINT64_MAX - UINT64_C(1000000000)
                    ? UINT64_MAX : clock.monotonic_ns + UINT64_C(1000000000);
            }
            if (!record->terminated && clock.monotonic_ns >= record->stop_deadline) {
                status = aether_platform_process_terminate(record->native);
                if (status == AETHER_STATUS_OK) record->terminated = true;
                else record->snapshot.io_status = status;
            }
        }
    }
    pump_write(service, record);
    status = aether_platform_process_query(record->native, &native);
    if (status != AETHER_STATUS_OK) { record->snapshot.io_status = status; return; }
    record->snapshot.exited = native.exited;
    record->snapshot.exit_code = native.exit_code;
    record->snapshot.stdout_eof = native.stdout_eof;
    record->snapshot.stderr_eof = native.stderr_eof;
    record->snapshot.close_complete = native.exited && native.owned_tree_exited &&
        native.stdout_eof && native.stderr_eof && !native.write_pending &&
        next_write(service, record) == NULL;
}

void aether_process_pump(void* context)
{
    AetherProcessService* service = context;
    unsigned char* scratch = service->scratch;
    for (;;) {
        bool complete = true;
        bool progressed = false;
        aether_platform_monitor_lock(service->monitor);
        for (size_t index = 0U; index < service->limits.processes; ++index) {
            ProcessRecord* record = &service->records[index];
            if (!record->occupied) continue;
            if (record->creating) { complete = false; continue; }
            size_t before_out = record->output[0].size;
            size_t before_err = record->output[1].size;
            pump_record(service, record, scratch);
            if (record->output[0].size > before_out || record->output[1].size > before_err) {
                progressed = true;
            }
            if (!record->snapshot.close_complete) complete = false;
        }
        if (service->closing && complete) {
            service->exited = true;
            aether_platform_monitor_unlock(service->monitor);
            return;
        }
        aether_platform_monitor_unlock(service->monitor);
        aether_platform_thread_pause(progressed ? 0U : 1U);
    }
}
