namespace HddSaver.Models;

public class DumpSession
{
    public int Id { get; set; }
    public DateTime StartTime { get; set; }
    public DateTime? EndTime { get; set; }
    public int TotalSectors { get; set; }
    public string Geometry { get; set; } = "";
    public byte HeadMask { get; set; }
    public byte Retries { get; set; }
    public List<Sector> Sectors { get; set; } = new();
}
