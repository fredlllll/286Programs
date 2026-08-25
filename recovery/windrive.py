"""Typed wrapper around kernel32 raw disk I/O."""

import ctypes
import sys
from ctypes import wintypes

# kernel32 constants
GENERIC_READ: int = 0x80000000
FILE_SHARE_READ: int = 0x1
FILE_SHARE_WRITE: int = 0x2
OPEN_EXISTING: int = 3
FILE_BEGIN: int = 0
IOCTL_LOCK_VOLUME: int = 0x00090018
INVALID_HANDLE_VALUE: int = -1

SECTOR_SIZE: int = 512


class WinDrive:
    """Typed wrapper around kernel32 raw disk I/O.

    Usage:
        with WinDrive('A') as drv:
            data = drv.read_sector(0)
    """

    _dll: ctypes.WinDLL
    _handle: int

    def __init__(self, letter: str) -> None:
        self._dll = ctypes.WinDLL('kernel32', use_last_error=True)
        self._configure_api()
        self._handle = self._open(letter)
        self.lock()

    def _configure_api(self) -> None:
        dll = self._dll
        dll.CreateFileW.restype = wintypes.HANDLE
        dll.CreateFileW.argtypes = [
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
            wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD,
            wintypes.HANDLE,
        ]
        dll.ReadFile.argtypes = [
            wintypes.HANDLE, wintypes.LPVOID, wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD), wintypes.LPVOID,
        ]
        dll.SetFilePointer.argtypes = [
            wintypes.HANDLE, ctypes.c_long,
            ctypes.POINTER(ctypes.c_long), wintypes.DWORD,
        ]
        dll.DeviceIoControl.argtypes = [
            wintypes.HANDLE, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD,
            wintypes.LPVOID, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
            wintypes.LPVOID,
        ]
        dll.CloseHandle.argtypes = [wintypes.HANDLE]

    def _open(self, letter: str) -> int:
        path = '\\\\.\\%s:' % letter.upper()
        handle = self._dll.CreateFileW(
            path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            None, OPEN_EXISTING, 0, None,
        )
        while handle == INVALID_HANDLE_VALUE:
            print('Cannot open %s (winerror %d). Is a disk inserted?' % (
                path, ctypes.get_last_error()))
            ans = input('Retry? [Y/n] ').strip().lower()
            if ans.startswith('n'):
                sys.exit(1)
            handle = self._dll.CreateFileW(
                path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                None, OPEN_EXISTING, 0, None,
            )
        return handle

    def lock(self) -> None:
        """Lock the volume for exclusive access."""
        self._dll.DeviceIoControl(
            self._handle, IOCTL_LOCK_VOLUME,
            None, 0, None, 0, None, None,
        )

    def seek(self, byte_offset: int) -> None:
        """Move the file pointer to byte_offset."""
        self._dll.SetFilePointer(self._handle, byte_offset, None, FILE_BEGIN)

    def read(self, byte_offset: int, size: int) -> bytes | None:
        """Read *size* bytes at *byte_offset*. Returns None on failure."""
        self.seek(byte_offset)
        buf = ctypes.create_string_buffer(size)
        got = wintypes.DWORD(0)
        ok = self._dll.ReadFile(
            self._handle, buf, size, ctypes.byref(got), None,
        )
        if ok and got.value == size:
            return buf.raw[:size]
        return None

    def read_sector(self, byte_offset: int) -> bytes | None:
        """Read one 512-byte sector. Returns None on failure."""
        return self.read(byte_offset, SECTOR_SIZE)

    def close(self) -> None:
        """Release the drive handle."""
        if self._handle != INVALID_HANDLE_VALUE:
            self._dll.CloseHandle(self._handle)
            self._handle = INVALID_HANDLE_VALUE

    def __enter__(self) -> 'WinDrive':
        return self

    def __exit__(self, exc_type: type | None, exc_val: BaseException | None,
                 exc_tb: object) -> None:
        self.close()
