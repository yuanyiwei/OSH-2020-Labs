[BITS 16]
[ORG 0x7C00]                            ; starts from 0x7c00, where MBR lies in memory

    sti
clear:
    mov ax, 03h
    int 10h
print_str:
    mov si, OSH                         ; si points to string OSH
cycle_:
    lodsb                               ; load char to al
    cmp al, 0                           ; is it the end of the string?
    je wait_                            ; if true, then wait 18
    mov ah, 0x0e                        ; if false, then set AH = 0x0e 
    int 0x10                            ; call BIOS interrupt procedure, print a char to screen
    jmp cycle_                          ; loop over to print all chars
wait_:
    mov edx, 0x046c
    mov eax, [edx]
    add eax, 18
    mov ebx, eax
loop:
    mov eax, [edx]
    cmp eax, ebx
    je print_str
    jmp loop


OSH db `Hello, OSH 2020 Lab1!`, 0       ; our string, null-terminated

TIMES 510 - ($ - $$) db 0               ; the size of MBR is 512 bytes, fill remaining bytes to 0
DW 0xAA55                               ; magic number, mark it as a valid bootloader to BIOS