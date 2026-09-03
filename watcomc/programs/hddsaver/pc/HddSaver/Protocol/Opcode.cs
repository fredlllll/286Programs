using System;
using System.Collections.Generic;
using System.Text;

namespace HddSaver.Protocol
{
    public enum Opcode : byte
    {
        None = 0,
        Start = 0x01,
        Stop = 0x02,
        Seek = 0x03,
        HeadMask = 0x04,
        Retries = 0x05,
        Ping = 0x06,
        Pong = 0x07,
        SendStatus = 0x08,
        Status = 0x09,
        Sector = 0x0A,


        Ack = 0xFE,
        Nack = 0xCC,
    }
}
