;
; A simple boot sector program that prints a message to the screen using a BIOS routine
;

mov ah, 0x0e    ; 0eh -> scrolling teletype BIOS routine

mov al, 'H'     ; move ASCII value for Character into al
int 0x10        ; 10h -> screen services interupt
mov al, 'e'
int 0x10
mov al, 'l'
int 0x10
mov al, 'l'
int 0x10
mov al, 'o'
int 0x10

jmp $           ; jump to the current adress (i.e forever)

times 510 - ($ - $$) db 0   ; pad the boot sector with zeros

dw 0xaa55       ; set last 2 bytes to BIOS magic number

