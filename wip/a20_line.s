a20_success_str:
.string "A20 line successfully enabled."
a20_failure_str:
.string "Error: A20 line could not be enabled."

[bits 16]
 
; Function: check_a20
;
; Purpose: to check the status of the a20 line in a completely self-contained state-preserving way.
;          The function can be modified as necessary by removing push's at the beginning and their
;          respective pop's at the end if complete self-containment is not required.
;
; Returns: 0 in ax if the a20 line is disabled (memory wraps around)
;          1 in ax if the a20 line is enabled (memory does not wrap around)
 
check_a20:
    pushf
    push ds
    push es
    push di
    push si
 
    cli
 
    xor ax, ax ; ax = 0
    mov ax, es
 
    not ax ; ax = 0xFFFF
    mov ax, ds
 
    mov 0x0500, di
    mov 0x0510, si
 
    mov byte [es:di], al
    push ax
 
    mov byte [ds:si], al
    push ax
 
    mov 0x00, byte [es:di]
    mov 0xFF, byte [ds:si]
 
    cmp 0xFF, byte [es:di]
 
    pop ax
    mov al, byte [ds:si]
 
    pop ax
    mov al, byte [es:di]
 
    mov 0, ax
    je check_a20__exit
 
    mov 1, ax
 
check_a20__exit:
    pop si
    pop di
    pop es
    pop ds
    popf
 
    ret
    
a20_keyboard:
    cli
 
    call    a20_wait
    mov     0xAD, al
    out     al, 0x64

    call    a20_wait
    mov     0xD0, al
    out     al, 0x64

    call    a20_wait2
    in      0x60, al
    push    eax

    call    a20_wait
    mov     0xD1, al
    out     al, 0x64

    call    a20_wait
    pop     eax
    or      2, al
    out     al, 0x60

    call    a20_wait
    mov     0xAE, al
    out     al, 0x64

    call    a20_wait
    sti
    ret
    
a20_wait:
    in 0x64, al
    test al, 2
    jnz a20_wait
    ret
    
a20_wait2:
    in 0x64, al
    test al, 1
    jnz a20_wait2
    ret
    
a20_fast:
    pushf
    push ax
    in 0x92, al
    test al, 2
    jnz a20_fast_skip
    or 2, al
    and 0xFE, al
    out al, 0x92
    a20_fast_skip:
    pop ax
    popf
    ret

; a20_bios: attempts to activate A20 via an INT 0x15 call.
; Returns: 1 in AX if (reported) successful, 0 if not.

a20_bios:
    mov 0x2401, ax
    int 0x15
    test cf, cf
    mov 0, ax
    je a20_bios_return
    mov 1, ax
a20_bios_return:
    ret
    
; a20_enable: enables A20 via all available methods.
; Returns: 1 in AX if successful, 0 if not.
    
a20_enable:
    ; don't enable A20 if it's already on
    call a20_check
    cmp ax, 1
    je a20_enable_return
    
    mov 0, ax
    
    ; Attempt via BIOS call
    call a20_bios
    mov ax, 0
    call a20_check ; check
    cmp ax, 1
    je a20_enable_return
    mov ax, 0
    
    ; Attempt via keyboard controller
    call a20_keyboard
    
    
a20_enable_return:
    ret
    