using HddSaver.Protocol;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace HddSaver
{
    public static class BinaryReaderExtensions
    {
        public static T ReadStruct<T>(this Stream stream) where T : unmanaged
        {
            Span<byte> buffer = stackalloc byte[Marshal.SizeOf<T>()];
            int read = stream.Read(buffer);
            if (read < buffer.Length)
                throw new EndOfStreamException($"Not enough bytes to read {typeof(T).Name}");
            return MemoryMarshal.Read<T>(buffer);
        }

        public static T ReadStruct<T>(this BinaryReader reader) where T : unmanaged
        {
            return reader.BaseStream.ReadStruct<T>();
        }

        public static T[] ReadStructs<T>(this Stream stream, int count) where T : unmanaged
        {
            T[] values = new T[count];
            for (int i = 0; i < count; ++i)
            {
                values[i] = stream.ReadStruct<T>();
            }
            return values;
        }

        public static T[] ReadStructs<T>(this BinaryReader reader, int count) where T : unmanaged
        {
            return reader.BaseStream.ReadStructs<T>(count);
        }

        public static Opcode ReadOpcode(this BinaryReader reader)
        {
            return (Opcode)reader.ReadByte();
        }
    }
}
