ENTRY(user_shell_entry)

SECTIONS
{
    . = 0x0000010000400000;

    .usershell.entry : ALIGN(0x1000)
    {
        KEEP(*(.usershell.entry .usershell.entry.*))
    }

    .usershell.text : ALIGN(0x1000)
    {
        KEEP(*(.usershell.text .usershell.text.*))
        KEEP(*(.text .text.*))
    }

    .usershell.rodata : ALIGN(0x1000)
    {
        KEEP(*(.usershell.rodata .usershell.rodata.*))
        KEEP(*(.rodata .rodata.*))
    }

    .data : ALIGN(0x1000)
    {
        *(.data .data.*)
    }

    .bss : ALIGN(0x1000)
    {
        *(COMMON)
        *(.bss .bss.*)
    }
}