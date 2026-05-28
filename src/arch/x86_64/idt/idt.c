#include <stdint.h>
#include <stddef.h>

#include "aster/arch/x86_64/idt.h"
#include "aster/drivers/ports.h"
#include "aster/debug/logging.h"
#include "aster/kernel/syscall/syscall.h"

#define IDT_ENTRIES 256

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

/*
 * Important:
 * Limine currently runs your kernel with CS=0x28 and SS=0x30.
 * Your QEMU log showed:
 *   CS = 0028
 *   SS = 0030
 *
 * Therefore IDT gates must use selector 0x28, not 0x08.
 */
#define KERNEL_CS 0x08
#define IDT_FLAGS 0x8E
#define IDT_FLAGS_USER 0xEE  // Interrupt gate with DPL=3 for user-space access

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t idtr;

extern void idt_load(idtr_t *idtr);

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr48(void);
extern void syscall_handler(registers_t *regs);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

static void (*isr_handlers[256])(registers_t *regs) = { NULL };
static void (*irq_handlers[16])(registers_t *regs) = { NULL };

static void pic_remap(void) {
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

static void pic_mask_all(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

static void pic_unmask_irq(uint8_t irq) {
    uint16_t port;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    uint8_t mask = inb(port);
    mask &= (uint8_t)~(1u << irq);
    outb(port, mask);
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    outb(PIC1_COMMAND, PIC_EOI);
}

static void idt_clear(void) {
    for (size_t i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }
}

static void idt_set_gate(uint8_t num, uint64_t base, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = (uint16_t)(base & 0xFFFF);
    idt[num].selector = selector;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].zero = 0;
}

static void idt_set_isr_gates(void) {
    idt_set_gate(0,  (uint64_t)isr0,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(1,  (uint64_t)isr1,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(2,  (uint64_t)isr2,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(3,  (uint64_t)isr3,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(4,  (uint64_t)isr4,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(5,  (uint64_t)isr5,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(6,  (uint64_t)isr6,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(7,  (uint64_t)isr7,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(8,  (uint64_t)isr8,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(9,  (uint64_t)isr9,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(10, (uint64_t)isr10, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(11, (uint64_t)isr11, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(12, (uint64_t)isr12, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(13, (uint64_t)isr13, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(14, (uint64_t)isr14, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(15, (uint64_t)isr15, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(16, (uint64_t)isr16, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(17, (uint64_t)isr17, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(18, (uint64_t)isr18, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(19, (uint64_t)isr19, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(20, (uint64_t)isr20, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(21, (uint64_t)isr21, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(22, (uint64_t)isr22, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(23, (uint64_t)isr23, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(24, (uint64_t)isr24, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(25, (uint64_t)isr25, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(26, (uint64_t)isr26, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(27, (uint64_t)isr27, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(28, (uint64_t)isr28, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(29, (uint64_t)isr29, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(30, (uint64_t)isr30, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(31, (uint64_t)isr31, KERNEL_CS, IDT_FLAGS);
}

static void idt_set_irq_gates(void) {
    idt_set_gate(32, (uint64_t)irq0,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(33, (uint64_t)irq1,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(34, (uint64_t)irq2,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(35, (uint64_t)irq3,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(36, (uint64_t)irq4,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(37, (uint64_t)irq5,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(38, (uint64_t)irq6,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(39, (uint64_t)irq7,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(40, (uint64_t)irq8,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(41, (uint64_t)irq9,  KERNEL_CS, IDT_FLAGS);
    idt_set_gate(42, (uint64_t)irq10, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(43, (uint64_t)irq11, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(44, (uint64_t)irq12, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(45, (uint64_t)irq13, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(46, (uint64_t)irq14, KERNEL_CS, IDT_FLAGS);
    idt_set_gate(47, (uint64_t)irq15, KERNEL_CS, IDT_FLAGS);
}

void idt_init(void) {
    idt_clear();

    idtr.base = (uint64_t)&idt;
    idtr.limit = sizeof(idt) - 1;

    pic_remap();
    pic_mask_all();

    /*
     * IRQ1 = keyboard.
     * IRQ2 = cascade line for the slave PIC.
     */
    pic_unmask_irq(1);
    pic_unmask_irq(2);

    idt_set_isr_gates();
    idt_set_irq_gates();

    // Set up syscall interrupt gate (0x30) with DPL=3 for user-space access
    idt_set_gate(0x30, (uint64_t)isr48, KERNEL_CS, IDT_FLAGS_USER);

    // Register the syscall handler
    idt_register_isr_handler(0x30, syscall_handler);
    idt_load(&idtr);

    log_ok("IDT loaded");
    log_ok("PIC remapped");
    log_ok("IRQ1 keyboard unmasked");
}

void idt_register_isr_handler(uint8_t n, void (*handler)(registers_t *regs)) {
    isr_handlers[n] = handler;
}

void idt_register_irq_handler(uint8_t n, void (*handler)(registers_t *regs)) {
    if (n < 16) {
        irq_handlers[n] = handler;
    }
}

static inline uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static const char *exception_names[] = {
    "#DE Divide Error",
    "#DB Debug",
    "#NMI Non-Maskable Interrupt",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved",
    "#MF x87 FPU Error",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XF SIMD Exception",
    "#VE Virtualization Exception",
    "#CP Control Protection",
};

static void decode_gp(uint64_t err_code) {
    if (err_code == 0) {
        log_info("#GP: not selector-related (null selector or internal)");
        return;
    }

    uint16_t index    = (err_code >> 3) & 0x1FFF;
    uint8_t  tbl      = (err_code >> 1) & 0x3;   /* 0=GDT, 1=IDT, 2=LDT, 3=IDT */
    uint8_t  ext      = err_code & 1;

    const char *table_name;
    switch (tbl) {
        case 0:  table_name = "GDT"; break;
        case 1:  table_name = "IDT"; break;
        case 2:  table_name = "LDT"; break;
        case 3:  table_name = "IDT"; break;
        default: table_name = "???"; break;
    }

    log_info("#GP: selector fault");
    log_u64("  selector index", index);
    log_info(ext ? "  source: external" : "  source: internal");

    /* Print the full selector value as it would appear in a segment register */
    uint16_t selector = (uint16_t)((index << 3) | (tbl == 2 ? 4 : 0) | (err_code & 3));
    log_hex64("  selector", selector);
    log_info(table_name[0] == 'G' ? "  table: GDT" :
             table_name[0] == 'L' ? "  table: LDT" : "  table: IDT");
}

void isr_handler(registers_t *regs) {
    if (regs == NULL) {
        log_error("ISR called with NULL regs");
        return;
    }

    /* Registered handler takes priority */
    if (regs->int_no < 256 && isr_handlers[regs->int_no] != NULL) {
        isr_handlers[regs->int_no](regs);
        return;
    }

    const char *name = (regs->int_no < 22)
                       ? exception_names[regs->int_no]
                       : "Unknown Exception";

    log_error(name);
    log_u64("int_no",   regs->int_no);
    log_hex64("err_code", regs->err_code);
    log_hex64("rip",    regs->rip);   /* add rip to registers_t if not present */
    log_hex64("cs",     regs->cs);
    log_hex64("rflags", regs->rflags);
    log_hex64("rsp",    regs->rsp);
    log_hex64("ss",     regs->ss);

    switch (regs->int_no) {
        case 14: /* #PF */
            log_hex64("cr2", read_cr2());
            log_info(regs->err_code & 1  ? "reason: protection violation"
                                         : "reason: non-present page");
            log_info(regs->err_code & 2  ? "access: write" : "access: read");
            log_info(regs->err_code & 4  ? "mode: user"    : "mode: supervisor");
            if (regs->err_code & 8)  log_info("type: reserved bit set");
            if (regs->err_code & 16) log_info("type: instruction fetch");
            break;

        case 13: /* #GP */
            decode_gp(regs->err_code);
            break;

        case 8:  /* #DF - always err=0, not recoverable */
        case 18: /* #MC */
            log_error("Non-recoverable exception - system halted");
            break;

        default:
            break;
    }

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void irq_handler(registers_t *regs) {
    if (regs == NULL) {
        return;
    }

    if (regs->int_no < 32 || regs->int_no > 47) {
        return;
    }

    uint8_t irq = (uint8_t)(regs->int_no - 32);

    if (irq_handlers[irq] != NULL) {
        irq_handlers[irq](regs);
    }

    pic_send_eoi(irq);
}
