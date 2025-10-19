; Context switching assembly functions
section .text

global context_switch
global save_context_asm
global restore_context_asm

; void context_switch(process_t* old_process, process_t* new_process)
; This function performs a complete context switch between processes
context_switch:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    
    ; Get parameters
    mov eax, [ebp + 8]      ; old_process
    mov ebx, [ebp + 12]     ; new_process
    
    ; Save old process context if not null
    test eax, eax
    jz .restore_new
    
    ; Save registers to old process context structure
    ; Offset 20 is start of cpu_context_t in process_t
    mov [eax + 20], eax     ; Save EAX (will be overwritten)
    mov [eax + 24], ebx     ; Save EBX
    mov [eax + 28], ecx     ; Save ECX
    mov [eax + 32], edx     ; Save EDX
    mov [eax + 36], esi     ; Save ESI
    mov [eax + 40], edi     ; Save EDI
    
    ; Save stack pointers
    mov [eax + 44], esp     ; Save ESP
    mov [eax + 48], ebp     ; Save EBP
    
    ; Save instruction pointer (return address)
    mov ecx, [ebp + 4]      ; Get return address from stack
    mov [eax + 52], ecx     ; Save EIP
    
    ; Save flags
    pushf
    pop ecx
    mov [eax + 56], ecx     ; Save EFLAGS
    
    ; Save segment registers
    mov cx, cs
    mov [eax + 60], cx      ; Save CS
    mov cx, ds
    mov [eax + 62], cx      ; Save DS
    mov cx, es
    mov [eax + 64], cx      ; Save ES
    mov cx, fs
    mov [eax + 66], cx      ; Save FS
    mov cx, gs
    mov [eax + 68], cx      ; Save GS
    mov cx, ss
    mov [eax + 70], cx      ; Save SS
    
    ; Save CR3 (page directory)
    mov ecx, cr3
    mov [eax + 72], ecx     ; Save CR3

.restore_new:
    ; Restore new process context
    test ebx, ebx
    jz .done
    
    ; Restore CR3 (page directory) first
    mov ecx, [ebx + 72]
    test ecx, ecx
    jz .skip_cr3
    mov cr3, ecx

.skip_cr3:
    ; Restore segment registers
    mov cx, [ebx + 62]      ; Restore DS
    mov ds, cx
    mov cx, [ebx + 64]      ; Restore ES
    mov es, cx
    mov cx, [ebx + 66]      ; Restore FS
    mov fs, cx
    mov cx, [ebx + 68]      ; Restore GS
    mov gs, cx
    mov cx, [ebx + 70]      ; Restore SS
    mov ss, cx
    
    ; Restore general purpose registers
    mov eax, [ebx + 20]     ; Restore EAX
    mov ecx, [ebx + 28]     ; Restore ECX
    mov edx, [ebx + 32]     ; Restore EDX
    mov esi, [ebx + 36]     ; Restore ESI
    mov edi, [ebx + 40]     ; Restore EDI
    
    ; Restore stack pointers
    mov esp, [ebx + 44]     ; Restore ESP
    mov ebp, [ebx + 48]     ; Restore EBP
    
    ; Restore flags
    mov ecx, [ebx + 56]     ; Get EFLAGS
    push ecx
    popf
    
    ; Restore EBX last and jump to new EIP
    push dword [ebx + 52]   ; Push new EIP onto stack
    mov ebx, [ebx + 24]     ; Restore EBX
    ret                     ; Jump to new EIP

.done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; void save_context_asm(cpu_context_t* context)
; Save current CPU context to the provided structure
save_context_asm:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]      ; Get context pointer
    
    ; Save general purpose registers
    mov [eax], eax          ; Save EAX (will be current context pointer)
    mov [eax + 4], ebx      ; Save EBX
    mov [eax + 8], ecx      ; Save ECX
    mov [eax + 12], edx     ; Save EDX
    mov [eax + 16], esi     ; Save ESI
    mov [eax + 20], edi     ; Save EDI
    mov [eax + 24], esp     ; Save ESP
    mov [eax + 28], ebp     ; Save EBP
    
    ; Save instruction pointer (return address)
    mov ebx, [ebp + 4]
    mov [eax + 32], ebx     ; Save EIP
    
    ; Save flags
    pushf
    pop ebx
    mov [eax + 36], ebx     ; Save EFLAGS
    
    ; Save segment registers
    mov bx, cs
    mov [eax + 40], bx      ; Save CS
    mov bx, ds
    mov [eax + 42], bx      ; Save DS
    mov bx, es
    mov [eax + 44], bx      ; Save ES
    mov bx, fs
    mov [eax + 46], bx      ; Save FS
    mov bx, gs
    mov [eax + 48], bx      ; Save GS
    mov bx, ss
    mov [eax + 50], bx      ; Save SS
    
    ; Save CR3
    mov ebx, cr3
    mov [eax + 52], ebx     ; Save CR3
    
    pop ebp
    ret

; void restore_context_asm(cpu_context_t* context)
; Restore CPU context from the provided structure
restore_context_asm:
    mov eax, [esp + 4]      ; Get context pointer (no frame setup needed)
    
    ; Restore CR3 first
    mov ebx, [eax + 52]
    test ebx, ebx
    jz .skip_cr3_restore
    mov cr3, ebx

.skip_cr3_restore:
    ; Restore segment registers
    mov bx, [eax + 42]      ; Restore DS
    mov ds, bx
    mov bx, [eax + 44]      ; Restore ES
    mov es, bx
    mov bx, [eax + 46]      ; Restore FS
    mov fs, bx
    mov bx, [eax + 48]      ; Restore GS
    mov gs, bx
    mov bx, [eax + 50]      ; Restore SS
    mov ss, bx
    
    ; Restore flags
    mov ebx, [eax + 36]
    push ebx
    popf
    
    ; Set up for final restoration
    mov esp, [eax + 24]     ; Restore ESP
    mov ebp, [eax + 28]     ; Restore EBP
    
    ; Push new EIP for return
    push dword [eax + 32]
    
    ; Restore general purpose registers
    mov ebx, [eax + 4]      ; Restore EBX
    mov ecx, [eax + 8]      ; Restore ECX
    mov edx, [eax + 12]     ; Restore EDX
    mov esi, [eax + 16]     ; Restore ESI
    mov edi, [eax + 20]     ; Restore EDI
    mov eax, [eax]          ; Restore EAX last
    
    ret                     ; Jump to restored EIP