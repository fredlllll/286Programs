
;adds AX:BX and CX:DX and stores in AX:BX
32bitadd:
add bx,dx
adc ax,cx
ret

;subs CX:DX from AX:BX and stores in AX:BX
32bitsub:
sub bx,dx
sbb ax,cx
ret

;muls AX:BX and CX:DX and stores in AX:BX
32bitmul:
;TODO
ret

;divs AX:BX by CX:DX and stores in AX:BX
32bitdiv:
;TODO
ret