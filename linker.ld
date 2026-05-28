ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    .text : ALIGN(0x1000)
    {
        *(.text .text.*)
    }

    .rodata : ALIGN(0x1000)
    {
        *(.rodata .rodata.*)
    }

    .usershell : ALIGN(0x1000)
    {
        __usershell_start = .;
        KEEP(*(.usershell .usershell.*))
        __usershell_end = .;
    }

    .data : ALIGN(0x1000)
    {
        *(.data .data.*)
    }

    .requests : ALIGN(0x1000)
    {
        KEEP(*(.requests_start_marker))
        KEEP(*(.requests))
        KEEP(*(.requests_end_marker))
    }

    .bss : ALIGN(0x1000)
    {
        *(COMMON)
        *(.bss .bss.*)
    }
}
