; Kernel entry stub with multiboot header

[BITS 32]

; Multiboot header constants
MULTIBOOT_HEADER_MAGIC equ 0x1BADB002
MULTIBOOT_HEADER_FLAGS equ 0x00000003
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM

section .text
extern kernel_main

global _start
_start:
    ; Set up stack
    mov esp, stack_top
    
    ; Call kernel main
    call kernel_main
    
    ; If kernel_main returns, halt
    cli
    hlt
    jmp $

section .bss
align 16
stack_bottom:
    resb 16384  ; 16 KB stack
stack_top: