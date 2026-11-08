#ifndef AETHER_PROCESS_INTERNAL_H
#define AETHER_PROCESS_INTERNAL_H
#include "aether/infrastructure/process.h"
#include "aether/platform/thread.h"
#include "aether/platform/platform.h"

typedef struct ProcessBuffer {
    unsigned char* bytes;
    size_t head;
    size_t size;
} ProcessBuffer;
typedef struct ProcessWrite {
    bool occupied;
    bool started;
    bool done;
    AetherId id;
    unsigned char* bytes;
    size_t size;
    size_t delivered;
    AetherStatus status;
} ProcessWrite;
typedef struct ProcessRecord {
    bool occupied;
    bool creating;
    AetherPlatformProcess* native;
    AetherProcessSnapshot snapshot;
    ProcessBuffer output[2];
    ProcessWrite* writes;
    void* storage;
    size_t write_bytes;
    bool close_requested;
    bool stdin_requested;
    bool terminated;
    uint64_t stop_deadline;
} ProcessRecord;
struct AetherProcessService {
    AetherProcessLimits limits;
    AetherPlatformMonitor* monitor;
    AetherPlatformThread* pump;
    ProcessRecord* records;
    unsigned char* scratch;
    bool closing;
    bool exited;
};
ProcessRecord* aether_process_find(AetherProcessService* service,
                                   AetherProcessRef ref, AetherStatus* status);
void aether_process_pump(void* context);
void aether_process_request_close_locked(ProcessRecord* record);
void aether_process_record_free(AetherProcessService* service, ProcessRecord* record);
void aether_process_write_dispose(ProcessWrite* write);
#endif
