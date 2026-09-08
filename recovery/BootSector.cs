using System.Text;

namespace Recovery;

/* the first 512-byte sector on any dos volume: boot code up front and
   the bios parameter block (bpb) from offset 11, which is what tells us
   where everything else lives. */
class BootSector
{
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

    public static BootSector Parse(HddImage img)
    {
        var b = new BootSector
        {
            BytesPerSector = img.ReadU16(11),
            SectorsPerCluster = img[13],
            ReservedSectors = img.ReadU16(14),
            NumFats = img[16],
            RootEntries = img.ReadU16(17),
            FatSizeSectors = img.ReadU16(22),
        };
        int total16 = img.ReadU16(19);
        b.TotalSectors = total16 > 0 ? total16 : img.ReadI32(32);
        b.VolumeLabel = Encoding.ASCII.GetString(img.Slice(43, 11)).Trim();
        b.FsType = Encoding.ASCII.GetString(img.Slice(54, 8)).Trim();

        b.FatStartSector = b.ReservedSectors;
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