namespace HddSaver.Protocol;

public static class Command
{
    public const byte START = 0x01;
    public const byte STOP = 0x02;
    public const byte SEEK = 0x03;
    public const byte PING = 0x04;
    public const byte STATUS = 0x05;
    public const byte HEAD_MASK = 0x10;
    public const byte RETRIES = 0x11;
    public const byte BAUD_RATE = 0x12;

    public const byte ACK = 0x06;
    public const byte NAK = 0x15;
    public const byte READY = 0x06;

    public const byte HEADER_MAGIC0 = 0xAA;
    public const byte HEADER_MAGIC1 = 0x55;

    public const int HeaderSize = 10;
    public const int DataSize = 512;
    public const int StatusReplySize = 22;
}
