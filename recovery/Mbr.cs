using Recovery;
using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Runtime.InteropServices;
using System.Text;

namespace recovery
{
    public class Mbr
    {
        public HddImage Hdd { get; }

        public readonly PartitionTableEntry[] tableEntries;

        public Mbr(HddImage hdd)
        {
            Hdd = hdd;
            tableEntries = new PartitionTableEntry[4];
        }

        public void Parse()
        {
            using var reader = new BinaryReader(Hdd.Stream, Encoding.ASCII, true);

            Console.WriteLine(Marshal.SizeOf<PartitionTableEntry>());
            reader.BaseStream.Position = 446;
            for (int i = 0; i < tableEntries.Length; ++i)
            {
                tableEntries[i] = reader.ReadStruct<PartitionTableEntry>();
            }
        }

        public void Report()
        {
            Console.WriteLine("--- Boot Sector ---");
            Console.WriteLine($"Partitions:       {tableEntries.Count(x=>x.type!= PartitionType.Empty_Hidden)}");
            for (int i = 0; i < tableEntries.Length; ++i)
            {
                var e = tableEntries[i];
                Console.WriteLine($"  Partition Table Entry {i+1}:");
                Console.WriteLine($"  Bootable:         {e.isBootable}");
                Console.WriteLine($"  First Sector CHS: {e.chsFirstSector}");
                Console.WriteLine($"  Type:             {e.type}(0x{(byte)e.type:X2})");
                Console.WriteLine($"  Last Sector CHS:  {e.chsLastSector}");
                Console.WriteLine($"  First Sector LBA: {e.firstSectorLba}");
                Console.WriteLine($"  Num Sectors LBA:  {e.numSectorsLba}");
            }
        }
    }
}
