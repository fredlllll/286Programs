using System.Text;

namespace Recovery;

/* a single 32-byte slot in a dos directory. byte 0 is the first char of
   the 8.3 name: 0x00 = end of directory, 0xE5 = deleted entry (the real
   letter is recovered from the low 5 bits), 0x05 = the genuine first
   char really was 0xE5. a 0x0F attribute byte marks an LFN shadow entry,
   which we skip. */
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

    public static DirEntry? TryParse(byte[] buf, int off)
    {
        byte attr = buf[off + 11];
        if (attr == 0x0F)
        {
            return null; // LFN shadow
        }
        if (buf[off] == 0x00)
        {
            return null; // end of directory
        }

        var raw = new byte[11];
        raw[0] = buf[off] switch
        {
            0x05 => 0xE5,                           // real char was 0xE5
            var b when (b & 0xE0) == 0xE0 => (byte)((b & 0x1F) | 0x40), // deleted: recover letter
            var b => b,
        };
        for (int i = 1; i < 11; i++)
        {
            raw[i] = buf[off + i];
        }

        string name = Encoding.ASCII.GetString(raw, 0, 8).TrimEnd();
        string ext = Encoding.ASCII.GetString(raw, 8, 3).TrimEnd();

        int date = buf[off + 18] | (buf[off + 19] << 8);
        return new DirEntry
        {
            Name = ext.Length > 0 ? $"{name}.{ext}" : name,
            Attributes = attr,
            FirstCluster = buf[off + 26] | (buf[off + 27] << 8),
            FileSize = (uint)(buf[off + 28] | (buf[off + 29] << 8) | (buf[off + 30] << 16) | (buf[off + 31] << 24)),
            Created = new DateTime(1980 + ((date >> 9) & 0x7F), 1, 1),
            IsDeleted = (buf[off] & 0xE0) == 0xE0,
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