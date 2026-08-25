"""On-disk format constants and parsing for the headerless v4 floppy layout.

Floppy layout (see watcomc/source/program.c):
  LBA 0..     repeating groups, one per hdd track batch:
                 [descriptor block: count byte + up to 101 entries of
                  lba(3 bytes le) + int13 status(1) + dataIdx(1),
                   padded, crc16 over bytes 0..505]
                 [the 512-byte data sectors whose status says data follows]

No third party dependencies, stdlib only.
"""

import struct
from collections.abc import Iterator

from structures import (ST_OK, ST_ECC, ST_HEADSKIP, has_data,
                        Descriptor, DescriptorBlock, FloppyEval)

SECTOR: int = 512
DISK_BYTES: int = 2880 * SECTOR

DESC_PER_BLOCK: int = 101       # 509 / sizeof(SectorDesc) on the 286
ENTRY_SIZE: int = 5             # lba24 + status8 + dataIdx8


INT13_ERRORS: dict[int, str] = {
    0x01: 'bad command', 0x02: 'address mark not found',
    0x03: 'write protected', 0x04: 'sector not found',
    0x06: 'media changed', 0x08: 'bad dma', 0x09: 'dma boundary',
    0x0c: 'media type unknown', 0x10: 'bad ecc on read',
    0x20: 'controller failure', 0x40: 'seek failed',
    0x80: 'timeout', 0xaa: 'drive not ready', 0xbb: 'undefined error',
    0xcc: 'write fault', 0xe0: 'status error',
}


def status_name(status: int) -> str:
    if status == ST_HEADSKIP:
        return 'head masked out'
    return INT13_ERRORS.get(status, 'bios error 0x%02x' % status)


# ---------------------------------------------------------------- crc16

_crc_table: list[int] = []
for _i in range(256):
    _c = _i << 8
    for _ in range(8):
        _c = (((_c << 1) ^ 0x1021) & 0xFFFF) if (_c & 0x8000) else ((_c << 1) & 0xFFFF)
    _crc_table.append(_c)


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    """CRC-16/CCITT-FALSE, identical to the 286 implementation."""
    tb = _crc_table
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ tb[((crc >> 8) ^ b) & 0xFF]
    return crc

# ---------------------------------------------------------------- groups


def iter_blocks(image: bytes) -> Iterator[DescriptorBlock]:
    """Walk the descriptor blocks of a dump image.

    Yields DescriptorBlock objects.  Stops at an all-zero block (the
    zero fill after the last group) or at a block whose crc/count is
    broken - everything behind such a block is uninterpretable anyway.
    """
    pos = 0
    while pos + SECTOR <= DISK_BYTES:
        block = image[pos:pos + SECTOR]
        if not any(block):
            break                                   # end-of-stream padding
        count = block[0]
        if count > DESC_PER_BLOCK:
            break                                   # corrupt, cannot trust
        crc_ok = struct.unpack_from('<H', block, 506)[0] \
            == crc16(bytes(block[0:506]))
        entries: list[Descriptor] = []
        for i in range(count):
            off = 1 + i * ENTRY_SIZE
            lba = int.from_bytes(block[off:off + 3], 'little')
            entries.append(Descriptor(lba, block[off + 3], block[off + 4]))
        ngood = sum(1 for e in entries if has_data(e.status))
        datas: list[bytes] = []
        dpos = pos + SECTOR
        for _k in range(ngood):
            if dpos + SECTOR > DISK_BYTES:
                break                               # truncated image
            datas.append(image[dpos:dpos + SECTOR])
            dpos += SECTOR
        yield DescriptorBlock(entries=entries, datas=datas, crc_ok=crc_ok)
        pos += SECTOR * (1 + ngood)


def evaluate_image(image: bytes) -> FloppyEval:
    """Validation of one 1.44MB dump (headerless format).

    Returns FloppyEval with:
      ok           group stream consistent (safe to place)
      reason       why not ok
      blocks       list of DescriptorBlock from iter_blocks()
      desc_total   descriptors actually found
      data_total   data sectors actually found
      blocks_bad   number of descriptor blocks failing their crc
    """
    if len(image) != DISK_BYTES:
        return FloppyEval(reason='unexpected size %d (want %d)' % (len(image), DISK_BYTES))

    blocks = list(iter_blocks(image))
    if not blocks:
        return FloppyEval(reason='no descriptor groups found (blank disk?)')

    desc_total = sum(len(b.entries) for b in blocks)
    data_total = sum(len(b.datas) for b in blocks)
    blocks_bad = sum(1 for b in blocks if not b.crc_ok)

    for b in blocks:
        if len(b.datas) < sum(1 for e in b.entries if has_data(e.status)):
            return FloppyEval(reason='group at lba %d truncated' % b.first_lba)

    return FloppyEval(ok=True, blocks=blocks, desc_total=desc_total,
                      data_total=data_total, blocks_bad=blocks_bad)
