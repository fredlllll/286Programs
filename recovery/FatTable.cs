namespace Recovery;

/* the file allocation table: one entry per cluster telling whether it
   is free, EOF, marked bad, or pointing to the next cluster of a file.
   a file is the chain of cluster indices starting at its first cluster,
   following pointers until an EOF marker. */
class FatTable
{
    private readonly int[] _entries;
    private readonly bool _fat12;

    private FatTable(int[] entries, bool fat12)
    {
        _entries = entries;
        _fat12 = fat12;
    }

    public int Length => _entries.Length;
    public int this[int cluster] => _entries[cluster];

    public static FatTable Read(HddImage img, BootSector boot)
    {
        int fatBytes = boot.FatSizeSectors * boot.BytesPerSector;
        int fatOffset = (int)boot.FatStartSector * boot.BytesPerSector;
        int maxClusters = boot.TotalSectors / boot.SectorsPerCluster + 2;
        bool fat12 = boot.FatType == "FAT12";

        var entries = new int[maxClusters];
        entries[0] = entries[1] = -1;

        if (fat12)
        {
            /* 12-bit entries are packed 2 per 3 bytes, byte-aligned only
               for even indices: entry n lives at byte n + n/2. the byte
               offsets are relative to the fat start; compare them against
               fatBytes, not the absolute image offset. */
            for (int n = 2; n < maxClusters; n += 2)
            {
                int rel = n + n / 2;
                if (rel + 2 >= fatBytes)
                {
                    break;
                }

                int off = fatOffset + rel;
                int b0 = img[off], b1 = img[off + 1], b2 = img[off + 2];
                entries[n] = b0 | ((b1 & 0x0F) << 8);
                if (n + 1 < maxClusters)
                {
                    entries[n + 1] = (b1 >> 4) | (b2 << 4);
                }
            }
        }
        else
        {
            for (int n = 2; n < maxClusters && n * 2 + 2 <= fatBytes; n++)
            {
                entries[n] = img.ReadU16(fatOffset + n * 2);
            }
        }
        return new FatTable(entries, fat12);
    }

    /* follow the chain from firstCluster until EOF, a bad marker, a free
       slot, or a loop. a damaged cluster contributes -1 so the caller can
       report which files touch destroyed regions. */
    public int[] Chain(int firstCluster)
    {
        var chain = new List<int>();
        int c = firstCluster;
        int guard = _entries.Length;
        while (c >= 2 && c < _entries.Length && guard-- > 0)
        {
            chain.Add(c);
            int next = _entries[c];
            if (next >= (_fat12 ? 0xFF8 : 0xFFF8))
            {
                break; // EOF
            }
            if (next == 0xFF7 || next == (_fat12 ? 0xFF7 : 0xFFF7))
            {
                chain.Add(-1);
                break; // bad
            }
            if (next < 2 || next <= c)
            {
                break; // free / corrupt / loop
            }

            c = next;
        }
        return chain.ToArray();
    }

    public (int Free, int Used, int Bad) Stats()
    {
        int free = 0, bad = 0, used = 0;
        for (int i = 2; i < _entries.Length; i++)
        {
            if (_entries[i] == 0)
            {
                free++;
            }
            else if (_entries[i] == (_fat12 ? 0xFF7 : 0xFFF7))
            {
                bad++;
            }
            else if (_entries[i] > 1)
            {
                used++;
            }
        }
        return (free, used, bad);
    }

    public void Report()
    {
        var (free, used, bad) = Stats();
        Console.WriteLine("--- FAT ---");
        Console.WriteLine($"  Entries: {Length}");
        Console.WriteLine($"  Free:    {free}");
        Console.WriteLine($"  Used:    {used}");
        Console.WriteLine($"  Bad:     {bad}");
    }
}