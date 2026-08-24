# build script for the hdd saver boot floppy.
#
# pipeline:
#   source/*.c  --wcc-->  obj/*.obj  --wlink-->  obj/main.bin
#   (raw binary image loaded at 0x7E00 by the bootloader)
#
#   bootloader.c --patch %%num_sectors%%--wcc--> obj/bootloader.tmp.obj
#                --wlink--> obj/bootloader.bin (loaded at 0x7C00)
#
#   bootloader.bin + main.bin (padded) -> output.img, a raw 1.44mb
#   floppy image ready to write to a disk with rawritewin/imagedisk/etc.

import os
import re
import sys

SOURCE_DIR = "source"
OBJ_DIR = "obj"

# must mirror ARENA_SEG in source/program.c: linear start of the far
# data arena. the flat image (code + dgroup) has to stay below it.
ARENA_SEG = 0x1000

WCC_FLAGS = "-2 -d0 -wx -ms -s -zl -i="+SOURCE_DIR
# compiler flags:
#   -2   : generate 286 code (the target machine)
#   -d0  : no debug info
#   -wx  : maximum warning level
#   -ms  : small memory model (one code + one data segment)
#   -s   : remove stack overflow checks (we manage ss:sp ourselves)
#   -zl  : remove library references -> freestanding, no runtime
#   -i=  : include path for headers

def silent_remove(filename):
    try:
        os.remove(filename)
    except:
        pass

# every .c in source/ becomes an object file and gets linked together,
# so new modules are picked up automatically. main.c must exist since
# it provides main() / the entry point.
def list_sources():
    names = []
    for f in os.listdir(SOURCE_DIR):
        if f.endswith(".c"):
            names.append(f[:-2])
    return sorted(names)

def compile_source(name):
    src = os.path.join(SOURCE_DIR, name+".c")
    obj = os.path.join(OBJ_DIR, name+".obj")
    err = os.path.join(OBJ_DIR, name+".err")
    result = os.system("wcc "+WCC_FLAGS+" -fo="+obj+" -fr="+err+" "+src)
    if result != 0:
        sys.exit("\r\nfailed to compile "+src)

# links ALL objects into one flat binary.
#   format raw bin      : plain bytes, no exe header
#   start=main_         : emitted image begins at main() ...
#   OFFSET=0x7E00       : ... which the loader places at address 0x7E00,
#                         exactly where the bootloader jumps to
#   NODEFAULTLIBS       : freestanding, no c runtime
#   order ...           : segment placement; keeps main's segment first
def link_main(names):
    objs = ",".join(os.path.join(OBJ_DIR, n+".obj") for n in names)
    result = os.system("wlink file "+objs+" format raw bin name "+os.path.join(OBJ_DIR,"main.bin")+" option NODEFAULTLIBS,verbose,start=main_,OFFSET=0x7E00,map="+os.path.join(OBJ_DIR,"main.map")+" order clname CODE SEGMENT start_segment")
    if result != 0:
        sys.exit("\r\nfailed to link main.bin")
    check_memory_layout()

# the program runs with all segment registers = 0 and 16-bit offsets,
# so everything the linker lays out (code + dgroup) must end below
# the 64k line AND below the far data arena. wlink will NOT warn if
# objects silently alias low memory after offset wraparound, so this
# check is the only line of defense - see the arena comment in
# source/program.c for background
def check_memory_layout():
    dgroup = None
    with open(os.path.join(OBJ_DIR,"main.map")) as f:
        for line in f:
            m = re.match(r"^DGROUP\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
            if m:
                dgroup = (int(m.group(1),16), int(m.group(2),16))
    if dgroup is None:
        sys.exit("memory layout check: no DGROUP found in main.map")
    base, size = dgroup
    end = base + size
    print("\r\ndgroup: linear 0x%05X..0x%05X (%d bytes)" % (base, end-1, size))
    if end > ARENA_SEG << 4:
        sys.exit("memory layout check: image ends at 0x%X, overlapping the far arena at 0x%X" % (end, ARENA_SEG << 4))
    if end > 0x10000:
        sys.exit("memory layout check: image ends at 0x%X, past the 16-bit offset ceiling 0x10000" % end)

def process_sources():
    if not os.path.isdir(SOURCE_DIR):
        sys.exit("missing folder: "+SOURCE_DIR)
    os.makedirs(OBJ_DIR, exist_ok=True)

    names = list_sources()
    if "main" not in names:
        sys.exit("no main.c found in "+SOURCE_DIR)
    print("sources: "+", ".join(n+".c" for n in names)+"\r\n")

    for n in names:
        silent_remove(os.path.join(OBJ_DIR, n+".obj"))
    for n in names:
        compile_source(n)
    link_main(names)

# the bootloader needs to know how many sectors to load at boot time -
# but that number is only known after main.bin was built! solved by
# text substitution: %%num_sectors%% in bootloader.c is replaced with
# the real count before compiling.
def process_bootloader():
    silent_remove(os.path.join(OBJ_DIR,"bootloader.tmp.obj"))
    silent_remove(os.path.join(OBJ_DIR,"bootloader.tmp.err"))
    #modify bootloader code
    with open("bootloader.c", 'r') as f:
        bootloader_code = f.read()

    with open(os.path.join(OBJ_DIR,"main.bin"), 'rb') as f:
        main = f.read()

    # sectors needed by main.bin (rounded up), starting at sector 2
    main_sectors = len(main) // 512 +1
    print(f"\r\nmain uses {main_sectors} sectors\r\n")

    bootloader_code = bootloader_code.replace("%%num_sectors%%", str(main_sectors))
    with open("bootloader.tmp.c",'w') as f:
        f.write(bootloader_code)

    obj = os.path.join(OBJ_DIR, "bootloader.tmp.obj")
    err = os.path.join(OBJ_DIR, "bootloader.tmp.err")
    result = os.system("wcc "+WCC_FLAGS+" -fo="+obj+" -fr="+err+" bootloader.tmp.c")
    os.remove("bootloader.tmp.c")
    if result != 0:
        sys.exit("\r\nfailed to compile bootloader.c")

    # same raw binary trick as main.bin but at 0x7C00, where the bios
    # drops the first sector of a boot floppy
    binfile = os.path.join(OBJ_DIR, "bootloader.bin")
    result = os.system("wlink file "+obj+" format raw bin name "+binfile+" option NODEFAULTLIBS,verbose,start=init_,OFFSET=0x7C00")
    if result != 0:
        sys.exit("\r\nfailed to link bootloader.obj")

    with open(binfile,'rb') as f:
        bootloader = f.read()

    # a boot sector MUST end with the magic signature 55 AA in its
    # last two bytes, or the bios refuses to boot it: pad the code
    # out to byte 510 and append it
    signature = 0x55AA.to_bytes(2,'big')
    bootloader = bootloader.ljust(510, b'\0') +signature

    # pad main.bin so the final image has exactly full-floppy size:
    # 2880 sectors total, minus 1 for the boot sector
    floppy_sectors = 2*80*18

    main = main.ljust((floppy_sectors-1)*512,b'\0')

    return bootloader,main


def main_func():
    process_sources()
    bootloader, main = process_bootloader()

    # final layout: sector 1 = bootloader, sectors 2.. = main.bin
    with open('output.img', 'wb') as f:
        f.write(bootloader)
        f.write(main)

    print("\r\nsuccess! wrote output to output.img")


main_func()
