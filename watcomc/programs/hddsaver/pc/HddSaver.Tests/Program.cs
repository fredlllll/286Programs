using System.Net.Sockets;
using HddSaver.Protocol;

class Tests
{
    static int _passed, _failed;

    static void Assert(bool cond, string msg)
    {
        if (!cond) throw new Exception(msg);
    }

    static void Test(string name, Action fn)
    {
        try { fn(); Console.WriteLine($"  PASS  {name}"); _passed++; }
        catch (Exception e) { Console.WriteLine($"  FAIL  {name}: {e.Message}"); _failed++; }
    }

    static TcpClient Connect()
    {
        var c = new TcpClient();
        c.Connect("localhost", 4321);
        c.NoDelay = true;
        return c;
    }

    static async Task SendAsync(NetworkStream s, byte cmd, byte[]? payload = null)
    {
        var buf = new byte[1 + (payload?.Length ?? 0)];
        buf[0] = cmd;
        payload?.CopyTo(buf, 1);
        await s.WriteAsync(buf);
    }

    static async Task<byte[]> ReadExactAsync(NetworkStream s, int n)
    {
        var buf = new byte[n];
        int off = 0;
        while (off < n)
        {
            int r = await s.ReadAsync(buf.AsMemory(off, n - off));
            if (r == 0) throw new TimeoutException("Connection closed");
            off += r;
        }
        return buf;
    }

    static async Task DrainAsync(NetworkStream s)
    {
        var buf = new byte[4096];
        try
        {
            s.ReadTimeout = 500;
            while (true)
            {
                int r = await s.ReadAsync(buf);
                if (r == 0) break;
            }
        }
        catch { }
    }

    static async Task WaitForIdle(TcpClient tcp)
    {
        var s = tcp.GetStream();
        await Task.Delay(300);
        await DrainAsync(s);
        await SendAsync(s, Command.PING);
        var resp = await ReadExactAsync(s, 1);
        Assert(resp[0] == Command.READY, $"Not idle: got 0x{resp[0]:X2}");
    }

    static async Task<(uint lba, byte status, ushort dataCrc)> ReadSectorHeader(NetworkStream s)
    {
        var hdr = await ReadExactAsync(s, Command.HeaderSize);
        var parsed = SectorHeader.Parse(hdr);
        return (parsed.Lba, parsed.Status, parsed.DataCrc);
    }

    static async Task Main()
    {
        Console.WriteLine("============================================================");
        Console.WriteLine("HddSaver C# Protocol Test Suite");
        Console.WriteLine("============================================================\n");

        // --- PING ---
        Console.WriteLine("[PING]");

        Test("PING returns READY", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await SendAsync(s, Command.PING);
            var resp = await ReadExactAsync(s, 1);
            Assert(resp[0] == Command.READY, $"Expected READY, got 0x{resp[0]:X2}");
        });

        Test("PING twice returns READY both times", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await SendAsync(s, Command.PING);
            var r = await ReadExactAsync(s, 1);
            Assert(r[0] == Command.READY, "First PING failed");
            await SendAsync(s, Command.PING);
            r = await ReadExactAsync(s, 1);
            Assert(r[0] == Command.READY, "Second PING failed");
        });

        // --- HEAD_MASK ---
        Console.WriteLine("\n[HEAD_MASK]");

        Test("HEAD_MASK returns ACK", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await SendAsync(s, Command.HEAD_MASK, [0x15]);
            var resp = await ReadExactAsync(s, 1);
            Assert(resp[0] == Command.ACK, $"Expected ACK, got 0x{resp[0]:X2}");
        });

        // --- RETRIES ---
        Console.WriteLine("\n[RETRIES]");

        Test("RETRIES returns ACK", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await SendAsync(s, Command.RETRIES, [3]);
            var resp = await ReadExactAsync(s, 1);
            Assert(resp[0] == Command.ACK, $"Expected ACK, got 0x{resp[0]:X2}");
        });

        // --- STATUS ---
        Console.WriteLine("\n[STATUS]");

        Test("STATUS returns valid geometry with correct CRC", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await SendAsync(s, Command.STATUS);
            var reply = await ReadExactAsync(s, 22);
            Assert(reply[0] == 0xAA && reply[1] == 0x55, "Bad magic");
            ushort cyls = (ushort)(reply[2] | (reply[3] << 8));
            byte heads = reply[4];
            byte spt = reply[5];
            uint total = (uint)reply[6] | ((uint)reply[7] << 8) | ((uint)reply[8] << 16) | ((uint)reply[9] << 24);
            uint lba = (uint)reply[10] | ((uint)reply[11] << 8) | ((uint)reply[12] << 16) | ((uint)reply[13] << 24);
            byte mask = reply[14];
            byte retries = reply[15];
            ushort crc = (ushort)(reply[20] | (reply[21] << 8));
            ushort computed = SectorHeader.Crc16(reply.AsSpan(0, 20));
            Assert(computed == crc, $"CRC mismatch: computed 0x{computed:X4}, got 0x{crc:X4}");
            Console.WriteLine($"         cyls={cyls} heads={heads} spt={spt} total={total} lba={lba} mask=0x{mask:X2} retries={retries}");
            Assert(cyls > 0 && heads > 0 && spt > 0, "Invalid geometry");
        });

        Test("STATUS CRC validation matches 286 implementation", async () =>
        {
            // Verify the table-driven CRC matches the bit-by-bit CRC
            byte[] testData = [0xAA, 0x55, 0x04, 0x02, 0x1A, 0x06, 0xD8, 0x0B];
            ushort tableCrc = SectorHeader.Crc16(testData);
            ushort bitByBit = 0xFFFF;
            foreach (byte b in testData)
            {
                bitByBit ^= (ushort)(b << 8);
                for (int i = 0; i < 8; i++)
                    bitByBit = (bitByBit & 0x8000) != 0
                        ? (ushort)((bitByBit << 1) ^ 0x1021)
                        : (ushort)(bitByBit << 1);
            }
            Assert(tableCrc == bitByBit, $"CRC mismatch: table=0x{tableCrc:X4}, bit=0x{bitByBit:X4}");
        });

        // --- START/STOP ---
        Console.WriteLine("\n[START/STOP]");

        Test("START streams 5 sectors then STOP", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await SendAsync(s, Command.START);
            for (int i = 0; i < 5; i++)
            {
                var (lba, status, _) = await ReadSectorHeader(s);
                bool hasData = status is 0x00 or 0x11;
                Console.WriteLine($"         LBA={lba} status=0x{status:X2} data={hasData}");
                if (hasData) await ReadExactAsync(s, Command.DataSize);
                await SendAsync(s, Command.ACK);
            }
            await SendAsync(s, Command.STOP);
            await Task.Delay(200);
        });

        // --- NAK retransmit ---
        Console.WriteLine("\n[NAK RETRANSMIT]");

        Test("NAK causes same sector retransmit", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await SendAsync(s, Command.START);
            var (lba1, st1, _) = await ReadSectorHeader(s);
            await SendAsync(s, Command.NAK);
            var (lba2, st2, _) = await ReadSectorHeader(s);
            Assert(lba1 == lba2, $"Different LBA: {lba1} vs {lba2}");
            Assert(st1 == st2, "Different status");
            await SendAsync(s, Command.ACK);
            await SendAsync(s, Command.STOP);
            await Task.Delay(200);
        });

        // --- Streaming config ---
        Console.WriteLine("\n[STREAMING CONFIG]");

        Test("HEAD_MASK + START streams correctly", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await WaitForIdle(tcp);
            await SendAsync(s, Command.HEAD_MASK, [0x01]);
            var resp = await ReadExactAsync(s, 1);
            Assert(resp[0] == Command.ACK, $"Expected ACK, got 0x{resp[0]:X2}");
            await SendAsync(s, Command.START);
            var (lba, status, _) = await ReadSectorHeader(s);
            Console.WriteLine($"         Got sector: LBA={lba} status=0x{status:X2}");
            await SendAsync(s, Command.ACK);
            await SendAsync(s, Command.STOP);
            await Task.Delay(200);
        });

        Test("RETRIES + START streams correctly", async () =>
        {
            using var tcp = Connect();
            var s = tcp.GetStream();
            await WaitForIdle(tcp);
            await SendAsync(s, Command.RETRIES, [10]);
            var resp = await ReadExactAsync(s, 1);
            Assert(resp[0] == Command.ACK, $"Expected ACK, got 0x{resp[0]:X2}");
            await SendAsync(s, Command.START);
            var (lba, status, _) = await ReadSectorHeader(s);
            Console.WriteLine($"         Got sector: LBA={lba} status=0x{status:X2}");
            await SendAsync(s, Command.ACK);
            await SendAsync(s, Command.STOP);
            await Task.Delay(200);
        });

        // --- SectorHeader parsing ---
        Console.WriteLine("\n[PROTOCOL PARSING]");

        Test("SectorHeader.Parse validates magic bytes", async () =>
        {
            byte[] badMagic = new byte[10];
            badMagic[0] = 0x00;
            try { SectorHeader.Parse(badMagic); Assert(false, "Should have thrown"); }
            catch (InvalidDataException) { }
        });

        Test("SectorHeader.Parse validates header CRC", async () =>
        {
            byte[] hdr = [0xAA, 0x55, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0xFF];
            try { SectorHeader.Parse(hdr); Assert(false, "Should have thrown"); }
            catch (InvalidDataException) { }
        });

        Test("SectorHeader.TryParse returns false on bad data", async () =>
        {
            byte[] bad = new byte[10];
            var ok = SectorHeader.TryParse(bad, out var result, out var error);
            Assert(!ok, "Should be false");
            Assert(error != null, "Should have error message");
        });

        Test("SectorHeader.TryParse succeeds on valid header", async () =>
        {
            // Build a valid header for LBA 42, status 0x01
            byte[] hdr = [0xAA, 0x55, 42, 0, 0, 0x01, 0, 0, 0, 0];
            ushort crc = SectorHeader.Crc16(hdr.AsSpan(0, 8));
            hdr[8] = (byte)(crc & 0xFF);
            hdr[9] = (byte)(crc >> 8);
            var ok = SectorHeader.TryParse(hdr, out var result, out var error);
            Assert(ok, $"Parse failed: {error}");
            Assert(result!.Lba == 42, $"LBA={result.Lba}");
            Assert(result.Status == 0x01, $"Status=0x{result.Status:X2}");
        });

        Test("ValidateDataCrc matches for real data", async () =>
        {
            byte[] data = new byte[512];
            for (int i = 0; i < 512; i++) data[i] = (byte)(i & 0xFF);
            ushort crc = SectorHeader.Crc16(data);
            Assert(SectorHeader.ValidateDataCrc(data, crc), "Should validate");
            Assert(!SectorHeader.ValidateDataCrc(data, (ushort)(crc + 1)), "Should fail with wrong CRC");
        });

        // --- Summary ---
        Console.WriteLine("\n" + new string('=', 60));
        int total = _passed + _failed;
        Console.WriteLine($"Results: {_passed}/{total} passed, {_failed} failed");
        Console.WriteLine(new string('=', 60));
        Environment.Exit(_failed == 0 ? 0 : 1);
    }
}
