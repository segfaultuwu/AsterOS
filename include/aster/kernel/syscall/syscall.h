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
    SYS_MAX
} syscall_num_t;

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
