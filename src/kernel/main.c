#include <stdint.h>

#include "aster/arch/x86_64/gdt.h"
#include "aster/arch/x86_64/idt.h"

#include "aster/drivers/console/console.h"
#include "aster/drivers/keyboard.h"
#include "aster/drivers/serial.h"

#include "aster/debug/logging.h"

#include "aster/mm/pmm.h"
#include "aster/scheduler/scheduler.h"
#include "aster/user/shell.h"

#include "aster/fs/vfs.h"
#include "aster/fs/ext2.h"
#include "aster/fs/ramfs.h"
#include "aster/drivers/ata.h"

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
    ata_init();

    static uint8_t sector[512] __attribute__((aligned(2)));

    if (ata_read_sector(ATA_PRIMARY_MASTER, 0, sector)) {
        log_ok("ATA sector 0 read OK");
        log_hex64("sector0 first qword", *(uint64_t *)sector);
    } else {
        log_warn("ATA sector 0 read failed");
    }

    if (!pmm_init()) {
        log_panic("PMM init failed");
    }

    if (!vfs_init()) {
        log_panic("VFS init failed");
    }

    if (ext2_init(ATA_PRIMARY_MASTER) && vfs_mount_root(ext2_get_ops())) {
        char buf[64];
        size_t n = vfs_read_file("/README.TXT", buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            log_info("ext2 read:");
            log_info(buf);
        } else {
            log_warn("ext2 root mounted, but /README.TXT was not found");
        }
    } else {
        log_warn("ext2 not found, falling back to ramfs");

        if (!ramfs_init()) {
            log_panic("ramfs init failed");
        }

        if (!vfs_register_root(ramfs_get_ops())) {
            log_panic("Failed to register ramfs root FS");
        }

        if (!vfs_mkdir("/Docs")) {
            log_panic("vfs mkdir failed");
        }

        vfs_write_file("/Docs/ReadMe.TXT", "Hello VFS", 9);

        char buf[64];
        size_t n = vfs_read_file("/DOCS/readme.txt", buf, sizeof(buf) - 1);
        buf[n] = '\0';
        log_info("ramfs read:");
        log_info(buf);
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

    if (!shell_spawn()) {
        log_panic("Failed to spawn user shell");
    }

    scheduler_run();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
