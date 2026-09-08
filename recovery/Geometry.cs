namespace Recovery;

/* drive geometry and sector addressing. the values come from the hdd
   saver firmware (definitions.h) and match what the bios reports. */
static class Geometry
{
    public const int Cyls = 820;
    public const int Heads = 6;
    public const int Spt = 26;
    public const int BytesPerSector = 512;

    /* converts a linear lba into chs coordinates on this drive. the
       mapping matches the firmware's LbaToChsWithLba and the badmap's
       lba = (cyl*heads + head)*spt + sec, so carved runs can be
       looked up directly against the badmap render. */
    public static (int Cyl, int Head, int Sec) LbaToChs(long lba)
    {
        long cyl = lba / (Heads * Spt);
        long rem = lba % (Heads * Spt);
        return ((int)cyl, (int)(rem / Spt), (int)(rem % Spt) + 1);
    }
}

