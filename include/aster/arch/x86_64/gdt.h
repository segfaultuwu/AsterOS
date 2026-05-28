#ifndef ASTER_ARCH_X86_64_GDT_H
#define ASTER_ARCH_X86_64_GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28

#define USER_DATA_SELECTOR (GDT_USER_DATA | 3)
#define USER_CODE_SELECTOR (GDT_USER_CODE | 3)

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);

#endif
