using recovery;
using System.Text;

namespace Recovery;

/* scans the root directory sector range into a flat list. the reader
   can later walk subdirectories by chaining their first cluster, but the
   immediate target is the filesystem root shown by the original tool. */
class DirectoryReader
{
    public List<DirEntry> Entries = new();

    public static DirectoryReader Read(HddImage img, BootSector boot, FatTable fat)
    {
        var reader = new DirectoryReader();
        using var br = new BinaryReader(img.Stream, Encoding.ASCII, true);
        br.BaseStream.Position = (long)boot.RootDirStartSector * boot.BytesPerSector;

        int slots = boot.BytesPerSector / 32;
        for (int s = 0; s < boot.RootDirSectors; s++)
        {
            for (int e = 0; e < slots; e++)
            {
                byte[] slot = br.ReadBytesExactly(32);
                if (slot[0] == 0x00)
                {
                    break; // end of directory
                }
                if (slot[11] == 0x0F)
                {
                    continue; // LFN shadow entry, skip and keep scanning
                }

                reader.Entries.Add(DirEntry.Parse(slot));
            }
        }
        return reader;
    }

    public void Report()
    {
        var files = Entries.Where(e => !e.IsDeleted && !e.IsDirectory && !e.IsVolumeLabel).ToList();
        var deleted = Entries.Where(e => e.IsDeleted).ToList();
        var dirs = Entries.Where(e => e.IsDirectory).ToList();

        Console.WriteLine("--- Root Directory ---");
        Console.WriteLine($"  Dirs:     {dirs.Count}");
        Console.WriteLine($"  Files:    {files.Count}");
        Console.WriteLine($"  Deleted:  {deleted.Count}");
        Console.WriteLine();
        Console.WriteLine($"  {"Name",-12} {"Size",8} {"Start",6} {"Clusters",10} Attr");
        Console.WriteLine($"  {"────",-12} {"────",8} {"─────",6} {"────────",10} ────");

        foreach (var e in files)
        {
            Console.WriteLine($"  {e.Name,-12} {e.FileSize,8} {e.FirstCluster,6} {ClustersNeeded(e.FileSize, 512),10} {e.AttrString()}");
        }

        Console.WriteLine();
        Console.WriteLine("--- Deleted ---");
        foreach (var e in deleted)
        {
            Console.WriteLine($"  {e.Name,-12} {e.FileSize,8} {e.FirstCluster,6}");
        }
    }

    private int ClustersNeeded(long size, int clusterBytes) =>
        size <= 0 ? 1 : (int)((size + clusterBytes - 1) / clusterBytes);
}