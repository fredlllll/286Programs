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
// normally main.bin lives in segment 0 right after us. when build.py
// compiles for a non-zero LOAD_SEG the references here are rewritten
// so the whole program is loaded into its own 64kb segment instead
// (e.g. 0x1000 -> physical 0x10000), which keeps bios int 13h dma
// traffic from ever touching our stack/globals.
//
// the %%num_sectors%% placeholder below is replaced by build.py with
// how many sectors main.bin occupies on the floppy image, starting
// at sector 2 (= physically the second sector of the disk).
// %%load_seg%% is the segment main.bin is loaded into (0 normally),
// %%load_off%% the offset within it (0x7E00 in segment 0, right
// behind us; a dedicated load segment has nothing at its bottom, so
// there build.py sets it to 0).
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
        xor ax, ax        ; ax = 0
        mov ds, ax        ; for now run in segment 0 so the bios int
        mov es, ax        ; 13h load has a conventional environment
        mov ss, ax
        mov sp, 7C00h     ; stack pointer just below our own code -
                          ; it grows downwards into free ram

        ; ---- load main.bin from the floppy --------------------------
        ; bios int 13h, function ah=02 = "read sector(s)".
        ; destination es:bx = %%load_seg%%:%%load_off%% (the image is
        ; loaded into its own segment, not segment 0).
        mov ax, %%load_seg%%
        mov es, ax
        mov bx, %%load_off%%
        mov ah, 02h
        mov al, %%num_sectors%% ; how many sectors to read (patched in)
        mov ch, 0         ; cylinder 0
        mov cl, 2         ; start at sector 2 (we ourselves are #1)
        mov dh, 0         ; head 0
        int 13h           ; call the bios - dl still holds the boot
                          ; drive number the bios gave us

        ; ---- move data/stack into the program's own segment ---------
        ; (reload the segment into ax: bios int 13h clobbers the
        ; registers, so we can't trust ax to still hold it here)
        cli
        mov ax, %%load_seg%%
        mov ds, ax        ; data segment = %%load_seg%% (matches dgroup)
        mov ss, ax        ; stack in the same segment
        mov sp, 7C00h     ; grows down from %%load_seg%%:7C00

        ; ---- far jump to %%load_seg%%:%%load_off%% -------------------
        push ax           ; segment
        mov ax, %%load_off%%
        push ax           ; offset
        retf              ; never returns here
    }
}
