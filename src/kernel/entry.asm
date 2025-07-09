; TradeKernel - 64-bit kernel entry point
; This is the first code that runs after the bootloader switches to long mode

[BITS 64]
section .text

global kernel_main
global context_switch_asm
global task_entry_point_asm
global get_cpu_id
global set_task_stack

extern cpp_kernel_main

; Kernel entry point from bootloader
kernel_main:
    ; Set up initial stack
    mov rsp, kernel_stack_top
    mov rbp, rsp
    
    ; Clear the direction flag
    cld
    
    ; Set up segment registers for kernel
    mov ax, 0x10        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Clear BSS section
    extern bss_start
    extern bss_end
    mov rdi, bss_start
    mov rcx, bss_end
    sub rcx, rdi
    xor al, al
    rep stosb
    
    ; Initialize FPU/SSE
    call init_fpu_sse
    
    ; Call C++ kernel main
    call cpp_kernel_main
    
    ; Should never reach here
    cli
    hlt

; Initialize FPU and SSE for floating point operations
init_fpu_sse:
    ; Initialize FPU
    fninit
    
    ; Enable SSE
    mov rax, cr0
    and ax, 0xFFFB      ; Clear coprocessor emulation CR0.EM
    or ax, 0x2          ; Set coprocessor monitoring  CR0.MP
    mov cr0, rax
    
    ; Enable OSFXSR and OSXMMEXCPT in CR4
    mov rax, cr4
    or ax, 0x600        ; Set CR4.OSFXSR and CR4.OSXMMEXCPT
    mov cr4, rax
    
    ret

; Context switch between tasks
; Parameters: RDI = from_context, RSI = to_context
context_switch_asm:
    ; Save current task's context
    mov [rdi + 0], rax      ; Save RAX
    mov [rdi + 8], rbx      ; Save RBX
    mov [rdi + 16], rcx     ; Save RCX
    mov [rdi + 24], rdx     ; Save RDX
    mov [rdi + 32], rsi     ; Save RSI
    mov [rdi + 40], rdi     ; Save RDI
    mov [rdi + 48], rbp     ; Save RBP
    mov [rdi + 56], rsp     ; Save RSP
    mov [rdi + 64], r8      ; Save R8
    mov [rdi + 72], r9      ; Save R9
    mov [rdi + 80], r10     ; Save R10
    mov [rdi + 88], r11     ; Save R11
    mov [rdi + 96], r12     ; Save R12
    mov [rdi + 104], r13    ; Save R13
    mov [rdi + 112], r14    ; Save R14
    mov [rdi + 120], r15    ; Save R15
    
    ; Save flags
    pushfq
    pop rax
    mov [rdi + 144], rax    ; Save RFLAGS
    
    ; Save RIP (return address)
    mov rax, [rsp]
    mov [rdi + 136], rax    ; Save RIP
    
    ; Save FPU/SSE state
    fxsave [rdi + 152]      ; Save FPU/SSE state (512 bytes)
    
    ; Restore new task's context
    mov rax, [rsi + 0]      ; Restore RAX
    mov rbx, [rsi + 8]      ; Restore RBX
    mov rcx, [rsi + 16]     ; Restore RCX
    mov rdx, [rsi + 24]     ; Restore RDX
    mov rbp, [rsi + 48]     ; Restore RBP
    mov rsp, [rsi + 56]     ; Restore RSP
    mov r8, [rsi + 64]      ; Restore R8
    mov r9, [rsi + 72]      ; Restore R9
    mov r10, [rsi + 80]     ; Restore R10
    mov r11, [rsi + 88]     ; Restore R11
    mov r12, [rsi + 96]     ; Restore R12
    mov r13, [rsi + 104]    ; Restore R13
    mov r14, [rsi + 112]    ; Restore R14
    mov r15, [rsi + 120]    ; Restore R15
    
    ; Restore FPU/SSE state
    fxrstor [rsi + 152]     ; Restore FPU/SSE state
    
    ; Restore flags
    mov rax, [rsi + 144]
    push rax
    popfq
    
    ; Restore RSI and RDI last
    mov rdi, [rsi + 40]     ; Restore RDI
    mov rax, [rsi + 32]     ; Get RSI value
    mov rcx, [rsi + 136]    ; Get RIP
    push rcx                ; Push RIP for return
    mov rsi, rax            ; Restore RSI
    
    ret                     ; Jump to new task's RIP

; Task entry point wrapper
task_entry_point_asm:
    ; This is called when a new task starts
    ; The task function pointer and argument are set up by the scheduler
    ; RDI contains the task function, RSI contains the argument
    call rdi                ; Call task function with argument in RSI
    
    ; If task returns, terminate it
    extern task_exit
    call task_exit
    
    ; Should never reach here
    cli
    hlt

; Get current CPU ID using CPUID
get_cpu_id:
    mov eax, 1
    cpuid
    shr ebx, 24             ; Get APIC ID from EBX[31:24]
    mov rax, rbx
    ret

; Set task stack pointer
set_task_stack:
    mov rsp, rdi
    ret

; Interrupt handlers
global timer_interrupt_handler
timer_interrupt_handler:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Call C++ timer handler
    extern handle_timer_interrupt
    call handle_timer_interrupt
    
    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Send EOI to APIC
    mov rax, 0xFEE000B0     ; APIC EOI register
    mov dword [rax], 0      ; Write 0 to EOI
    
    iretq

; Network interrupt handler
global network_interrupt_handler
network_interrupt_handler:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Call C++ network handler
    extern handle_network_interrupt
    call handle_network_interrupt
    
    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Send EOI to APIC
    mov rax, 0xFEE000B0     ; APIC EOI register
    mov dword [rax], 0      ; Write 0 to EOI
    
    iretq

section .bss
    resb 8192               ; 8KB kernel stack
kernel_stack_top:
