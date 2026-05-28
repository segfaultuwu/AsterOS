#include "aster/user/process.h"
#include "aster/user/elf.h"
#include "aster/mm/pmm.h"
#include "aster/mm/vmm.h"
#include "aster/scheduler/scheduler.h"
#include "aster/debug/logging.h"
#include "aster/arch/x86_64/usermode.h"

#include <string.h>

#define MAX_PROCESSES 64
static AddressSpace g_address_spaces[MAX_PROCESSES];
static Process g_processes[MAX_PROCESSES];
static uint32_t g_next_pid = 1;

/* Simple strncpy for kernel use */
static void _strncpy(char *dest, const char *src, size_t count) {
    size_t i = 0;
    while (i < count - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* Process entry point wrapper */
static void _process_entry(void *context) {
    Process *proc = (Process *)context;
    if (!proc || !proc->addr_space) {
        return;
    }

    /* Switch to user address space and jump to entry point */
    /* Note: This is a simplified version. In a real OS, this would:
     * - Load the process's page tables
     * - Setup the stack
     * - Jump to user code
     */
    log_infof("Process %s (PID %u) starting at entry point", proc->name, proc->pid);
}

Process *process_create(const char *name) {
    /* Find a free process slot */
    Process *proc = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].pid == 0) {
            proc = &g_processes[i];
            break;
        }
    }

    if (!proc) {
        log_warn("process_create: Max processes reached");
        return NULL;
    }

    /* Initialize process */
    proc->pid = g_next_pid++;
    _strncpy(proc->name, name, TASK_NAME_MAX);

    /* Find a free address space */
    AddressSpace *addr_space = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (g_address_spaces[i].pml4 == 0) {
            addr_space = &g_address_spaces[i];
            break;
        }
    }

    if (!addr_space) {
        log_warn("process_create: No address space available");
        memset(proc, 0, sizeof(Process));
        return NULL;
    }

    proc->addr_space = addr_space;
    memset(addr_space, 0, sizeof(AddressSpace));

    /* Allocate page table (this would normally use a real PML4 copy) */
    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        log_warn("process_create: Failed to allocate PML4");
        memset(proc, 0, sizeof(Process));
        return NULL;
    }

    pmm_zero_page(pml4_phys);
    proc->addr_space->pml4 = pml4_phys;

    log_infof("Process created: %s (PID %u)", name, proc->pid);
    return proc;
}

bool process_alloc_stack(Process *proc) {
    if (!proc || !proc->addr_space) {
        return false;
    }

    uint64_t stack_vaddr = 0x00007ffffffde000ULL;
    uint64_t stack_pages = (USER_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < stack_pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            log_warn("process_alloc_stack: Failed to allocate page");
            return false;
        }

        pmm_zero_page(phys);

        if (!vmm_map_page(stack_vaddr + i * PAGE_SIZE, phys,
                          PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NX)) {
            log_warn("process_alloc_stack: Failed to map page");
            return false;
        }
    }

    proc->addr_space->stack_start = stack_vaddr;
    proc->addr_space->stack_end = stack_vaddr + USER_STACK_SIZE;

    log_infof("Stack allocated: 0x%lx - 0x%lx", proc->addr_space->stack_start,
               proc->addr_space->stack_end);
    return true;
}

bool process_alloc_heap(Process *proc) {
    if (!proc || !proc->addr_space) {
        return false;
    }

    uint64_t heap_vaddr = 0x0000400000000000ULL;
    uint64_t heap_pages = (USER_HEAP_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < heap_pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            log_warn("process_alloc_heap: Failed to allocate page");
            return false;
        }

        pmm_zero_page(phys);

        if (!vmm_map_page(heap_vaddr + i * PAGE_SIZE, phys,
                          PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NX)) {
            log_warn("process_alloc_heap: Failed to map page");
            return false;
        }
    }

    proc->addr_space->heap_start = heap_vaddr;
    proc->addr_space->heap_end = heap_vaddr + USER_HEAP_SIZE;

    log_infof("Heap allocated: 0x%lx - 0x%lx", proc->addr_space->heap_start,
               proc->addr_space->heap_end);
    return true;
}

bool process_load_elf(Process *proc, const void *elf_data, uint64_t elf_size) {
    if (!proc || !proc->addr_space || !elf_data) {
        return false;
    }

    if (!elf_validate(elf_data, elf_size)) {
        log_warn("process_load_elf: Invalid ELF file");
        return false;
    }

    elf_load_result_t elf_result;
    if (!elf_load_user(elf_data, elf_size, proc->addr_space->pml4, &elf_result)) {
        log_warn("process_load_elf: Failed to load ELF");
        return false;
    }

    log_infof("ELF loaded for process %s: entry=0x%lx", proc->name, elf_result.entry);
    return true;
}

bool process_exec(Process *proc) {
    if (!proc) {
        return false;
    }

    /* Create scheduler task for this process */
    proc->task = scheduler_create_task(proc->name, _process_entry, proc);
    if (!proc->task) {
        log_warn("process_exec: Failed to create scheduler task");
        return false;
    }

    log_infof("Process %s (PID %u) scheduled for execution", proc->name, proc->pid);
    return true;
}

void process_destroy(Process *proc) {
    if (!proc || proc->pid == 0) {
        return;
    }

    log_infof("Destroying process %s (PID %u)", proc->name, proc->pid);

    if (proc->addr_space) {
        memset(proc->addr_space, 0, sizeof(AddressSpace));
        proc->addr_space = NULL;
    }

    memset(proc, 0, sizeof(Process));
}
