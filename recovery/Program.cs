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

        var boot = BootSector.Parse(hdd);
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
            TextCarver.Carve(hdd, args[1]);
        }
    }
}