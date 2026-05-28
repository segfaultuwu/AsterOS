#include <stdint.h>
#include <stdbool.h>

#include "aster/user/test_user.h"
#include "aster/mm/pmm.h"
#include "aster/mm/vmm.h"
#include "aster/arch/x86_64/usermode.h"
#include "aster/debug/logging.h"

#define USER_CODE_VADDR  0x0000010000000000ULL
#define USER_STACK_VADDR 0x0000010000100000ULL

static void *phys_to_virt(uint64_t phys) {
    return (void *)(pmm_get_hhdm_offset() + phys);
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint64_t len) {
    for (uint64_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

bool user_test_setup(void) {
    uint64_t code_phys = pmm_alloc_page();
    uint64_t stack_phys = pmm_alloc_page();

    if (code_phys == 0 || stack_phys == 0) {
        log_error("user_test: failed to allocate pages");
        return false;
    }

    /*
     * User program:
     *   nop
     *   jmp $
     *
     * Simple infinite loop for now
     */
    static const uint8_t user_code[] = {
        // mov rax, 1 ; SYS_WRITE
        0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,

        // mov rdi, 1 ; stdout
        0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00,

        // mov rsi, USER_CODE_VADDR + 64
        0x48, 0xBE,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,

        // mov rdx, 20
        0x48, 0xC7, 0xC2, 0x14, 0x00, 0x00, 0x00,

        // int 0x30
        0xCD, 0x30,

        // jmp $
        0xEB, 0xFE,

        // padding until offset 64
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0,

        // offset 64:
        'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', 'u', 's', 'e', 'r', '!', '\n', 0
    };

    if (!vmm_map_page(
            USER_CODE_VADDR,
            code_phys,
            PAGE_PRESENT | PAGE_USER
        )) {
        log_error("user_test: failed to map user code");
        return false;
    }

    if (!vmm_map_page(
            USER_STACK_VADDR,
            stack_phys,
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
        )) {
        log_error("user_test: failed to map user stack");
        return false;
    }

    log_ok("User code page mapped");
    log_ok("User stack page mapped");

    usermode_enter(USER_CODE_VADDR, USER_STACK_VADDR + PAGE_SIZE);

    return true;
}
