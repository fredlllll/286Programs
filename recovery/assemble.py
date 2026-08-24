"""Stitch all dump floppies into one hdd.img and write a report."""

import glob
import os
import sys
import time

from format import SECTOR, ST_HEADSKIP, has_data, status_name, evaluate_image


def load_dumps(dumps_dir):
    """Load all .raw files from dumps_dir. Returns (records, rejected)."""
    records = []
    rejected = []
    for fn in sorted(glob.glob(os.path.join(dumps_dir, '*.raw'))):
        with open(fn, 'rb') as f:
            image = f.read()
        ev = evaluate_image(image)
        base = os.path.basename(fn)
        if not ev['ok']:
            rejected.append((base, ev['reason']))
            continue
        records.append({'file': base, 'ev': ev})
    return records, rejected


def run_assembly(dumps_dir, out_path, total, verbose=True):
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
    records, rejected = load_dumps(dumps_dir)
    if not records:
        sys.exit('No valid dump disks found in %s' % dumps_dir)

    image_out = bytearray(total * SECTOR)
    covered = bytearray(total)
    suspect = bytearray(total)
    err_codes = {}        # lba -> bios status of the failed read
    headskipped_lbas = set()
    overlaps = []
    upgrades_total = 0

    for rec in records:
        ov = 0
        upgraded = 0
        for g in rec['ev']['groups']:
            trust = g['crc_ok']
            for lba, st, didx in g['entries']:
                if lba >= total:
                    continue
                if has_data(st):
                    if didx < len(g['datas']):
                        sec = g['datas'][didx]
                        if covered[lba]:
                            ov += 1
                            if trust and suspect[lba]:
                                suspect[lba] = 0
                                image_out[lba * SECTOR:(lba + 1) * SECTOR] = sec
                                upgraded += 1
                            continue
                        image_out[lba * SECTOR:(lba + 1) * SECTOR] = sec
                    if covered[lba]:
                        ov += 1
                        continue
                    covered[lba] = 1
                    if not trust:
                        suspect[lba] = 1
                elif st == ST_HEADSKIP:
                    headskipped_lbas.add(lba)
                else:
                    prev = err_codes.get(lba)
                    if prev is None or st < prev:
                        err_codes[lba] = st
        if ov:
            overlaps.append((rec['file'], ov))
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
        'chosen_files': [r['file'] for r in records],
        'err_codes': err_codes,
        'headskipped_lbas': headskipped_lbas,
        'suspect_count': sum(suspect),
        'coverage': coverage,
        'overlaps': overlaps,
        'upgrades': upgrades_total,
        'complete': not gaps,
        'total': total,
        'rejected': rejected,
        'chosen_records': records,
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
    ap('%-24s %-9s %-9s %-7s' %
       ('file', 'lbas', 'data', 'blocks'))
    for rec in rep['chosen_records']:
        ap('%-24s %-9d %-9d %-7d' % (
            rec['file'], rec['ev']['desc_total'], rec['ev']['data_total'],
            rec['ev']['blocks_bad']))
    if rep['rejected']:
        ap('')
        ap('files rejected (no usable data):')
        for fn, why in rep['rejected']:
            ap('  %-24s %s' % (fn, why))
    if rep['overlaps']:
        ap('')
        ap('overlapping placements (first valid wins):')
        for fn, ov in rep['overlaps']:
            ap('  %s: %d sectors already covered' % (fn, ov))
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
                      'INCOMPLETE - missing ranges'))
    with open(path, 'w') as f:
        f.write('\n'.join(lines) + '\n')


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
