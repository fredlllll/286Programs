"""Synthetic round-trip test of the assembler logic."""

import argparse
import os
import struct
import sys
import tempfile

from structures import (ST_OK, ST_HEADSKIP, has_data,
                        Descriptor, DescriptorBlock)
from format import (SECTOR, DISK_BYTES, DESC_PER_BLOCK, ENTRY_SIZE,
                    MAGIC, crc16, _crc8)
from assemble import run_assembly, missing_lba_ranges


def fake_sector(lba: int) -> bytes:
    return bytes(((lba * 7 + j * 13 + (j >> 4)) & 0xFF) for j in range(SECTOR))


def good_range(lo: int, hi: int) -> list[Descriptor]:
    """Produce Descriptors with ST_OK for every lba in [lo, hi)."""
    return [Descriptor(l, ST_OK, 0) for l in range(lo, hi)]


def make_disk(seq: list[Descriptor], total: int,
              corrupt_group: int | None = None,
              corrupt_byte: int | None = None) -> bytes:
    """Build a 1.44MB image exactly like the 286 tool writes it.

    seq: ordered list of Descriptors this disk covers. Data is
    generated for every descriptor that carries data; others contribute
    only their descriptor. corrupt_group/corrupt_byte flip one byte in
    the given group's descriptor block AFTER its crc was appended,
    simulating media rot.
    """
    img = bytearray(DISK_BYTES)
    groups = [seq[i:i + DESC_PER_BLOCK] for i in range(0, len(seq), DESC_PER_BLOCK)]
    pos = 0
    for gi, grp in enumerate(groups):
        block = bytearray(SECTOR)
        block[0] = len(grp)
        datas: list[bytes] = []
        for k, desc in enumerate(grp):
            off = 1 + k * ENTRY_SIZE
            block[off:off + 3] = desc.lba.to_bytes(3, 'little')
            block[off + 3] = desc.status
            if has_data(desc.status):
                block[off + 4] = len(datas)
                datas.append(fake_sector(desc.lba))
            block[off + 5] = _crc8(bytes(block[off:off + 5]))
        hdr_off = 1 + DESC_PER_BLOCK * ENTRY_SIZE
        block[hdr_off:hdr_off + 5] = MAGIC
        # crc covers data sectors, not the descriptor block
        data_blob = b''.join(datas)
        struct.pack_into('<H', block, 510, crc16(data_blob) if data_blob else 0)
        if gi == corrupt_group:
            block[corrupt_byte] ^= 0xFF
        img[pos:pos + SECTOR] = block
        pos += SECTOR
        for sec in datas:
            img[pos:pos + SECTOR] = sec
            pos += SECTOR
    return bytes(img)


def _write_file(directory: str, name: str, data: bytes) -> None:
    with open(os.path.join(directory, name), 'wb') as f:
        f.write(data)


def cmd_selftest(args: argparse.Namespace) -> None:
    """Synthetic round-trip: generate disks like the 286 would write them,
    then check that run_assembly reconstructs and reports correctly."""
    checks: list[tuple[str, bool]] = []

    # scenario A: ONE disk covers a whole (small) drive, with one dead
    # sector (lba 5, bios error 0x04 logged in its descriptor), one
    # head-masked sector (lba 1500) and one corrupted descriptor block.
    # none of those are gaps: every lba is accounted for -> COMPLETE.
    total_a = 2000
    seq0 = good_range(0, total_a)
    seq0[5] = Descriptor(5, 0x04, 0)       # sector not found
    seq0[1500] = Descriptor(1500, ST_HEADSKIP, 0)  # head masked
    with tempfile.TemporaryDirectory() as td:
        _write_file(td, 'disk_000.raw',
                    make_disk(seq0, total_a, corrupt_group=1, corrupt_byte=510))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, total_a, verbose=False)
        data = open(out, 'rb').read()

        checks.append(('A image size', len(data) == total_a * SECTOR))
        spot_ok = all(
            data[l * SECTOR:(l + 1) * SECTOR] == fake_sector(l)
            for l in [0, 1, 4, 6, 999, 1000, 1999])
        checks.append(('A good sectors intact', spot_ok))
        checks.append(('A dead sector left empty',
                       data[5 * SECTOR:6 * SECTOR] == bytes(SECTOR)))
        checks.append(('A dead sector error logged',
                       rep.err_codes.get(5) == 0x04))
        checks.append(('A headskip tracked',
                       1500 in rep.headskipped_lbas
                       and data[1500 * SECTOR:(1501) * SECTOR] == bytes(SECTOR)))
        checks.append(('A no true gaps', rep.gaps == []))
        checks.append(('A corrupt block detected',
                       rep.records[0].ev.blocks_bad == 1))
        checks.append(('A corrupt block data suspect',
                       rep.suspect_count == DESC_PER_BLOCK))
        checks.append(('A complete despite known-bad', rep.complete))

    # scenario B: only first half of the drive dumped -> the uncovered
    # tail must be reported as a gap and the status must be INCOMPLETE.
    with tempfile.TemporaryDirectory() as td:
        _write_file(td, 'disk_000.raw',
                    make_disk(good_range(0, 2000), 3000))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, 3000, verbose=False)
        checks.append(('B tail reported as gap',
                       rep.gaps == [(2000, 2999)]))
        checks.append(('B incomplete without full coverage',
                       rep.complete is False))

    # scenario C: same range dumped twice - once with a corrupt descriptor
    # block (data lands as suspect), then cleanly. the clean dump must
    # upgrade every suspect sector.
    total_c = 2000
    seq3 = good_range(500, 1500)
    with tempfile.TemporaryDirectory() as td:
        _write_file(td, 'disk_000.raw',
                    make_disk(seq3, total_c, corrupt_group=0, corrupt_byte=511))
        _write_file(td, 'disk_001.raw', make_disk(seq3, total_c))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, total_c, verbose=False)

        checks.append(('C upgrade happened', rep.upgrades > 0))
        checks.append(('C no suspects remain', rep.suspect_count == 0))
        checks.append(('C overlap counted',
                       len(rep.overlaps) == 1
                       and rep.overlaps[0].file == 'disk_001.raw'
                       and rep.overlaps[0].count == len(seq3)))

    # scenario D: test missing_lba_ranges with a mix of covered,
    # headskipped, read-failed, and truly missing sectors.
    total_d = 2000
    seq4 = good_range(100, 400)        # 100..399 covered
    seq4[50] = Descriptor(150, ST_HEADSKIP, 0)   # 150 headskipped
    seq4[100] = Descriptor(300, 0x04, 0)         # 300 read-failed
    # lba 0..99 never attempted, 400..1999 never attempted
    with tempfile.TemporaryDirectory() as td:
        _write_file(td, 'disk_000.raw', make_disk(seq4, total_d))
        out = os.path.join(td, 'out.img')
        rep = run_assembly(td, out, total_d, verbose=False)
        truly_missing, missing_or_skipped = missing_lba_ranges(rep)

        checks.append(('D truly_missing has gap at start',
                       truly_missing[0] == (0, 99)))
        checks.append(('D truly_missing has gap at end',
                       truly_missing[-1] == (400, 1999)))
        checks.append(('D truly_missing excludes headskipped',
                       all(150 not in range(g0, g1 + 1)
                           for g0, g1 in truly_missing)))
        checks.append(('D truly_missing excludes read-failed',
                       all(300 not in range(g0, g1 + 1)
                           for g0, g1 in truly_missing)))
        checks.append(('D missing_or_skipped includes headskipped',
                       any(150 in range(g0, g1 + 1)
                           for g0, g1 in missing_or_skipped)))
        checks.append(('D missing_or_skipped excludes read-failed',
                       all(300 not in range(g0, g1 + 1)
                           for g0, g1 in missing_or_skipped)))

    print()
    failed = False
    for name, passed in checks:
        print('  %-34s %s' % (name, 'PASS' if passed else 'FAIL'))
        failed |= not passed
    print('\nSELFTEST %s' % ('FAILED' if failed else 'PASSED'))
    sys.exit(1 if failed else 0)
