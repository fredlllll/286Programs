format binary
use16
org 7C00h
start:
mov si,as_string
mov cx,str_len
print:
mov ah,0Eh
mov bh,0
mov bl,01h
lodsb
int 10h
loop print
programEnd:
hlt
jmp programEnd
as_string db 'format binary',13,10
db 'use16',13,10
db 'org 7C00h',13,10
db 'start:',13,10
db 'mov si,as_string',13,10
db 'mov cx,str_len',13,10
db 'print:',13,10
db 'mov ah,0Eh',13,10
db 'mov bh,0',13,10
db 'mov bl,01h',13,10
db 'lodsb',13,10
db 'int 10h',13,10
db 'loop print',13,10
db 'programEnd:',13,10
db 'hlt',13,10
db 'jmp programEnd',13,10
db "as_string db '...',13,10",13,10
db 'str_len=$-as_string',13,10
str_len=$-as_string

rb 7C00h+512-2-$ ; fill up to the boot record signature, will throw an error on compile if code before it would push out the following signature
db 055h,0AAh ; signature is expected by some bioses or it will ignore this bootloader