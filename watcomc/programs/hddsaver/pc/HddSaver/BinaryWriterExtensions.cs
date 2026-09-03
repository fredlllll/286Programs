using HddSaver.Protocol;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace HddSaver
{
    public static class BinaryWriterExtensions
    {
        public static void WriteStruct<T>(this Stream stream, T obj) where T : unmanaged
        {
            Span<byte> buffer = stackalloc byte[Marshal.SizeOf<T>()];
            MemoryMarshal.Write(buffer, in obj);
            stream.Write(buffer);
        }

        public static void WriteStruct<T>(this BinaryWriter writer, T obj) where T : unmanaged
        {
            writer.BaseStream.WriteStruct(obj);
        }

        public static void WriteStructs<T>(this Stream stream, T[] values) where T : unmanaged
        {
            for (int i = 0; i < values.Length; ++i)
            {
                stream.WriteStruct(values[i]);
            }
        }

        public static void WriteStructs<T>(this BinaryWriter writer, T[] values) where T : unmanaged
        {
            writer.BaseStream.WriteStructs(values);
        }

        public static void Write(this BinaryWriter writer, Opcode opcode)
        {
            writer.Write((byte)opcode);
        }
    }
}
