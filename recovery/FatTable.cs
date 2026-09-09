using recovery;
using System.Text;

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
        int maxClusters = boot.TotalSectors / boot.SectorsPerCluster + 2;
        bool fat12 = boot.FatType == "FAT12";

        /* the fat is just a packed blob; read it off the shared stream in
           one go and unpack against this local buffer, so all offset math
           stays relative to the fat start. */
        using var reader = new BinaryReader(img.Stream, Encoding.ASCII, true);
        reader.BaseStream.Position = (long)boot.FatStartSector * boot.BytesPerSector;
        byte[] fat = reader.ReadBytesExactly(fatBytes);

        var entries = new int[maxClusters];
        entries[0] = entries[1] = -1;

        if (fat12)
        {
            /* 12-bit entries are packed 2 per 3 bytes, byte-aligned only
               for even indices: entry n lives at byte n + n/2. */
            for (int n = 2; n < maxClusters; n += 2)
            {
                int rel = n + n / 2;
                if (rel + 2 >= fatBytes)
                {
                    break;
                }

                int b0 = fat[rel], b1 = fat[rel + 1], b2 = fat[rel + 2];
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
                entries[n] = fat[n * 2] | (fat[n * 2 + 1] << 8);
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