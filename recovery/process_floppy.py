#!/usr/bin/env python3
"""Windows-side companion to the Tandon 286 "HDD saver" floppy dumper.

Subcommands:
  read      copy one physical floppy (or an image file) into dumps/
  assemble  stitch all dumps into hdd.img and write hdd_report.txt
  selftest  synthetic round-trip test of the assembler logic

Typical workflow:
  python process_floppy.py read --drive A --dumps dumps      (repeat per disk)
  python process_floppy.py assemble --dumps dumps --out hdd.img

Floppy layout written by the 286 tool (v2):
  LBA 0        header sector ('HDSV', version 2, see watcomc/main.c):
               magic, version, flags, disk index, start LBA, sector count,
               payload crc16, bad hdd list, total sectors, bad floppy
               offset list, header crc16
  LBA 1..9     reserved
  LBA 10..2879 payload = consecutive hdd sectors, starting at header startLBA

Unreadable spots are filled with '!BAD-SECTOR!' by the 286 tool itself.
This script treats those bytes as intentional markers, never as data.

No third party dependencies, stdlib only.
"""

import argparse
import glob
import hashlib
import os
import struct
import sys
import time

SECTOR = 512
HEADER_LBAS = 10
DISK_CAPACITY = 2870                     # payload sectors per 1.44MB floppy
DISK_BYTES = 2880 * SECTOR
MAGIC = b'HDSV'
VERSION = 2
FLAG_FINAL = 0x01
MAX_BAD_HDD = 48
MAX_BAD_FLP = 30

BADFILL = ('!BAD-SECTOR!'.encode('ascii') * (SECTOR // 12 + 1))[:SECTOR]

# ---------------------------------------------------------------- crc16

_crc_table = []
for _i in range(256):
    _c = _i << 8
    for _ in range(8):
        _c = (((_c << 1) ^ 0x1021) & 0xFFFF) if (_c & 0x8000) else ((_c << 1) & 0xFFFF)
    _crc_table.append(_c)


def crc16(data, crc=0xFFFF):
    """CRC-16/CCITT-FALSE, identical to the 286 implementation."""
    tb = _crc_table
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ tb[((crc >> 8) ^ b) & 0xFF]
    return crc

# ---------------------------------------------------------------- header


def parse_header(sector0):
    """Parse the header sector. Returns dict or None if magic mismatch."""
    if len(sector0) < SECTOR or sector0[0:4] != MAGIC:
        return None
    version = sector0[4]
    flags = sector0[5]
    disk_index, start_lba = struct.unpack_from('<II', sector0, 6)
    count, pcrc, bad_hdd_n = struct.unpack_from('<HHH', sector0, 14)
    total_sectors = struct.unpack_from('<I', sector0, 20)[0]
    bad_flp_n = struct.unpack_from('<H', sector0, 254)[0]
    return {
        'version': version,
        'flags': flags,
        'disk_index': disk_index,
        'start_lba': start_lba,
        'total_sectors': total_sectors,
        'count': count,
        'payload_crc': pcrc,
        'bad_hdd_n': bad_hdd_n,
        'bad_flp_n': bad_flp_n,
        'bad_hdd': list(struct.unpack_from('<%dI' % min(bad_hdd_n, MAX_BAD_HDD),
                                           sector0, 24)) if bad_hdd_n else [],
        'bad_flp': list(struct.unpack_from('<%dH' % min(bad_flp_n, MAX_BAD_FLP),
                                           sector0, 256)) if bad_flp_n else [],
        'header_crc_ok':
            struct.unpack_from('<H', sector0, 500)[0] == crc16(bytes(sector0[0:500])),
    }


def evaluate_image(image):
    """Full validation of one 1.44MB dump.

    Returns dict:
      ok          header sane and geometry plausible (safe to place)
      reason      why not ok, or why the payload crc failed
      info        parsed header dict (None when magic missing)
      crc_ok      True/False, None when not ok
      payload     post-substitution payload bytes (when ok)
      undeclared_badfill  offsets equal to BADFILL but not listed in header
    """
    res = {'ok': False, 'reason': '', 'info': None, 'crc_ok': None,
           'payload': None, 'undeclared_badfill': []}
    if len(image) != DISK_BYTES:
        res['reason'] = 'unexpected size %d (want %d)' % (len(image), DISK_BYTES)
        return res
    hdr = parse_header(image[:SECTOR])
    res['info'] = hdr
    if hdr is None:
        res['reason'] = 'no HDSV header (blank or foreign disk?)'
        return res
    if not hdr['header_crc_ok']:
        res['reason'] = 'header CRC mismatch'
        return res
    if hdr['version'] != VERSION:
        res['reason'] = 'unsupported version %d' % hdr['version']
        return res
    if hdr['count'] == 0 or hdr['count'] > DISK_CAPACITY:
        res['reason'] = 'implausible sector count %d' % hdr['count']
        return res
    if hdr['start_lba'] + hdr['count'] > hdr['total_sectors']:
        res['reason'] = 'range %d..%d exceeds drive size %d' % (
            hdr['start_lba'], hdr['start_lba'] + hdr['count'] - 1,
            hdr['total_sectors'])
        return res

    payload = bytearray(
        image[HEADER_LBAS * SECTOR:(HEADER_LBAS + hdr['count']) * SECTOR])
    for off in hdr['bad_flp']:
        if off < hdr['count']:
            payload[off * SECTOR:(off + 1) * SECTOR] = BADFILL
    calc = crc16(payload)
    res['crc_ok'] = (calc == hdr['payload_crc'])
    if not res['crc_ok']:
        res['reason'] = 'payload CRC mismatch (stored %04X, got %04X)' % (
            hdr['payload_crc'], calc)

    declared = set(hdr['bad_flp'])
    declared.update(b - hdr['start_lba'] for b in hdr['bad_hdd']
                    if 0 <= b - hdr['start_lba'] < hdr['count'])
    res['undeclared_badfill'] = [
        i for i in range(hdr['count'])
        if i not in declared and payload[i * SECTOR:(i + 1) * SECTOR] == BADFILL]
    res['payload'] = bytes(payload)
    res['ok'] = True
    return res

# ------------------------------------------------------- raw drive access


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
        i = ev['info']
        name = 'disk_%03d' % i['disk_index']
        print('Header OK : disk %d, LBA %d..%d, count %d%s, bad hdd %d, bad flp %d'
              % (i['disk_index'], i['start_lba'], i['start_lba'] + i['count'] - 1,
                 i['count'],
                 ', FINAL' if i['flags'] & FLAG_FINAL else '',
                 i['bad_hdd_n'], i['bad_flp_n']))
        if ev['crc_ok']:
            print('Payload CRC OK - disk is good.')
        else:
            print('WARNING: %s - re-read this disk.' % ev['reason'])
        for off in ev['undeclared_badfill']:
            print('WARNING: BADFILL at undeclared offset %d - re-read advised.' % off)
        if errors:
            print('NOTE: %d unreadable region(s) during raw read.' % len(errors))
    else:
        name = 'unknown_%d' % int(time.time())
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

# ------------------------------------------------------------ assemble cmd


def load_dumps(dumps_dir):
    candidates = {}       # disk_index -> list of records
    rejected = []         # (filename, reason)
    totals = {}           # claimed drive size -> count of disks agreeing
    for fn in sorted(glob.glob(os.path.join(dumps_dir, '*.raw'))):
        with open(fn, 'rb') as f:
            image = f.read()
        ev = evaluate_image(image)
        base = os.path.basename(fn)
        if not ev['ok']:
            rejected.append((base, ev['reason']))
            continue
        i = ev['info']
        totals[i['total_sectors']] = totals.get(i['total_sectors'], 0) + 1
        candidates.setdefault(i['disk_index'], []).append({'file': base, 'ev': ev})
    return candidates, rejected, totals


def run_assembly(dumps_dir, out_path, verbose=True):
    """Stitch all dumps into one image. Returns report dict."""
    candidates, rejected, totals = load_dumps(dumps_dir)
    if not candidates:
        sys.exit('No valid HDSV disks found in %s' % dumps_dir)
    if len(totals) > 1:
        print('WARNING: disks disagree about drive size: %s' % totals)
    total = max(totals.items(), key=lambda kv: kv[1])[0]

    # best variant per disk index: verified crc first, then filename
    chosen, skipped_dupes = [], []
    for idx in sorted(candidates):
        recs = sorted(candidates[idx],
                      key=lambda r: (not r['ev']['crc_ok'], r['file']))
        chosen.append(recs[0])
        skipped_dupes.extend(r['file'] for r in recs[1:])
    chosen.sort(key=lambda r: r['ev']['info']['start_lba'])

    image_out = bytearray(total * SECTOR)
    covered = bytearray(total)
    suspect = bytearray(total)
    overlaps = []
    declared_bad_hdd = set()
    declared_bad_spots = set()
    final_seen = False

    for rec in chosen:
        i = rec['ev']['info']
        s, c = i['start_lba'], i['count']
        pl = rec['ev']['payload']
        if i['flags'] & FLAG_FINAL:
            final_seen = True
        ov = 0
        for j in range(c):
            lba = s + j
            if lba >= total:
                break
            if covered[lba]:
                ov += 1
                continue
            covered[lba] = 1
            if not rec['ev']['crc_ok']:
                suspect[lba] = 1
            image_out[lba * SECTOR:(lba + 1) * SECTOR] = pl[j * SECTOR:(j + 1) * SECTOR]
        if ov:
            overlaps.append((i['disk_index'], s, c, ov))
        # enforce the sectors the 286 tool itself declared untrustworthy
        for blba in i['bad_hdd']:
            if blba < total and covered[blba]:
                image_out[blba * SECTOR:(blba + 1) * SECTOR] = BADFILL
                declared_bad_hdd.add(blba)
        for off in i['bad_flp']:
            lba = s + off
            if off < c and lba < total and covered[lba]:
                image_out[lba * SECTOR:(lba + 1) * SECTOR] = BADFILL
                declared_bad_spots.add(lba)

    gaps = []
    run_start = None
    for lba in range(total):
        if not covered[lba] and run_start is None:
            run_start = lba
        elif covered[lba] and run_start is not None:
            gaps.append((run_start, lba - 1))
            run_start = None
    if run_start is not None:
        gaps.append((run_start, total - 1))

    suspect_count = sum(suspect)
    coverage = sum(covered)
    complete = (not gaps) and final_seen

    with open(out_path, 'wb') as f:
        f.write(image_out)

    report = {
        'gaps': gaps,
        'chosen_files': [r['file'] for r in chosen],
        'final_seen': final_seen,
        'declared_bad_hdd': declared_bad_hdd,
        'declared_bad_spots': declared_bad_spots,
        'suspect_count': suspect_count,
        'coverage': coverage,
        'overlaps': overlaps,
        'complete': complete,
        'total': total,
        'rejected': rejected,
        'skipped_dupes': skipped_dupes,
        'chosen_records': chosen,
    }
    report_path = os.path.splitext(out_path)[0] + '_report.txt'
    write_report(report_path, report, dumps_dir)
    if verbose:
        print_assembly_summary(report, out_path, report_path)
    return report


def write_report(path, rep, dumps_dir):
    lines = []
    ap = lines.append
    ap('HDD recovery assembly report')
    ap('generated : %s' % time.strftime('%Y-%m-%d %H:%M:%S'))
    ap('source    : %s' % os.path.abspath(dumps_dir))
    ap('output    : %s' % os.path.abspath(path).replace('_report', ''))
    ap('drive size: %d sectors (%.2f MB)' % (rep['total'], rep['total'] * SECTOR / 1048576.0))
    ap('')
    ap('=== disks used ===')
    ap('%-5s %-24s %-10s %-6s %-4s %-7s %-7s' %
       ('idx', 'file', 'lba range', 'count', 'crc', 'badHDD', 'badFLP'))
    for rec in rep['chosen_records']:
        i = rec['ev']['info']
        ap('%-5d %-24s %6d-%-5d %-6d %-4s %-7d %-7d%s' % (
            i['disk_index'], rec['file'], i['start_lba'],
            i['start_lba'] + i['count'] - 1, i['count'],
            'OK' if rec['ev']['crc_ok'] else 'FAIL',
            i['bad_hdd_n'], i['bad_flp_n'],
            ' FINAL' if i['flags'] & FLAG_FINAL else ''))
    if rep['skipped_dupes']:
        ap('')
        ap('duplicate variants ignored:')
        for fn in rep['skipped_dupes']:
            ap('  %s' % fn)
    if rep['rejected']:
        ap('')
        ap('files rejected (no usable header):')
        for fn, why in rep['rejected']:
            ap('  %-24s %s' % (fn, why))
    if rep['overlaps']:
        ap('')
        ap('overlapping ranges (first disk placed wins):')
        for idx, s, c, ov in rep['overlaps']:
            ap('  disk %d at %d..%d: %d sectors already covered' % (idx, s, s + c - 1, ov))
    ap('')
    ap('=== coverage ===')
    ap('sectors covered : %d / %d (%.2f%%)' % (
        rep['coverage'], rep['total'], 100.0 * rep['coverage'] / rep['total']))
    ap('suspect sectors : %d (from disks with failed payload crc)' % rep['suspect_count'])
    ap('declared bad hdd: %d' % len(rep['declared_bad_hdd']))
    ap('declared bad flp: %d (mapped to image lbas below)' % len(rep['declared_bad_spots']))
    if rep['gaps']:
        ap('')
        ap('missing ranges:')
        for g0, g1 in rep['gaps']:
            ap('  lba %d..%d  (%d sectors, %.2f MB)' % (
                g0, g1, g1 - g0 + 1, (g1 - g0 + 1) * SECTOR / 1048576.0))
            ap('    to fill: boot dumper with resume LBA=%d, new disk number,' % g0)
            ap('    let it run to the end; overlapping data is harmless.')
    else:
        ap('missing ranges: none')
    bad_all = sorted(rep['declared_bad_hdd'] | rep['declared_bad_spots'])
    if bad_all:
        ap('')
        ap('image lbas containing !BAD-SECTOR! filler:')
        line = ' '
        for b in bad_all:
            line += ' %d' % b
            if len(line) > 74:
                ap(line)
                line = ' '
        if line.strip():
            ap(line)
    ap('')
    ap('status: %s' % ('COMPLETE - all sectors accounted for'
                      if rep['complete'] else
                      'INCOMPLETE - missing ranges or final flag not seen'))
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def print_assembly_summary(rep, out_path, report_path):
    print('Disks used   : %d (%d duplicates ignored, %d files rejected)'
          % (len(rep['chosen_files']), len(rep['skipped_dupes']), len(rep['rejected'])))
    print('Coverage     : %d / %d sectors (%.2f%%)'
          % (rep['coverage'], rep['total'], 100.0 * rep['coverage'] / rep['total']))
    print('Suspect      : %d sectors' % rep['suspect_count'])
    print('Declared bad : %d hdd lbas, %d floppy spots'
          % (len(rep['declared_bad_hdd']), len(rep['declared_bad_spots'])))
    if rep['gaps']:
        print('Missing ranges:')
        for g0, g1 in rep['gaps']:
            print('  %d..%d (%d sectors)' % (g0, g1, g1 - g0 + 1))
    print('Wrote %s' % out_path)
    print('Wrote %s' % report_path)
    print('Status: %s' % ('COMPLETE' if rep['complete'] else 'INCOMPLETE'))

# ---------------------------------------------------------------- selftest


def fake_sector(lba):
    return bytes(((lba * 7 + j * 13 + (j >> 4)) & 0xFF) for j in range(SECTOR))


def make_synthetic_disk(index, start_lba, total, bad_hdd=(), bad_flp=(),
                        final=False, corrupt_byte=None):
    """Build a 1.44MB image exactly like the 286 tool would write it."""
    count = min(DISK_CAPACITY, total - start_lba)
    stream = bytearray()
    for i in range(count):
        lba = start_lba + i
        if lba in bad_hdd or i in bad_flp:
            stream += BADFILL
        else:
            stream += fake_sector(lba)
    stored_crc = crc16(bytes(stream))
    if corrupt_byte is not None:
        # simulate media rot AFTER the fact: header still claims old crc
        stream[corrupt_byte] ^= 0xFF
    hdr = bytearray(SECTOR)
    hdr[0:4] = MAGIC
    hdr[4] = VERSION
    hdr[5] = FLAG_FINAL if final else 0
    struct.pack_into('<II', hdr, 6, index, start_lba)
    struct.pack_into('<HHH', hdr, 14, count, stored_crc, len(bad_hdd))
    struct.pack_into('<I', hdr, 20, total)
    struct.pack_into('<H', hdr, 254, len(bad_flp))
    for k, lba in enumerate(sorted(bad_hdd)[:MAX_BAD_HDD]):
        struct.pack_into('<I', hdr, 24 + 4 * k, lba)
    for k, off in enumerate(sorted(bad_flp)[:MAX_BAD_FLP]):
        struct.pack_into('<H', hdr, 256 + 2 * k, off)
    hdr[216:216 + 12] = b'HDDSAVER 2.1'
    struct.pack_into('<H', hdr, 500, crc16(bytes(hdr[0:500])))
    img = bytearray(DISK_BYTES)
    img[0:SECTOR] = hdr
    img[HEADER_LBAS * SECTOR:HEADER_LBAS * SECTOR + count * SECTOR] = stream
    return bytes(img)


def cmd_selftest(args):
    """Synthetic round-trip: generate disks like the 286 would write them,
    then check that run_assembly reconstructs and reports correctly."""
    import tempfile

    total = 6000
    cap = DISK_CAPACITY                      # 2870
    s0, s1, s2 = 0, cap, 2 * cap             # disk ranges: 0-2869, 2870-5739, 5740-5999
    bad_hdd_d0 = {5}
    bad_flp_d2 = {100}
    flip_off = 42 * SECTOR + 7               # corruption inside disk1 payload

    checks = []

    # scenario A: disks 0+2 present (disk 2 has FINAL flag), plus a corrupt
    # duplicate variant of disk 0 which must lose against the good one.
    with tempfile.TemporaryDirectory() as td:
        write = lambda name, data: open(os.path.join(td, name), 'wb').write(data)
        write('disk_000.raw', make_synthetic_disk(0, s0, total, bad_hdd=bad_hdd_d0))
        write('disk_000_bad.raw',
              make_synthetic_disk(0, s0, total, corrupt_byte=flip_off))
        write('disk_002.raw', make_synthetic_disk(2, s2, total,
                                                  bad_flp=bad_flp_d2, final=True))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, verbose=False)
        data = open(out, 'rb').read()

        checks.append(('A image size', len(data) == total * SECTOR))
        spot_ok = True
        for lba in [0, 1, 4, 100, 2868, 2869]:
            if data[lba * SECTOR:(lba + 1) * SECTOR] != fake_sector(lba):
                spot_ok = False
        checks.append(('A good sectors intact', spot_ok))
        checks.append(('A bad hdd lba -> BADFILL',
                       data[5 * SECTOR:6 * SECTOR] == BADFILL))
        gap_zeroed = all(data[l * SECTOR:(l + 1) * SECTOR] == bytes(SECTOR)
                         for l in range(s1, 2 * cap))
        checks.append(('A gap zero-filled', gap_zeroed))
        checks.append(('A gap reported', rep['gaps'] == [(s1, s2 - 1)]))
        checks.append(('A valid variant wins', rep['chosen_files'][0] == 'disk_000.raw'))
        checks.append(('A final flag seen', rep['final_seen']))
        flp_lba = s2 + list(bad_flp_d2)[0]
        checks.append(('A bad flp mapped', flp_lba in rep['declared_bad_spots']
                       and data[flp_lba * SECTOR:(flp_lba + 1) * SECTOR] == BADFILL))
        checks.append(('A declared bad hdd set', rep['declared_bad_hdd'] == {5}))
        checks.append(('A suspect only from failed disk', rep['suspect_count'] == 0))

    # scenario B: ONLY the corrupt variant of disk 0 -> placed as suspect
    with tempfile.TemporaryDirectory() as td:
        with open(os.path.join(td, 'disk_000.raw'), 'wb') as f:
            f.write(make_synthetic_disk(0, s0, total, corrupt_byte=flip_off))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, verbose=False)
        data = open(out, 'rb').read()
        checks.append(('B suspect counted', rep['suspect_count'] == min(cap, total)))
        got = data[(s0 + 42) * SECTOR + 7]
        checks.append(('B corrupted byte preserved', got == (fake_sector(42)[7] ^ 0xFF)))

    print()
    failed = False
    for name, passed in checks:
        print('  %-34s %s' % (name, 'PASS' if passed else 'FAIL'))
        failed |= not passed
    print('\nSELFTEST %s' % ('FAILED' if failed else 'PASSED'))
    sys.exit(1 if failed else 0)

# -------------------------------------------------------- emulator test loop


def pattern_sector(lba):
    """Deterministic fake-HDD sector content, purely a function of its LBA."""
    return hashlib.sha256(struct.pack('<I', lba)).digest() * 16


def cmd_mkpattern(args):
    t0 = time.time()
    buf = bytearray()
    with open(args.out, 'wb') as f:
        for lba in range(args.total):
            buf += pattern_sector(lba)
            if len(buf) >= 1 << 20:
                f.write(buf)
                buf = bytearray()
        if buf:
            f.write(buf)
    print('wrote %s: %d sectors, %d bytes (%.1f s)'
          % (args.out, args.total, args.total * SECTOR, time.time() - t0))


def coverage_and_declared(dumps_dir, total):
    """Covered-LBA map + declared-bad set from the chosen dump per disk index."""
    candidates, rejected, totals = load_dumps(dumps_dir)
    covered = bytearray(total)
    declared = set()
    n = 0
    for idx in sorted(candidates):
        recs = sorted(candidates[idx],
                      key=lambda r: (not r['ev']['crc_ok'], r['file']))
        info = recs[0]['ev']['info']
        s, c = info['start_lba'], info['count']
        if s < total:
            m = min(c, total - s)
            covered[s:s + m] = b'\x01' * m
        for b in info['bad_hdd']:
            if b < total:
                declared.add(b)
        for off in info['bad_flp']:
            lba = s + off
            if 0 <= lba < total:
                declared.add(lba)
        n += 1
    return covered, declared, n


def cmd_verify(args):
    data = open(args.image, 'rb').read()
    if len(data) % SECTOR:
        print('WARNING: image size %d not a multiple of %d'
              % (len(data), SECTOR))
    total_img = min(len(data) // SECTOR, args.total)
    covered, declared, ndisks = coverage_and_declared(args.dumps, args.total)
    tallies = {'ok': 0, 'bad-declared': 0, 'missing': 0,
               'zero-unexpected': 0, 'badfill-undeclared': 0, 'MISMATCH': 0}
    samples = {}
    zero = bytes(SECTOR)

    def note(cls, lba):
        samples.setdefault(cls, [])
        if len(samples[cls]) < 8:
            samples[cls].append(lba)

    for lba in range(total_img):
        got = data[lba * SECTOR:(lba + 1) * SECTOR]
        if not covered[lba]:
            # nothing was dumped here: only zeros are acceptable
            if got == zero:
                tallies['missing'] += 1
            else:
                tallies['MISMATCH'] += 1
                note('MISMATCH', lba)
        elif got == pattern_sector(lba):
            tallies['ok'] += 1
        elif got == BADFILL:
            cls = 'bad-declared' if lba in declared else 'badfill-undeclared'
            tallies[cls] += 1
            note(cls, lba)
        elif got == zero:
            tallies['zero-unexpected'] += 1
            note('zero-unexpected', lba)
        else:
            tallies['MISMATCH'] += 1
            note('MISMATCH', lba)

    print('disks used : %d' % ndisks)
    print('coverage   : %d / %d sectors (%.2f%%)'
          % (sum(covered), args.total, 100.0 * sum(covered) / args.total))
    for k in sorted(tallies, key=lambda x: -tallies[x]):
        line = '  %-19s %7d' % (k, tallies[k])
        if samples.get(k):
            line += '   e.g. LBA ' + ', '.join(map(str, samples[k][:6]))
        print(line)
    bad = (tallies['MISMATCH'] or tallies['badfill-undeclared']
           or tallies['zero-unexpected'])
    if bad:
        verdict = 'FAILED'
    elif tallies['missing']:
        verdict = 'INCOMPLETE (consistent so far)'
    else:
        verdict = 'PASSED'
    print('\nVERIFY %s' % verdict)
    sys.exit(1 if bad else 0)


# ------------------------------------------------------------ bad sector map

MAP_COLORS = {
    0: '#bdbdbd',   # never dumped
    1: '#2ea44f',   # readable
    2: '#d93025',   # hdd read failed -> BADFILL
    3: '#e08a00',   # hdd read ok, floppy write failed -> recoverable by rerun
}
MAP_NAMES = {0: 'not dumped', 1: 'readable', 2: 'hdd read failed',
             3: 'floppy write failed'}


def build_badmap_state(dumps_dir, cyls, heads, spt):
    """Per-LBA state byte + counters, from the best dump of each disk."""
    candidates, rejected, totals = load_dumps(dumps_dir)
    total = cyls * heads * spt
    state = bytearray(total)          # 0 = untouched
    chosen = []
    for idx in sorted(candidates):
        recs = sorted(candidates[idx],
                      key=lambda r: (not r['ev']['crc_ok'], r['file']))
        chosen.append(recs[0])
    for rec in chosen:
        info = rec['ev']['info']
        s, c = info['start_lba'], info['count']
        if s >= total:
            continue
        for j in range(min(c, total - s)):
            state[s + j] = 1
    nh = nf = 0
    for rec in chosen:
        info = rec['ev']['info']
        for b in info['bad_hdd']:
            if b < total:
                state[b] = 2
                nh += 1
        for off in info['bad_flp']:
            lba = info['start_lba'] + off
            if 0 <= lba < total:
                state[lba] = 3
                nf += 1
    return state, nh, nf, len(chosen)


def cmd_badmap(args):
    C, H, S = args.cyls, args.heads, args.spt
    state, nh, nf, ndisks = build_badmap_state(args.dumps, C, H, S)
    CW, CH = 3, 12                    # pixel size of one sector cell
    ML, MT = 56, 78                   # left margin / top offset
    panel_gap, tick_h = 18, 14
    W = ML + C * CW + 14
    panel_h = S * CH + tick_h
    Ht = MT + H * (panel_h + panel_gap)

    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<svg xmlns="http://www.w3.org/2000/svg" width="%d" '
           'height="%d" font-family="monospace">' % (W, Ht),
           '<rect width="100%%" height="100%%" fill="#fafafa"/>',
           '<text x="%d" y="24" font-size="15">Tandon HDD bad-sector map '
           '(from %d floppies)</text>' % (ML, ndisks)]
    lx = ML
    for st in (1, 2, 3, 0):
        out.append('<rect x="%d" y="36" width="12" height="12" fill="%s"/>'
                   % (lx, MAP_COLORS[st]))
        out.append('<text x="%d" y="46" font-size="11" fill="#222">%s</text>'
                   % (lx + 16, MAP_NAMES[st]))
        lx += 16 + len(MAP_NAMES[st]) * 7 + 20
    cnt = [state.count(i) for i in range(4)]
    out.append('<text x="%d" y="66" font-size="11" fill="#222">'
               '%d readable | %d hdd-bad | %d flp-bad | %d not dumped'
               '</text>' % (ML, cnt[1], cnt[2], cnt[3], cnt[0]))

    for h in range(H):
        y0 = MT + h * (panel_h + panel_gap)
        out.append('<text x="%d" y="%d" font-size="12" fill="#222">'
                   'Head %d</text>' % (ML, y0 + 10, h))
        gy = y0 + 16
        for c in range(0, C + 1, 100):
            out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" '
                       'stroke="#dddddd"/>' % (ML + c * CW, gy,
                                               ML + c * CW, gy + S * CH))
        for sec in range(S):
            ry = gy + sec * CH
            x = 0
            while x < C:
                st = state[(x * H + h) * S + sec]
                x2 = x + 1
                while x2 < C and state[(x2 * H + h) * S + sec] == st:
                    x2 += 1
                out.append('<rect x="%d" y="%d" width="%d" height="%d" '
                           'fill="%s"/>' % (ML + x * CW, ry, (x2 - x) * CW,
                                            CH, MAP_COLORS[st]))
                x = x2
        for c in range(0, C, 100):
            out.append('<text x="%d" y="%d" font-size="9" fill="#666">%d'
                       '</text>' % (ML + c * CW, gy + S * CH + 11, c))

    out.append('</svg>')
    with open(args.out, 'w') as f:
        f.write('\n'.join(out))
    print('wrote %s' % args.out)
    per = [[0, 0, 0, 0] for _ in range(H)]
    for lba, st in enumerate(state):
        per[(lba // S) % H][st] += 1
    print('per head:  ok / hdd-bad / flp-bad / not-dumped')
    for h in range(H):
        p = per[h]
        print('  head %d: %6d / %5d / %5d / %6d'
              % (h, p[1], p[2], p[3], p[0]))
    if nh or nf:
        print('\nreading tip: failure runs ending right before multiples '
              'of %d (track boundaries)\nlook like format-time defect '
              'sparing, random scatter looks like decay.' % S)


# --------------------------------------------------------------------- cli


def main():
    ap = argparse.ArgumentParser(
        description='Process floppy dumps from the Tandon 286 HDD saver.',
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    p = sub.add_parser('read', help='read one floppy into dumps/')
    p.add_argument('--drive', default='A',
                   help='floppy drive letter or path to an existing image file')
    p.add_argument('--dumps', default='dumps', help='directory for raw dumps')
    p.set_defaults(fn=cmd_read)

    p = sub.add_parser('assemble', help='stitch dumps into hdd.img + report')
    p.add_argument('--dumps', default='dumps')
    p.add_argument('--out', default='hdd.img')
    p.set_defaults(fn=lambda a: run_assembly(a.dumps, a.out))

    p = sub.add_parser('selftest', help='synthetic round-trip test')
    p.set_defaults(fn=cmd_selftest)

    p = sub.add_parser('mkpattern',
                       help='create a fake HDD image for emulator testing')
    p.add_argument('--out', default='fake_hdd.img')
    p.add_argument('--total', type=int, default=127920,
                   help='sector count (default: full 62.5MB drive)')
    p.set_defaults(fn=cmd_mkpattern)

    p = sub.add_parser('verify',
                       help='check assembled hdd.img against mkpattern data')
    p.add_argument('--image', default='hdd.img')
    p.add_argument('--dumps', default='dumps')
    p.add_argument('--total', type=int, default=127920)
    p.set_defaults(fn=cmd_verify)

    p = sub.add_parser('badmap',
                       help='svg graphic: per-head good/bad sector map')
    p.add_argument('--dumps', default='dumps')
    p.add_argument('--out', default='badmap.svg')
    p.add_argument('--cyls', type=int, default=820)
    p.add_argument('--heads', type=int, default=6)
    p.add_argument('--spt', type=int, default=26)
    p.set_defaults(fn=cmd_badmap)

    args = ap.parse_args()
    args.fn(args)


if __name__ == '__main__':
    main()



