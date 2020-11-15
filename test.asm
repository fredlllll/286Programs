include "16bitinit.inc"
start:

;30000*30000 = 900000000 = 35A4 E900
;35A4 = 13732
;E900 = 59648
;AX:BX *= CX:DX

;01101101 01011101 01100100 11101001 00000000
;23908


;mov ax,30000
;mov dx,30000
;mul dx
;push dx
;push ax

;jmp printResults; skip the lib thing

mov ax,10
mov bx,30000
mov cx,ax
mov dx,bx
call mul32bit
push bx
push ax

printResults:;print stuff on stack
pop ax
mov dx,0
mov si, uitoaOut
call uitoa16
call println

pop ax
mov dx,0
mov si, uitoaOut
call uitoa16
call println

programEnd:
hlt
jmp programEnd

uitoaOut db 0,0,0,0,0,0

include "32bitmath.inc"
include "formatting.inc"

include "bootloadersig.inc"