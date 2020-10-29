
;adds AX:BX and CX:DX and stores in AX:BX
add32bit:
add bx,dx
adc ax,cx
ret

;subs CX:DX from AX:BX and stores in AX:BX
sub32bit:
sub bx,dx
sbb ax,cx
ret

;muls AX:BX and CX:DX and stores in AX:BX
mul32bit:
push dx ; push original dx value
mul dx
push ax ; push resulting lower part which will be shifted later
mov ax,bx
mul cx
push ax ; push resulting lower part too
mov ax,bx
mov bx,[bp+4]
mul bx
mov bx,ax
mov ax,dx
pop cx
add ax,cx
pop cx
add ax,cx
pop dx; pop dx
ret

;a little bit of info on how the mul works
;imagine you want to multiply 12*34
;you break it up as follows
;12×34 = 10*30 + 10*4 + 2*30 + 2*4
;to add to your misery, you only have a calculator that can do single digit multiplication, so 9x9 = 81 for example
;we have to break up our numbers a bit to make use of the calculator
;12*34 = 1*3 * 100 + 1*4 * 10 +  2*3 * 10 + 2*4
;now the important bits can be done with the calculator, and we can multiply by 10 and 100 by shifting the number to the left
;we can now transfer that principle to binary numbers, we have just done this with base-10, so now lets do it with base-65536
;AX:BX * CX:DX
;AX*CX << 32 + AX*DX << 16 + BX * CX << 16 + BX *DX
;as you see we can skip AX*CX as we are shifting this right out of our 32 bit target
;shifting by 16 bits will be done by simply adding the value to the higher register


;divs AX:BX by CX:DX and stores in AX:BX
div32bit:
;TODO https://en.wikipedia.org/wiki/Division_algorithm#Integer_division_(unsigned)_with_remainder
ret