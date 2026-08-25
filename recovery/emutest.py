"""Emulator test helpers: mkpattern and verify commands."""

import argparse
import hashlib
import os
import struct
import sys
import time

from structures import ST_HEADSKIP, has_data
from format import SECTOR
from assemble import load_dumps


def pattern_sector(lba: int) -> bytes:
    """Deterministic fake-HDD sector content, purely a function of its LBA."""
    return hashlib.sha256(struct.pack('<I', lba)).digest() * 16


def cmd_mkpattern(args: argparse.Namespace) -> None:
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


def load_placement(dumps_dir: str, total: int) -> tuple[bytearray, dict[int, int], set[int], int]:
    """Covered-LBA map, error map and headskip set, merged across ALL dumps.

    Returns (covered, errs, skips, ndisks).
    """
    records, _rejected = load_dumps(dumps_dir)
    covered = bytearray(total)
    errs: dict[int, int] = {}
    skips: set[int] = set()
    for rec in records:
        for blk in rec.ev.blocks:
            for e in blk.entries:
                if e.lba >= total:
                    continue
                if has_data(e.status):
                    covered[e.lba] = 1
                elif e.status == ST_HEADSKIP:
                    skips.add(e.lba)
                else:
                    errs.setdefault(e.lba, e.status)
    return covered, errs, skips, len(records)


def cmd_verify(args: argparse.Namespace) -> None:
    data = open(args.image, 'rb').read()
    if len(data) % SECTOR:
        print('WARNING: image size %d not a multiple of %d'
              % (len(data), SECTOR))
    total_img = min(len(data) // SECTOR, args.total)
    covered, errs, skips, ndisks = load_placement(args.dumps, args.total)
    zero = bytes(SECTOR)
    tallies: dict[str, int] = {
        'ok': 0, 'readerror-declared': 0, 'headskipped': 0,
        'missing': 0, 'MISMATCH': 0,
    }
    samples: dict[str, list[int]] = {}

    def note(cls: str, lba: int) -> None:
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
