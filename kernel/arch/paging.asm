; Paging assembly support functions
section .text

global load_page_directory
global enable_paging_asm
global get_page_fault_address

; Load page directory address into CR3
; void load_page_directory(uint32_t physical_addr)
load_page_directory:
    push ebp
    mov ebp, esp
    
    mov eax, [ebp + 8]      ; Get physical address parameter
    mov cr3, eax            ; Load page directory into CR3
    
    pop ebp
    ret

; Enable paging by setting bit 31 of CR0
; void enable_paging_asm(void)
enable_paging_asm:
    push ebp
    mov ebp, esp
    
    mov eax, cr0            ; Get current CR0
    or eax, 0x80000000      ; Set paging bit (bit 31)
    mov cr0, eax            ; Enable paging
    
    pop ebp
    ret

; Get the address that caused a page fault from CR2
; uint32_t get_page_fault_address(void)
get_page_fault_address:
    mov eax, cr2            ; CR2 contains the faulting address
    ret