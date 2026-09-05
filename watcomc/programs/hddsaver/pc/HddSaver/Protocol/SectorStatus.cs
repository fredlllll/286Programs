namespace HddSaver.Protocol;

/// <summary>
/// Sector status codes, shared with the 286 firmware (protocol.h) and the
/// recovery tool (recovery/structures.py).
/// </summary>
public static class SectorStatus
{
    public const byte Ok = 0x00;         // clean read, data present
    public const byte Ecc = 0x11;        // read after ecc correction, data present
    public const byte HeadSkip = 0xFE;   // head masked out this pass, never attempted

    public static bool HasData(byte status) => status == Ok || status == Ecc;
}