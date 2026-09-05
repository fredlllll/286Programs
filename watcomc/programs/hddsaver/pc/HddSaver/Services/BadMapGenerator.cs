using HddSaver.Data;
using HddSaver.Protocol;
using Microsoft.EntityFrameworkCore;

namespace HddSaver.Services;

public static class BadMapGenerator
{
    private static readonly Dictionary<int, string> MapColors = new()
    {
        { 0, "#bdbdbd" }, // not attempted
        { 1, "#2ea44f" }, // readable
        { 2, "#d93025" }, // hdd read failed
        { 3, "#e08a00" }, // head masked
    };

    private static readonly Dictionary<int, string> MapNames = new()
    {
        { 0, "not attempted" },
        { 1, "readable" },
        { 2, "hdd read failed" },
        { 3, "head masked" },
    };

    public static void Generate(string outputPath, int cyls, int heads, int spt)
    {
        int total = cyls * heads * spt;
        var state = new byte[total]; // 0 = untouched

        using var ctx = new HddSaverContext();
        var sectors = ctx.Sectors.ToList();

        foreach (var sec in sectors)
        {
            if (sec.Lba >= total) continue;
            if (SectorStatus.HasData(sec.Status))
            {
                state[sec.Lba] = 1; // readable
            }
            else if (sec.Status == SectorStatus.HeadSkip)
            {
                // head skip — treat as head-masked, never clobber a good read
                if (state[sec.Lba] == 0)
                    state[sec.Lba] = 3;
            }
            else
            {
                // read failed, but a good read anywhere wins over it
                if (state[sec.Lba] < 2)
                    state[sec.Lba] = 2;
            }
        }

        int cw = 3, ch = 12;   // pixel size of one sector cell
        int ml = 56, mt = 78;  // left margin / top offset
        int panelGap = 18, tickH = 14;
        int W = ml + cyls * cw + 14;
        int panelH = spt * ch + tickH;
        int Ht = mt + heads * (panelH + panelGap);

        var outLines = new List<string>
        {
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
            $"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{W}\" height=\"{Ht}\" font-family=\"monospace\">",
            "<rect width=\"100%\" height=\"100%\" fill=\"#fafafa\"/>",
            $"<text x=\"{ml}\" y=\"24\" font-size=\"15\">Tandon HDD bad-sector map ({sectors.Count} sectors)</text>",
        };

        // legend
        int lx = ml;
        foreach (var st in new[] { 1, 2, 3, 0 })
        {
            outLines.Add($"<rect x=\"{lx}\" y=\"36\" width=\"12\" height=\"12\" fill=\"{MapColors[st]}\"/>");
            outLines.Add($"<text x=\"{lx + 16}\" y=\"46\" font-size=\"11\" fill=\"#222\">{MapNames[st]}</text>");
            lx += 16 + MapNames[st].Length * 7 + 20;
        }

        // summary counts
        var cnt = new int[4];
        foreach (var b in state) cnt[b]++;
        outLines.Add($"<text x=\"{ml}\" y=\"66\" font-size=\"11\" fill=\"#222\">" +
                     $"{cnt[1]} readable | {cnt[2]} read-failed | {cnt[3]} head-masked | {cnt[0]} not attempted</text>");

        // per-head panels
        for (int h = 0; h < heads; h++)
        {
            int y0 = mt + h * (panelH + panelGap);
            outLines.Add($"<text x=\"{ml}\" y=\"{y0 + 10}\" font-size=\"12\" fill=\"#222\">Head {h}</text>");
            int gy = y0 + 16;

            // grid lines every 100 cylinders
            for (int c = 0; c <= cyls; c += 100)
            {
                outLines.Add($"<line x1=\"{ml + c * cw}\" y1=\"{gy}\" x2=\"{ml + c * cw}\" y2=\"{gy + spt * ch}\" stroke=\"#dddddd\"/>");
            }

            // sector rows
            for (int sec = 0; sec < spt; sec++)
            {
                int ry = gy + sec * ch;
                int x = 0;
                while (x < cyls)
                {
                    byte st = state[(x * heads + h) * spt + sec];
                    int x2 = x + 1;
                    while (x2 < cyls && state[(x2 * heads + h) * spt + sec] == st)
                        x2++;
                    int w = (x2 - x) * cw;
                    outLines.Add($"<rect x=\"{ml + x * cw}\" y=\"{ry}\" width=\"{w}\" height=\"{ch}\" fill=\"{MapColors[st]}\"/>");
                    x = x2;
                }
            }

            // cylinder labels
            for (int c = 0; c < cyls; c += 100)
            {
                outLines.Add($"<text x=\"{ml + c * cw}\" y=\"{gy + spt * ch + 11}\" font-size=\"9\" fill=\"#666\">{c}</text>");
            }
        }

        outLines.Add("</svg>");
        File.WriteAllText(outputPath, string.Join("\n", outLines));
    }
}
