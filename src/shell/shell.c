#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aster/shell.h"
#include "aster/drivers/console/console.h"
#include "aster/drivers/keyboard.h"
#include "aster/drivers/serial.h"
#include "aster/drivers/ports.h"

#define SHELL_PROMPT "> "
#define SHELL_LINE_MAX 128

#define ANSI_RESET   "\033[0m"
#define ANSI_DIM     "\033[2m"
#define ANSI_RED     "\033[38;2;243;139;168m"
#define ANSI_GREEN   "\033[38;2;166;227;161m"
#define ANSI_YELLOW  "\033[38;2;249;226;175m"
#define ANSI_BLUE    "\033[38;2;137;180;250m"
#define ANSI_MAUVE   "\033[38;2;203;166;247m"
#define ANSI_TEXT    "\033[38;2;205;214;244m"

static char shell_line[SHELL_LINE_MAX];
static size_t shell_length = 0;

static void shell_reset_line(void) {
    shell_length = 0;
    shell_line[0] = '\0';
}

static void shell_print_prompt(void) {
    console_write(ANSI_MAUVE);
    console_write(SHELL_PROMPT);
    console_write(ANSI_RESET);
}

static bool shell_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void shell_trim(char **start, char **end) {
    while (*start < *end && shell_is_space(**start)) {
        (*start)++;
    }

    while (*end > *start && shell_is_space((*end)[-1])) {
        (*end)--;
    }
}

static bool shell_equals(const char *a, const char *b, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return b[length] == '\0';
}

static void shell_print_slice(const char *start, const char *end) {
    for (const char *p = start; p < end; p++) {
        console_putc(*p);
    }
}

static void shell_print_header(void) {
    console_write(ANSI_MAUVE);
    console_write("AsterOS shell\n");
    console_write(ANSI_DIM);
    console_write("Type 'help' for available commands.\n");
    console_write(ANSI_RESET);
}

static void shell_cmd_help(void) {
    console_write(ANSI_BLUE);
    console_write("Available commands:\n");
    console_write(ANSI_RESET);

    console_write("  help        Show this help message\n");
    console_write("  clear       Clear the screen\n");
    console_write("  echo TEXT   Print text\n");
    console_write("  about       Show system information\n");
    console_write("  reboot      Reboot the machine\n");
    console_write("  halt        Halt the CPU\n");
}

static void shell_cmd_about(void) {
    console_write(ANSI_MAUVE);
    console_write("AsterOS\n");
    console_write(ANSI_RESET);

    console_write("  arch: x86_64\n");
    console_write("  boot: Limine\n");
    console_write("  console: Flanterm framebuffer\n");
}

static void shell_cmd_reboot(void) {
    console_write(ANSI_YELLOW);
    console_write("Rebooting...\n");
    console_write(ANSI_RESET);

    // PS/2 controller reset pulse.
    while (inb(0x64) & 0x02) {
        __asm__ volatile ("pause");
    }

    outb(0x64, 0xFE);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void shell_cmd_halt(void) {
    console_write(ANSI_YELLOW);
    console_write("System halted.\n");
    console_write(ANSI_RESET);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void shell_unknown_command(const char *start, const char *end) {
    console_write(ANSI_RED);
    console_write("Unknown command: ");
    shell_print_slice(start, end);
    console_write(ANSI_RESET);
    console_putc('\n');
}

static void shell_run_command(char *line, size_t length) {
    char *start = line;
    char *end = line + length;

    shell_trim(&start, &end);

    if (start == end) {
        return;
    }

    size_t command_length = 0;

    while (start + command_length < end &&
           !shell_is_space(start[command_length])) {
        command_length++;
    }

    char *args = start + command_length;
    shell_trim(&args, &end);

    if (shell_equals(start, "help", command_length)) {
        shell_cmd_help();
        return;
    }

    if (shell_equals(start, "clear", command_length)) {
        console_clear();
        return;
    }

    if (shell_equals(start, "echo", command_length)) {
        shell_print_slice(args, end);
        console_putc('\n');
        return;
    }

    if (shell_equals(start, "about", command_length)) {
        shell_cmd_about();
        return;
    }

    if (shell_equals(start, "reboot", command_length)) {
        shell_cmd_reboot();
        return;
    }

    if (shell_equals(start, "halt", command_length)) {
        shell_cmd_halt();
        return;
    }

    shell_unknown_command(start, start + command_length);
}

static void shell_handle_backspace(void) {
    if (shell_length == 0) {
        return;
    }

    shell_length--;
    shell_line[shell_length] = '\0';

    console_write("\b \b");
}

static void shell_handle_enter(void) {
    console_putc('\n');

    shell_line[shell_length] = '\0';
    shell_run_command(shell_line, shell_length);

    shell_reset_line();
    shell_print_prompt();
}

static void shell_handle_regular_char(char c) {
    if (shell_length + 1 >= SHELL_LINE_MAX) {
        console_write("\n");
        console_write(ANSI_RED);
        console_write("error: line too long\n");
        console_write(ANSI_RESET);

        shell_reset_line();
        shell_print_prompt();
        return;
    }

    shell_line[shell_length] = c;
    shell_length++;
    shell_line[shell_length] = '\0';

    console_putc(c);
}

static void shell_handle_char(char c) {
    if (c == '\b') {
        shell_handle_backspace();
        return;
    }

    if (c == '\n' || c == '\r') {
        shell_handle_enter();
        return;
    }

    if (c == '\t') {
        return;
    }

    shell_handle_regular_char(c);
}

void shell_run(void) {
    shell_reset_line();

    shell_print_header();
    shell_print_prompt();

    for (;;) {
        while (!keyboard_has_data()) {
            __asm__ volatile ("hlt");
        }

        char c = keyboard_read();

        if (c == 0) {
            continue;
        }

        shell_handle_char(c);
    }
}
