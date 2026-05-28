#ifndef ASTER_USER_PROCESS_H
#define ASTER_USER_PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#include "aster/scheduler/task.h"

typedef struct {
    uint64_t pml4;          /* Root page table */
    uint64_t heap_start;
    uint64_t heap_end;
    uint64_t stack_start;
    uint64_t stack_end;
} AddressSpace;

typedef struct {
    uint32_t pid;
    char name[TASK_NAME_MAX];
    AddressSpace *addr_space;
    Task *task;
    uint64_t entry;
    uint64_t user_stack_top;
} Process;

#define USER_STACK_SIZE (64 * 4096)  /* 256 KB */
#define USER_HEAP_SIZE  (256 * 4096) /* 1 MB */

/* Process creation and management */
Process *process_create(const char *name);
bool process_load_elf(Process *proc, const void *elf_data, uint64_t elf_size);
bool process_exec(Process *proc);
void process_destroy(Process *proc);

/* Memory management for process */
bool process_alloc_stack(Process *proc);
bool process_alloc_heap(Process *proc);

#endif
