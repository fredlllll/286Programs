using recovery;

namespace Recovery;

class Program
{
    static void Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.Error.WriteLine("Usage: recovery <image> [output_dir]");
            return;
        }

        var hdd = new HddImage(args[0]);

        var mbr = new Mbr(hdd);
        mbr.Parse();
        mbr.Report();
        Console.WriteLine();

        var part = mbr.PickFatPartition();
        if (part == null)
        {
            Console.WriteLine("No usable FAT partition in the MBR - carving raw data only.");
        }
        else
        {
            Console.WriteLine($"Using partition at LBA {part.Value.firstSectorLba} ({part.Value.type})");
            Console.WriteLine();

            try
            {
                var boot = BootSector.Parse(hdd, part.Value.firstSectorLba);
                boot.Report();
                Console.WriteLine();

                var fat = FatTable.Read(hdd, boot);
                fat.Report();
                Console.WriteLine();

                var root = DirectoryReader.Read(hdd, boot, fat);
                root.Report();
                Console.WriteLine();

                if (args.Length > 1)
                {
                    FileRecoverer.Dump(hdd, boot, fat, root, args[1]);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Filesystem parse failed: {ex.Message}");
            }
        }
        Console.WriteLine();

        if (args.Length > 1)
        {
            TextCarver.Carve(hdd, args[1]);
        }
    }
}