; TradeKernel Bootloader
; Ultra-low latency boot sequence for x86_64
; 
; This bootloader sets up the minimal environment needed for TradeKernel
; and switches to 64-bit mode with maximum performance optimizations

[BITS 16]
[ORG 0x7C00]

section .text
global _start

_start:
    ; Clear interrupts and set up segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack below bootloader
    
    ; Save boot drive
    mov [boot_drive], dl
    
    ; Print boot message
    mov si, boot_msg
    call print_string
    
    ; Load kernel from disk
    call load_kernel
    
    ; Enable A20 line for full memory access
    call enable_a20
    
    ; Set up GDT and switch to protected mode
    lgdt [gdt_descriptor]
    
    ; Enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to flush pipeline and enter protected mode
    jmp CODE_SEG:protected_mode_start

[BITS 32]
protected_mode_start:
    ; Set up protected mode segments
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Set up stack
    mov ebp, 0x90000
    mov esp, ebp
    
    ; Print protected mode message
    mov esi, pm_msg
    call print_string_pm
    
    ; Set up paging for long mode
    call setup_paging
    
    ; Enable PAE
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    
    ; Set long mode bit in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr
    
    ; Enable paging (activates long mode)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    
    ; Jump to 64-bit kernel
    jmp LONG_CODE_SEG:0x100000

[BITS 16]
; Function to print string in real mode
print_string:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Function to load kernel from disk
load_kernel:
    pusha
    mov ah, 0x02            ; Read sectors
    mov al, 32              ; Number of sectors to read
    mov ch, 0               ; Cylinder
    mov cl, 2               ; Start from sector 2
    mov dh, 0               ; Head
    mov dl, [boot_drive]    ; Drive
    mov bx, 0x1000          ; Load to 0x1000
    int 0x13
    
    jc disk_error
    
    ; Copy kernel to higher memory (1MB)
    mov esi, 0x1000
    mov edi, 0x100000
    mov ecx, 8192           ; 32 sectors * 512 bytes / 4
    cld
    a32 rep movsd
    
    popa
    ret

disk_error:
    mov si, disk_error_msg
    call print_string
    hlt

; Enable A20 line
enable_a20:
    pusha
    
    ; Try keyboard controller method
    call wait_8042
    mov al, 0xAD
    out 0x64, al
    
    call wait_8042
    mov al, 0xD0
    out 0x64, al
    
    call wait_8042_data
    in al, 0x60
    push ax
    
    call wait_8042
    mov al, 0xD1
    out 0x64, al
    
    call wait_8042
    pop ax
    or al, 2
    out 0x60, al
    
    call wait_8042
    mov al, 0xAE
    out 0x64, al
    
    call wait_8042
    popa
    ret

wait_8042:
    in al, 0x64
    test al, 2
    jnz wait_8042
    ret

wait_8042_data:
    in al, 0x64
    test al, 1
    jz wait_8042_data
    ret

[BITS 32]
; Print string in protected mode
print_string_pm:
    pusha
    mov edx, 0xB8000        ; VGA text buffer
.loop:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0F            ; White on black
    mov [edx], ax
    add edx, 2
    jmp .loop
.done:
    popa
    ret

; Set up paging for long mode
setup_paging:
    ; Clear page tables
    mov edi, 0x1000
    mov cr3, edi
    xor eax, eax
    mov ecx, 4096
    rep stosd
    mov edi, cr3
    
    ; Set up page tables
    ; PML4
    mov dword [edi], 0x2003     ; Point to PDP
    ; PDP
    mov dword [edi + 0x1000], 0x3003   ; Point to PD
    ; PD
    mov dword [edi + 0x2000], 0x4003   ; Point to PT
    
    ; Identity map first 2MB
    mov edi, 0x4000             ; PT
    mov eax, 0x003              ; Present + Writable
    mov ecx, 512                ; 512 entries
.set_entry:
    mov [edi], eax
    add eax, 0x1000             ; Next page
    add edi, 8                  ; Next entry
    loop .set_entry
    
    ret

; Data section
boot_msg db 'TradeKernel v1.0 - Ultra-Low Latency OS', 13, 10, 'Booting...', 13, 10, 0
pm_msg db 'Protected Mode Active', 0
disk_error_msg db 'Disk read error!', 13, 10, 0
boot_drive db 0

; GDT
gdt_start:
    ; Null descriptor
    dq 0

gdt_code:
    ; Code segment descriptor
    dw 0xFFFF       ; Limit (low)
    dw 0x0000       ; Base (low)
    db 0x00         ; Base (middle)
    db 10011010b    ; Access byte
    db 11001111b    ; Granularity
    db 0x00         ; Base (high)

gdt_data:
    ; Data segment descriptor  
    dw 0xFFFF       ; Limit (low)
    dw 0x0000       ; Base (low)
    db 0x00         ; Base (middle)
    db 10010010b    ; Access byte
    db 11001111b    ; Granularity
    db 0x00         ; Base (high)

gdt_long_code:
    ; Long mode code segment
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011000b    ; Access byte (executable, conforming)
    db 00100000b    ; Granularity (long mode)
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size
    dd gdt_start                ; Offset

; Constants
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start  
LONG_CODE_SEG equ gdt_long_code - gdt_start

; Boot signature
times 510-($-$$) db 0
dw 0xAA55
