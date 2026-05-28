#ifndef ASTER_MM_VMM_H
#define ASTER_MM_VMM_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PAGE_ACCESSED (1ULL << 5)
#define PAGE_DIRTY    (1ULL << 6)
#define PAGE_HUGE     (1ULL << 7)
#define PAGE_GLOBAL   (1ULL << 8)
#define PAGE_NX       (1ULL << 63)

bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
bool vmm_map_range(uint64_t virt, uint64_t phys, uint64_t length, uint64_t flags);

#endif
