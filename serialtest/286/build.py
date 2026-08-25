import os
import re
import sys

SOURCE = "serial_test"
OBJ_DIR = "obj"
BOOTLOADER = "../../watcomc/bootloader.c"

ARENA_SEG = 0x1000

WCC_FLAGS = "-2 -d0 -wx -ms -s -zl -i=. -i=../../watcomc/source"

def silent_remove(filename):
    try:
        os.remove(filename)
    except:
        pass

def compile_source(name):
    src = name + ".c"
    obj = os.path.join(OBJ_DIR, name + ".obj")
    err = os.path.join(OBJ_DIR, name + ".err")
    result = os.system("wcc " + WCC_FLAGS + " -fo=" + obj + " -fr=" + err + " " + src)
    if result != 0:
        sys.exit("failed to compile " + src)

def link_main():
    obj = os.path.join(OBJ_DIR, SOURCE + ".obj")
    result = os.system("wlink file " + obj + " format raw bin name " +
                       os.path.join(OBJ_DIR, "main.bin") +
                       " option NODEFAULTLIBS,verbose,start=main_,OFFSET=0x7E00,map=" +
                       os.path.join(OBJ_DIR, "main.map"))
    if result != 0:
        sys.exit("failed to link main.bin")
    check_memory_layout()

def check_memory_layout():
    dgroup = None
    with open(os.path.join(OBJ_DIR, "main.map")) as f:
        for line in f:
            m = re.match(r"^DGROUP\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
            if m:
                dgroup = (int(m.group(1), 16), int(m.group(2), 16))
    if dgroup is None:
        sys.exit("memory layout check: no DGROUP found in main.map")
    base, size = dgroup
    end = base + size
    print("\r\ndgroup: linear 0x%05X..0x%05X (%d bytes)" % (base, end - 1, size))
    if end > ARENA_SEG << 4:
        sys.exit("memory layout check: image overlaps far arena")
    if end > 0x10000:
        sys.exit("memory layout check: image past 16-bit offset ceiling")

def process_bootloader():
    silent_remove(os.path.join(OBJ_DIR, "bootloader.tmp.obj"))
    silent_remove(os.path.join(OBJ_DIR, "bootloader.tmp.err"))

    with open(BOOTLOADER, 'r') as f:
        bootloader_code = f.read()

    with open(os.path.join(OBJ_DIR, "main.bin"), 'rb') as f:
        main = f.read()

    main_sectors = (len(main) + 511) // 512
    print("\r\nmain uses %d sectors\r\n" % main_sectors)

    bootloader_code = bootloader_code.replace("%%num_sectors%%", str(main_sectors))
    with open("bootloader.tmp.c", 'w') as f:
        f.write(bootloader_code)

    obj = os.path.join(OBJ_DIR, "bootloader.tmp.obj")
    err = os.path.join(OBJ_DIR, "bootloader.tmp.err")
    result = os.system("wcc " + WCC_FLAGS + " -fo=" + obj + " -fr=" + err + " bootloader.tmp.c")
    os.remove("bootloader.tmp.c")
    if result != 0:
        sys.exit("failed to compile bootloader")

    binfile = os.path.join(OBJ_DIR, "bootloader.bin")
    result = os.system("wlink file " + obj + " format raw bin name " + binfile +
                       " option NODEFAULTLIBS,verbose,start=init_,OFFSET=0x7C00")
    if result != 0:
        sys.exit("failed to link bootloader")

    with open(binfile, 'rb') as f:
        bootloader = f.read()

    signature = 0x55AA.to_bytes(2, 'big')
    bootloader = bootloader.ljust(510, b'\0') + signature

    main = main.ljust(main_sectors * 512, b'\0')

    return bootloader, main

def main_func():
    os.makedirs(OBJ_DIR, exist_ok=True)

    silent_remove(os.path.join(OBJ_DIR, SOURCE + ".obj"))
    compile_source(SOURCE)
    link_main()
    bootloader, main = process_bootloader()

    with open('output.img', 'wb') as f:
        f.write(bootloader)
        f.write(main)

    print("\r\nsuccess! wrote output.img")

main_func()
