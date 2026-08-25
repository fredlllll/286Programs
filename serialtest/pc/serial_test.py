import sys
import serial

def main():
    port = "COM1"
    if len(sys.argv) > 1:
        port = sys.argv[1]

    print("opening " + port + " at 9600 8N1...")
    try:
        ser = serial.Serial(port, 9600, timeout=5)
    except serial.SerialException as e:
        print("failed to open " + port + ": " + str(e))
        print("make sure the port exists and is not in use")
        sys.exit(1)

    ser.reset_input_buffer()

    print("sending ping...")
    ser.write(b"ping")

    print("waiting for pong...")
    response = ser.read(4)

    if response == b"pong":
        print("SUCCESS: got pong!")
    else:
        print("FAIL: expected 'pong', got " + repr(response))

    ser.close()

if __name__ == "__main__":
    main()
