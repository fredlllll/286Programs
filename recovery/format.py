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

SECTOR: int = 512
DISK_BYTES: int = 2880 * SECTOR

DESC_PER_BLOCK: int = 101       # 509 / sizeof(SectorDesc) on the 286
ENTRY_SIZE: int = 5             # lba24 + status8 + dataIdx8

ST_OK: int = 0x00               # read clean, data present
ST_ECC: int = 0x11              # read after ecc correction, data present
ST_HEADSKIP: int = 0xFE         # head masked out this pass, never attempted

# group dict keys: first_lba (int), entries (list[tuple[int,int,int]]),
#                  datas (list[bytes]), crc_ok (bool)
GroupDict = dict

# evaluate_image result dict: ok (bool), reason (str), groups (list[GroupDict]),
#                             desc_total (int), data_total (int), blocks_bad (int)
EvalDict = dict


def has_data(status: int) -> bool:
    """True when the descriptor's sector carries real data."""
    return status in (ST_OK, ST_ECC)


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


def iter_groups(image: bytes) -> Iterator[GroupDict]:
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
        entries: list[tuple[int, int, int]] = []
        for i in range(count):
            off = 1 + i * ENTRY_SIZE
            lba = int.from_bytes(block[off:off + 3], 'little')
            entries.append((lba, block[off + 3], block[off + 4]))
        ngood = sum(1 for _, st, _ in entries if has_data(st))
        datas: list[bytes] = []
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


def evaluate_image(image: bytes) -> EvalDict:
    """Validation of one 1.44MB dump (headerless format).

    Returns dict:
      ok           group stream consistent (safe to place)
      reason       why not ok
      groups       list from iter_groups()
      desc_total   descriptors actually found
      data_total   data sectors actually found
      blocks_bad   number of descriptor blocks failing their crc
    """
    res: EvalDict = {'ok': False, 'reason': '', 'groups': [],
                     'desc_total': 0, 'data_total': 0, 'blocks_bad': 0}
    if len(image) != DISK_BYTES:
        res['reason'] = 'unexpected size %d (want %d)' % (len(image), DISK_BYTES)
        return res

    groups = list(iter_groups(image))
    if not groups:
        res['reason'] = 'no descriptor groups found (blank disk?)'
        return res
    res['groups'] = groups
    res['desc_total'] = sum(len(g['entries']) for g in groups)
    res['data_total'] = sum(len(g['datas']) for g in groups)
    res['blocks_bad'] = sum(1 for g in groups if not g['crc_ok'])
    for g in groups:
        if len(g['datas']) < sum(1 for _, st, _ in g['entries'] if has_data(st)):
            res['reason'] = 'group at lba %d truncated' % g['first_lba']
            return res
    res['ok'] = True
    return res
