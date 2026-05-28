#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Registers structure for interrupt handlers (64-bit)
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

// Initialize IDT
void idt_init();

// Register interrupt handlers
void idt_register_isr_handler(uint8_t n, void (*handler)(registers_t*));
void idt_register_irq_handler(uint8_t n, void (*handler)(registers_t*));

// ISR and IRQ handlers (defined in assembly)
void isr_handler(registers_t* regs);
void irq_handler(registers_t* regs);

#endif // IDT_H