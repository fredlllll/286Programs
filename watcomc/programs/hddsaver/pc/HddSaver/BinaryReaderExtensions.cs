using HddSaver.Protocol;
using System;
using System.IO;
using System.Runtime.InteropServices;

namespace HddSaver
{
    public static class BinaryReaderExtensions
    {
        /// <summary>
        /// Reads exactly count bytes. Each byte goes through <see cref="BinaryReader.ReadByte"/>,
        /// which on a serial port blocks (honoring ReadTimeout) instead of returning 0 like a
        /// multi-byte <c>Stream.Read</c> on <see cref="SerialPort.BaseStream"/> would.
        /// </summary>
        public static byte[] ReadBytesExactly(this BinaryReader reader, int count)
        {
            byte[] data = new byte[count];
            for (int i = 0; i < count; i++)
                data[i] = reader.ReadByte();
            return data;
        }

        public static uint ReadUInt32Exactly(this BinaryReader reader)
        {
            Span<byte> buffer = stackalloc byte[4];
            for (int i = 0; i < buffer.Length; i++)
                buffer[i] = reader.ReadByte();
            return MemoryMarshal.Read<uint>(buffer);
        }

        public static T ReadStruct<T>(this BinaryReader reader) where T : unmanaged
        {
            Span<byte> buffer = stackalloc byte[Marshal.SizeOf<T>()];
            for (int i = 0; i < buffer.Length; i++)
                buffer[i] = reader.ReadByte();
            return MemoryMarshal.Read<T>(buffer);
        }

        public static Opcode ReadOpcode(this BinaryReader reader)
        {
            return (Opcode)reader.ReadByte();
        }
    }
}