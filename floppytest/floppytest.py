#!/usr/bin/env python3
"""
Floppy disk surface test.

Writes a deterministic pattern to every sector of a 1.44MB floppy,
reads it back, and reports any mismatches. Requires admin privileges
on Windows for raw disk access.

Usage:
    python floppytest.py --drive A          # full write + read test
    python floppytest.py --drive A --read   # read-only (no write, just dump)
    python floppytest.py --drive A --write  # write-only (no read-back verify)
"""

import argparse
import ctypes
import ctypes.wintypes as wintypes
import os
import sys
import time

# -- Windows constants --------------------------------------------------------

GENERIC_READ            = 0x80000000
GENERIC_WRITE           = 0x40000000
FILE_SHARE_READ         = 0x00000001
FILE_SHARE_WRITE        = 0x00000002
OPEN_EXISTING           = 3
FILE_BEGIN              = 0
INVALID_HANDLE_VALUE    = -1

IOCTL_LOCK_VOLUME       = 0x00090018
IOCTL_UNLOCK_VOLUME     = 0x0009001C

SECTOR_SIZE             = 512
DISK_SECTORS            = 2880
DISK_BYTES              = DISK_SECTORS * SECTOR_SIZE

# -- Kernel32 wrappers --------------------------------------------------------

dll = ctypes.WinDLL('kernel32', use_last_error=True)


def _check(cond, msg):
    if not cond:
        err = ctypes.get_last_error()
        raise OSError(f"{msg} (error {err})")


class FloppyDrive:
    """Low-level raw read/write access to a Windows floppy drive."""

    def __init__(self, letter):
        self.letter = letter.upper()
        self._handle = None
        self._open()

    def _open(self):
        path = f'\\\\.\\{self.letter}:'
        h = dll.CreateFileW(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            None,
            OPEN_EXISTING,
            0,
            None,
        )
        if h == INVALID_HANDLE_VALUE:
            err = ctypes.get_last_error()
            raise OSError(
                f"Cannot open {path} (error {err}). "
                "Are you running as Administrator?"
            )
        self._handle = h

    def lock(self):
        ok = dll.DeviceIoControl(
            self._handle, IOCTL_LOCK_VOLUME,
            None, 0, None, 0, None, None,
        )
        if not ok:
            err = ctypes.get_last_error()
            raise OSError(f"Lock volume failed (error {err})")

    def unlock(self):
        dll.DeviceIoControl(
            self._handle, IOCTL_UNLOCK_VOLUME,
            None, 0, None, 0, None, None,
        )

    def seek(self, byte_offset):
        result = dll.SetFilePointer(
            self._handle, byte_offset, None, FILE_BEGIN,
        )
        if result == INVALID_HANDLE_VALUE:
            err = ctypes.get_last_error()
            raise OSError(f"Seek to {byte_offset} failed (error {err})")

    def read_raw(self, size):
        buf = ctypes.create_string_buffer(size)
        n = wintypes.DWORD()
        ok = dll.ReadFile(self._handle, buf, size, ctypes.byref(n), None)
        if not ok:
            err = ctypes.get_last_error()
            raise OSError(f"Read failed (error {err})")
        return buf.raw[:n.value]

    def write_raw(self, data):
        buf = ctypes.create_string_buffer(data)
        n = wintypes.DWORD()
        ok = dll.WriteFile(self._handle, buf, len(data), ctypes.byref(n), None)
        if not ok:
            err = ctypes.get_last_error()
            raise OSError(f"Write failed (error {err})")
        if n.value != len(data):
            raise OSError(f"Short write: {n.value}/{len(data)} bytes")
        return n.value

    def read_sector(self, sector_lba):
        self.seek(sector_lba * SECTOR_SIZE)
        return self.read_raw(SECTOR_SIZE)

    def write_sector(self, sector_lba, data):
        assert len(data) == SECTOR_SIZE
        self.seek(sector_lba * SECTOR_SIZE)
        self.write_raw(data)

    def close(self):
        if self._handle is not None:
            self.unlock()
            dll.CloseHandle(self._handle)
            self._handle = None

    def __enter__(self):
        self.lock()
        return self

    def __exit__(self, *exc):
        self.close()


# -- Test pattern --------------------------------------------------------------

def make_sector_data(sector_lba):
    """
    Generate a deterministic 512-byte pattern for a given sector.
    First 8 bytes: sector number (little-endian uint32 x2 for padding).
    Remaining 504 bytes: repeating 4-byte counter based on sector + offset.
    Easy to visually spot and easy to verify programmatically.
    """
    buf = bytearray(SECTOR_SIZE)
    # sector number in first 4 bytes
    buf[0] = sector_lba & 0xFF
    buf[1] = (sector_lba >> 8) & 0xFF
    buf[2] = (sector_lba >> 16) & 0xFF
    buf[3] = (sector_lba >> 24) & 0xFF
    # pad / duplicate
    buf[4:8] = buf[0:4]
    # fill rest with a predictable pattern
    for i in range(8, SECTOR_SIZE, 4):
        val = (sector_lba * 0x10000 + i) & 0xFFFFFFFF
        buf[i]   = val & 0xFF
        buf[i+1] = (val >> 8) & 0xFF
        buf[i+2] = (val >> 16) & 0xFF
        buf[i+3] = (val >> 24) & 0xFF
    return bytes(buf)


def verify_sector(sector_lba, actual):
    expected = make_sector_data(sector_lba)
    return expected == actual


# -- Progress helper -----------------------------------------------------------

def print_progress(label, sectors_done, total, start_time, errors):
    elapsed = time.time() - start_time
    pct = sectors_done / total * 100
    rate = (sectors_done * SECTOR_SIZE) / 1024 / max(elapsed, 0.001)
    err_count = len(errors)
    sys.stdout.write(
        f"\r  {label} {pct:5.1f}%  "
        f"({sectors_done}/{total} sectors)  "
        f"{rate:6.1f} KB/s  "
        f"errors: {err_count}    "
    )
    sys.stdout.flush()


# -- Main test logic -----------------------------------------------------------

def write_floppy(drv):
    """Write the test pattern to every sector on the floppy."""
    print(f"Writing test pattern to {drv.letter}: ...")
    errors = []
    t0 = time.time()
    for lba in range(DISK_SECTORS):
        data = make_sector_data(lba)
        try:
            drv.write_sector(lba, data)
        except OSError as e:
            errors.append((lba, str(e)))
        if (lba + 1) % 10 == 0 or lba == DISK_SECTORS - 1:
            print_progress("WRITE", lba + 1, DISK_SECTORS, t0, errors)
    print()
    return errors


def read_floppy(drv):
    """Read back every sector and verify the pattern."""
    print(f"Reading {drv.letter}: ...")
    errors = []
    mismatches = []
    t0 = time.time()
    for lba in range(DISK_SECTORS):
        try:
            data = drv.read_sector(lba)
            if len(data) < SECTOR_SIZE:
                errors.append((lba, f"short read: {len(data)} bytes"))
            elif not verify_sector(lba, data):
                mismatches.append(lba)
                errors.append((lba, "data mismatch"))
        except OSError as e:
            errors.append((lba, str(e)))
        if (lba + 1) % 10 == 0 or lba == DISK_SECTORS - 1:
            print_progress("READ ", lba + 1, DISK_SECTORS, t0, errors)
    print()
    return errors, mismatches


def dump_sectors(drv):
    """Read every sector and print a hex preview of the first 32 bytes."""
    print(f"Reading {drv.letter}: (dump mode)")
    t0 = time.time()
    for lba in range(DISK_SECTORS):
        data = drv.read_sector(lba)
        hex_str = ' '.join(f'{b:02X}' for b in data[:32])
        print(f"  LBA {lba:4d}: {hex_str}")
        if (lba + 1) % 100 == 0:
            elapsed = time.time() - t0
            pct = (lba + 1) / DISK_SECTORS * 100
            print(f"  ... {pct:.0f}% ({elapsed:.1f}s)")
    print("Done.")


def main():
    ap = argparse.ArgumentParser(
        description="Floppy disk surface test: write pattern, read back, verify.",
    )
    ap.add_argument('--drive', '-d', default='A',
                    help="Floppy drive letter (default: A)")
    ap.add_argument('--read', action='store_true',
                    help="Read-only mode (no writes)")
    ap.add_argument('--write', action='store_true',
                    help="Write-only mode (no read-back verify)")
    ap.add_argument('--quick', type=int, default=0,
                    help="Only test N sectors (for quick smoke test)")
    args = ap.parse_args()

    if args.read and args.write:
        ap.error("Cannot specify both --read and --write")

    if os.name != 'nt':
        ap.error("Floppy access requires Windows (raw disk via kernel32)")

    if args.quick:
        global DISK_SECTORS, DISK_BYTES
        DISK_SECTORS = args.quick
        DISK_BYTES = DISK_SECTORS * SECTOR_SIZE

    letter = args.drive.upper()
    total = DISK_SECTORS * SECTOR_SIZE

    print(f"Floppy Test: drive {letter}:  ({DISK_SECTORS} sectors, {total} bytes)")

    try:
        drv = FloppyDrive(letter)
    except OSError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    try:
        with drv:
            if args.read:
                dump_sectors(drv)
                return

            # Write phase
            if not args.write:
                write_errors = write_floppy(drv)
                if write_errors:
                    print(f"\n*** Write phase had {len(write_errors)} error(s):")
                    for lba, msg in write_errors:
                        print(f"    LBA {lba}: {msg}")
                else:
                    print("Write phase complete. No errors.")

            # Read-back verify phase
            if not args.write:
                # Flush / re-seek by reopening context (unlock/lock cycle)
                pass
                read_errors, mismatches = read_floppy(drv)

                if read_errors:
                    print(f"\n*** Read phase had {len(read_errors)} error(s):")
                    for lba, msg in read_errors:
                        print(f"    LBA {lba}: {msg}")
                else:
                    print("Read-back verified. All sectors match.")

                if mismatches:
                    print(f"\n*** FAIL: {len(mismatches)} sector(s) had data mismatches")
                    sys.exit(1)
                else:
                    print("\nPASS: Floppy surface test OK.")
    except OSError as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
