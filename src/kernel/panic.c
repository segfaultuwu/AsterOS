#include "aster/debug/logging.h"
#include "aster/panic.h"

void panic(const char* msg) {
    log_panic(msg);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
