"""SVG bad-sector map generator."""

import argparse

from structures import has_data, ST_HEADSKIP
from assemble import load_dumps


def build_badmap_state(dumps_dir: str, cyls: int, heads: int,
                       spt: int) -> tuple[bytearray, int, int, int]:
    """Per-LBA state byte + counters, merged across ALL dumps.

    Priority per lba: readable beats everything (a later pass may have
    recovered it); otherwise a logged bios error; otherwise head-skip;
    otherwise untouched.

    Returns (state, nerr, nskip, ndisks).
    """
    records, _rejected = load_dumps(dumps_dir)
    total = cyls * heads * spt
    state = bytearray(total)          # 0 = untouched
    for rec in records:
        for blk in rec.ev.blocks:
            for e in blk.entries:
                if e.lba >= total:
                    continue
                if has_data(e.status):
                    state[e.lba] = 1
                elif e.status == ST_HEADSKIP:
                    if state[e.lba] == 0:
                        state[e.lba] = 3
                else:
                    if state[e.lba] < 2:
                        state[e.lba] = 2
    nerr = sum(1 for l in range(total) if state[l] == 2)
    nskip = sum(1 for l in range(total) if state[l] == 3)
    return state, nerr, nskip, len(records)


MAP_COLORS: dict[int, str] = {
    0: '#bdbdbd',   # never attempted
    1: '#2ea44f',   # readable
    2: '#d93025',   # hdd read failed (bios error logged in descriptor)
    3: '#e08a00',   # head masked out this pass, awaiting another run
}
MAP_NAMES: dict[int, str] = {
    0: 'not attempted', 1: 'readable', 2: 'hdd read failed',
    3: 'head masked',
}


def cmd_badmap(args: argparse.Namespace) -> None:
    cyls, heads, spt = args.cyls, args.heads, args.spt
    state, nerr, nskip, ndisks = build_badmap_state(args.dumps, cyls, heads, spt)
    CW, CH = 3, 12                    # pixel size of one sector cell
    ML, MT = 56, 78                   # left margin / top offset
    panel_gap, tick_h = 18, 14
    W = ML + cyls * CW + 14
    panel_h = spt * CH + tick_h
    Ht = MT + heads * (panel_h + panel_gap)

    out: list[str] = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="%d" '
        'height="%d" font-family="monospace">' % (W, Ht),
        '<rect width="100%%" height="100%%" fill="#fafafa"/>',
        '<text x="%d" y="24" font-size="15">Tandon HDD bad-sector map '
        '(from %d floppies)</text>' % (ML, ndisks),
    ]
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

    for h in range(heads):
        y0 = MT + h * (panel_h + panel_gap)
        out.append('<text x="%d" y="%d" font-size="12" fill="#222">'
                   'Head %d</text>' % (ML, y0 + 10, h))
        gy = y0 + 16
        for c in range(0, cyls + 1, 100):
            out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" '
                       'stroke="#dddddd"/>' % (ML + c * CW, gy,
                                               ML + c * CW, gy + spt * CH))
        for sec in range(spt):
            ry = gy + sec * CH
            x = 0
            while x < cyls:
                st = state[(x * heads + h) * spt + sec]
                x2 = x + 1
                while x2 < cyls and state[(x2 * heads + h) * spt + sec] == st:
                    x2 += 1
                out.append('<rect x="%d" y="%d" width="%d" height="%d" '
                           'fill="%s"/>' % (ML + x * CW, ry, (x2 - x) * CW,
                                            CH, MAP_COLORS[st]))
                x = x2
        for c in range(0, cyls, 100):
            out.append('<text x="%d" y="%d" font-size="9" fill="#666">%d'
                       '</text>' % (ML + c * CW, gy + spt * CH + 11, c))

    out.append('</svg>')
    with open(args.out, 'w') as f:
        f.write('\n'.join(out))
    print('wrote %s' % args.out)
    per = [[0, 0, 0, 0] for _ in range(heads)]
    for lba, st in enumerate(state):
        per[(lba // spt) % heads][st] += 1
    print('per head:  ok / read-failed / head-masked / not-attempted')
    for h in range(heads):
        p = per[h]
        print('  head %d: %6d / %5d / %5d / %6d'
              % (h, p[1], p[2], p[3], p[0]))
    if nerr:
        print('\nreading tip: failure runs ending right before multiples '
              'of %d (track boundaries)\nlook like format-time defect '
              'sparing, random scatter looks like decay.' % spt)
