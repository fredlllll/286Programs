; this is a bootloader that only prints a message to the screen.
; used to verify that a computer can boot from a floppy
format binary ; output raw binary
use16 ; generate 16 bit code

org 7C00h ; we expect our code to be located in this point in memory by default

start:
    ;init stack to use the 30kb before this boot sector
    mov ax,0
    mov ss,ax ; init stack segment
    mov ds,ax ; init data segment
    mov sp,7C00h ; init stack pointer to right before this sector #TODO: check if this overwrites the first instructions

    mov si,strStartupMessage ; move address of message to si
    mov cx,CONSTlenStartupMessage ; move message length to loop counter
    call println ; print startup message

    call waitForEnterKey

    mov si, strRemainingSectors
    mov cx, CONSTlenRemainingSectors
    call print
    mov ax,word [hddRemainingSectors]
    mov dx,word [hddRemainingSectors+2];little endian, reverse bitorder
    mov si,uitoaOut
    call uitoa16
    call println


copyNextMB:
    mov si, strPressEnterToStartCopying
    mov cx, CONSTlenPressEnterToStartCopying
    call println
    call waitForEnterKey

    ;start writing after sector 1
    mov [floppyCurrentSector],1
    mov cx, floppyTotalSectors-1
copyNextLoop:
    mov ax,word [hddCurrentSector]
    mov dx,word [hddCurrentSector+2]
    call readSectorHdd
    call increase32 ;increase current hdd sector
    mov word [hddCurrentSector], ax
    mov word [hddCurrentSector+2],dx
    
    
    mov ax,[floppyCurrentSector]
    mov dx,0
    call writeSectorFloppy
    add [floppyCurrentSector],1    
    
    ;jump out of loop if we are done reading
    cmp [hddRemainingSectors],0
    jnz copyNextDoneHdd
    
    loop copyNextLoop
    jmp copyNextDone
copyNextDoneHdd:
    ;tell user how many sectors have been written
copyNextDone
    mov si, strDone
    mov cx, CONSTlenDone
    call println



    ;TODO: start reading sectors and copy to floppy till we have reached 1MB, after that pause till user says to copy next MB to floppy

    call programEnd


; procedures ------------------------------

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

convertSectorToCHS: ; sectors in DX:AX, SectorsPerTrack[top of stack] and Heads on stack
    pop bx
    div bx ; divide by sectors per track. Temp in AX, sector in DX
    inc dl ; sector is 1 based, so add 1
    mov cl,dl ; put sector in CL
    xor dx,dx ; zero dx
    pop bx
    div bx ; divide temp by amount of heads
    mov ch,al; al contains track lower 8 bytes
    shr ax,2
    and ax,0C0h ; get bit 9 and 10 into right position to or it onto cl
    or cl,al
    mov dh,dl; dx contains head
    ret

convertSectorToCHSHdd: ; 0 based sector number in DX:AX to CHS in CX and DL
    push word [hddHeads]
    push word [hddSectorsPerTrack]
    call convertSectorToCHS
    ret

convertSectorToCHSFloppy: ; 0 based sector number in AX
    push word [floppyHeads]
    push word [floppySectorsPerTrack]
    call convertSectorToCHS
    ret

readSectorHdd: ; read sector DX:AX of fixed drive 0 into currentSector
    call convertSectorToCHSHdd; prepare values for int call
    mov ax, 0
    mov es, ax
    mov bx,currentSector ; setting address for target buffer in es:bx
    mov ah, 02h ;reading
    mov al, 1 ; read only one sector
    mov dl, 80h ; read from fixed drive 0
    int 13h
    ret

writeSectorFloppy: ; writes currentSector to sector DX:AX on floppy
    call convertSectorToCHSFloppy
    mov ax,0
    mov es,ax
    mov bx,currentSector
    mov ah, 03h ;writing
    mov al, 1 ;one sector
    mov dl, 1 ; write to floppy 1
    int 13h
    ret

park:
    mov ah,19h
    mov dl,80h ; fixed 1
    int 13h
    mov ah,19h
    mov dl,81h ; fixed 2
    int 13h
    mov ah,19h
    mov dl,0 ; floppy 1
    int 13h
    mov ah,19h
    mov dl,1 ; floppy 2
    int 13h
    ret

getNextKeyPress:
    mov ah,0 ; wait for keypress
    int 16h ; keyboard functions, on return: AH = keyboard scan code, AL = ASCII character or zero if special function key, https://stanislavs.org/helppc/scan_codes.html
    ret

waitForEnterKey:
    mov bl,0Dh ; enter key
    call getNextKeyPress
    cmp al,bl ; check if pressed key was enter
    jne waitForEnterKey ; if not enter key, try again
    ret

uitoa16: ; number in ax, si has to point to 6 bytes of free memory. si will be adjusted to point to first digit, length will be in cx
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
    
increase32: ;increase number in dx:ax by 1
    add ax,1
    ;check carry flag
    jnc increase32end
    add dx,1
increase32end:
    ret
    


programEnd:
    hlt
    jmp programEnd

; ---------------------- constants

strStartupMessage db 'HDD saver 1.0, press Enter to begin'
CONSTlenStartupMessage = $-strStartupMessage
strRemainingSectors db 'Remaining Sectors: '
CONSTlenRemainingSectors = $-strRemainingSectors
strPressEnterToStartCopying db 'Press Enter to start copying'
CONSTlenPressEnterToStartCopying = $-strPressEnterToStartCopying
strDone db 'Done'
CONSTlenDone = $-strDone
szCRLF dw 0D0Ah
floppyTracks db 80
floppyHeads db 2
floppySectorsPerTrack db 18
floppyTotalSectors dw 80*2*18
hddTracks dw 820
hddHeads db 6
hddSectorsPerTrack db 26
CONSThddTotalSectors = 820*6*26
hddTotalSectors dd CONSThddTotalSectors
ten16 dw 10
CONSTascii0 = '0'
;ascii0 db CONSTascii0


; --------------------- variables

uitoaOut db 0,0,0,0,0,0
hddRemainingSectors dd CONSThddTotalSectors
hddCurrentSector dd 0
floppyCurrentSector dw 0

; --------------------- stuff
rb 7C00h+512-2-$ ; fill up to the boot record signature, will throw an error on compile if code before it would push out the following signature
db 055h,0AAh ; signature is expected by some bioses or it will ignore this bootloader

currentSector rb 512 ; basically reserve 512 bytes for temporary sector, but this one will not be copied, but makes program nicer i guess?