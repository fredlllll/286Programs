namespace HddSaver.Models;

public class Sector
{
    public int Id { get; set; }
    public int SessionId { get; set; }
    public uint Lba { get; set; }
    public byte Status { get; set; }
    public byte[]? Data { get; set; }
    public DateTime ReceivedAt { get; set; }
    public DumpSession Session { get; set; } = null!;
}
