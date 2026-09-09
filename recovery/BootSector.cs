using System.Text;

namespace Recovery;

/* the first 512-byte sector on any dos volume: boot code up front and
   the bios parameter block (bpb) from offset 11, which is what tells us
   where everything else lives.

   the volume's boot sector is not image sector 0 - sector 0 is the
   master boot record, whose partition table points at the volume. the
   boot sector sits at that partition's start, and every layout field
   below is an absolute image sector number so downstream readers can
   index the raw image directly. */
class BootSector
{
    public uint PartitionStartSector;
    public int BytesPerSector;
    public int SectorsPerCluster;
    public int ReservedSectors;
    public int NumFats;
    public int RootEntries;
    public int TotalSectors;
    public int FatSizeSectors;
    public string VolumeLabel = "";
    public string FsType = "";
    public string FatType = "";
    public long FatStartSector;
    public long RootDirStartSector;
    public long DataStartSector;

    public int RootDirSectors => (RootEntries * 32 + BytesPerSector - 1) / BytesPerSector;
    public int TotalClusters => (TotalSectors - ReservedSectors - FatSizeSectors * NumFats - RootDirSectors) / SectorsPerCluster;

    public static BootSector Parse(HddImage img, uint partitionStartLba)
    {
        int pb = (int)((long)partitionStartLba * Geometry.BytesPerSector);
        if (pb >= img.Length)
        {
            throw new InvalidDataException($"partition at lba {partitionStartLba} starts beyond the end of the image");
        }

        var b = new BootSector
        {
            PartitionStartSector = partitionStartLba,
            BytesPerSector = img.ReadU16(pb + 11),
            SectorsPerCluster = img[pb + 13],
            ReservedSectors = img.ReadU16(pb + 14),
            NumFats = img[pb + 16],
            RootEntries = img.ReadU16(pb + 17),
            FatSizeSectors = img.ReadU16(pb + 22),
        };
        int total16 = img.ReadU16(pb + 19);
        b.TotalSectors = total16 > 0 ? total16 : img.ReadI32(pb + 32);
        b.VolumeLabel = Encoding.ASCII.GetString(img.Slice(pb + 43, 11)).Trim();
        b.FsType = Encoding.ASCII.GetString(img.Slice(pb + 54, 8)).Trim();

        /* sanity-check that this really is a fat bpb: a damaged mbr can
           point the reader at the middle of anything. everything below
           assumes 512-byte sectors, so refuse anything else. */
        bool spcPow2 = b.SectorsPerCluster > 0 && (b.SectorsPerCluster & (b.SectorsPerCluster - 1)) == 0;
        if (b.BytesPerSector != Geometry.BytesPerSector ||
            !spcPow2 ||
            b.ReservedSectors == 0 ||
            b.NumFats == 0 || b.NumFats > 4 ||
            b.FatSizeSectors == 0)
        {
            throw new InvalidDataException($"bytes at partition lba {partitionStartLba} don't look like a fat boot sector");
        }

        b.FatStartSector = partitionStartLba + b.ReservedSectors;
        b.RootDirStartSector = b.FatStartSector + (long)b.FatSizeSectors * b.NumFats;
        b.DataStartSector = b.RootDirStartSector + b.RootDirSectors;

        b.FatType = b.TotalClusters <= 4085 ? "FAT12"
                  : b.TotalClusters <= 65525 ? "FAT16"
                  : "FAT32";
        return b;
    }

    public void Report()
    {
        Console.WriteLine("--- Boot Sector ---");
        Console.WriteLine($"  Partition start:    sector {PartitionStartSector}");
        Console.WriteLine($"  Bytes/sector:       {BytesPerSector}");
        Console.WriteLine($"  Sectors/cluster:    {SectorsPerCluster}");
        Console.WriteLine($"  Reserved sectors:   {ReservedSectors}");
        Console.WriteLine($"  Number of FATs:     {NumFats}");
        Console.WriteLine($"  Root dir entries:   {RootEntries}");
        Console.WriteLine($"  Total sectors:      {TotalSectors}");
        Console.WriteLine($"  FAT size:           {FatSizeSectors} sectors");
        Console.WriteLine($"  Volume label:       {VolumeLabel}");
        Console.WriteLine($"  FS type:            {FsType}");
        Console.WriteLine($"  FAT type:           {FatType}");
        Console.WriteLine($"  Total clusters:     {TotalClusters}");
        Console.WriteLine("--- Layout ---");
        Console.WriteLine($"  FAT start:          sector {FatStartSector}");
        Console.WriteLine($"  Root dir:           sector {RootDirStartSector}");
        Console.WriteLine($"  Data area:          sector {DataStartSector}");
    }
}