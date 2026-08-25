"""Raw floppy drive access and the read command."""

import argparse
import os
import struct
import sys
import time

from structures import has_data, ST_HEADSKIP
from format import (DISK_BYTES, SECTOR, DESC_PER_BLOCK, ENTRY_SIZE,
                    crc16, evaluate_image, iter_blocks)
from windrive import WinDrive

RETRY_SECTORS: int = 4


def _retry_bad_crc_blocks(drv: WinDrive, buf: bytearray,
                          img: bytes) -> int:
    """Re-read sectors whose descriptor block CRC failed. Returns retry count."""
    retries = 0
    pos = 0
    while pos + SECTOR <= DISK_BYTES:
        block = img[pos:pos + SECTOR]
        if not any(block):
            break
        count = block[0]
        if count > DESC_PER_BLOCK:
            break
        crc_ok = struct.unpack_from('<H', block, 506)[0] \
            == crc16(bytes(block[0:506]))
        if not crc_ok:
            for _attempt in range(RETRY_SECTORS):
                retries += 1
                data = drv.read_sector(pos)
                if data is not None:
                    buf[pos:pos + SECTOR] = data
                    block_new = bytes(buf[pos:pos + SECTOR])
                    crc_ok_new = struct.unpack_from('<H', block_new, 506)[0] \
                        == crc16(bytes(block_new[0:506]))
                    if crc_ok_new:
                        img = bytes(buf)
                        block = block_new
                        count = block_new[0]
                        break
        ngood = sum(1 for i in range(count)
                    if has_data(block[1 + i * ENTRY_SIZE + 3]))
        pos += SECTOR * (1 + ngood)
    return retries


def read_drive(letter: str, expect_bytes: int = DISK_BYTES) -> tuple[bytes, list[tuple[int, int]]]:
    r"""Read a whole floppy from \\.\X:. Returns (data, error_regions).

    After the initial bulk read, every descriptor block whose CRC
    fails is re-read sector by sector (up to RETRY_SECTORS times)
    to recover from soft read errors."""
    with WinDrive(letter) as drv:
        buf = bytearray(expect_bytes)
        errors: list[tuple[int, int]] = []
        pos = 0
        chunk = 65536
        t0 = time.time()
        next_progress = 0
        while pos < expect_bytes:
            n = min(chunk, expect_bytes - pos)
            sizes: list[int] = []
            s = min(chunk, n)
            while s >= SECTOR:
                sizes.append(s)
                s //= 2
            done = False
            for sz in sizes:
                data = drv.read(pos, sz)
                if data is not None:
                    buf[pos:pos + sz] = data
                    pos += sz
                    done = True
                    break
            if not done:
                errors.append((pos, n))
                print('\nunreadable region at byte %d (%d bytes), filled with zeros'
                      % (pos, n))
                pos += n
            if pos >= next_progress:
                rate = int(pos / max(time.time() - t0, 0.001) / 1024)
                print('\r%d%% (%d KB/s)   ' % (100 * pos // expect_bytes, rate),
                      end='', flush=True)
                next_progress += 32 * 1024
        print('\rbulk read done, checking CRCs...              ', end='',
              flush=True)

        retries = _retry_bad_crc_blocks(drv, buf, bytes(buf))
        bad_crcs = sum(1 for b in iter_blocks(bytes(buf)) if not b.crc_ok)
        if retries:
            print('\rretried %d sector(s), %d CRC error(s) remaining   '
                  % (retries, bad_crcs), flush=True)
        else:
            print('\rno CRC errors - disk is clean                     ',
                  flush=True)
    return bytes(buf), errors


# ---------------------------------------------------------------- read cmd


def read_one_disk(src: str, dumps_dir: str) -> None:
    if os.path.isfile(src):
        with open(src, 'rb') as f:
            image = f.read(DISK_BYTES)
        errors: list[tuple[int, int]] = []
        print('Read %d bytes from file %s' % (len(image), src))
    elif os.name == 'nt' and len(src) == 1 and src.isalpha():
        input('Insert the floppy into drive %s: and press Enter...' % src.upper())
        image, errors = read_drive(src)
    else:
        sys.exit('--drive must be a letter (A..Z) or a path to an image file')

    ev = evaluate_image(image)
    if ev.ok:
        err_n = sum(1 for b in ev.blocks for e in b.entries
                    if not has_data(e.status) and e.status != ST_HEADSKIP)
        skip_n = sum(1 for b in ev.blocks for e in b.entries
                     if e.status == ST_HEADSKIP)
        print('OK: %d descriptors, %d data sectors, '
              '%d read errors, %d head-skipped%s'
              % (ev.desc_total, ev.data_total,
                 err_n, skip_n,
                 ', %d BAD BLOCK(S)' % ev.blocks_bad if ev.blocks_bad else ''))
        if ev.blocks_bad:
            print('WARNING: %d descriptor block(s) fail their crc - affected '
                  'data will be marked suspect.' % ev.blocks_bad)
        if errors:
            print('NOTE: %d unreadable region(s) during raw read.' % len(errors))
        name = 'disk_%s' % time.strftime('%Y%m%d_%H%M%S')
    else:
        name = 'unknown_%s' % time.strftime('%Y%m%d_%H%M%S')
        print('PROBLEM: %s' % ev.reason)
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


def cmd_read(args: argparse.Namespace) -> None:
    while True:
        read_one_disk(args.drive, args.dumps)
        try:
            again = input('Read another disk? [Y/n] ').strip().lower()
        except EOFError:
            break
        if again.startswith('n'):
            break
