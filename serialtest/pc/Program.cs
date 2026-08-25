using System;
using System.IO.Ports;

class Program
{
    static void Main(string[] args)
    {
        string portName = args.Length > 0 ? args[0] : "COM1";

        Console.WriteLine("opening " + portName + " at 9600 8N1...");

        using var port = new SerialPort(portName, 9600, Parity.None, 8, StopBits.One);
        try
        {
            port.Open();
        }
        catch (Exception e)
        {
            Console.WriteLine("failed to open " + portName + ": " + e.Message);
            Environment.Exit(1);
        }

        port.DiscardInBuffer();
        port.DiscardOutBuffer();

        Console.WriteLine("sending ping...");
        port.Write("ping");

        Console.WriteLine("waiting for pong...");
        port.ReadTimeout = 5000;
        char[] buf = new char[4];
        int n = 0;
        while (n < 4)
        {
            try
            {
                int r = port.Read(buf, n, 4 - n);
                if (r == 0) break;
                n += r;
            }
            catch (TimeoutException)
            {
                break;
            }
        }

        string response = new string(buf, 0, n);
        if (response == "pong")
        {
            Console.WriteLine("SUCCESS: got pong!");
        }
        else
        {
            Console.WriteLine("FAIL: expected 'pong', got '" + response + "'");
        }
    }
}
