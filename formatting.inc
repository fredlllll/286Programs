print: ; print string which is at adress si with length cx
    mov ah,0Eh ; TTY output function number
    mov bh,0 ; page 0
    mov bl,01h ; foreground color
    lodsb ; loads byte from [si] into al and advances pointer in si by 1
    int 10h ; BIOS video interrupt, write byte to screen
    loop print ; loop till message is printed
    ret

println: ; print string which is at address si with length cx and add CR LF after
    call print
    mov si, szCRLF
    mov cx, 2
    call print
    ret
    
uitoa16: ; number in (dx:)ax, si has to point to 6 bytes of free memory. si will be adjusted to point to first digit, length will be in cx
    ;655550 would be max printable number, so it can basically do 19bits and a half
    add si,6 ; move pointer after end of string
    mov cx,0
uitoa16Loop:
    dec si
    div word [ten16]; div ax by 10, ax will contain new number, dx will contain remainer to add to string
    add dl,CONSTascii0
    mov byte [si],dl;write character to string
    mov dx,0
    inc cx
    cmp ax,0
    jnz uitoa16Loop ; jmp to loop if we are not at 0 yet
    ret
    
CONSTascii0 = '0'
szCRLF dw 0D0Ah
ten16 dw 10