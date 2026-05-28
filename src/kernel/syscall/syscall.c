#include <stdint.h>
#include <stddef.h>

#include "aster/arch/x86_64/idt.h"
#include "aster/kernel/syscall/syscall.h"
#include "aster/debug/logging.h"
#include "aster/drivers/console/console.h"
#include "aster/drivers/ata.h"
#include "aster/drivers/keyboard.h"
#include "aster/fs/ramfs.h"
#include "aster/fs/ext2.h"
#include "aster/user/process.h"
#include "aster/scheduler/scheduler.h"
#include "aster/fs/vfs.h"

#define SYSCALL_ERROR ((uint64_t)-1)

static uint64_t sys_exit(
    uint64_t status,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_write(
    uint64_t fd,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_read(
    uint64_t fd,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_getpid(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_mkdir(
    uint64_t path,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_vfs_read(
    uint64_t path,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_vfs_write(
    uint64_t path,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_vfs_list(
    uint64_t path,
    uint64_t entries,
    uint64_t max_entries,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_lsblk(
    uint64_t entries,
    uint64_t max_entries,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_mount(
    uint64_t target,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static uint64_t sys_exec(
    uint64_t path,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

#define EXEC_IMAGE_MAX (8ULL * 1024ULL * 1024ULL)
static uint8_t exec_image[EXEC_IMAGE_MAX];

static syscall_handler_t syscall_table[SYS_MAX] = {
    [SYS_EXIT] = NULL,
    [SYS_WRITE] = NULL,
    [SYS_READ] = NULL,
    [SYS_OPEN] = NULL,
    [SYS_CLOSE] = NULL,
    [SYS_GETPID] = NULL,
    [SYS_MKDIR] = NULL,
    [SYS_VFS_READ] = NULL,
    [SYS_VFS_WRITE] = NULL,
    [SYS_VFS_LIST] = NULL,
    [SYS_LSBLK] = NULL,
    [SYS_MOUNT] = NULL,
    [SYS_EXEC] = NULL,
};

void syscall_register(syscall_num_t num, syscall_handler_t handler) {
    if (num >= SYS_MAX) {
        return;
    }

    syscall_table[num] = handler;
}

static uint64_t sys_wait(
    uint64_t pid,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    if (pid == 0) return SYSCALL_ERROR;

    /* Busy-wait until the task reaches TASK_FINISHED. */
    for (;;) {
        const Task *t = scheduler_get_task((uint32_t)pid);
        if (t == NULL) {
            return SYSCALL_ERROR;
        }

        if (t->state == TASK_FINISHED) {
            return 0;
        }

        __asm__ volatile ("sti\n\thlt");
    }
}

void syscall_register_defaults(void) {
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_READ, sys_read);
    syscall_register(SYS_GETPID, sys_getpid);
    syscall_register(SYS_MKDIR, sys_mkdir);
    syscall_register(SYS_VFS_READ, sys_vfs_read);
    syscall_register(SYS_VFS_WRITE, sys_vfs_write);
    syscall_register(SYS_VFS_LIST, sys_vfs_list);
    syscall_register(SYS_LSBLK, sys_lsblk);
    syscall_register(SYS_MOUNT, sys_mount);
    syscall_register(SYS_WAIT, sys_wait);
    syscall_register(SYS_EXEC, sys_exec);
}

void syscall_init(void) {
    syscall_register_defaults();
    log_ok("Syscalls initialized");
}

void syscall_handler(registers_t *regs) {
    if (regs == NULL) {
        log_error("syscall: NULL regs");
        return;
    }

    uint64_t syscall_num = regs->rax;

    if (syscall_num >= SYS_MAX) {
        regs->rax = SYSCALL_ERROR;
        return;
    }

    syscall_handler_t handler = syscall_table[syscall_num];

    if (handler == NULL) {
        regs->rax = SYSCALL_ERROR;
        return;
    }

    /*
     * ABI:
     *   rax = syscall number
     *   rdi = arg0
     *   rsi = arg1
     *   rdx = arg2
     *   r10 = arg3
     *   r8  = arg4
     *   r9  = arg5
     *
     * return:
     *   rax = result
     */
    regs->rax = handler(
        regs->rdi,
        regs->rsi,
        regs->rdx,
        regs->r10,
        regs->r8,
        regs->r9
    );
}

static uint64_t sys_exit(
    uint64_t status,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    log_info("sys_exit");
    log_u64("status", status);

    Task *task = scheduler_current_task();
    if (task != NULL) {
        task->state = TASK_FINISHED;
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }

    return 0;
}

static uint64_t sys_write(
    uint64_t fd,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg3;
    (void)arg4;
    (void)arg5;

    /*
     * fd:
     *   1 = stdout
     *   2 = stderr
     *
     * Na razie oba lecą na console.
     */
    if (fd != 1 && fd != 2) {
        return SYSCALL_ERROR;
    }

    if (buffer == 0) {
        return SYSCALL_ERROR;
    }

    const char *str = (const char *)buffer;

    for (uint64_t i = 0; i < length; i++) {
        console_putc(str[i]);
    }

    return length;
}

static uint64_t sys_read(
    uint64_t fd,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg3;
    (void)arg4;
    (void)arg5;

    if (fd != 0 || buffer == 0 || length == 0) {
        return SYSCALL_ERROR;
    }

    char *dst = (char *)buffer;
    size_t written = 0;

    while (written < length) {
        if (!keyboard_has_data()) {
            __asm__ volatile ("sti");
            __asm__ volatile ("hlt");
            continue;
        }

        dst[written++] = keyboard_read();
    }

    return written;
}

static uint64_t sys_getpid(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    Task *task = scheduler_current_task();
    if (task == NULL) {
        return 0;
    }

    return task->PID;
}

static uint64_t sys_mkdir(
    uint64_t path,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    if (path == 0) return SYSCALL_ERROR;

    const char *p = (const char *)path;

    if (vfs_mkdir(p)) return 0;
    return SYSCALL_ERROR;
}

static uint64_t sys_vfs_read(
    uint64_t path,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg3; (void)arg4; (void)arg5;

    if (path == 0 || buffer == 0) return SYSCALL_ERROR;

    const char *p = (const char *)path;
    void *buf = (void *)buffer;

    size_t n = vfs_read_file(p, buf, (size_t)length);

    return (uint64_t)n;
}

static uint64_t sys_vfs_write(
    uint64_t path,
    uint64_t buffer,
    uint64_t length,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg3; (void)arg4; (void)arg5;

    if (path == 0 || buffer == 0) return SYSCALL_ERROR;

    const char *p = (const char *)path;
    const void *buf = (const void *)buffer;

    if (vfs_write_file(p, buf, (size_t)length)) return length;
    return SYSCALL_ERROR;
}

typedef struct {
    syscall_dir_entry_t *entries;
    size_t max_entries;
    size_t count;
} vfs_list_context_t;

static void sys_vfs_list_callback(const char *name, bool is_dir, size_t size, void *context) {
    vfs_list_context_t *ctx = (vfs_list_context_t *)context;

    if (ctx == NULL || ctx->entries == NULL || ctx->count >= ctx->max_entries) {
        return;
    }

    syscall_dir_entry_t *entry = &ctx->entries[ctx->count++];
    size_t i = 0;

    while (name != NULL && name[i] != '\0' && i + 1 < sizeof(entry->name)) {
        entry->name[i] = name[i];
        i++;
    }

    entry->name[i] = '\0';
    entry->is_dir = is_dir ? 1 : 0;
    entry->reserved[0] = 0;
    entry->reserved[1] = 0;
    entry->reserved[2] = 0;
    entry->reserved[3] = 0;
    entry->reserved[4] = 0;
    entry->reserved[5] = 0;
    entry->reserved[6] = 0;
    entry->size = (uint64_t)size;
}

static uint64_t sys_vfs_list(
    uint64_t path,
    uint64_t entries,
    uint64_t max_entries,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg3;
    (void)arg4;
    (void)arg5;

    if (path == 0 || entries == 0 || max_entries == 0) {
        return SYSCALL_ERROR;
    }

    const char *p = (const char *)path;
    syscall_dir_entry_t *out = (syscall_dir_entry_t *)entries;
    vfs_list_context_t ctx = {
        .entries = out,
        .max_entries = (size_t)max_entries,
        .count = 0,
    };

    if (!vfs_list_dir(p, sys_vfs_list_callback, &ctx)) {
        return SYSCALL_ERROR;
    }

    return (uint64_t)ctx.count;
}

static uint64_t sys_lsblk(
    uint64_t entries,
    uint64_t max_entries,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    if (entries == 0 || max_entries == 0) {
        return SYSCALL_ERROR;
    }

    syscall_block_device_t *out = (syscall_block_device_t *)entries;
    size_t count = 0;

    for (uint8_t drive = 0; drive < 4 && count < (size_t)max_entries; drive++) {
        const ata_device_t *dev = ata_get_device((ata_drive_t)drive);
        if (dev == NULL || !dev->present) {
            continue;
        }

        syscall_block_device_t *entry = &out[count++];
        entry->drive = drive;
        entry->present = 1;
        entry->reserved0 = 0;
        entry->sectors_28 = dev->sectors_28;

        size_t i = 0;
        while (dev->model[i] != '\0' && i + 1 < sizeof(entry->model)) {
            entry->model[i] = dev->model[i];
            i++;
        }
        entry->model[i] = '\0';
        while (i + 1 < sizeof(entry->model)) {
            entry->model[++i - 1] = '\0';
        }
    }

    return (uint64_t)count;
}

static uint64_t sys_mount(
    uint64_t target,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    if (target == SYS_MOUNT_EXT2) {
        if (ext2_init(ATA_PRIMARY_MASTER) && vfs_mount_root(ext2_get_ops())) {
            return 0;
        }

        return SYSCALL_ERROR;
    }

    if (target == SYS_MOUNT_RAMFS) {
        if (ramfs_init() && vfs_mount_root(ramfs_get_ops())) {
            return 0;
        }

        return SYSCALL_ERROR;
    }

    return SYSCALL_ERROR;
}

static uint64_t sys_exec(
    uint64_t path,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    if (path == 0) {
        return SYSCALL_ERROR;
    }

    char exec_path[256];
    const char *src = (const char *)path;
    size_t i = 0;

    while (i + 1 < sizeof(exec_path) && src[i] != '\0' && src[i] != ' ') {
        exec_path[i] = src[i];
        i++;
    }

    if (i == 0 || (src[i] != '\0' && src[i] != ' ')) {
        return SYSCALL_ERROR;
    }

    exec_path[i] = '\0';

    size_t image_size = vfs_read_file(exec_path, exec_image, sizeof(exec_image));
    log_infof("sys_exec: read %lu bytes for %s", (unsigned long)image_size, exec_path);
    if (image_size == 0 || image_size >= sizeof(exec_image)) {
        log_warnf("sys_exec: failed to read %s", exec_path);
        return SYSCALL_ERROR;
    }

    Process *proc = process_create(exec_path);
    if (proc == NULL) {
        log_warn("sys_exec: failed to create process");
        return SYSCALL_ERROR;
    }

    if (!process_load_elf(proc, exec_image, (uint64_t)image_size)) {
        process_destroy(proc);
        return SYSCALL_ERROR;
    }

    if (!process_exec(proc)) {
        process_destroy(proc);
        return SYSCALL_ERROR;
    }

    log_infof("sys_exec: launched %s as PID %u", exec_path, proc->pid);
    return proc->pid;
}
