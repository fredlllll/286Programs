using System.Runtime.InteropServices;

namespace HddSaver.Protocol;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct SectorHeader
{
    public byte status;
    public uint lba;
    
    public ushort dataCrc;

    public bool HasData => status == 0x00 || status == 0x11;

    public bool VerifyDataCrc(byte[] data)
    {
        var crc = Crc.Crc16(data);
        return crc == dataCrc;
    }
}
