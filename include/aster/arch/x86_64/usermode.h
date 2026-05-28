#ifndef ASTER_ARCH_X86_64_USERMODE_H
#define ASTER_ARCH_X86_64_USERMODE_H

#include <stdint.h>

void usermode_enter(uint64_t user_rip, uint64_t user_rsp);

#endif
