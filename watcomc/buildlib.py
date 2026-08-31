# shared build utilities for watcomc 286 freestanding programs.
# each program's build.py imports this and provides configuration.

import os
import re
import sys

ARENA_SEG = 0x1000   # default, override per program if needed

WCC_BASE = "-2 -d0 -wx -ms -s -zl"

def silent_remove(filename):
    try:
        os.remove(filename)
    except:
        pass

def make_wcc_flags(source_dir, stdlib_dir=None):
    flags = WCC_BASE + " -i=" + source_dir
    if stdlib_dir:
        flags += " -i=" + stdlib_dir
    return flags

def list_sources(source_dir):
    names = []
    for f in os.listdir(source_dir):
        if f.endswith(".c"):
            names.append(f[:-2])
    return sorted(names)

def compile_source(name, src_dir, wcc_flags, obj_dir):
    src = os.path.join(src_dir, name + ".c")
    obj = os.path.join(obj_dir, name + ".obj")
    err = os.path.join(obj_dir, name + ".err")
    result = os.system("wcc " + wcc_flags + " -fo=" + obj + " -fr=" + err + " " + src)
    if result != 0:
        sys.exit("failed to compile " + src)

def check_memory_layout(obj_dir, arena_seg):
    dgroup = None
    with open(os.path.join(obj_dir, "main.map")) as f:
        for line in f:
            m = re.match(r"^DGROUP\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
            if m:
                dgroup = (int(m.group(1), 16), int(m.group(2), 16))
    if dgroup is None:
        sys.exit("memory layout check: no DGROUP found in main.map")
    base, size = dgroup
    end = base + size
    print("\r\ndgroup: linear 0x%05X..0x%05X (%d bytes)" % (base, end - 1, size))
    if end > arena_seg << 4:
        sys.exit("memory layout check: image overlaps far arena")
    if end > 0x10000:
        sys.exit("memory layout check: image past 16-bit offset ceiling")

def link_main(names, obj_dir, wcc_flags, arena_seg):
    objs = ",".join(os.path.join(obj_dir, n + ".obj") for n in names)
    result = os.system("wlink file " + objs + " format raw bin name " +
                       os.path.join(obj_dir, "main.bin") +
                       " option NODEFAULTLIBS,verbose,start=main_,OFFSET=0x7E00,map=" +
                       os.path.join(obj_dir, "main.map") +
                       " order clname CODE SEGMENT start_segment")
    if result != 0:
        sys.exit("failed to link main.bin")
    check_memory_layout(obj_dir, arena_seg)

def process_bootloader(bootloader_path, obj_dir, wcc_flags):
    silent_remove(os.path.join(obj_dir, "bootloader.tmp.obj"))
    silent_remove(os.path.join(obj_dir, "bootloader.tmp.err"))

    with open(bootloader_path, 'r') as f:
        bootloader_code = f.read()

    with open(os.path.join(obj_dir, "main.bin"), 'rb') as f:
        main = f.read()

    main_sectors = (len(main) + 511) // 512
    print("\r\nmain uses %d sectors\r\n" % main_sectors)

    bootloader_code = bootloader_code.replace("%%num_sectors%%", str(main_sectors))
    with open("bootloader.tmp.c", 'w') as f:
        f.write(bootloader_code)

    obj = os.path.join(obj_dir, "bootloader.tmp.obj")
    err = os.path.join(obj_dir, "bootloader.tmp.err")
    result = os.system("wcc " + wcc_flags + " -fo=" + obj + " -fr=" + err + " bootloader.tmp.c")
    os.remove("bootloader.tmp.c")
    if result != 0:
        sys.exit("failed to compile bootloader")

    binfile = os.path.join(obj_dir, "bootloader.bin")
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

def write_image(output_path, bootloader, main):
    with open(output_path, 'wb') as f:
        f.write(bootloader)
        f.write(main)
    print("\r\nsuccess! wrote " + output_path)

def build_program(config):
    """build a freestanding 286 program from a config dict.

    config keys:
      source_dir    - directory containing .c files (or "." for single-file)
      sources       - optional: explicit list of source names (without .c)
                     if omitted, auto-detected from source_dir
      source_file   - optional: single source file name (without .c)
                     alternative to sources for single-file programs
      stdlib_dir    - optional: path to stdlib (for programs using shared print)
      bootloader    - path to bootloader.c
      output        - output image path
      arena_seg     - optional: far arena segment (default 0x1000)
      extra_obj     - optional: list of pre-existing .obj files to link
    """
    source_dir = config["source_dir"]
    bootloader_path = config["bootloader"]
    output_path = config.get("output", "output.img")
    arena_seg = config.get("arena_seg", ARENA_SEG)
    stdlib_dir = config.get("stdlib_dir")
    obj_dir = "obj"

    wcc_flags = make_wcc_flags(source_dir, stdlib_dir)
    os.makedirs(obj_dir, exist_ok=True)

    # determine which sources to compile
    if "source_file" in config:
        names = [config["source_file"]]
    elif "sources" in config:
        names = config["sources"]
    else:
        names = list_sources(source_dir)
        if "main" not in names:
            sys.exit("no main.c found in " + source_dir)

    print("sources: " + ", ".join(n + ".c" for n in names) + "\r\n")

    # compile stdlib if needed
    if stdlib_dir:
        stdlib_wcc = make_wcc_flags(stdlib_dir)
        silent_remove(os.path.join(obj_dir, "print.obj"))
        compile_source("print", stdlib_dir, stdlib_wcc, obj_dir)

    # compile program sources
    for n in names:
        silent_remove(os.path.join(obj_dir, n + ".obj"))
    for n in names:
        compile_source(n, source_dir, wcc_flags, obj_dir)

    # link
    all_names = names + (["print"] if stdlib_dir else [])
    link_main(all_names, obj_dir, wcc_flags, arena_seg)

    # build bootloader and final image
    bootloader, main = process_bootloader(bootloader_path, obj_dir, wcc_flags)
    write_image(output_path, bootloader, main)
