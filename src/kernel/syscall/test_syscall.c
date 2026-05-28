#include "aster/arch/x86_64/idt.h"
#include "aster/kernel/syscall/syscall.h"
#include "aster/debug/logging.h"

// Kernel-level syscall test
void test_syscall_from_kernel(void) {
    log_info("Testing syscall from kernel space");
    
    // Create a fake registers structure for testing
    registers_t regs;
    regs.rax = SYS_GETPID; // Test getpid syscall
    
    // Call the syscall handler directly
    syscall_handler((void *)&regs);
    
    log_info("Syscall test completed");
    log_u64("Return value", regs.rax);
}