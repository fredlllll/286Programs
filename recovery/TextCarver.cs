using System.Text;

namespace Recovery;

/* when the filesystem is damaged or simply not there, the raw sectors
   still hold readable text. this scans the full image for runs of
   printable material and reports each one with its LBA and CHS so it can
   be checked against the badmap. */
static class TextCarver
{
    private const int MinRun = 32;
    private const double MinScore = 0.80;

    /* the dump stores western-european text in oem codepage 850; the .txt
       files are written as utf-8 so they read correctly on a modern pc. */
    private static readonly Encoding Cp850;

    static TextCarver()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        Cp850 = Encoding.GetEncoding(850);
    }

    /* a text byte is what you expect while reading a plain or wordstar
       file: printable ascii plus tab/cr/lf. 0x80..0xff is tolerated
       inside a run (wordstar markup sits in the high bit) but does not
       count toward the score. any other byte is a hard break. */
    private static bool IsTextByte(byte b) =>
        b >= 0x20 && b <= 0x7E || b == 0x09 || b == 0x0A || b == 0x0D;

    public static void Carve(HddImage img, string? outDir)
    {
        var runs = ScanTextRuns(img);
        Console.WriteLine("--- Carving ---");
        Console.WriteLine($"  Found {runs.Count} text runs (len>={MinRun}, score>={MinScore * 100:0}%)");

        foreach (var run in runs)
        {
            long lba = run.Offset / Geometry.BytesPerSector;
            var (cyl, head, sec) = Geometry.LbaToChs(lba);
            var s = run.Sample;
            if (s.Length > 44)
            {
                s = s[..44];
            }
            Console.WriteLine($"  LBA {lba,-7} {cyl}/{head}/{sec,-11} len {run.Length,6} score {run.Score,3}% lines {run.Lines,3} highbit {run.HighBitRatio * 100,3:0}%  {s}");
        }

        DumpEscapedRuns(runs, img, outDir);
        Signatures(img);
    }

    private static List<TextRun> ScanTextRuns(HddImage img)
    {
        var runs = new List<TextRun>();
        int start = -1;
        int total = 0, good = 0, hi = 0, lines = 0;

        void Flush()
        {
            if (start < 0)
            {
                return;
            }

            if (total >= MinRun && (double)good / total >= MinScore)
            {
                int sampleLen = Math.Min(100, total);
                var sample = new byte[sampleLen];
                for (int i = 0; i < sampleLen; i++)
                {
                    sample[i] = img[start + i] < 0x20 ? (byte)0x20 : img[start + i];
                }

                runs.Add(new TextRun(start, total)
                {
                    Score = good * 100 / total,
                    Lines = lines,
                    HighBitRatio = (double)hi / total,
                    Sample = Cp850.GetString(sample),
                });
            }
            start = -1;
        }

        for (int i = 0; i < img.Length; i++)
        {
            byte b = img[i];
            if (IsTextByte(b))
            {
                if (start < 0)
                {
                    start = i;
                    total = good = hi = lines = 0;
                }

                total++;
                good++;
                if (b == '\n' || b == '\r')
                {
                    lines++;
                }
            }
            else if (b >= 0x80)
            {
                if (start >= 0)
                {
                    total++;
                    hi++;
                }
            }
            else
            {
                Flush();
            }
        }
        Flush();
        return runs;
    }

    private static void DumpEscapedRuns(List<TextRun> runs, HddImage img, string? outDir)
    {
        if (outDir == null)
        {
            return;
        }

        var carvedDir = Path.Combine(outDir, "carved");
        Directory.CreateDirectory(carvedDir);
        foreach (var run in runs)
        {
            long lba = run.Offset / Geometry.BytesPerSector;
            string file = Path.Combine(carvedDir,
                $"{lba:000000}_{run.Offset % Geometry.BytesPerSector:000}_{run.Length:00000}.txt");
            string text = Cp850.GetString(img.Slice(run.Offset, run.Length));
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
        public int Offset;
        public int Length;
        public int Score;
        public int Lines;
        public double HighBitRatio;
        public string Sample = "";

        public TextRun(int offset, int length)
        {
            Offset = offset;
            Length = length;
        }
    }
}