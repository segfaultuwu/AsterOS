#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aster/kernel/syscall/syscall.h"

#define SHELL_LINE_MAX 128
#define USHELL_TEXT __attribute__((section(".usershell.text"), used, noinline))
#define USHELL_RODATA __attribute__((section(".usershell.rodata"), used))
#define USHELL_ENTRY __attribute__((section(".usershell.entry"), used))

static const char shell_banner[] USHELL_RODATA =
    "AsterOS user shell\n"
    "Type 'help' for commands.\n";

static const char shell_prompt[] USHELL_RODATA =
    "\033[1;32master> \033[0m";

static const char shell_help_text[] USHELL_RODATA =
    "Commands: help, clear, echo <text>, ls [path], lsblk, mount [device] [dir] <ext2|ramfs>, ./path/to/elf, exit\n";

static const char shell_unknown_text[] USHELL_RODATA =
    "Unknown command. Type 'help'.\n";

static const char shell_ls_error_text[] USHELL_RODATA =
    "ls: not a directory or unavailable\n";

static const char shell_lsblk_header_text[] USHELL_RODATA =
    "drive  present  sectors    model\n";

static const char shell_mount_usage_text[] USHELL_RODATA =
    "mount: usage: mount [device] [dir] <ext2|ramfs>\n";

static const char shell_mount_ext2_ok_text[] USHELL_RODATA =
    "mounted ext2\n";

static const char shell_mount_ramfs_ok_text[] USHELL_RODATA =
    "mounted ramfs\n";

static const char shell_mount_failed_ext2_text[] USHELL_RODATA =
    "mount: failed to mount ext2\n";

static const char shell_mount_failed_ramfs_text[] USHELL_RODATA =
    "mount: failed to mount ramfs\n";

static const char shell_mount_unknown_text[] USHELL_RODATA =
    "mount: unknown filesystem\n";

static const char shell_exec_failed_text[] USHELL_RODATA =
    "exec: failed\n";

static const char shell_exec_started_text[] USHELL_RODATA =
    "exec: started pid ";

static const char shell_two_spaces[] USHELL_RODATA = "  ";
static const char shell_slash[] USHELL_RODATA = "/";
static const char shell_open_paren[] USHELL_RODATA = " (";
static const char shell_close_paren[] USHELL_RODATA = ")";
static const char shell_device_prefix[] USHELL_RODATA = "s";
static const char shell_yes_text[] USHELL_RODATA = "      yes      ";
static const char shell_four_spaces[] USHELL_RODATA = "    ";
static const char shell_lsblk_unavailable_text[] USHELL_RODATA = "lsblk: unavailable\n";
static const char shell_backspace_text[] USHELL_RODATA = "\b \b";
static const char shell_help_command[] USHELL_RODATA = "help";
static const char shell_clear_command[] USHELL_RODATA = "clear";
static const char shell_echo_command[] USHELL_RODATA = "echo";
static const char shell_lsblk_command[] USHELL_RODATA = "lsblk";
static const char shell_ls_command[] USHELL_RODATA = "ls";
static const char shell_mount_command[] USHELL_RODATA = "mount";
static const char shell_ext2_name[] USHELL_RODATA = "ext2";
static const char shell_ramfs_name[] USHELL_RODATA = "ramfs";
static const char shell_root_path[] USHELL_RODATA = "/";

USHELL_TEXT static uint64_t shell_syscall6(
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

USHELL_TEXT static uint64_t shell_write(const void *buffer, size_t length) {
    return shell_syscall6(1, 1, (uint64_t)buffer, length, 0, 0, 0);
}

USHELL_TEXT static uint64_t shell_read(void *buffer, size_t length) {
    return shell_syscall6(2, 0, (uint64_t)buffer, length, 0, 0, 0);
}

USHELL_TEXT static uint64_t shell_exit(uint64_t status) {
    return shell_syscall6(0, status, 0, 0, 0, 0, 0);
}

USHELL_TEXT static size_t shell_strlen(const char *text) {
    size_t length = 0;

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

USHELL_TEXT static bool shell_str_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

USHELL_TEXT static const char *shell_skip_spaces(const char *text) {
    while (*text == ' ') {
        text++;
    }

    return text;
}

USHELL_TEXT static bool shell_command_is(const char *line, const char *command) {
    size_t i = 0;

    while (command[i] != '\0') {
        if (line[i] != command[i]) {
            return false;
        }

        i++;
    }

    return line[i] == '\0' || line[i] == ' ';
}

USHELL_TEXT static const char *shell_command_args(const char *line, size_t command_len) {
    return shell_skip_spaces(&line[command_len]);
}

USHELL_TEXT static bool shell_copy_first_token(const char *line, char *out, size_t out_size) {
    const char *text = shell_skip_spaces(line);
    size_t i = 0;

    if (out == NULL || out_size == 0 || *text == '\0') {
        return false;
    }

    while (text[i] != '\0' && text[i] != ' ') {
        if (i + 1 >= out_size) {
            return false;
        }

        out[i] = text[i];
        i++;
    }

    out[i] = '\0';
    return i > 0;
}

USHELL_TEXT static const char *shell_last_token(const char *text) {
    const char *last = NULL;

    while (*text != '\0') {
        text = shell_skip_spaces(text);
        if (*text == '\0') {
            break;
        }

        last = text;

        while (*text != '\0' && *text != ' ') {
            text++;
        }
    }

    return last;
}

USHELL_TEXT static void shell_print_str(const char *text) {
    if (text != NULL) {
        shell_write(text, shell_strlen(text));
    }
}

USHELL_TEXT static void shell_print_newline(void) {
    char newline = '\n';
    shell_write(&newline, 1);
}

USHELL_TEXT static void shell_print_help(void) {
    shell_write(shell_help_text, sizeof(shell_help_text) - 1);
}

typedef struct {
    char name[64];
    uint8_t is_dir;
    uint8_t reserved[7];
    uint64_t size;
} shell_dir_entry_t;

typedef struct {
    uint8_t drive;
    uint8_t present;
    uint16_t reserved0;
    uint32_t sectors_28;
    char model[41];
} shell_block_device_t;

USHELL_TEXT static uint64_t shell_list_dir(const char *path, shell_dir_entry_t *entries, size_t max_entries) {
    return shell_syscall6(SYS_VFS_LIST, (uint64_t)path, (uint64_t)entries, max_entries, 0, 0, 0);
}

USHELL_TEXT static uint64_t shell_lsblk(shell_block_device_t *entries, size_t max_entries) {
    return shell_syscall6(SYS_LSBLK, (uint64_t)entries, max_entries, 0, 0, 0, 0);
}

USHELL_TEXT static uint64_t shell_mount(syscall_mount_target_t target) {
    return shell_syscall6(SYS_MOUNT, (uint64_t)target, 0, 0, 0, 0, 0);
}

USHELL_TEXT static uint64_t shell_exec(const char *path) {
    return shell_syscall6(SYS_EXEC, (uint64_t)path, 0, 0, 0, 0, 0);
}

USHELL_TEXT static void shell_print_u64(uint64_t value) {
    char buffer[32];
    size_t index = 0;

    if (value == 0) {
        char zero = '0';
        shell_write(&zero, 1);
        return;
    }

    while (value > 0 && index < sizeof(buffer)) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        index--;
        shell_write(&buffer[index], 1);
    }
}

USHELL_TEXT static void shell_print_dir_entry(const shell_dir_entry_t *entry) {
    if (entry == NULL) {
        return;
    }

    shell_write(shell_two_spaces, sizeof(shell_two_spaces) - 1);
    shell_print_str(entry->name);
    if (entry->is_dir != 0) {
        shell_write(shell_slash, sizeof(shell_slash) - 1);
    }
    if (entry->size > 0 && entry->is_dir == 0) {
        shell_write(shell_open_paren, sizeof(shell_open_paren) - 1);
        shell_print_u64(entry->size);
        shell_write(shell_close_paren, sizeof(shell_close_paren) - 1);
    }
    shell_print_newline();
}

USHELL_TEXT static void shell_run_ls(const char *path) {
    shell_dir_entry_t entries[32];
    const char *target = path;

    if (target == NULL || *target == '\0') {
        target = shell_root_path;
    }

    uint64_t count = shell_list_dir(target, entries, sizeof(entries) / sizeof(entries[0]));
    if (count == (uint64_t)-1) {
        shell_write(shell_ls_error_text, sizeof(shell_ls_error_text) - 1);
        return;
    }

    for (uint64_t i = 0; i < count; i++) {
        shell_print_dir_entry(&entries[i]);
    }
}

USHELL_TEXT static void shell_run_lsblk(void) {
    shell_block_device_t devices[4];

    shell_write(shell_lsblk_header_text, sizeof(shell_lsblk_header_text) - 1);

    uint64_t count = shell_lsblk(devices, sizeof(devices) / sizeof(devices[0]));
    if (count == (uint64_t)-1) {
        shell_write(shell_lsblk_unavailable_text, sizeof(shell_lsblk_unavailable_text) - 1);
        return;
    }

    for (uint64_t i = 0; i < count; i++) {
        const shell_block_device_t *dev = &devices[i];
        shell_write(shell_device_prefix, sizeof(shell_device_prefix) - 1);
        char disk_letter = (char)('a' + dev->drive);
        shell_write(&disk_letter, 1);
        shell_write(shell_yes_text, sizeof(shell_yes_text) - 1);
        shell_print_u64(dev->sectors_28);
        shell_write(shell_four_spaces, sizeof(shell_four_spaces) - 1);
        shell_print_str(dev->model);
        shell_print_newline();
    }
}

USHELL_TEXT static void shell_mount_root(const char *name) {
    const char *fs_name = shell_last_token(name);

    if (fs_name == NULL || *fs_name == '\0') {
        shell_write(shell_mount_usage_text, sizeof(shell_mount_usage_text) - 1);
        return;
    }

    if (shell_str_eq(fs_name, shell_ext2_name)) {
        if (shell_mount(SYS_MOUNT_EXT2) == 0) {
            shell_write(shell_mount_ext2_ok_text, sizeof(shell_mount_ext2_ok_text) - 1);
            return;
        }

        shell_write(shell_mount_failed_ext2_text, sizeof(shell_mount_failed_ext2_text) - 1);
        return;
    }

    if (shell_str_eq(fs_name, shell_ramfs_name)) {
        if (shell_mount(SYS_MOUNT_RAMFS) == 0) {
            shell_write(shell_mount_ramfs_ok_text, sizeof(shell_mount_ramfs_ok_text) - 1);
            return;
        }

        shell_write(shell_mount_failed_ramfs_text, sizeof(shell_mount_failed_ramfs_text) - 1);
        return;
    }

    shell_write(shell_mount_unknown_text, sizeof(shell_mount_unknown_text) - 1);
}

USHELL_TEXT static void shell_run_exec(const char *line) {
    char path[SHELL_LINE_MAX];

    if (!shell_copy_first_token(line, path, sizeof(path))) {
        shell_write(shell_exec_failed_text, sizeof(shell_exec_failed_text) - 1);
        return;
    }

    uint64_t result = shell_exec(path);
    if (result == (uint64_t)-1) {
        shell_write(shell_exec_failed_text, sizeof(shell_exec_failed_text) - 1);
        return;
    }

    shell_write(shell_exec_started_text, sizeof(shell_exec_started_text) - 1);
    shell_print_u64(result);
    shell_print_newline();

    /* Wait for the child to finish */
    shell_syscall6(SYS_WAIT, result, 0, 0, 0, 0, 0);

    /* Notify completion */
    shell_write("exec: finished ", sizeof("exec: finished ") - 1);
    shell_print_u64(result);
    shell_print_newline();
}

USHELL_ENTRY
void user_shell_entry(void) {
    char line[SHELL_LINE_MAX];

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
                    shell_write(shell_backspace_text, sizeof(shell_backspace_text) - 1);
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

        if (shell_command_is(line, shell_help_command)) {
            shell_print_help();
            continue;
        }

        if (shell_command_is(line, shell_clear_command)) {
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

        if (shell_command_is(line, shell_echo_command)) {
            const char *args = shell_command_args(line, 4);
            char newline = '\n';

            if (*args != '\0') {
                shell_write(args, shell_strlen(args));
            }

            shell_write(&newline, 1);
            continue;
        }

        if (shell_command_is(line, shell_lsblk_command)) {
            shell_run_lsblk();
            continue;
        }

        if (shell_command_is(line, shell_ls_command)) {
            const char *rest = shell_command_args(line, 2);
            shell_run_ls(*rest == '\0' ? shell_root_path : rest);
            continue;
        }

        if (shell_command_is(line, shell_mount_command)) {
            shell_mount_root(shell_command_args(line, 5));
            continue;
        }

        if ((line[0] == '.' && line[1] == '/') || line[0] == '/') {
            shell_run_exec(line);
            continue;
        }

        shell_write(shell_unknown_text, sizeof(shell_unknown_text) - 1);
    }
}
