using System.Runtime.InteropServices;

namespace HddSaver.Protocol;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct StatusReply
{
    public ushort totalCyls;
    public byte totalHeads;
    public byte totalSpt;
    public uint totalSectors;
    public uint currentLba;
    public byte headMask;
    public byte retries;
}
