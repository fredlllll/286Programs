; this is a bootloader that only prints a message to the screen.
; used to verify that a computer can boot from a floppy
format binary ; output raw binary
use16 ; generate 16 bit code

org 7C00h ; we expect our code to be located in this point in memory by default

start:

    ;mov ss,0 ; init stack segment
    ;mov sp,7C00h ; init stack pointer to right before this sector
    mov si,szMessage ; move address of message to si
    mov cx,lenMessage ; move message length to loop counter

write:
    mov ah,0Eh ; TTY output function number
    mov bh,0 ; page 0
    mov bl,01h ; foreground color
    lodsb ; loads byte from [si] into al and advances pointer in si by 1
    int 10h ; BIOS video interrupt, write byte to screen
    loop write ; loop till message is printed

write_done:
    hlt ; halt indefinitely
    jmp write_done

szMessage db 'This is a test message, we have boot.'
lenMessage = $-szMessage

rb 7C00h+512-2-$ ; fill up to the boot record signature, will throw an error on compile if code before it would push out the following signature
db 055h,0AAh ; signature is expected by some bioses or it will ignore this bootloader
