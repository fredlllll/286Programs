format binary ; output raw binary
use16 ; generate 16 bit code

org 7C00h ; we expect our code to be located in this point in memory by default

;init stack to use the 30kb before this boot sector
;init other segment registers
mov bp,7C00h ; stack base pointer
xor ax,ax ; ax = 0
mov ds,ax ; data segment = 0
mov es,ax ; extra segment
mov ss,ax ; stack segment = 0
mov sp,bp ; stack pointer = 7C00h
