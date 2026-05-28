#include <stdint.h>
#include <stddef.h>

#include "aster/arch/x86_64/idt.h"
#include "aster/kernel/syscall/syscall.h"
#include "aster/debug/logging.h"
#include "aster/drivers/console/console.h"

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

static uint64_t sys_getpid(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
);

static syscall_handler_t syscall_table[SYS_MAX] = {
    [SYS_EXIT] = NULL,
    [SYS_WRITE] = NULL,
    [SYS_READ] = NULL,
    [SYS_OPEN] = NULL,
    [SYS_CLOSE] = NULL,
    [SYS_GETPID] = NULL,
};

void syscall_register(syscall_num_t num, syscall_handler_t handler) {
    if (num >= SYS_MAX) {
        return;
    }

    syscall_table[num] = handler;
}

void syscall_register_defaults(void) {
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_GETPID, sys_getpid);
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

    return 1;
}
