#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aster/user/shell.h"

#include "aster/arch/x86_64/usermode.h"
#include "aster/debug/logging.h"
#include "aster/mm/pmm.h"
#include "aster/mm/vmm.h"
#include "aster/scheduler/scheduler.h"
#include "aster/string.h"

#define SHELL_USER_BASE 0x0000010000400000ULL
#define SHELL_STACK_BASE 0x0000010000500000ULL
#define SHELL_STACK_PAGES 4
#define SHELL_LINE_MAX 128

extern const uint8_t __usershell_start[];
extern const uint8_t __usershell_end[];
extern void user_shell_entry(void);

static inline uint64_t shell_syscall6(
    uint64_t number,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    register uint64_t rax __asm__("rax") = number;
    register uint64_t rdi __asm__("rdi") = arg0;
    register uint64_t rsi __asm__("rsi") = arg1;
    register uint64_t rdx __asm__("rdx") = arg2;
    register uint64_t r10 __asm__("r10") = arg3;
    register uint64_t r8 __asm__("r8") = arg4;
    register uint64_t r9 __asm__("r9") = arg5;

    __asm__ volatile (
        "int $0x30"
        : "+a"(rax)
        : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );

    return rax;
}

static inline uint64_t shell_write(const void *buffer, size_t length) {
    return shell_syscall6(1, 1, (uint64_t)buffer, length, 0, 0, 0);
}

static inline uint64_t shell_read(void *buffer, size_t length) {
    return shell_syscall6(2, 0, (uint64_t)buffer, length, 0, 0, 0);
}

static inline uint64_t shell_exit(uint64_t status) {
    return shell_syscall6(0, status, 0, 0, 0, 0, 0);
}

__attribute__((section(".usershell"), used, noinline))
void user_shell_entry(void) {
    char line[SHELL_LINE_MAX];
    char shell_banner[] = "AsterOS user shell\nType 'help' for commands.\n";
    char shell_prompt[] = "aster> ";
    char shell_help[] = "Commands: help, clear, echo <text>, exit\n";
    char shell_unknown[] = "Unknown command. Type 'help'.\n";

    shell_write(shell_banner, sizeof(shell_banner) - 1);

    for (;;) {
        shell_write(shell_prompt, sizeof(shell_prompt) - 1);

        size_t len = 0;

        for (;;) {
            char ch = 0;
            if (shell_read(&ch, 1) != 1) {
                continue;
            }

            if (ch == '\r') {
                continue;
            }

            if (ch == '\b') {
                if (len > 0) {
                    len--;

                    char backspace[] = "\b \b";
                    shell_write(backspace, sizeof(backspace) - 1);
                }

                continue;
            }

            if (ch == '\n') {
                char newline = '\n';
                shell_write(&newline, 1);
                break;
            }

            if (len + 1 < sizeof(line)) {
                line[len++] = ch;
                shell_write(&ch, 1);
            }
        }

        line[len] = '\0';

        if (len == 0) {
            continue;
        }

        if (len == 4 && line[0] == 'h' && line[1] == 'e' && line[2] == 'l' && line[3] == 'p') {
            shell_write(shell_help, sizeof(shell_help) - 1);
            continue;
        }

        if (len == 5 && line[0] == 'c' && line[1] == 'l' && line[2] == 'e' && line[3] == 'a' && line[4] == 'r') {
            char newline = '\n';

            for (size_t i = 0; i < 24; i++) {
                shell_write(&newline, 1);
            }

            continue;
        }

        if (len == 4 && line[0] == 'e' && line[1] == 'x' && line[2] == 'i' && line[3] == 't') {
            shell_exit(0);

            for (;;) {
                __asm__ volatile ("hlt");
            }
        }

        if (len > 5 && line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o' && line[4] == ' ') {
            char newline = '\n';

            shell_write(&line[5], len - 5);
            shell_write(&newline, 1);
            continue;
        }

        shell_write(shell_unknown, sizeof(shell_unknown) - 1);
    }
}

static void shell_task_entry(void *context) {
    (void)context;

    size_t shell_size = (size_t)(__usershell_end - __usershell_start);
    size_t shell_pages = (shell_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t shell_entry_offset = (uint64_t)((uintptr_t)&user_shell_entry - (uintptr_t)__usershell_start);
    uint64_t shell_entry_vaddr = SHELL_USER_BASE + shell_entry_offset;

    for (size_t i = 0; i < shell_pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) {
            log_error("shell: failed to allocate code page");
            goto fail;
        }

        pmm_zero_page(phys);

        if (!vmm_map_page(SHELL_USER_BASE + i * PAGE_SIZE, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)) {
            log_error("shell: failed to map code page");
            goto fail;
        }
    }

    memcpy((void *)SHELL_USER_BASE, __usershell_start, shell_size);

    for (size_t i = 0; i < SHELL_STACK_PAGES; i++) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) {
            log_error("shell: failed to allocate stack page");
            goto fail;
        }

        pmm_zero_page(phys);

        if (!vmm_map_page(SHELL_STACK_BASE + i * PAGE_SIZE, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NX)) {
            log_error("shell: failed to map stack page");
            goto fail;
        }
    }

    log_ok("User shell mapped, entering ring 3");
    usermode_enter(shell_entry_vaddr, SHELL_STACK_BASE + SHELL_STACK_PAGES * PAGE_SIZE);
    return;

fail:
    Task *task = scheduler_current_task();
    if (task != NULL) {
        task->state = TASK_FINISHED;
    }
}

bool shell_spawn(void) {
    return scheduler_create_task("shell", shell_task_entry, NULL) != NULL;
}