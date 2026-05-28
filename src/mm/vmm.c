#include <stdint.h>
#include <stdbool.h>

#include "aster/mm/pmm.h"
#include "aster/mm/vmm.h"
#include "aster/debug/logging.h"

#define PAGE_MASK 0x000FFFFFFFFFF000ULL

typedef uint64_t page_table_t[512];

static inline uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static inline void invlpg(uint64_t virt) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(pmm_get_hhdm_offset() + phys);
}

static inline uint16_t pml4_index(uint64_t virt) {
    return (virt >> 39) & 0x1FF;
}

static inline uint16_t pdpt_index(uint64_t virt) {
    return (virt >> 30) & 0x1FF;
}

static inline uint16_t pd_index(uint64_t virt) {
    return (virt >> 21) & 0x1FF;
}

static inline uint16_t pt_index(uint64_t virt) {
    return (virt >> 12) & 0x1FF;
}

static page_table_t *get_next_table(page_table_t *table, uint16_t index, bool user) {
    uint64_t entry = (*table)[index];

    if ((entry & PAGE_PRESENT) == 0) {
        uint64_t new_table_phys = pmm_alloc_page();

        if (new_table_phys == 0) {
            return NULL;
        }

        uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE;

        if (user) {
            flags |= PAGE_USER;
        }

        (*table)[index] = new_table_phys | flags;

        return (page_table_t *)phys_to_virt(new_table_phys);
    }

    /*
     * If this path will contain user pages, every upper-level entry
     * also needs PAGE_USER.
     */
    if (user) {
        (*table)[index] |= PAGE_USER;
    }

    if (entry & PAGE_HUGE) {
        log_error("VMM: huge page in mapping path");
        return NULL;
    }

    uint64_t next_phys = entry & PAGE_MASK;
    return (page_table_t *)phys_to_virt(next_phys);
}

bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if ((virt & 0xFFF) != 0 || (phys & 0xFFF) != 0) {
        log_error("VMM: unaligned map_page");
        log_hex64("virt", virt);
        log_hex64("phys", phys);
        return false;
    }

    uint64_t cr3 = read_cr3();
    uint64_t pml4_phys = cr3 & PAGE_MASK;

    page_table_t *pml4 = (page_table_t *)phys_to_virt(pml4_phys);

    bool user = (flags & PAGE_USER) != 0;

    page_table_t *pdpt = get_next_table(pml4, pml4_index(virt), user);
    if (pdpt == NULL) {
        return false;
    }

    page_table_t *pd = get_next_table(pdpt, pdpt_index(virt), user);
    if (pd == NULL) {
        return false;
    }

    page_table_t *pt = get_next_table(pd, pd_index(virt), user);
    if (pt == NULL) {
        return false;
    }

    uint16_t pti = pt_index(virt);

    (*pt)[pti] = (phys & PAGE_MASK) | flags;

    invlpg(virt);

    return true;
}

bool vmm_map_range(uint64_t virt, uint64_t phys, uint64_t length, uint64_t flags) {
    uint64_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < pages; i++) {
        if (!vmm_map_page(
                virt + i * PAGE_SIZE,
                phys + i * PAGE_SIZE,
                flags
            )) {
            return false;
        }
    }

    return true;
}
