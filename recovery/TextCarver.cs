using System.Text;

namespace Recovery;

/* when the filesystem is damaged or simply not there, the raw sectors
   still hold readable text. a sector belongs to a single file, so a
   sector that sits in a text file is entirely text - no need to pick
   passages out of binaries. each sector is classified on its own and
   contiguous text sectors are dumped whole. */
static class TextCarver
{
    private const double MinRatio = 0.80;

    /* the dump stores western-european text in oem codepage 850; the .txt
       files are written as utf-8 so they read correctly on a modern pc. */
    private static readonly Encoding Cp850;

    static TextCarver()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        Cp850 = Encoding.GetEncoding(850);
    }

    /* a text byte is what you expect while reading a plain or wordstar
       file: printable ascii plus tab/cr/lf. oem-850 accented characters
       live in 0x80..0xff; they are cp850 text but deliberately not counted
       here - the media-descriptor entries in a fat sector are high bytes
       too, and must not push a binary sector over the threshold. */
    private static bool IsTextByte(byte b) =>
        b >= 0x20 && b <= 0x7E || b == 0x09 || b == 0x0A || b == 0x0D;

    public static void Carve(HddImage img, string? outDir)
    {
        var runs = ScanTextRuns(img);
        Console.WriteLine("--- Carving ---");
        Console.WriteLine($"  Found {runs.Count} text sector runs (ratio >= {MinRatio * 100:0}%)");
        Console.WriteLine($"  LBA          CHS          sectors ratio lines  sample");

        foreach (var run in runs)
        {
            var (cyl, head, sec) = Geometry.LbaToChs(run.StartSector);
            var s = run.Sample;
            if (s.Length > 44)
            {
                s = s[..44];
            }

            string range = run.SectorCount == 1
                ? $"{run.StartSector}"
                : $"{run.StartSector}-{run.StartSector + run.SectorCount - 1}";
            Console.WriteLine($"  LBA {range,-8} {cyl}/{head}/{sec,-9} {run.SectorCount,4}  {run.MinRatio,4}% {run.Lines,5}  {s}");
        }

        DumpRuns(runs, img, outDir);
        Signatures(img);
    }

    /* classifies sectors, grouping contiguous text sectors into runs. the
       integer ratio is over the used part of the sector (up to the last
       non-zero byte), so the partially-filled final sector of a text file
       still counts while an all-empty sector does not. */
    private static List<TextRun> ScanTextRuns(HddImage img)
    {
        var runs = new List<TextRun>();
        int runStart = -1;
        int minRatio = 0, lines = 0;

        void Flush(int endSector)
        {
            if (runStart >= 0)
            {
                runs.Add(new TextRun(runStart, endSector - runStart, lines, minRatio));
            }
            runStart = -1;
        }

        for (int s = 0; s < img.SectorCount; s++)
        {
            if (SectorIsText(img, s, out int ratio, out int secLines))
            {
                if (runStart < 0)
                {
                    runStart = s;
                    minRatio = ratio;
                    lines = 0;
                }
                minRatio = Math.Min(minRatio, ratio);
                lines += secLines;
            }
            else
            {
                Flush(s);
            }
        }
        Flush(img.SectorCount);

        foreach (var run in runs)
        {
            int sampleLen = Math.Min(48, run.SectorCount * Geometry.BytesPerSector);
            var sample = new byte[sampleLen];
            int off = run.StartSector * Geometry.BytesPerSector;
            for (int i = 0; i < sampleLen; i++)
            {
                sample[i] = img[off + i] < 0x20 ? (byte)0x20 : img[off + i];
            }
            run.Sample = Cp850.GetString(sample);
        }
        return runs;
    }

    private static bool SectorIsText(HddImage img, int sector, out int ratio, out int lines)
    {
        int off = sector * Geometry.BytesPerSector;
        int text = 0, lastNonZero = -1;
        lines = 0;
        for (int i = 0; i < Geometry.BytesPerSector; i++)
        {
            byte b = img[off + i];
            if (b != 0)
            {
                lastNonZero = i;
            }
            if (IsTextByte(b))
            {
                text++;
                if (b == '\n' || b == '\r')
                {
                    lines++;
                }
            }
        }

        ratio = lastNonZero >= 0 ? text * 100 / (lastNonZero + 1) : 0;
        return ratio >= MinRatio * 100;
    }

    private static void DumpRuns(List<TextRun> runs, HddImage img, string? outDir)
    {
        if (outDir == null)
        {
            return;
        }

        var carvedDir = Path.Combine(outDir, "carved");
        Directory.CreateDirectory(carvedDir);
        foreach (var run in runs)
        {
            string file = Path.Combine(carvedDir, $"{run.StartSector:000000}_{run.SectorCount:00000}.txt");
            string text = Cp850.GetString(img.Slice(run.StartSector * Geometry.BytesPerSector, run.SectorCount * Geometry.BytesPerSector));
            File.WriteAllText(file, text, new UTF8Encoding(false));
        }
    }

    private static void Signatures(HddImage img)
    {
        const int MaxHits = 12;
        Console.WriteLine("\n  Signature scan:");
        int hits = 0;
        for (int i = 0; i + 2 < img.Length && hits < MaxHits; i++)
        {
            if (img[i] == 0x4D && img[i + 1] == 0x5A) // "MZ", dos exe
            {
                long lba = i / Geometry.BytesPerSector;
                var (cyl, head, sec) = Geometry.LbaToChs(lba);
                Console.WriteLine($"    MZ (DOS exe) at LBA {lba} ({cyl}/{head}/{sec})");
                hits++;
            }
        }
        if (hits == 0)
        {
            Console.WriteLine("    none");
        }
    }

    class TextRun
    {
        public int StartSector;
        public int SectorCount;
        public int Lines;
        public int MinRatio;
        public string Sample = "";

        public TextRun(int startSector, int sectorCount, int lines, int minRatio)
        {
            StartSector = startSector;
            SectorCount = sectorCount;
            Lines = lines;
            MinRatio = minRatio;
        }
    }
}