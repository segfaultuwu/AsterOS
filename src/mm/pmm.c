#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <limine.h>

#include "aster/mm/pmm.h"
#include "aster/arch/x86_64/boot/limine.h"
#include "aster/debug/logging.h"

static uint64_t hhdm_offset = 0;

static size_t memmap_index = 0;
static uint64_t current_phys = 0;
static uint64_t current_end = 0;

static uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

uint64_t pmm_get_hhdm_offset(void) {
    return hhdm_offset;
}

static void *phys_to_virt(uint64_t phys) {
    return (void *)(hhdm_offset + phys);
}

bool pmm_init(void) {
    if (hhdm_request.response == NULL) {
        log_error("PMM: HHDM response is NULL");
        return false;
    }

    if (memmap_request.response == NULL) {
        log_error("PMM: memmap response is NULL");
        return false;
    }

    hhdm_offset = hhdm_request.response->offset;

    memmap_index = 0;
    current_phys = 0;
    current_end = 0;

    log_ok("PMM initialized");
    log_hex64("hhdm", hhdm_offset);
    log_u64("memmap entries", memmap_request.response->entry_count);

    return true;
}

uint64_t pmm_alloc_page(void) {
    struct limine_memmap_response *memmap = memmap_request.response;

    while (true) {
        if (current_phys != 0 && current_phys + PAGE_SIZE <= current_end) {
            uint64_t page = current_phys;
            current_phys += PAGE_SIZE;
            pmm_zero_page(page);
            return page;
        }

        if (memmap_index >= memmap->entry_count) {
            log_error("PMM: out of memory");
            return 0;
        }

        struct limine_memmap_entry *entry = memmap->entries[memmap_index++];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t start = align_up(entry->base, PAGE_SIZE);
        uint64_t end = entry->base + entry->length;

        /*
         * Avoid very low memory for now.
         */
        if (start < 0x100000) {
            start = 0x100000;
        }

        if (start + PAGE_SIZE > end) {
            continue;
        }

        current_phys = start;
        current_end = end;
    }
}

void pmm_zero_page(uint64_t phys) {
    uint8_t *ptr = (uint8_t *)phys_to_virt(phys);

    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        ptr[i] = 0;
    }
}
