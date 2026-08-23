import os
import sys

SOURCE_DIR = "source"
OBJ_DIR = "obj"

WCC_FLAGS = "-2 -d0 -wx -ms -s -zl -i="+SOURCE_DIR

def silent_remove(filename):
    try:
        os.remove(filename)
    except:
        pass

def list_sources():
    names = []
    for f in os.listdir(SOURCE_DIR):
        if f.endswith(".c"):
            names.append(f[:-2])
    return sorted(names)

def compile_source(name):
    src = os.path.join(SOURCE_DIR, name+".c")
    obj = os.path.join(OBJ_DIR, name+".obj")
    result = os.system("wcc "+WCC_FLAGS+" -fo="+obj+" "+src)
    if result != 0:
        sys.exit("\r\nfailed to compile "+src)

def link_main(names):
    objs = ",".join(os.path.join(OBJ_DIR, n+".obj") for n in names)
    result = os.system("wlink file "+objs+" format raw bin name main.bin option NODEFAULTLIBS,verbose,start=main_,OFFSET=0x7E00 order clname CODE SEGMENT start_segment")
    if result != 0:
        sys.exit("\r\nfailed to link main.bin")

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

def process_bootloader():
    silent_remove("bootloader.tmp.obj")
    #modify bootloader code
    with open("bootloader.c", 'r') as f:
        bootloader_code = f.read()

    with open("main.bin", 'rb') as f:
        main = f.read()
    main_sectors = len(main) // 512 +1
    print(f"\r\nmain uses {main_sectors} sectors\r\n")

    bootloader_code = bootloader_code.replace("%%num_sectors%%", str(main_sectors))
    with open("bootloader.tmp.c",'w') as f:
        f.write(bootloader_code)

    obj = os.path.join(OBJ_DIR, "bootloader.tmp.obj")
    result = os.system("wcc "+WCC_FLAGS+" -fo="+obj+" bootloader.tmp.c")
    os.remove("bootloader.tmp.c")
    if result != 0:
        sys.exit("\r\nfailed to compile bootloader.c")
    result = os.system("wlink file "+obj+" format raw bin name bootloader.bin option NODEFAULTLIBS,verbose,start=init_,OFFSET=0x7C00")
    if result != 0:
        sys.exit("\r\nfailed to link bootloader.obj")

    with open('bootloader.bin','rb') as f:
        bootloader = f.read()

    signature = 0x55AA.to_bytes(2,'big')
    bootloader = bootloader.ljust(510, b'\0') +signature

    floppy_sectors = 2*80*18

    main = main.ljust((floppy_sectors-1)*512,b'\0')

    return bootloader,main


def main_func():
    process_sources()
    bootloader, main = process_bootloader()

    with open('output.img', 'wb') as f:
        f.write(bootloader)
        f.write(main)

    print("\r\nsuccess! wrote output to output.img")


main_func()
