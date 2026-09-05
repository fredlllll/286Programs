# shared build utilities for watcomc 286 freestanding programs.
# each program's build.py imports this and provides configuration.

import os
import re
import sys

ARENA_SEG = 0x1000   # default, override per program if needed

# default load segment. 0 keeps the historic behaviour (program loaded at
# 0000:7E00). a non-zero value relocates the whole image to run inside a
# separate 64kb segment (e.g. 0x1000 -> physical 0x10000) so bios int 13h
# / dma traffic poking at low memory can't scribble over our stack/globals.
LOAD_SEG = 0

# offset into the load segment where main.bin's code begins.
# for segment 0 it must sit right behind the bootloader (0000:7E00, just
# after 0000:7C00's 512 byte bootstrap). a dedicated load segment is fully
# ours from offset 0, so there it can (and should) be 0x0000.
LOAD_OFF = 0x7E00

WCC_BASE = "-2 -d0 -wx -ms -s -zl"

def silent_remove(filename):
    try:
        os.remove(filename)
    except:
        pass

def make_wcc_flags(source_dir, stdlib_dir=None):
    flags = WCC_BASE + ' -i="' + source_dir + '"'
    if stdlib_dir:
        flags += ' -i="' + stdlib_dir + '"'
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
    result = os.system('wcc ' + wcc_flags + ' -fo="' + obj + '" -fr="' + err + '" "' + src + '"')
    if result != 0:
        sys.exit("failed to compile " + src)

def check_memory_layout(obj_dir, arena_seg, load_seg):
    dgroup = None
    with open(os.path.join(obj_dir, "main.map")) as f:
        for line in f:
            m = re.match(r"^DGROUP\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})", line)
            if m:
                dgroup = (int(m.group(1), 16), int(m.group(2), 16))
    if dgroup is None:
        sys.exit("memory layout check: no DGROUP found in main.map")
    base, size = dgroup
    off_end = base + size
    # the map reports addresses as if loaded at segment 0 (raw bin format
    # only supports a single OFFSET base). relocate by the real load
    # segment: physical = load_seg*16 + map_linear.
    phys_base = (load_seg << 4) + base
    phys_end = phys_base + size
    print("\r\ndgroup: seg %04X offset 0x%04X..0x%04X -> phys 0x%05X..0x%05X (%d bytes)"
          % (load_seg, base, off_end - 1, phys_base, phys_end - 1, size))
    # near addressing wraps within the 64kb load segment:
    if off_end > 0x10000:
        sys.exit("memory layout check: image past 16-bit offset ceiling")
    # and the relocated image must stay below the 1MB barrier.
    # (the old arena_seg check kept the image below the far/dma arena at
    # 64kb; relocating into its own higher segment intentionally moves
    # past that, so only the 1MB wall matters now.)
    if phys_end > 0x100000:
        sys.exit("memory layout check: image overlaps 1MB barrier")

def link_main(names, obj_dir, wcc_flags, arena_seg, load_seg, load_off):
    main_bin = os.path.join(obj_dir, "main.bin")
    main_map = os.path.join(obj_dir, "main.map")
    rsp = os.path.join(obj_dir, "main.lnk")
    with open(rsp, 'w') as f:
        for n in names:
            f.write("FILE " + os.path.join(obj_dir, n + ".obj") + "\n")
        f.write("FORMAT raw bin\n")
        f.write("NAME " + main_bin + "\n")
        f.write("OPTION NODEFAULTLIBS,verbose,start=main_,OFFSET=0x%04X,map=" % load_off + main_map + "\n")
        f.write("ORDER clname CODE SEGMENT start_segment\n")
    result = os.system("wlink @" + rsp)
    if result != 0:
        sys.exit("failed to link main.bin")
    check_memory_layout(obj_dir, arena_seg, load_seg)

def process_bootloader(bootloader_path, obj_dir, wcc_flags, load_seg, load_off):
    silent_remove(os.path.join(obj_dir, "bootloader.tmp.obj"))
    silent_remove(os.path.join(obj_dir, "bootloader.tmp.err"))

    with open(bootloader_path, 'r') as f:
        bootloader_code = f.read()

    with open(os.path.join(obj_dir, "main.bin"), 'rb') as f:
        main = f.read()

    main_sectors = (len(main) + 511) // 512
    print("\r\nmain uses %d sectors\r\n" % main_sectors)

    bootloader_code = bootloader_code.replace("%%num_sectors%%", str(main_sectors))
    bootloader_code = bootloader_code.replace("%%load_seg%%", "0%04Xh" % load_seg)
    bootloader_code = bootloader_code.replace("%%load_off%%", "0%04Xh" % load_off)
    with open("bootloader.tmp.c", 'w') as f:
        f.write(bootloader_code)

    obj = os.path.join(obj_dir, "bootloader.tmp.obj")
    err = os.path.join(obj_dir, "bootloader.tmp.err")
    result = os.system('wcc ' + wcc_flags + ' -fo="' + obj + '" -fr="' + err + '" bootloader.tmp.c')
    os.remove("bootloader.tmp.c")
    if result != 0:
        sys.exit("failed to compile bootloader")

    binfile = os.path.join(obj_dir, "bootloader.bin")
    rsp = os.path.join(obj_dir, "bootloader.lnk")
    with open(rsp, 'w') as f:
        f.write("FILE " + obj + "\n")
        f.write("FORMAT raw bin\n")
        f.write("NAME " + binfile + "\n")
        f.write("OPTION NODEFAULTLIBS,verbose,start=init_,OFFSET=0x7C00\n")
    result = os.system("wlink @" + rsp)
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
      load_seg      - optional: segment the image runs in (default 0)
      load_off      - optional: offset into the load segment where the
                      image starts (default 0x7E00; use 0 for a dedicated
                      load segment)
      extra_obj     - optional: list of pre-existing .obj files to link
    """
    source_dir = config["source_dir"]
    bootloader_path = config["bootloader"]
    output_path = config.get("output", "output.img")
    arena_seg = config.get("arena_seg", ARENA_SEG)
    load_seg = config.get("load_seg", LOAD_SEG)
    load_off = config.get("load_off", LOAD_OFF)
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
    stdlib_names = []
    if stdlib_dir:
        stdlib_wcc = make_wcc_flags(stdlib_dir)
        stdlib_names = list_sources(stdlib_dir)
        for n in stdlib_names:
            silent_remove(os.path.join(obj_dir, n + ".obj"))
        for n in stdlib_names:
            compile_source(n, stdlib_dir, stdlib_wcc, obj_dir)

    # compile program sources
    for n in names:
        silent_remove(os.path.join(obj_dir, n + ".obj"))
    for n in names:
        compile_source(n, source_dir, wcc_flags, obj_dir)

    # link
    all_names = names + stdlib_names
    link_main(all_names, obj_dir, wcc_flags, arena_seg, load_seg, load_off)

    # build bootloader and final image
    bootloader, main = process_bootloader(bootloader_path, obj_dir, wcc_flags, load_seg, load_off)
    write_image(output_path, bootloader, main)
