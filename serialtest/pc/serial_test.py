import sys
import ctypes
from ctypes import wintypes

kernel32 = ctypes.windll.kernel32

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
INVALID_HANDLE_VALUE = -1

PurgeRx = 0x0008
PurgeTx = 0x0004

DCB_BAUDRATE = 0x00000001
DCB_DATABITS = 0x00000004
DCB_STOPBITS = 0x00000008
DCB_PARITY   = 0x00000002

NOPARITY    = 0
ONESTOPBIT  = 0
EIGHTDBITS  = 8

class DCB(ctypes.Structure):
    _fields_ = [
        ("DCBlength", wintypes.DWORD),
        ("BaudRate", wintypes.DWORD),
        ("fBinary", wintypes.DWORD, 1),
        ("fParity", wintypes.DWORD, 1),
        ("fOutxCtsFlow", wintypes.DWORD, 1),
        ("fOutxDsrFlow", wintypes.DWORD, 1),
        ("fDtrControl", wintypes.DWORD, 2),
        ("fDsrSensitivity", wintypes.DWORD, 1),
        ("fTXContinueOnXoff", wintypes.DWORD, 1),
        ("fOutX", wintypes.DWORD, 1),
        ("fInX", wintypes.DWORD, 1),
        ("fErrorChar", wintypes.DWORD, 1),
        ("fNull", wintypes.DWORD, 1),
        ("fRtsControl", wintypes.DWORD, 2),
        ("fAbortOnError", wintypes.DWORD, 1),
        ("XonChar", ctypes.c_char),
        ("XoffChar", ctypes.c_char),
        ("XonLim", wintypes.WORD),
        ("XoffLim", wintypes.WORD),
        ("ByteSize", ctypes.c_byte),
        ("Parity", ctypes.c_byte),
        ("StopBits", ctypes.c_byte),
    ]

class COMMTIMEOUTS(ctypes.Structure):
    _fields_ = [
        ("ReadIntervalTimeout", wintypes.DWORD),
        ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
        ("ReadTotalTimeoutConstant", wintypes.DWORD),
        ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
        ("WriteTotalTimeoutConstant", wintypes.DWORD),
    ]

def open_serial(port, baud):
    name = "\\\\.\\" + port
    handle = kernel32.CreateFileA(
        name, GENERIC_READ | GENERIC_WRITE, 0, None,
        OPEN_EXISTING, 0, None)
    if handle == INVALID_HANDLE_VALUE:
        return None

    dcb = DCB()
    dcb.DCBlength = ctypes.sizeof(DCB)
    if not kernel32.GetCommState(handle, ctypes.byref(dcb)):
        kernel32.CloseHandle(handle)
        return None

    dcb.BaudRate = baud
    dcb.ByteSize = EIGHTDBITS
    dcb.Parity = NOPARITY
    dcb.StopBits = ONESTOPBIT
    dcb.fBinary = 1
    dcb.fParity = 0
    dcb.fOutxCtsFlow = 0
    dcb.fOutxDsrFlow = 0
    dcb.fDtrControl = 1
    dcb.fRtsControl = 1
    dcb.fOutX = 0
    dcb.fInX = 0

    if not kernel32.SetCommState(handle, ctypes.byref(dcb)):
        kernel32.CloseHandle(handle)
        return None

    timeouts = COMMTIMEOUTS()
    timeouts.ReadIntervalTimeout = 0xFFFFFFFF
    timeouts.ReadTotalTimeoutMultiplier = 0
    timeouts.ReadTotalTimeoutConstant = 5000
    kernel32.SetCommTimeouts(handle, ctypes.byref(timeouts))

    kernel32.PurgeComm(handle, PurgeRx | PurgeTx)
    return handle

def serial_write(handle, data):
    written = wintypes.DWORD(0)
    buf = ctypes.create_string_buffer(data)
    kernel32.WriteFile(handle, buf, len(data), ctypes.byref(written), None)
    return written.value

def serial_read(handle, count):
    buf = ctypes.create_string_buffer(count)
    read = wintypes.DWORD(0)
    kernel32.ReadFile(handle, buf, count, ctypes.byref(read), None)
    return buf.raw[:read.value]

def main():
    port = "COM1"
    if len(sys.argv) > 1:
        port = sys.argv[1]

    print("opening " + port + " at 9600 8N1...")
    handle = open_serial(port, 9600)
    if handle is None:
        print("failed to open " + port)
        sys.exit(1)

    print("sending ping...")
    serial_write(handle, b"ping")

    print("waiting for pong...")
    response = serial_read(handle, 4)

    if response == b"pong":
        print("SUCCESS: got pong!")
    else:
        print("FAIL: expected 'pong', got " + repr(response))

    kernel32.CloseHandle(handle)

if __name__ == "__main__":
    main()
