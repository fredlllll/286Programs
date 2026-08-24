"""Raw floppy drive access and the read command."""

import os
import sys
import time

from format import DISK_BYTES, SECTOR, has_data, ST_HEADSKIP, evaluate_image


def read_drive(letter, expect_bytes=DISK_BYTES):
    r"""Read a whole floppy from \\.\X:. Returns (data, error_regions)."""
    import ctypes
    from ctypes import wintypes

    k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    k32.CreateFileW.restype = wintypes.HANDLE
    k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD,
                                wintypes.HANDLE]
    k32.ReadFile.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
                             ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID]
    k32.SetFilePointer.argtypes = [wintypes.HANDLE, ctypes.c_long,
                                   ctypes.POINTER(ctypes.c_long), wintypes.DWORD]
    k32.CloseHandle.argtypes = [wintypes.HANDLE]

    invalid = wintypes.HANDLE(-1).value
    path = '\\\\.\\%s:' % letter.upper()
    handle = k32.CreateFileW(path, 0x80000000, 0x1 | 0x2, None, 3, 0, None)
    while handle == invalid:
        print('Cannot open %s (winerror %d). Is a disk inserted?' % (
            path, ctypes.get_last_error()))
        ans = input('Retry? [Y/n] ').strip().lower()
        if ans.startswith('n'):
            sys.exit(1)
        handle = k32.CreateFileW(path, 0x80000000, 0x1 | 0x2, None, 3, 0, None)

    buf = bytearray(expect_bytes)
    errors = []
    try:
        # lock volume best effort so Explorer does not interfere
        k32.DeviceIoControl(handle, 0x00090018, None, 0, None, 0, None, None)
        pos = 0
        chunk = 65536
        t0 = time.time()
        next_progress = 1024 * 1024
        while pos < expect_bytes:
            n = min(chunk, expect_bytes - pos)
            sizes = []
            s = min(chunk, n)
            while s >= SECTOR:
                sizes.append(s)
                s //= 2
            done = False
            for sz in sizes:
                rb = ctypes.create_string_buffer(sz)
                for attempt in range(2):
                    got = wintypes.DWORD(0)
                    if k32.ReadFile(handle, rb, sz, ctypes.byref(got), None) \
                            and got.value == sz:
                        buf[pos:pos + sz] = rb.raw
                        pos += sz
                        done = True
                        break
                    k32.SetFilePointer(handle, pos, None, 0)   # rewind before retry
                if done:
                    break
            if not done:
                errors.append((pos, n))
                print('\nunreadable region at byte %d (%d bytes), filled with zeros'
                      % (pos, n))
                pos += n                                       # skip whole region
            if pos >= next_progress:
                rate = int(pos / max(time.time() - t0, 0.001) / 1024)
                print('\r%d%% (%d KB/s)   ' % (100 * pos // expect_bytes, rate),
                      end='', flush=True)
                next_progress += 1024 * 1024
        print('\rdone, %d unreadable region(s)          ' % len(errors))
    finally:
        k32.CloseHandle(handle)
    return bytes(buf), errors

# ---------------------------------------------------------------- read cmd


def read_one_disk(src, dumps_dir):
    if os.path.isfile(src):
        with open(src, 'rb') as f:
            image = f.read(DISK_BYTES)
        errors = []
        print('Read %d bytes from file %s' % (len(image), src))
    elif os.name == 'nt' and len(src) == 1 and src.isalpha():
        input('Insert the floppy into drive %s: and press Enter...' % src.upper())
        image, errors = read_drive(src)
    else:
        sys.exit('--drive must be a letter (A..Z) or a path to an image file')

    ev = evaluate_image(image)
    if ev['ok']:
        err_n = sum(1 for g in ev['groups'] for _, st, _ in g['entries']
                    if not has_data(st) and st != ST_HEADSKIP)
        skip_n = sum(1 for g in ev['groups'] for _, st, _ in g['entries']
                     if st == ST_HEADSKIP)
        print('OK: %d descriptors, %d data sectors, '
              '%d read errors, %d head-skipped%s'
              % (ev['desc_total'], ev['data_total'],
                 err_n, skip_n,
                 ', %d BAD BLOCK(S)' % ev['blocks_bad'] if ev['blocks_bad'] else ''))
        if ev['blocks_bad']:
            print('WARNING: %d descriptor block(s) fail their crc - affected '
                  'data will be marked suspect.' % ev['blocks_bad'])
        if errors:
            print('NOTE: %d unreadable region(s) during raw read.' % len(errors))
        name = 'disk_%s' % time.strftime('%Y%m%d_%H%M%S')
    else:
        name = 'unknown_%s' % time.strftime('%Y%m%d_%H%M%S')
        print('PROBLEM: %s' % ev['reason'])
        ans = input('Save anyway as %s? [y/N] ' % name).strip().lower()
        if not ans.startswith('y'):
            return

    os.makedirs(dumps_dir, exist_ok=True)
    out_path = os.path.join(dumps_dir, name + '.raw')
    if os.path.exists(out_path):
        ans = input('%s exists, overwrite? [Y/n] ' % out_path).strip().lower()
        if ans.startswith('n'):
            out_path = os.path.join(dumps_dir, '%s_%d.raw' % (name, int(time.time())))
    with open(out_path, 'wb') as f:
        f.write(image)
    print('Saved %s' % out_path)


def cmd_read(args):
    while True:
        read_one_disk(args.drive, args.dumps)
        try:
            again = input('Read another disk? [Y/n] ').strip().lower()
        except EOFError:
            break
        if again.startswith('n'):
            break
