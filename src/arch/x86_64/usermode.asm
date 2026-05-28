bits 64

global usermode_enter

; void usermode_enter(uint64_t user_rip, uint64_t user_rsp)
; rdi = user RIP
; rsi = user RSP

usermode_enter:
    cli

    mov ax, 0x1B        ; USER_DATA_SELECTOR = GDT index 3, RPL 3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x1B     ; SS  - data selector
    push rsi            ; RSP

    pushfq
    pop rax
    or rax, 0x200       ; IF = 1
    push rax            ; RFLAGS

    push qword 0x23     ; CS  - code selector (GDT index 4, RPL 3)
    push rdi            ; RIP

    iretq
