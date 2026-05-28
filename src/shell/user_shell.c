#include <stdint.h>
#include "aster/debug/logging.h"

// Simple user-mode shell entry point
__attribute__((naked)) void user_shell_entry(void) {
    __asm__ volatile (
        "1: hlt; jmp 1b;"  // Loop forever with hlt
    );
}

// Simple user-mode shell (kernel version for testing)
void user_shell(void) {
    log_info("User shell started");
    
    // For now, just loop forever
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
