include "16bitinit.inc"
start:
mov ax,0
mov bx,30000
mov cx,ax
mov dx,bx
;muls AX:BX and CX:DX and stores in AX:BX
call mul32bit
mov dx,0
mov si, uitoaOut
call uitoa16
call println ; print ax
mov ax,bx
mov si, uitoaOut
call uitoa16
call println ; print bx

mov si,strStartupMessage ; move address of message to si
mov cx,CONSTlenStartupMessage ; move message length to loop counter
call println ; print startup message


programEnd:
hlt
jmp programEnd


strStartupMessage db 'HDD saver 1.0, press Enter to begin'
CONSTlenStartupMessage = $-strStartupMessage
uitoaOut db 0,0,0,0,0,0

include "32bitmath.inc"
include "formatting.inc"

include "bootloadersig.inc"