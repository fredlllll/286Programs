using System.Text;

namespace Recovery;

/* when the filesystem is damaged or simply not there, the raw sectors
   still hold readable text. a sector belongs to a single file, so a
   sector that sits in a text file is entirely text - no need to pick
   passages out of binaries. text sectors are dumped one by one; adjacent
   text sectors need not belong to the same file, so nothing is merged. */
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
        var sectors = ScanTextSectors(img);
        Console.WriteLine("--- Carving ---");
        Console.WriteLine($"  Found {sectors.Count} text sectors (ratio >= {MinRatio * 100:0}%)");
        Console.WriteLine($"  LBA          CHS          ratio lines  sample");

        foreach (var s in sectors)
        {
            var (cyl, head, sec) = Geometry.LbaToChs(s.StartSector);
            Console.WriteLine($"  LBA {s.StartSector,-9} {cyl}/{head}/{sec,-9} {s.MinRatio,4}% {s.Lines,5}  {s.Sample}");
        }

        DumpSectors(sectors, img, outDir);
        Signatures(img);
    }

    /* collects the text sectors; the ratio is over the used part of the
       sector (up to the last non-zero byte), so the partially-filled final
       sector of a text file still counts while an all-empty sector does
       not. */
    private static List<TextSector> ScanTextSectors(HddImage img)
    {
        var sectors = new List<TextSector>();
        for (int s = 0; s < img.SectorCount; s++)
        {
            if (SectorIsText(img, s, out int ratio, out int lines))
            {
                var run = new TextSector(s, ratio, lines);
                int sampleLen = Math.Min(48, Geometry.BytesPerSector);
                var sample = new byte[sampleLen];
                int off = s * Geometry.BytesPerSector;
                for (int i = 0; i < sampleLen; i++)
                {
                    sample[i] = img[off + i] < 0x20 ? (byte)0x20 : img[off + i];
                }
                run.Sample = Cp850.GetString(sample);
                sectors.Add(run);
            }
        }
        return sectors;
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

    private static void DumpSectors(List<TextSector> sectors, HddImage img, string? outDir)
    {
        if (outDir == null)
        {
            return;
        }

        var carvedDir = Path.Combine(outDir, "carved");
        Directory.CreateDirectory(carvedDir);
        foreach (var sector in sectors)
        {
            string file = Path.Combine(carvedDir, $"{sector.StartSector:000000}.txt");
            string text = Cp850.GetString(img.Slice(sector.StartSector * Geometry.BytesPerSector, Geometry.BytesPerSector));
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

    class TextSector
    {
        public int StartSector;
        public int MinRatio;
        public int Lines;
        public string Sample = "";

        public TextSector(int startSector, int minRatio, int lines)
        {
            StartSector = startSector;
            MinRatio = minRatio;
            Lines = lines;
        }
    }
}