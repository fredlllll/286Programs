using System;
using System.Collections.Generic;
using System.Text;

namespace HddSaver
{
    public static class Crc
    {
        public static ushort Crc16(ReadOnlySpan<byte> data)
        {
            ushort crc = 0xFFFF;
            foreach (var b in data)
            {
                crc = (ushort)((crc << 8) ^ CrcTable[((crc >> 8) ^ b) & 0xFF]);
            }
            return crc;
        }

        private static readonly ushort[] CrcTable = GenerateCrcTable();

        private static ushort[] GenerateCrcTable()
        {
            var table = new ushort[256];
            for (int i = 0; i < 256; i++)
            {
                ushort c = (ushort)(i << 8);
                for (int j = 0; j < 8; j++)
                {
                    c = (c & 0x8000) != 0
                        ? (ushort)((c << 1) ^ 0x1021)
                        : (ushort)(c << 1);
                }
                table[i] = c;
            }
            return table;
        }
    }
}
