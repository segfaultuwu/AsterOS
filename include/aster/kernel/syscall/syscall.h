#ifndef ASTER_KERNEL_SYSCALL_H
#define ASTER_KERNEL_SYSCALL_H

#include <stdint.h>

#include "aster/arch/x86_64/idt.h"

typedef enum {
    SYS_EXIT = 0,
    SYS_WRITE = 1,
    SYS_READ = 2,
    SYS_OPEN = 3,
    SYS_CLOSE = 4,
    SYS_GETPID = 5,
    SYS_MKDIR = 6,
    SYS_VFS_READ = 7,
    SYS_VFS_WRITE = 8,
    SYS_VFS_LIST = 9,
    SYS_LSBLK = 10,
    SYS_MOUNT = 11,
    SYS_WAIT = 12,
    SYS_EXEC = 13,
    SYS_MAX
} syscall_num_t;

typedef struct {
    char name[64];
    uint8_t is_dir;
    uint8_t reserved[7];
    uint64_t size;
} syscall_dir_entry_t;

typedef struct {
    uint8_t drive;
    uint8_t present;
    uint16_t reserved0;
    uint32_t sectors_28;
    char model[41];
} syscall_block_device_t;

typedef enum {
    SYS_MOUNT_RAMFS = 1,
    SYS_MOUNT_EXT2 = 2,
} syscall_mount_target_t;

typedef uint64_t (*syscall_handler_t)(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

void syscall_init(void);
void syscall_handler(registers_t *regs);

void syscall_register(syscall_num_t num, syscall_handler_t handler);
void syscall_register_defaults(void);

#endif
