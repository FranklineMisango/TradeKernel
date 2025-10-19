; Interrupt handler wrappers
; These assembly functions save/restore registers and call C handlers

[BITS 32]

extern timer_handler
extern keyboard_handler
extern page_fault_interrupt_handler
extern network_handler

global timer_interrupt_wrapper
global keyboard_interrupt_wrapper
global page_fault_interrupt_wrapper
global network_interrupt_wrapper

timer_interrupt_wrapper:
    pusha                   ; Save all general-purpose registers
    call timer_handler      ; Call C handler
    popa                   ; Restore all general-purpose registers
    iret                   ; Return from interrupt

keyboard_interrupt_wrapper:
    pusha                   ; Save all general-purpose registers
    call keyboard_handler   ; Call C handler
    popa                   ; Restore all general-purpose registers
    iret                   ; Return from interrupt

page_fault_interrupt_wrapper:
    pusha                   ; Save all general-purpose registers
    call page_fault_interrupt_handler ; Call C handler
    popa                   ; Restore all general-purpose registers
    add esp, 4             ; Remove error code from stack
    iret                   ; Return from interrupt

network_interrupt_wrapper:
    pusha                   ; Save all general-purpose registers
    call network_handler    ; Call C handler
    popa                   ; Restore all general-purpose registers
    iret                   ; Return from interrupt