using System;
using System.Collections.Generic;
using System.Reflection.Metadata.Ecma335;
using System.Runtime.InteropServices;
using System.Text;

namespace recovery
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PartitionTableEntry
    {
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct PartitionChs
        {
            public byte _bitfield1;
            public byte _bitfield2;
            public byte _bitfield3;

            public ushort Cylinder { get { return (ushort)(((_bitfield2 & 0xC0) << 2) | _bitfield3); } }
            public byte Head { get { return _bitfield1; } }
            public byte Sector { get { return (byte)(_bitfield2 & 0x3F); } }
            

            public override string ToString()
            {
                return $"{Cylinder}/{Head}/{Sector} (0x{_bitfield1:X2},0x{_bitfield2:X2},0x{_bitfield3:X2})";
            }
        }

        public byte isBootable;
        public PartitionChs chsFirstSector;
        public PartitionType type;
        public PartitionChs chsLastSector;
        public uint firstSectorLba;
        public uint numSectorsLba;
    }

    public enum PartitionType :byte{
    Empty_Hidden = 0,
    Fat12 = 0x01,
    Fat16Small = 0x04,
    ExtendedPartition = 0x05,
    Fat16Big = 0x06,
    NTFS = 0x07,
    Fat32Chs = 0x0B,
    Fat32Lba = 0x0C,
    Fat16BigLba = 0x0E,
    ExtendedPartitionLba = 0x0F,
    OEM_Hidden = 0x12,
    }
}
