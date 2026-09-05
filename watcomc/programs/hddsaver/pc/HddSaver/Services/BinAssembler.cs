using HddSaver.Data;
using HddSaver.Protocol;

namespace HddSaver.Services;

public static class BinAssembler
{
    private const int SectorSize = 512;

    /// <summary>
    /// Writes every dumped (data-bearing) sector into a flat 512-byte-per-LBA
    /// image. Sectors that were never read successfully (missing, head-masked,
    /// or read-failed) are filled with zeros. Multiple rows per LBA are fine:
    /// only rows carrying data are copied, so a good read anywhere wins.
    /// </summary>
    public static void Assemble(string outputPath, int cyls, int heads, int spt)
    {
        int total = cyls * heads * spt;
        var image = new byte[(long)total * SectorSize];

        using var ctx = new HddSaverContext();
        foreach (var sec in ctx.Sectors)
        {
            if (sec.Lba >= total) continue;
            if (sec.Data is not { Length: SectorSize } data) continue;
            Array.Copy(data, 0, image, sec.Lba * SectorSize, SectorSize);
        }

        File.WriteAllBytes(outputPath, image);
    }
}