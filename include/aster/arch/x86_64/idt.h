#ifndef ASTER_ARCH_X86_64_IDT_H
#define ASTER_ARCH_X86_64_IDT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

_Static_assert(offsetof(registers_t, int_no)   == 15 * sizeof(uint64_t), "int_no offset");
_Static_assert(offsetof(registers_t, rip)      == 17 * sizeof(uint64_t), "rip offset");
_Static_assert(offsetof(registers_t, rsp)      == 20 * sizeof(uint64_t), "rsp offset");
_Static_assert(sizeof(registers_t)             == 22 * sizeof(uint64_t), "registers_t size");

// Initialize IDT
void idt_init();

// Register interrupt handlers
void idt_register_isr_handler(uint8_t n, void (*handler)(registers_t*));
void idt_register_irq_handler(uint8_t n, void (*handler)(registers_t*));

// ISR and IRQ handlers (defined in assembly)
void isr_handler(registers_t *regs);
void irq_handler(registers_t* regs);

#endif // IDT_H
