using Recovery;
using System;
using System.Linq;
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

            reader.BaseStream.Position = 446;
            for (int i = 0; i < tableEntries.Length; ++i)
            {
                tableEntries[i] = reader.ReadStruct<PartitionTableEntry>();
            }
        }

        /* picks the fat partition to recover from: first non-empty
           entry, preferring the bootable one if several exist. returns
           null when there is nothing usable (all empty, extended only,
           or unknown types). */
        public PartitionTableEntry? PickFatPartition()
        {
            PartitionTableEntry? best = null;
            foreach (var e in tableEntries)
            {
                if (e.type == PartitionType.Empty_Hidden || !IsFatType(e.type))
                {
                    continue;
                }

                if (best == null || (e.isBootable != 0 && best.Value.isBootable == 0))
                {
                    best = e;
                }
            }
            return best;
        }

        private static bool IsFatType(PartitionType t) =>
            t == PartitionType.Fat12 ||
            t == PartitionType.Fat16Small ||
            t == PartitionType.Fat16Big ||
            t == PartitionType.Fat16BigLba ||
            t == PartitionType.Fat32Chs ||
            t == PartitionType.Fat32Lba;

        public void Report()
        {
            Console.WriteLine("--- MBR ---");
            Console.WriteLine($"Partitions:       {tableEntries.Count(x => x.type != PartitionType.Empty_Hidden)}");
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
