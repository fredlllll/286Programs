namespace Recovery;

/* reconstructs alive files from their cluster chains and writes them to
   outDir. files whose chain hits a bad FAT slot are written as far as the
   intact prefix allows and reported as partial. */
static class FileRecoverer
{
    public static void Dump(HddImage img, BootSector boot, FatTable fat, DirectoryReader dirs, string outDir)
    {
        Directory.CreateDirectory(outDir);
        int okBytes = 0, partialCount = 0;
        var output = new List<string>();

        foreach (var e in dirs.Entries)
        {
            if (e.IsDeleted || e.IsDirectory || e.IsVolumeLabel)
            {
                continue;
            }

            var chain = fat.Chain(e.FirstCluster);
            if (chain.Length == 0)
            {
                Console.WriteLine($"  {e.Name}: no clusters, skipped");
                continue;
            }

            var data = new MemoryStream();
            bool partial = false;
            foreach (int cluster in chain)
            {
                if (cluster < 0)
                {
                    partial = true;
                    break;
                }

                int sector = (int)boot.DataStartSector + (cluster - 2) * boot.SectorsPerCluster;
                data.Write(img.Slice(sector * boot.BytesPerSector, boot.BytesPerSector * boot.SectorsPerCluster));
            }

            var bytes = data.ToArray();
            if (bytes.Length > e.FileSize)
            {
                bytes = bytes[..(int)e.FileSize];
            }

            string file = Path.Combine(outDir, SafeName(e.Name));
            File.WriteAllBytes(file, bytes);

            output.Add($"  {e.Name}: {bytes.Length} bytes [{(partial ? "PARTIAL" : "OK")}]");
            if (!partial)
            {
                okBytes += (int)e.FileSize;
            }
            else
            {
                partialCount++;
            }
        }

        foreach (var line in output)
        {
            Console.WriteLine(line);
        }
        Console.WriteLine($"\n  Recovered {output.Count} files ({okBytes} bytes intact) to {outDir}" +
                          (partialCount > 0 ? $", {partialCount} partial" : ""));
    }

    private static string SafeName(string name)
    {
        var invalid = Path.GetInvalidFileNameChars();
        var s = new string(name.Select(c => invalid.Contains(c) ? '_' : c).ToArray());
        return s.Length == 0 ? "_" : s;
    }
}