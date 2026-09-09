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

    /* set when this boot sector was synthesized by Locate() rather than
       parsed from the volume, i.e. the original boot sector was unreadable */
    public bool Inferred;

    public int RootDirSectors => (RootEntries * 32 + BytesPerSector - 1) / BytesPerSector;
    public int TotalClusters => (TotalSectors - ReservedSectors - FatSizeSectors * NumFats - RootDirSectors) / SectorsPerCluster;

    public static BootSector Parse(HddImage img, uint partitionStartLba)
    {
        long start = (long)partitionStartLba * Geometry.BytesPerSector;
        if (start >= img.Length)
        {
            throw new InvalidDataException($"partition at lba {partitionStartLba} starts beyond the end of the image");
        }

        using var reader = new BinaryReader(img.Stream, Encoding.ASCII, true);
        reader.BaseStream.Position = start;

        reader.ReadBytes(3);                            // jump code
        reader.ReadBytes(8);                            // oem name, unused here
        var b = new BootSector
        {
            PartitionStartSector = partitionStartLba,
            BytesPerSector = reader.ReadUInt16(),       // 11
            SectorsPerCluster = reader.ReadByte(),      // 13
            ReservedSectors = reader.ReadUInt16(),      // 14
            NumFats = reader.ReadByte(),                // 16
            RootEntries = reader.ReadUInt16(),          // 17
        };
        int total16 = reader.ReadUInt16();              // 19
        reader.ReadByte();                              // 21 media descriptor
        b.FatSizeSectors = reader.ReadUInt16();         // 22
        reader.ReadUInt16();                            // 24 sectors/track
        reader.ReadUInt16();                            // 26 heads
        reader.ReadUInt32();                            // 28 hidden sectors
        int total32 = reader.ReadInt32();               // 32 (always consumed)
        b.TotalSectors = total16 > 0 ? total16 : total32;
        reader.ReadByte();                              // 36 drive number
        reader.ReadByte();                              // 37 reserved
        reader.ReadByte();                              // 38 boot signature
        reader.ReadUInt32();                            // 39 volume id
        b.VolumeLabel = Encoding.ASCII.GetString(reader.ReadBytes(11)).Trim(); // 43
        b.FsType = Encoding.ASCII.GetString(reader.ReadBytes(8)).Trim();       // 54

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

    /* ---- boot-sector-less fallback ----------------------------------------
       when the boot sector is unreadable (common - it sits on one dead
       platter region or another), the fat itself is often still readable,
       and a fat sector is easy to find: it starts with the media byte
       (0xF8/0xF0) followed by the EOF marker, a distinctive 3-byte pattern
       at a sector boundary. the layout parameters we did not get from the
       bpb are all tiny integers, so the rest is a bounded search:
       candidates are scored on root-directory plausibility, fat chain
       coherence and - when the volume keeps two fats - agreement between
       the copies. the winning layout is synthesized into a BootSector the
       rest of the pipeline consumes unchanged. */

    /* find the filesystem layout when the boot sector at partitionStartLba
       is missing or zeroed. returns null when nothing plausible is found
       (fats themselves destroyed - carving is the only option left). */
    public static BootSector? Locate(HddImage img, uint partitionStartLba, uint partitionSectors)
    {
        int pStart = (int)partitionStartLba;
        int pEnd = (int)Math.Min(partitionStartLba + partitionSectors, (uint)img.SectorCount);
        if (pEnd - pStart < 3)
        {
            return null;
        }

        BootSector? best = null;
        double bestScore = -1;

        /* scan every sector for the fat header; larger root sizes first so
           an exact tie between neighboring layouts keeps the larger (and
           thus more conservative) root-directory guess. */
        for (int s = pStart + 1; s < pEnd; s++)
        {
            int o = s * Geometry.BytesPerSector;
            if (img[o] != 0xF8 && img[o] != 0xF0) continue;
            if ((img[o + 1] & 0x0F) != 0x0F) continue;
            if (img[o + 2] != 0xFF) continue;

            foreach (bool fat12 in new[] { true, false })
            {
                foreach (int re in new[] { 512, 128, 64, 32, 16 })
                {
                    int rootSectors = (re * 32 + Geometry.BytesPerSector - 1) / Geometry.BytesPerSector;
                    foreach (int n in new[] { 1, 2 })
                    {
                        for (int f = 1; f <= 512; f++)
                        {
                            int rootStart = s + n * f;
                            if (rootStart + rootSectors > pEnd)
                            {
                                break;
                            }
                            if (!PlausibleRootSlot(img, rootStart))
                            {
                                continue;
                            }

                            double score = ScoreCandidate(img, s, fat12, rootStart, rootSectors, n, f, pEnd, out int names);
                            if (names < 1 || score <= bestScore)
                            {
                                continue;
                            }

                            /* if only the second fat copy survives, re-express
                               the layout with the alive copy as fatStart so
                               the downstream pipeline reads it directly. */
                            int winS = s, winN = n;
                            if (n > 1 && !IsFatSignature(img, s) && IsFatSignature(img, s + f))
                            {
                                winS = s + f;
                                winN = 1;
                            }

                            bestScore = score;
                            best = MakeBoot(pStart, pEnd, winS, fat12, re, rootStart, winN, f);
                        }
                    }
                }
            }
        }

        if (best == null || bestScore < 2.5)
        {
            return null;
        }

        best.SectorsPerCluster = InferSpc(img, best);
        best.Inferred = true;
        return best;
    }

    private static BootSector MakeBoot(int pStart, int pEnd, int fatStart, bool fat12,
        int rootEntries, int rootStart, int numFats, int fatSize)
    {
        var b = new BootSector
        {
            PartitionStartSector = (uint)pStart,
            BytesPerSector = Geometry.BytesPerSector,
            SectorsPerCluster = 1,
            ReservedSectors = fatStart - pStart,
            NumFats = numFats,
            RootEntries = rootEntries,
            TotalSectors = pEnd - pStart,
            FatSizeSectors = fatSize,
            VolumeLabel = "",
            FsType = "",
            FatType = fat12 ? "FAT12" : "FAT16",
            FatStartSector = fatStart,
            RootDirStartSector = rootStart,
        };
        b.DataStartSector = rootStart + b.RootDirSectors;
        return b;
    }

    /* cheap gate: the first slot of the supposed root must look like an
       entry, a deleted one, or the clean end-of-directory byte; anything
       else (binary file data, ascii text) cannot be a root start and the
       fat-size sweep can move on. */
    private static bool PlausibleRootSlot(HddImage img, int sector)
    {
        int o = sector * Geometry.BytesPerSector;
        byte b0 = img[o];
        if (b0 == 0x00 || (b0 & 0xE0) == 0xE0)
        {
            return true;
        }
        if (b0 < 0x20 || b0 >= 0x7F)
        {
            return false;
        }
        byte attr = img[o + 11];
        return attr == 0x0F || (attr & 0xC0) == 0;
    }

    private static double ScoreCandidate(HddImage img, int fatStart, bool fat12,
        int rootStart, int rootSectors, int numFats, int fatSize, int pEnd, out int names)
    {
        int fatBytes = fatSize * Geometry.BytesPerSector;

        /* the fat sector may be either the first copy (both alive) or the
           second (first dead - read whichever one survives). */
        bool firstAlive = IsFatSignature(img, fatStart);
        bool secondAlive = numFats > 1 && IsFatSignature(img, fatStart + fatSize);
        if (!firstAlive && !secondAlive)
        {
            names = 0;
            return -1;
        }
        int readFat = firstAlive ? fatStart : fatStart + fatSize;

        double rootScore = ScoreRoot(img, rootStart, rootSectors, pEnd, out names);
        if (rootScore < 0.30)
        {
            return -1;
        }

        int covered = Math.Min(Math.Min(0xFFFF, fatBytes * (fat12 ? 341 : 256)), pEnd - 1 + 2);
        covered = Math.Max(covered, 2);
        int[] entries = DecodeEntries(img, readFat, fat12, covered, fatBytes);
        int free = 0, eof = 0;
        for (int c = 2; c < entries.Length; c++)
        {
            int v = entries[c];
            if (v == 0)
            {
                free++;
            }
            else if (v >= (fat12 ? 0xFF8 : 0xFFF8))
            {
                eof++;
            }
        }
        double coherence = entries.Length > 2 ? (double)(free + eof) / (entries.Length - 2) : 0;

        double chainScore = ScoreChains(img, rootStart, rootSectors, pEnd, entries, fat12);

        double score = 3 * rootScore + 0.5 * coherence + 2 * chainScore + 1.5 * Math.Min(1, names / 4.0);

        if (numFats == 2 && firstAlive && secondAlive)
        {
            int o1 = fatStart * Geometry.BytesPerSector;
            int o2 = (fatStart + fatSize) * Geometry.BytesPerSector;
            if (o2 + fatBytes <= pEnd * Geometry.BytesPerSector)
            {
                int same = 0;
                for (int i = 0; i < fatBytes; i++)
                {
                    if (img[o1 + i] == img[o2 + i])
                    {
                        same++;
                    }
                }
                score += 3.0 * same / fatBytes;
            }
        }
        return score;
    }

    private static bool IsFatSignature(HddImage img, int sector)
    {
        int o = sector * Geometry.BytesPerSector;
        if (img[o] != 0xF8 && img[o] != 0xF0)
        {
            return false;
        }
        return (img[o + 1] & 0x0F) == 0x0F && img[o + 2] == 0xFF;
    }

    /* plausible 8.3 name slots over the root region. a slot is valid if it
       is a deleted entry, the clean 0x00 end-of-directory (making every
       later slot a cheap win - but a non-zero byte after the marker loses
       it), or a real name. binary data and ascii text fail these checks
       fast. */
    private static double ScoreRoot(HddImage img, int rootStart, int rootSectors, int pEnd, out int names)
    {
        int slots = rootSectors * (Geometry.BytesPerSector / 32);
        int valid = 0, garbage = 0;
        names = 0;
        bool endOfDir = false;
        for (int i = 0; i < slots; i++)
        {
            int o = rootStart * Geometry.BytesPerSector + i * 32;
            if (o + 32 > pEnd * Geometry.BytesPerSector)
            {
                break;
            }

            byte b0 = img[o];
            if (endOfDir)
            {
                if (b0 == 0)
                {
                    valid++;
                }
                else
                {
                    garbage++;
                }
                continue;
            }
            if (b0 == 0x00)
            {
                valid++;
                endOfDir = true;
            }
            else if ((b0 & 0xE0) == 0xE0)
            {
                valid++; // deleted entry
            }
            else if (img[o + 11] == 0x0F)
            {
                valid++; // lfn shadow
            }
            else if (b0 < 0x20 || b0 >= 0x7F)
            {
                garbage++;
            }
            else if ((img[o + 11] & 0xC0) == 0 && NameBytesOk(img, o + 1))
            {
                valid++;
                names++;
            }
            else
            {
                garbage++;
            }
        }

        return valid + garbage > 0 ? (double)valid / (valid + garbage) : 0;
    }

    private static bool NameBytesOk(HddImage img, int off)
    {
        for (int i = 0; i < 10; i++)
        {
            byte b = img[off + i];
            if (b < 0x20 || b >= 0x7F)
            {
                return false;
            }
        }
        return true;
    }

    /* files found in the root must have a chain that reaches an EOF marker
       - a strong test, since the cluster index is exactly the position in
       the fat, and garbage fat entries walk into the void in a step or two. */
    private static double ScoreChains(HddImage img, int rootStart, int rootSectors, int pEnd, int[] entries, bool fat12)
    {
        int files = 0, good = 0;
        int slots = rootSectors * (Geometry.BytesPerSector / 32);
        for (int i = 0; i < slots; i++)
        {
            int o = rootStart * Geometry.BytesPerSector + i * 32;
            if (o + 32 > pEnd * Geometry.BytesPerSector)
            {
                break;
            }

            byte b0 = img[o];
            byte attr = img[o + 11];
            if ((b0 & 0xE0) == 0xE0 || attr == 0x0F || (attr & 0x18) != 0)
            {
                continue; // deleted / lfn / dir / volume label
            }
            if (b0 == 0 || b0 < 0x20 || b0 >= 0x7F)
            {
                continue;
            }

            int first = img[o + 26] | (img[o + 27] << 8);
            if (first < 2 || first >= entries.Length)
            {
                continue;
            }

            files++;
            if (ChainIsEof(entries, first, fat12))
            {
                good++;
            }
        }
        return files > 0 ? (double)good / files : 0;
    }

    private static bool ChainIsEof(int[] entries, int first, bool fat12)
    {
        int c = first;
        for (int steps = 0; steps < 64; steps++)
        {
            if (c < 2 || c >= entries.Length)
            {
                return false;
            }
            int next = entries[c];
            if (next >= (fat12 ? 0xFF8 : 0xFFF8))
            {
                return true;
            }
            if (next < 2 || next <= c)
            {
                return false;
            }
            c = next;
        }
        return false;
    }

    private static int[] DecodeEntries(HddImage img, int fatStart, bool fat12, int maxClusters, int fatBytes)
    {
        int off = fatStart * Geometry.BytesPerSector;
        var entries = new int[maxClusters];
        entries[0] = entries[1] = -1;
        if (fat12)
        {
            for (int c = 2; c < maxClusters; c += 2)
            {
                int rel = c + c / 2;
                if (rel + 2 >= fatBytes)
                {
                    break;
                }
                int b0 = img[off + rel], b1 = img[off + rel + 1], b2 = img[off + rel + 2];
                entries[c] = b0 | ((b1 & 0x0F) << 8);
                if (c + 1 < maxClusters)
                {
                    entries[c + 1] = (b1 >> 4) | (b2 << 4);
                }
            }
        }
        else
        {
            for (int c = 2; c < maxClusters && c * 2 + 2 <= fatBytes; c++)
            {
                entries[c] = img[off + c * 2] | (img[off + c * 2 + 1] << 8);
            }
        }
        return entries;
    }

    /* the bpb also carries sectors-per-cluster, which we never got to read.
       infer it from the files: a file of size bytes over k clusters needs
       ceil(size / (k * 512)) sectors per cluster, and the most common such
       estimate across all root files wins. files that fit in one cluster
       say nothing (their estimate is always 1) but the majority vote is
       robust against that. */
    private static int InferSpc(HddImage img, BootSector boot)
    {
        int fatBytes = boot.FatSizeSectors * boot.BytesPerSector;
        bool fat12 = boot.FatType == "FAT12";
        bool firstAlive = IsFatSignature(img, (int)boot.FatStartSector);
        bool secondAlive = boot.NumFats > 1 && IsFatSignature(img, (int)boot.FatStartSector + boot.FatSizeSectors);
        int readFat = firstAlive ? (int)boot.FatStartSector : secondAlive ? (int)boot.FatStartSector + boot.FatSizeSectors : (int)boot.FatStartSector;
        int covered = Math.Min(Math.Min(0xFFFF, fatBytes * (fat12 ? 341 : 256)), boot.TotalSectors + 2);
        int[] entries = DecodeEntries(img, readFat, fat12, covered, fatBytes);

        var votes = new Dictionary<int, int>();
        int slots = boot.RootDirSectors * (Geometry.BytesPerSector / 32);
        for (int i = 0; i < slots; i++)
        {
            int o = (int)boot.RootDirStartSector * Geometry.BytesPerSector + i * 32;
            byte b0 = img[o];
            byte attr = img[o + 11];
            if ((b0 & 0xE0) == 0xE0 || attr == 0x0F || (attr & 0x18) != 0)
            {
                continue;
            }
            if (b0 == 0 || b0 < 0x20 || b0 >= 0x7F)
            {
                continue;
            }

            int first = img[o + 26] | (img[o + 27] << 8);
            long size = (uint)(img[o + 28] | (img[o + 29] << 8) | (img[o + 30] << 16) | (img[o + 31] << 24));
            int len = ChainLength(entries, first, fat12);
            if (len <= 0 || size <= 0)
            {
                continue;
            }

            int spc = Math.Max(1, (int)((size + len * 512L - 1) / (len * 512L)));
            votes[spc] = votes.TryGetValue(spc, out int c) ? c + 1 : 1;
        }

        if (votes.Count == 0)
        {
            return 1;
        }
        return votes.OrderByDescending(v => v.Value).ThenBy(v => v.Key).First().Key;
    }

    private static int ChainLength(int[] entries, int first, bool fat12)
    {
        int c = first, len = 0;
        for (int steps = 0; steps < 64; steps++)
        {
            if (c < 2 || c >= entries.Length)
            {
                return -1;
            }
            int next = entries[c];
            if (next >= (fat12 ? 0xFF8 : 0xFFF8))
            {
                return len + 1;
            }
            if (next < 2 || next <= c)
            {
                return -1;
            }
            c = next;
            len++;
        }
        return -1;
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
        Console.WriteLine($"  Volume label:       {(VolumeLabel.Length > 0 ? VolumeLabel : "(unknown)")}");
        Console.WriteLine($"  FS type:            {(FsType.Length > 0 ? FsType : "(unknown)")}");
        Console.WriteLine($"  FAT type:           {FatType}");
        Console.WriteLine($"  Total clusters:     {TotalClusters}");
        Console.WriteLine("--- Layout ---");
        Console.WriteLine($"  FAT start:          sector {FatStartSector}");
        Console.WriteLine($"  Root dir:           sector {RootDirStartSector}");
        Console.WriteLine($"  Data area:          sector {DataStartSector}");
    }
}