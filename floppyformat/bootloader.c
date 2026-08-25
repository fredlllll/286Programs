// bootloader: first code that runs when the pc starts from our floppy
//
// how a pc boots from floppy:
//   1. power on -> bios runs its self test
//   2. bios tries boot devices; for a floppy it reads the very first
//      sector (512 bytes, cylinder 0/head 0/sector 1) into memory at
//      address 0000:7C00. (0x7C00 is an ibm pc convention from 1981:
//      just below it live the bios data areas, just above grows our
//      stack.)
//   3. if the last two bytes of that sector are 55 AA, the bios
//      considers it bootable and jumps to 0000:7C00.
//
// 512 bytes are far too small for the dumper, so this little stub
// loads the rest of the image right behind itself and jumps there:
//
//   0x7C00 .. 0x7DFF : this bootloader (exactly one sector)
//   0x7E00 ..        : main.bin built from source/
//
// the %%num_sectors%% placeholder below is replaced by build.py with
// how many sectors main.bin occupies on the floppy image, starting
// at sector 2 (= physically the second sector of the disk).
//
// build.py also appends the required 55 AA signature to this code,
// which is why you will not find it here.

// naked  = compiler adds no function prologue/epilogue; the inline
//          asm IS the entire function body
// noreturn = we jump away and never come back
void __declspec ( naked ) __declspec ( noreturn ) init (void)
{
    __asm {
        ; ---- set up a sane cpu state --------------------------------
        mov bp, 7C00h     ; remember where we were loaded
        xor ax, ax        ; ax = 0
        mov ds, ax        ; data/extra/stack segment registers to 0:
        mov es, ax        ; all addresses become flat "segment:offset"
        mov ss, ax        ; pairs like 0000:7E00 instead of anything
        mov sp, bp        ; stack pointer just below our own code -
                          ; it grows downwards into free ram

        ; ---- load main.bin from the floppy --------------------------
        ; bios int 13h, function ah=02 = "read sector(s)"
        mov ah, 02h
        mov al, %%num_sectors%% ; how many sectors to read (patched in)
        mov ch, 0         ; cylinder 0
        mov cl, 2         ; start at sector 2 (we ourselves are #1)
        mov dh, 0         ; head 0
        mov bx, 7E00h     ; destination es:bx = 0000:7E00, right after us
        int 13h           ; call the bios - dl still holds the boot
                          ; drive number the bios gave us

        ; ---- hand control to the freshly loaded program -------------
        mov ax, 7E00h
        jmp ax            ; never returns
    }
}
