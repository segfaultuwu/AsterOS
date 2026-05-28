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

#include "aster/fs/vfs.h"
#include "aster/fs/ramfs.h"

#include "aster/kernel/syscall/syscall.h"
#include "aster/kernel/syscall/test_syscall.h"

#include "aster/generated/version.h"

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

    log_infof("Booting AsterOS %s-%s", ASTER_VERSION_STRING, ASTER_BUILD_DATE);

    gdt_init();
    log_ok("GDT initialized");

    idt_init();
    log_ok("IDT initialized");

    keyboard_init();
    log_ok("Keyboard initialized");

    if (!pmm_init()) {
        log_panic("PMM init failed");
    }

    if (!vfs_init()) {
        log_panic("VFS init failed");
    }

    if (!ramfs_init()) {
        log_panic("ramfs init failed");
    }

    if (!vfs_register_root(ramfs_get_ops())) {
        log_panic("Failed to register root FS");
    }

    if (!vfs_mkdir("/Docs")) {
        log_panic("vfs mkdir failed");
    }

    vfs_write_file("/Docs/ReadMe.TXT", "Hello VFS", 9);

    char buf[64];
    size_t n = vfs_read_file("/DOCS/readme.txt", buf, sizeof(buf)-1);
    buf[n] = '\0';
    log_info("VFS read:");
    log_info(buf);

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
