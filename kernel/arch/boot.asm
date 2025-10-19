; Simple bootloader for TradeKernel OS
; This bootloader will be loaded by QEMU and transition to 32-bit protected mode

[BITS 16]
[ORG 0x7C00]

start:
    ; Initialize segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Print boot message
    mov si, boot_msg
    call print_string

    ; Load kernel from disk
    mov ah, 0x02        ; Read sectors function
    mov al, 30          ; Number of sectors to read (kernel size)
    mov ch, 0           ; Cylinder
    mov cl, 2           ; Sector (start from sector 2, sector 1 is bootloader)
    mov dh, 0           ; Head
    mov dl, 0x80        ; Drive number (first hard disk)
    mov bx, 0x1000      ; Load kernel at 0x1000:0x0000
    mov es, bx
    mov bx, 0
    int 0x13            ; BIOS disk interrupt

    jc disk_error       ; If carry flag is set, disk read failed

    ; Print success message
    mov si, kernel_loaded_msg
    call print_string

    ; Enter protected mode
    cli                 ; Disable interrupts
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 1           ; Set protected mode bit
    mov cr0, eax

    ; Far jump to flush instruction pipeline and enter 32-bit mode
    jmp 0x08:protected_mode

disk_error:
    mov si, disk_error_msg
    call print_string
    hlt

; 16-bit print string function
print_string:
    lodsb               ; Load byte from SI into AL
    or al, al           ; Check if null terminator
    jz .done
    mov ah, 0x0E        ; BIOS teletype function
    int 0x10            ; BIOS video interrupt
    jmp print_string
.done:
    ret

[BITS 32]
protected_mode:
    ; Set up segments for protected mode
    mov ax, 0x10        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000    ; Set stack pointer

    ; Call kernel main function
    call 0x10000        ; Jump to kernel entry point

    ; If kernel returns, halt
    hlt

; Global Descriptor Table
gdt_start:
    ; Null descriptor
    dq 0x0

    ; Code segment descriptor
    dw 0xFFFF           ; Limit (bits 0-15)
    dw 0x0000           ; Base (bits 0-15)
    db 0x00             ; Base (bits 16-23)
    db 10011010b        ; Access byte (executable, readable)
    db 11001111b        ; Flags + Limit (bits 16-19)
    db 0x00             ; Base (bits 24-31)

    ; Data segment descriptor
    dw 0xFFFF           ; Limit (bits 0-15)
    dw 0x0000           ; Base (bits 0-15)
    db 0x00             ; Base (bits 16-23)
    db 10010010b        ; Access byte (writable)
    db 11001111b        ; Flags + Limit (bits 16-19)
    db 0x00             ; Base (bits 24-31)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT size
    dd gdt_start                ; GDT address

; Boot messages
boot_msg db 'TradeKernel OS Booting...', 0x0D, 0x0A, 0
kernel_loaded_msg db 'Kernel loaded successfully!', 0x0D, 0x0A, 0
disk_error_msg db 'Disk read error!', 0x0D, 0x0A, 0

; Fill remaining space and add boot signature
times 510-($-$$) db 0
dw 0xAA55               ; Boot signature