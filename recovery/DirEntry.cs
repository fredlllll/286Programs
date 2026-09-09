using System.Text;

namespace Recovery;

/* a single 32-byte slot in a dos directory. byte 0 is the first char of
   the 8.3 name: 0x00 = end of directory, 0xE5 = deleted entry (the real
   letter is recovered from the low 5 bits), 0x05 = the genuine first
   char really was 0xE5. a 0x0F attribute byte marks an LFN shadow entry,
   which the directory reader skips. */
class DirEntry
{
    public string Name = "";
    public byte Attributes;
    public int FirstCluster;
    public long FileSize;
    public DateTime Created;
    public bool IsDeleted;
    public bool IsDirectory => (Attributes & 0x10) != 0;
    public bool IsVolumeLabel => (Attributes & 0x08) != 0;

    /* parse one 32-byte directory slot prepared by the caller, which also
       checks the sentinels: byte 0 == 0x00 (end of directory) and an
       0x0F attribute byte (LFN shadow entry, skipped). */
    public static DirEntry Parse(byte[] slot)
    {
        byte attr = slot[11];
        var raw = new byte[11];
        raw[0] = slot[0] switch
        {
            0x05 => 0xE5,                           // real char was 0xE5
            var b when (b & 0xE0) == 0xE0 => (byte)((b & 0x1F) | 0x40), // deleted: recover letter
            var b => b,
        };
        for (int i = 1; i < 11; i++)
        {
            raw[i] = slot[i];
        }

        string name = Encoding.ASCII.GetString(raw, 0, 8).TrimEnd();
        string ext = Encoding.ASCII.GetString(raw, 8, 3).TrimEnd();

        int date = slot[18] | (slot[19] << 8);
        return new DirEntry
        {
            Name = ext.Length > 0 ? $"{name}.{ext}" : name,
            Attributes = attr,
            FirstCluster = slot[26] | (slot[27] << 8),
            FileSize = (uint)(slot[28] | (slot[29] << 8) | (slot[30] << 16) | (slot[31] << 24)),
            Created = new DateTime(1980 + ((date >> 9) & 0x7F), 1, 1),
            IsDeleted = (slot[0] & 0xE0) == 0xE0,
        };
    }

    public string AttrString()
    {
        var s = new StringBuilder();
        if ((Attributes & 0x01) != 0)
        {
            s.Append('R');
        }
        if ((Attributes & 0x02) != 0)
        {
            s.Append('H');
        }
        if ((Attributes & 0x04) != 0)
        {
            s.Append('S');
        }
        if ((Attributes & 0x08) != 0)
        {
            s.Append('V');
        }
        if ((Attributes & 0x10) != 0)
        {
            s.Append('D');
        }
        if ((Attributes & 0x20) != 0)
        {
            s.Append('A');
        }
        return s.Length > 0 ? s.ToString() : "-";
    }
}