using System.Buffers.Binary;
using System.Security.Cryptography;

namespace HddSaver.Protocol;

public class SectorHeader
{
    public uint Lba { get; set; }
    public byte Status { get; set; }
    public ushort DataCrc { get; set; }
    public ushort HeaderCrc { get; set; }

    public bool HasData => Status == 0x00 || Status == 0x11;

    public static SectorHeader Parse(ReadOnlySpan<byte> header)
    {
        if (header.Length < Command.HeaderSize)
            throw new ArgumentException($"Header too short: {header.Length} bytes");

        if (header[0] != Command.HEADER_MAGIC0 || header[1] != Command.HEADER_MAGIC1)
            throw new InvalidDataException($"Bad magic: 0x{header[0]:X2} 0x{header[1]:X2}");

        var hdr = new SectorHeader
        {
            Lba = (uint)header[2] | ((uint)header[3] << 8) | ((uint)header[4] << 16),
            Status = header[5],
            DataCrc = (ushort)(header[6] | (header[7] << 8)),
            HeaderCrc = (ushort)(header[8] | (header[9] << 8))
        };

        var computedCrc = Crc16(header[..8]);
        if (computedCrc != hdr.HeaderCrc)
            throw new InvalidDataException(
                $"Header CRC mismatch: computed 0x{computedCrc:X4}, got 0x{hdr.HeaderCrc:X4}");

        return hdr;
    }

    public static bool TryParse(ReadOnlySpan<byte> header, out SectorHeader? result, out string? error)
    {
        result = null;
        error = null;

        if (header.Length < Command.HeaderSize)
        {
            error = "Header too short";
            return false;
        }

        if (header[0] != Command.HEADER_MAGIC0 || header[1] != Command.HEADER_MAGIC1)
        {
            error = $"Bad magic: 0x{header[0]:X2} 0x{header[1]:X2}";
            return false;
        }

        var hdr = new SectorHeader
        {
            Lba = (uint)header[2] | ((uint)header[3] << 8) | ((uint)header[4] << 16),
            Status = header[5],
            DataCrc = (ushort)(header[6] | (header[7] << 8)),
            HeaderCrc = (ushort)(header[8] | (header[9] << 8))
        };

        var computedCrc = Crc16(header[..8]);
        if (computedCrc != hdr.HeaderCrc)
        {
            error = $"Header CRC mismatch: computed 0x{computedCrc:X4}, got 0x{hdr.HeaderCrc:X4}";
            return false;
        }

        result = hdr;
        return true;
    }

    public static bool ValidateDataCrc(ReadOnlySpan<byte> data, ushort expectedCrc)
    {
        if (data.Length < Command.DataSize)
            return false;
        return Crc16(data[..Command.DataSize]) == expectedCrc;
    }

    public static ushort Crc16(ReadOnlySpan<byte> data)
    {
        ushort crc = 0xFFFF;
        foreach (var b in data)
        {
            crc = (ushort)((crc << 8) ^ CrcTable[((crc >> 8) ^ b) & 0xFF]);
        }
        return crc;
    }

    private static readonly ushort[] CrcTable = GenerateCrcTable();

    private static ushort[] GenerateCrcTable()
    {
        var table = new ushort[256];
        for (int i = 0; i < 256; i++)
        {
            ushort c = (ushort)(i << 8);
            for (int j = 0; j < 8; j++)
            {
                c = (c & 0x8000) != 0
                    ? (ushort)((c << 1) ^ 0x1021)
                    : (ushort)(c << 1);
            }
            table[i] = c;
        }
        return table;
    }
}

public class StatusReply
{
    public ushort TotalCyls { get; set; }
    public byte TotalHeads { get; set; }
    public byte TotalSpt { get; set; }
    public uint TotalSectors { get; set; }
    public uint CurrentLba { get; set; }
    public byte HeadMask { get; set; }
    public byte Retries { get; set; }

    public static bool TryParse(ReadOnlySpan<byte> data, out StatusReply? result, out string? error)
    {
        result = null;
        error = null;

        if (data.Length < Command.StatusReplySize)
        {
            error = "Status reply too short";
            return false;
        }

        if (data[0] != Command.HEADER_MAGIC0 || data[1] != Command.HEADER_MAGIC1)
        {
            error = "Bad status magic";
            return false;
        }

        var reply = new StatusReply
        {
            TotalCyls = (ushort)(data[2] | (data[3] << 8)),
            TotalHeads = data[4],
            TotalSpt = data[5],
            TotalSectors = (uint)data[6] | ((uint)data[7] << 8) | ((uint)data[8] << 16) | ((uint)data[9] << 24),
            CurrentLba = (uint)data[10] | ((uint)data[11] << 8) | ((uint)data[12] << 16) | ((uint)data[13] << 24),
            HeadMask = data[14],
            Retries = data[15]
        };

        var computedCrc = SectorHeader.Crc16(data[..20]);
        var expectedCrc = (ushort)(data[20] | (data[21] << 8));
        if (computedCrc != expectedCrc)
        {
            error = $"Status CRC mismatch: computed 0x{computedCrc:X4}, got 0x{expectedCrc:X4}";
            return false;
        }

        result = reply;
        return true;
    }
}
