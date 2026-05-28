bits 64

global gdt_load
global tss_load

gdt_load:
    lgdt [rdi]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; FS/GS zostaw na razie albo ustaw na data.
    mov fs, ax
    mov gs, ax

    push qword 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ret

tss_load:
    mov ax, di
    ltr ax
    ret

global isr128
