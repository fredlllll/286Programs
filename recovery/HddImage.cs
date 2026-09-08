namespace Recovery;

/* a flat image of the hdd, just bytes plus a convenient typed accessor */
class HddImage
{
    private readonly byte[] _data;

    public HddImage(string path)
    {
        _data = File.ReadAllBytes(path);
    }

    public int Length => _data.Length;
    public int SectorCount => _data.Length / Geometry.BytesPerSector;

    public byte this[int index] => _data[index];

    public int ReadU16(int offset) => _data[offset] | (_data[offset + 1] << 8);
    public int ReadI32(int offset) =>
        _data[offset] | (_data[offset + 1] << 8) | (_data[offset + 2] << 16) | (_data[offset + 3] << 24);

    public byte[] Slice(int offset, int length) => _data[offset..(offset + length)];
    public byte[] SliceToEnd(int offset) => _data[offset..];
}
