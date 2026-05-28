#include <stdint.h>
#include <stddef.h>

#include "aster/arch/x86_64/gdt.h"
#include "aster/debug/logging.h"

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_entry_t;

typedef struct {
    gdt_entry_t null;
    gdt_entry_t kernel_code;
    gdt_entry_t kernel_data;
    gdt_entry_t user_data;
    gdt_entry_t user_code;
    tss_entry_t tss;
} __attribute__((packed)) gdt_t;

extern void gdt_load(gdtr_t *gdtr);
extern void tss_load(uint16_t selector);

static gdt_t gdt;
static gdtr_t gdtr;
static tss_t tss;

static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

static gdt_entry_t gdt_make_entry(uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt_entry_t entry;

    entry.limit_low = (uint16_t)(limit & 0xFFFF);
    entry.base_low = (uint16_t)(base & 0xFFFF);
    entry.base_mid = (uint8_t)((base >> 16) & 0xFF);
    entry.access = access;
    entry.granularity = (uint8_t)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    entry.base_high = (uint8_t)((base >> 24) & 0xFF);

    return entry;
}

static void gdt_set_tss(tss_entry_t *entry, uint64_t base, uint32_t limit) {
    entry->limit_low = (uint16_t)(limit & 0xFFFF);
    entry->base_low = (uint16_t)(base & 0xFFFF);
    entry->base_mid = (uint8_t)((base >> 16) & 0xFF);
    entry->access = 0x89;
    entry->granularity = (uint8_t)((limit >> 16) & 0x0F);
    entry->base_high = (uint8_t)((base >> 24) & 0xFF);
    entry->base_upper = (uint32_t)(base >> 32);
    entry->reserved = 0;
}

static void tss_init(void) {
    uint8_t *tss_bytes = (uint8_t *)&tss;

    for (size_t i = 0; i < sizeof(tss); i++) {
        tss_bytes[i] = 0;
    }

    tss.rsp0 = (uint64_t)&kernel_stack[sizeof(kernel_stack)];
    tss.iomap_base = sizeof(tss_t);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

void gdt_init(void) {
    gdt.null = gdt_make_entry(0, 0, 0, 0);

    /*
     * Long mode code segment:
     * access 0x9A = present, ring0, code, readable
     * flags  0x20 = long mode
     */
    gdt.kernel_code = gdt_make_entry(0, 0, 0x9A, 0x20);

    /*
     * Data segments are mostly ignored in long mode,
     * but valid descriptors are still required.
     */
    gdt.kernel_data = gdt_make_entry(0, 0, 0x92, 0x00);

    /*
     * User data:
     * access 0xF2 = present, ring3, data, writable
     */
    gdt.user_data = gdt_make_entry(0, 0, 0xF2, 0x00);

    /*
     * User code:
     * access 0xFA = present, ring3, code, readable
     * flags  0x20 = long mode
     */
    gdt.user_code = gdt_make_entry(0, 0, 0xFA, 0x20);

    tss_init();
    gdt_set_tss(&gdt.tss, (uint64_t)&tss, sizeof(tss_t) - 1);

    gdtr.base = (uint64_t)&gdt;
    gdtr.limit = sizeof(gdt) - 1;

    gdt_load(&gdtr);
    tss_load(GDT_TSS);

    log_ok("GDT loaded");
    log_ok("TSS loaded");
}
