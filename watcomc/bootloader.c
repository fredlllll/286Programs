//initializes the registers, loads code into memory and jumps to it
void __declspec ( naked ) __declspec ( noreturn ) init (void)
{
    __asm {
        mov bp, 7C00h
        xor ax, ax
        mov ds, ax
        mov es, ax
        mov ss, ax
        mov sp, bp
        
        mov ah, 02h
        mov al, 1 ; num sectors
        mov ch, 0
        mov cl, 2
        mov dh, 0
        mov bx, 7E00h
        int 13h
        
        mov ax, 7E00h
        jmp ax
    }
}
