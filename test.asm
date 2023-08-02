include "16bitinit.inc"
start:
readSectorHdd:
    mov ax, 0
    mov es, ax
    mov bx,currentSector ; setting address for target buffer in es:bx
	mov ch, 0 ; sector
	mov cl, 1 ; track
	mov dh, 0 ; head
	mov dl, 80h ; read from fixed drive 0
        
    mov ah, 02h ;reading
    mov al, 1 ; read only one sector
    int 13h
        
printLoop:
	mov bx, currentSector
	add bx, [currentSectorOffset]
	
	mov al, [bx]
	
	call printALHex
	
	inc [currentSectorOffset]
	cmp [currentSectorOffset], 512
	jl printLoop


programEnd:
	hlt
	jmp programEnd

printALHex:
	mov ah, 0Eh
	mov ch, 0
	mov cl, al ; copy of al in cl
	and cl, 0Fh ; lower 4 bits only
	shr al, 4 ; get upper 4 bits only
	
	mov bx, hexTable ; load table address in bx

	xlatb; MOV AL, [BX+AL]
	
	mov bx, 1 ; page 0, color 1
	int 10h ; print char in al
	
	mov bx, hexTable ; load table address in bx
	mov al, cl
	
	xlatb
	
	mov bx, 1 ; page 0, color 1
	int 10h ; print char in al
	
	mov al ,32
	int 10h ; print space
	
	ret


ten16 dw 10 ; value 10 in 16 byte number
hexTable db "0123456789ABCDEF"
currentSectorOffset dw 0

include "bootloadersig.inc"

currentSector rb 512 ; basically reserve 512 bytes for temporary sector, but this one will not be copied, but makes program nicer i guess?