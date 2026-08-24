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
  python process_floppy.py assemble --dumps dumps --total 122880 --out hdd.img

No third party dependencies, stdlib only.
"""

import argparse

from drive import cmd_read
from assemble import run_assembly
from selftest import cmd_selftest
from emutest import cmd_mkpattern, cmd_verify
from badmap import cmd_badmap


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
    p.add_argument('--total', type=int, required=True,
                   help='total HDD sectors (e.g. 122880 for 60MB)')
    p.set_defaults(fn=lambda a: run_assembly(a.dumps, a.out, a.total))

    p = sub.add_parser('selftest', help='synthetic round-trip test')
    p.set_defaults(fn=cmd_selftest)

    p = sub.add_parser('mkpattern',
                       help='create a fake HDD image for emulator testing')
    p.add_argument('--out', default='fake_hdd.img')
    p.add_argument('--total', type=int, default=122880,
                   help='sector count (default: full 60MB drive)')
    p.set_defaults(fn=cmd_mkpattern)

    p = sub.add_parser('verify',
                       help='check assembled hdd.img against mkpattern data')
    p.add_argument('--image', default='hdd.img')
    p.add_argument('--dumps', default='dumps')
    p.add_argument('--total', type=int, default=122880)
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
