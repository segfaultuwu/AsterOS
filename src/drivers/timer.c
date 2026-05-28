#include "aster/drivers/timer.h"
#include "aster/drivers/ports.h"
#include "aster/arch/x86_64/idt.h"
#include "aster/debug/logging.h"

/* PIT constants */
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_FREQUENCY 1193182U

static volatile uint64_t g_ticks = 0;

static void pit_handler(registers_t *regs) {
    (void)regs;
    g_ticks++;
}

void timer_init(void) {
    /* Set PIT to 100 Hz */
    uint32_t freq = 100;
    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / freq);

    outb(PIT_COMMAND_PORT, 0x36); /* channel 0, low/high, mode 3, binary */
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    idt_register_irq_handler(0, pit_handler);

    log_ok("PIT initialized (100 Hz)");
}

uint64_t timer_get_ticks(void) {
    return g_ticks;
}
