; System call interrupt handler
section .text

extern syscall_handler
global syscall_interrupt_handler

; System call interrupt handler (int 0x80)
; Arguments are passed in registers: EAX=syscall_num, EBX=arg1, ECX=arg2, EDX=arg3, ESI=arg4
syscall_interrupt_handler:
    ; Save all registers
    push ebp
    push edi
    push esi
    push edx
    push ecx
    push ebx
    push eax
    
    ; Save segment registers
    push ds
    push es
    push fs
    push gs
    
    ; Set up kernel data segments
    mov ax, 0x10    ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Prepare arguments for syscall_handler
    ; Arguments are already in the right registers from user space
    push esi        ; arg4
    push edx        ; arg3
    push ecx        ; arg2
    push ebx        ; arg1
    push eax        ; syscall_num
    
    ; Call the C system call handler
    call syscall_handler
    
    ; Clean up stack (remove 5 arguments)
    add esp, 20
    
    ; Store return value in original EAX position on stack
    mov [esp + 16], eax  ; Overwrite saved EAX with return value
    
    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Restore general purpose registers
    pop eax         ; This now contains the return value
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    pop ebp
    
    ; Return to user space
    iret