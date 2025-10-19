section .data
    hello:     db 'Hello, World!',10
    helloLen:  equ $-hello

section .text
    global _start

_start:
    ; sys_write system call
    mov rax, 1           ; sys_write (64-bit syscall number)
    mov rdi, 1           ; stdout
    mov rsi, hello       ; message buffer
    mov rdx, helloLen    ; message length
    syscall              ; 64-bit system call
    
    ; sys_exit system call
    mov rax, 60          ; sys_exit (64-bit syscall number)
    mov rdi, 0           ; exit status
    syscall              ; 64-bit system call