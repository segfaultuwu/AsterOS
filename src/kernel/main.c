#include <stdint.h>

#include "aster/arch/x86_64/gdt.h"
#include "aster/arch/x86_64/idt.h"

#include "aster/drivers/console/console.h"
#include "aster/drivers/keyboard.h"
#include "aster/drivers/serial.h"

#include "aster/debug/logging.h"

#include "aster/mm/pmm.h"
#include "aster/scheduler/scheduler.h"
#include "aster/user/test_user.h"

#include "aster/kernel/syscall/syscall.h"
#include "aster/kernel/syscall/test_syscall.h"

void kmain(void) {
    serial_init();

    if (!console_init()) {
        serial_puts("console_init failed\n");

        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    console_clear();
    log_init();

    log_ok("Console initialized");

    gdt_init();
    log_ok("GDT initialized");

    idt_init();
    log_ok("IDT initialized");

    keyboard_init();
    log_ok("Keyboard initialized");

    if (!pmm_init()) {
        log_panic("PMM init failed");
    }

    scheduler_init();
    log_ok("Scheduler initialized");

    __asm__ volatile ("sti");
    log_ok("Interrupts enabled");

    syscall_init();
    syscall_register_defaults();
    log_ok("Syscalls initialized");

    // Test syscall from kernel space
    test_syscall_from_kernel();

    user_test_setup();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
