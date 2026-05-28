#ifndef ASTER_MM_PMM_H
#define ASTER_MM_PMM_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096ULL

bool pmm_init(void);
uint64_t pmm_alloc_page(void);
void pmm_zero_page(uint64_t phys);

uint64_t pmm_get_hhdm_offset(void);

#endif
