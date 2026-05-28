bits 64

section .text
global _start
extern kmain

_start:
    cli

    mov rsp, stack_top
    mov rbp, 0

    call kmain

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16

stack_bottom:
    resb 65536
stack_top:
