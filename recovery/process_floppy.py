#!/usr/bin/env python3
"""Windows-side companion to the Tandon 286 "HDD saver" floppy dumper.

Subcommands:
  read      copy one physical floppy (or an image file) into dumps/
  assemble  stitch all dumps into hdd.img and write hdd_report.txt
  selftest  synthetic round-trip test of the assembler logic
  mkpattern create a fake HDD image for emulator testing
  verify    check assembled hdd.img against mkpattern data
  badmap    svg graphic: per-head good/bad sector map

Typical workflow:
  python process_floppy.py read --drive A --dumps dumps      (repeat per disk)
  python process_floppy.py assemble --dumps dumps --out hdd.img

Floppy layout written by the 286 tool (v3, see watcomc/source/floppy.h):
  LBA 0        header sector ('HDSV', version 3): magic, version, flags,
               disk index (label only), total sectors, descriptor count,
               data sector count, floppy failure count, tool id, crc16
  LBA 1..9     reserved
  LBA 10..     repeating groups, one per hdd track batch:
                 [descriptor block: count byte + up to 26 entries of
                  lba(3 bytes le) + int13 status(1) + dataIdx(1),
                  padded, crc16 over bytes 0..509]
                 [the 512-byte data sectors whose status says data follows]

Every dumped hdd sector carries its own lba, so disk order does not
matter and gaps are legal. Sectors that could not be read get a
descriptor with the bios error code and NO data - they cost nothing.
Statuses with data: 0x00 (read ok) and 0x11 (ecc corrected). 0xfe means
the head was masked out this pass; anything else is the raw bios error.

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
DISK_BYTES = 2880 * SECTOR
MAGIC = b'HDSV'
VERSION = 3
FLAG_FINAL = 0x01

DESC_PER_BLOCK = 26        # descriptors per block == BATCH_SECTORS on the 286
ENTRY_SIZE = 5             # lba24 + status8 + dataIdx8

ST_OK = 0x00               # read clean, data present
ST_ECC = 0x11              # read after ecc correction, data present
ST_HEADSKIP = 0xFE         # head masked out this pass, never attempted


def has_data(status):
    """True when the descriptor's sector carries real data."""
    return status in (ST_OK, ST_ECC)


INT13_ERRORS = {
    0x01: 'bad command', 0x02: 'address mark not found',
    0x03: 'write protected', 0x04: 'sector not found',
    0x06: 'media changed', 0x08: 'bad dma', 0x09: 'dma boundary',
    0x0c: 'media type unknown', 0x10: 'bad ecc on read',
    0x20: 'controller failure', 0x40: 'seek failed',
    0x80: 'timeout', 0xaa: 'drive not ready', 0xbb: 'undefined error',
    0xcc: 'write fault', 0xe0: 'status error',
}


def status_name(status):
    if status == ST_HEADSKIP:
        return 'head masked out'
    return INT13_ERRORS.get(status, 'bios error 0x%02x' % status)


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
    """Parse the v3 header sector. Returns dict or None if magic mismatch."""
    if len(sector0) < SECTOR or sector0[0:4] != MAGIC:
        return None
    version = sector0[4]
    flags = sector0[5]
    disk_index, total_sectors = struct.unpack_from('<II', sector0, 6)
    desc_count, data_count, flp_fail = struct.unpack_from('<HHH', sector0, 14)
    return {
        'version': version,
        'flags': flags,
        'disk_index': disk_index,
        'total_sectors': total_sectors,
        'desc_count': desc_count,
        'data_count': data_count,
        'flp_fail': flp_fail,
        'tool_id': bytes(sector0[20:56]).rstrip(b'\0').decode('ascii', 'replace'),
        'header_crc_ok':
            struct.unpack_from('<H', sector0, 510)[0] == crc16(bytes(sector0[0:510])),
    }


# ---------------------------------------------------------------- groups


def iter_groups(image):
    """Walk the descriptor groups of a dump image.

    Yields dicts:
      first_lba   hdd lba of the group's first descriptor
      entries     list of (lba, status, data_idx)
      datas       list of 512-byte data sectors (index == dataIdx)
      crc_ok      block crc check result
    Stops at an all-zero block (the zero fill after the last group) or
    at a block whose crc/count is broken - everything behind such a
    block is uninterpretable anyway.
    """
    pos = HEADER_LBAS * SECTOR
    while pos + SECTOR <= DISK_BYTES:
        block = image[pos:pos + SECTOR]
        if not any(block):
            break                                   # end-of-stream padding
        count = block[0]
        if count > DESC_PER_BLOCK:
            break                                   # corrupt, cannot trust
        crc_ok = struct.unpack_from('<H', block, 510)[0] \
            == crc16(bytes(block[0:510]))
        entries = []
        for i in range(count):
            off = 1 + i * ENTRY_SIZE
            lba = int.from_bytes(block[off:off + 3], 'little')
            entries.append((lba, block[off + 3], block[off + 4]))
        ngood = sum(1 for _, st, _ in entries if has_data(st))
        datas = []
        dpos = pos + SECTOR
        for _k in range(ngood):
            if dpos + SECTOR > DISK_BYTES:
                break                               # truncated image
            datas.append(image[dpos:dpos + SECTOR])
            dpos += SECTOR
        yield {
            'first_lba': entries[0][0] if entries else 0,
            'entries': entries,
            'datas': datas,
            'crc_ok': crc_ok,
        }
        pos += SECTOR * (1 + ngood)


def evaluate_image(image):
    """Full validation of one 1.44MB dump.

    Returns dict:
      ok           header sane and group stream consistent (safe to place)
      reason       why not ok
      info         parsed header dict (None when magic missing)
      groups       list from iter_groups() (when ok)
      desc_total   descriptors actually found
      data_total   data sectors actually found
      blocks_bad   number of descriptor blocks failing their crc
    """
    res = {'ok': False, 'reason': '', 'info': None, 'groups': [],
           'desc_total': 0, 'data_total': 0, 'blocks_bad': 0}
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

    groups = list(iter_groups(image))
    res['groups'] = groups
    res['desc_total'] = sum(len(g['entries']) for g in groups)
    res['data_total'] = sum(len(g['datas']) for g in groups)
    res['blocks_bad'] = sum(1 for g in groups if not g['crc_ok'])
    for g in groups:
        if len(g['datas']) < sum(1 for _, st, _ in g['entries'] if has_data(st)):
            res['reason'] = 'group at lba %d truncated' % g['first_lba']
            return res
    if hdr['desc_count'] != res['desc_total']:
        res['reason'] = ('header claims %d descriptors, found %d'
                         % (hdr['desc_count'], res['desc_total']))
        return res
    if hdr['data_count'] != res['data_total']:
        res['reason'] = ('header claims %d data sectors, found %d'
                         % (hdr['data_count'], res['data_total']))
        return res
    if any(e[0] >= hdr['total_sectors'] for g in groups for e in g['entries']):
        res['reason'] = 'descriptor lba beyond drive size %d' % hdr['total_sectors']
        return res
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
        err_n = sum(1 for g in ev['groups'] for _, st, _ in g['entries']
                    if not has_data(st) and st != ST_HEADSKIP)
        skip_n = sum(1 for g in ev['groups'] for _, st, _ in g['entries']
                     if st == ST_HEADSKIP)
        print('Header OK : disk %d, %d descriptors, %d data sectors%s, '
              '%d read errors, %d head-skipped%s'
              % (i['disk_index'], ev['desc_total'], ev['data_total'],
                 ', FINAL' if i['flags'] & FLAG_FINAL else '',
                 err_n, skip_n,
                 ', %d BAD BLOCK(S)' % ev['blocks_bad'] if ev['blocks_bad'] else ''))
        if ev['blocks_bad']:
            print('WARNING: %d descriptor block(s) fail their crc - affected '
                  'data will be marked suspect.' % ev['blocks_bad'])
        if i['flp_fail']:
            print('WARNING: header reports %d surrendered floppy write(s) '
                  '- re-read this disk.' % i['flp_fail'])
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
    """Stitch all dumps into one image. Returns report dict.

    Placement rules, per descriptor:
      status has data  -> scatter its data sector to file offset lba*512
      status HEADSKIP  -> leave the lba uncovered (awaiting another pass)
      other status     -> leave uncovered, remember the error code
    A dump whose descriptor block fails its crc still gets placed, but
    every sector it provides is flagged suspect. Overlaps: first valid
    placement wins; a suspect placement can be upgraded by a later
    trustworthy one.
    """
    candidates, rejected, totals = load_dumps(dumps_dir)
    if not candidates:
        sys.exit('No valid HDSV disks found in %s' % dumps_dir)
    if len(totals) > 1:
        print('WARNING: disks disagree about drive size: %s' % totals)
    total = max(totals.items(), key=lambda kv: kv[1])[0]

    chosen = []
    for lst in candidates.values():
        chosen.extend(lst)
    chosen.sort(key=lambda r: (min((e[0] for g in r['ev']['groups']
                                    for e in g['entries']), default=0),
                               r['file']))

    image_out = bytearray(total * SECTOR)
    covered = bytearray(total)
    suspect = bytearray(total)
    err_codes = {}        # lba -> bios status of the failed read
    headskipped_lbas = set()
    overlaps = []
    upgrades_total = 0
    final_seen = False

    for rec in chosen:
        i = rec['ev']['info']
        if i['flags'] & FLAG_FINAL:
            final_seen = True
        ov = 0
        upgraded = 0
        for g in rec['ev']['groups']:
            trust = g['crc_ok']
            for lba, st, didx in g['entries']:
                if lba >= total:
                    continue
                if has_data(st):
                    sec = g['datas'][didx]
                    if covered[lba]:
                        ov += 1
                        if trust and suspect[lba]:
                            # trustworthy re-dump upgrades earlier
                            # suspect data from a damaged-block group
                            suspect[lba] = 0
                            image_out[lba * SECTOR:(lba + 1) * SECTOR] = sec
                            upgraded += 1
                        continue
                    covered[lba] = 1
                    if not trust:
                        suspect[lba] = 1
                    image_out[lba * SECTOR:(lba + 1) * SECTOR] = sec
                elif st == ST_HEADSKIP:
                    headskipped_lbas.add(lba)
                else:
                    prev = err_codes.get(lba)
                    if prev is None or st < prev:
                        err_codes[lba] = st
        if ov:
            overlaps.append((i['disk_index'], ov))
        upgrades_total += upgraded

    gaps = []
    run_start = None
    for lba in range(total):
        attempted = covered[lba] or lba in err_codes or lba in headskipped_lbas
        if not attempted and run_start is None:
            run_start = lba
        elif attempted and run_start is not None:
            gaps.append((run_start, lba - 1))
            run_start = None
    if run_start is not None:
        gaps.append((run_start, total - 1))

    coverage = sum(covered)

    with open(out_path, 'wb') as f:
        f.write(image_out)

    report = {
        'gaps': gaps,
        'chosen_files': [r['file'] for r in chosen],
        'final_seen': final_seen,
        'err_codes': err_codes,
        'headskipped_lbas': headskipped_lbas,
        'suspect_count': sum(suspect),
        'coverage': coverage,
        'overlaps': overlaps,
        'upgrades': upgrades_total,
        'complete': (not gaps) and final_seen,
        'total': total,
        'rejected': rejected,
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
    ap('%-5s %-24s %-9s %-9s %-7s %-6s' %
       ('idx', 'file', 'lbas', 'data', 'blocks', 'flags'))
    for rec in rep['chosen_records']:
        i = ev_info(rec)
        flags = []
        if i['flags'] & FLAG_FINAL:
            flags.append('FINAL')
        if i['flp_fail']:
            flags.append('FLPFAIL=%d' % i['flp_fail'])
        ap('%-5d %-24s %-9d %-9d %-7d %s' % (
            i['disk_index'], rec['file'], i['desc_count'], i['data_count'],
            rec['ev']['blocks_bad'], ' '.join(flags)))
    if rep['rejected']:
        ap('')
        ap('files rejected (no usable header):')
        for fn, why in rep['rejected']:
            ap('  %-24s %s' % (fn, why))
    if rep['overlaps']:
        ap('')
        ap('overlapping placements (first valid wins):')
        for idx, ov in rep['overlaps']:
            ap('  disk %d: %d sectors already covered' % (idx, ov))
    ap('')
    ap('=== coverage ===')
    ap('sectors covered : %d / %d (%.2f%%)' % (
        rep['coverage'], rep['total'], 100.0 * rep['coverage'] / rep['total']))
    ap('suspect sectors : %d (data from descriptor blocks with failed crc)'
       % rep['suspect_count'])
    ap('head-filtered   : %d lbas were never attempted (head bitmask), left '
       'for another pass' % len(rep['headskipped_lbas']))
    ap('upgraded        : %d sectors where a later dump recovered suspect data'
       % rep['upgrades'])
    ap('read failures   : %d lbas with logged bios error codes'
       % len(rep['err_codes']))
    if rep['err_codes']:
        ap('')
        ap('=== read failures by code ===')
        by_code = {}
        for lba in sorted(rep['err_codes']):
            by_code.setdefault(rep['err_codes'][lba], []).append(lba)
        for code in sorted(by_code):
            lbas = by_code[code]
            ap('  0x%02x %-22s : %d sectors' % (
                code, status_name(code), len(lbas)))
            line = '    '
            for b in lbas:
                line += ' %d' % b
                if len(line) > 74:
                    ap(line)
                    line = '    '
            if line.strip():
                ap(line)
    if rep['gaps']:
        ap('')
        ap('missing ranges (never attempted):')
        for g0, g1 in rep['gaps']:
            ap('  lba %d..%d  (%d sectors, %.2f MB)' % (
                g0, g1, g1 - g0 + 1, (g1 - g0 + 1) * SECTOR / 1048576.0))
            ap('    to fill: boot dumper with resume LBA=%d, new disk number,' % g0)
            ap('    let it run to the end; overlapping data is harmless.')
    else:
        ap('missing ranges: none')
    ap('')
    ap('status: %s' % ('COMPLETE - all sectors either restored or known-bad'
                      if rep['complete'] else
                      'INCOMPLETE - missing ranges or final flag not seen'))
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')


def ev_info(rec):
    return rec['ev']['info']


def print_assembly_summary(rep, out_path, report_path):
    print('Disks used   : %d (%d files rejected)'
          % (len(rep['chosen_files']), len(rep['rejected'])))
    print('Coverage     : %d / %d sectors (%.2f%%)'
          % (rep['coverage'], rep['total'], 100.0 * rep['coverage'] / rep['total']))
    print('Suspect      : %d sectors (from blocks with failed crc)' % rep['suspect_count'])
    print('Head-filtered: %d lbas (not attempted, awaiting another pass)'
          % len(rep['headskipped_lbas']))
    print('Upgrades     : %d sectors (recovered by a later dump)' % rep['upgrades'])
    print('Read failures: %d lbas with logged bios error codes' % len(rep['err_codes']))
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


def make_v3_disk(index, seq, total, final=False, corrupt_group=None,
                 corrupt_byte=None, flp_fail=0):
    """Build a 1.44MB image exactly like the 286 tool writes it.

    seq: ordered list of (lba, status) pairs this disk covers. Data is
    generated for every status that carries data; others contribute
    only their descriptor. corrupt_group/corrupt_byte flip one byte in
    the given group's descriptor block AFTER its crc was appended,
    simulating media rot.
    """
    img = bytearray(DISK_BYTES)
    hdr = bytearray(SECTOR)
    hdr[0:4] = MAGIC
    hdr[4] = VERSION
    hdr[5] = FLAG_FINAL if final else 0
    struct.pack_into('<II', hdr, 6, index, total)
    hdr[20:20 + 12] = b'HDDSAVER 3.0'

    groups = [seq[i:i + DESC_PER_BLOCK] for i in range(0, len(seq), DESC_PER_BLOCK)]
    pos = HEADER_LBAS * SECTOR
    desc_total = 0
    data_total = 0
    for gi, grp in enumerate(groups):
        block = bytearray(SECTOR)
        block[0] = len(grp)
        datas = []
        for k, (lba, st) in enumerate(grp):
            off = 1 + k * ENTRY_SIZE
            block[off:off + 3] = lba.to_bytes(3, 'little')
            block[off + 3] = st
            if has_data(st):
                block[off + 4] = len(datas)
                datas.append(fake_sector(lba))
        struct.pack_into('<H', block, 510, crc16(bytes(block[0:510])))
        if gi == corrupt_group:
            block[corrupt_byte] ^= 0xFF
        img[pos:pos + SECTOR] = block
        pos += SECTOR
        desc_total += len(grp)
        data_total += len(datas)
        for sec in datas:
            img[pos:pos + SECTOR] = sec
            pos += SECTOR
    struct.pack_into('<HHH', hdr, 14, desc_total, data_total, flp_fail)
    struct.pack_into('<H', hdr, 510, crc16(bytes(hdr[0:510])))
    img[0:SECTOR] = hdr
    return bytes(img)


def cmd_selftest(args):
    """Synthetic round-trip: generate disks like the 286 would write them,
    then check that run_assembly reconstructs and reports correctly."""
    import tempfile

    good = lambda lo, hi: [(l, ST_OK) for l in range(lo, hi)]

    checks = []

    # scenario A: ONE disk covers a whole (small) drive, with one dead
    # sector (lba 5, bios error 0x04 logged in its descriptor), one
    # head-masked sector (lba 1500) and one corrupted descriptor block.
    # none of those are gaps: every lba is accounted for -> COMPLETE.
    total_a = 2000
    seq0 = good(0, total_a)
    seq0[5] = (5, 0x04)                       # sector not found
    seq0[1500] = (1500, ST_HEADSKIP)          # head masked this pass
    with tempfile.TemporaryDirectory() as td:
        write = lambda name, data: open(os.path.join(td, name), 'wb').write(data)
        write('disk_000.raw', make_v3_disk(0, seq0, total_a, final=True,
                                           corrupt_group=1, corrupt_byte=400))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, verbose=False)
        data = open(out, 'rb').read()

        checks.append(('A image size', len(data) == total_a * SECTOR))
        spot_ok = all(
            data[l * SECTOR:(l + 1) * SECTOR] == fake_sector(l)
            for l in [0, 1, 4, 6, 999, 1000, 1999])
        checks.append(('A good sectors intact', spot_ok))
        checks.append(('A dead sector left empty',
                       data[5 * SECTOR:6 * SECTOR] == bytes(SECTOR)))
        checks.append(('A dead sector error logged',
                       rep['err_codes'].get(5) == 0x04))
        checks.append(('A headskip tracked',
                       1500 in rep['headskipped_lbas']
                       and data[1500 * SECTOR:(1501) * SECTOR] == bytes(SECTOR)))
        checks.append(('A no true gaps', rep['gaps'] == []))
        checks.append(('A final flag seen', rep['final_seen']))
        checks.append(('A corrupt block detected',
                       rep['chosen_records'][0]['ev']['blocks_bad'] == 1))
        checks.append(('A corrupt block data suspect',
                       rep['suspect_count'] == DESC_PER_BLOCK))
        checks.append(('A complete despite known-bad', rep['complete']))

    # scenario B: only disk 0 of two planned disks present, without the
    # final flag -> the uncovered tail must be reported as a gap and the
    # status must be INCOMPLETE.
    with tempfile.TemporaryDirectory() as td:
        write = lambda name, data: open(os.path.join(td, name), 'wb').write(data)
        write('disk_000.raw', make_v3_disk(0, good(0, 2000), 3000))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, verbose=False)
        checks.append(('B tail reported as gap',
                       rep['gaps'] == [(2000, 2999)]))
        checks.append(('B incomplete without final flag',
                       rep['complete'] is False))

    # scenario C: same range dumped twice - once with a corrupt descriptor
    # block (data lands as suspect), then cleanly. the clean dump must
    # upgrade every suspect sector.
    total_c = 2000
    seq3 = good(500, 1500)
    with tempfile.TemporaryDirectory() as td:
        write = lambda name, data: open(os.path.join(td, name), 'wb').write(data)
        write('disk_000.raw', make_v3_disk(0, seq3, total_c,
                                           corrupt_group=0, corrupt_byte=200))
        write('disk_001.raw', make_v3_disk(1, seq3, total_c))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, verbose=False)

        checks.append(('C upgrade happened', rep['upgrades'] > 0))
        checks.append(('C no suspects remain', rep['suspect_count'] == 0))
        checks.append(('C overlap counted', rep['overlaps'] == [(1, len(seq3))]))

    # scenario D: bit rot inside the header sector must reject the whole
    # dump (the header crc covers it).
    with tempfile.TemporaryDirectory() as td:
        img = bytearray(make_v3_disk(0, good(0, 100), 2000))
        img[123] ^= 0xFF                          # inside header, below crc
        with open(os.path.join(td, 'disk_000.raw'), 'wb') as f:
            f.write(bytes(img))
        candidates, rejected, _totals = load_dumps(td)
        checks.append(('D bad header rejected', len(candidates) == 0
                       and len(rejected) == 1))

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


def load_placement(dumps_dir, total):
    """Covered-LBA map, error map and headskip set, merged across ALL dumps."""
    candidates, rejected, totals = load_dumps(dumps_dir)
    covered = bytearray(total)
    errs = {}
    skips = set()
    ndisks = 0
    for lst in candidates.values():
        for rec in lst:
            ndisks += 1
            for g in rec['ev']['groups']:
                for lba, st, didx in g['entries']:
                    if lba >= total:
                        continue
                    if has_data(st):
                        covered[lba] = 1
                    elif st == ST_HEADSKIP:
                        skips.add(lba)
                    else:
                        errs.setdefault(lba, st)
    return covered, errs, skips, ndisks


def cmd_verify(args):
    data = open(args.image, 'rb').read()
    if len(data) % SECTOR:
        print('WARNING: image size %d not a multiple of %d'
              % (len(data), SECTOR))
    total_img = min(len(data) // SECTOR, args.total)
    covered, errs, skips, ndisks = load_placement(args.dumps, args.total)
    zero = bytes(SECTOR)
    tallies = {'ok': 0, 'readerror-declared': 0, 'headskipped': 0,
               'missing': 0, 'MISMATCH': 0}
    samples = {}

    def note(cls, lba):
        samples.setdefault(cls, [])
        if len(samples[cls]) < 8:
            samples[cls].append(lba)

    for lba in range(total_img):
        got = data[lba * SECTOR:(lba + 1) * SECTOR]
        if lba in errs:
            tallies['readerror-declared'] += 1
            note('readerror-declared', lba)
        elif lba in skips:
            tallies['headskipped'] += 1
            note('headskipped', lba)
        elif not covered[lba]:
            if got == zero:
                tallies['missing'] += 1
            else:
                tallies['MISMATCH'] += 1
                note('MISMATCH', lba)
        elif got == pattern_sector(lba):
            tallies['ok'] += 1
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
    bad = tallies['MISMATCH']
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
    0: '#bdbdbd',   # never attempted
    1: '#2ea44f',   # readable
    2: '#d93025',   # hdd read failed (bios error logged in descriptor)
    3: '#e08a00',   # head masked out this pass, awaiting another run
}
MAP_NAMES = {0: 'not attempted', 1: 'readable', 2: 'hdd read failed',
             3: 'head masked'}


def build_badmap_state(dumps_dir, cyls, heads, spt):
    """Per-LBA state byte + counters, merged across ALL dumps.

    Priority per lba: readable beats everything (a later pass may have
    recovered it); otherwise a logged bios error; otherwise head-skip;
    otherwise untouched.
    """
    candidates, rejected, totals = load_dumps(dumps_dir)
    total = cyls * heads * spt
    state = bytearray(total)          # 0 = untouched
    recs = [r for lst in candidates.values() for r in lst]
    nerr = 0
    for rec in recs:
        for g in rec['ev']['groups']:
            for lba, st, _didx in g['entries']:
                if lba >= total:
                    continue
                if has_data(st):
                    state[lba] = 1
                elif st == ST_HEADSKIP:
                    if state[lba] == 0:
                        state[lba] = 3
                else:
                    if state[lba] < 2:
                        state[lba] = 2
    nerr = sum(1 for l in range(total) if state[l] == 2)
    nskip = sum(1 for l in range(total) if state[l] == 3)
    return state, nerr, nskip, len(recs)


def cmd_badmap(args):
    C, H, S = args.cyls, args.heads, args.spt
    state, nerr, nskip, ndisks = build_badmap_state(args.dumps, C, H, S)
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
               '%d readable | %d read-failed | %d head-masked | %d not attempted'
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
    print('per head:  ok / read-failed / head-masked / not-attempted')
    for h in range(H):
        p = per[h]
        print('  head %d: %6d / %5d / %5d / %6d'
              % (h, p[1], p[2], p[3], p[0]))
    if nerr:
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
