using HddSaver.Data;
using HddSaver.Models;
using Microsoft.EntityFrameworkCore;
using System.IO;
using System.IO.Ports;
using System.Text;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;

namespace HddSaver.Protocol;

public class SerialConnection : IDisposable
{
    private class PendingMessage
    {
        public byte[] Bytes = null!;
        public DateTime SentAt;
        public int Attempts;
    }

    public const byte HEADER_MAGIC0 = 0xAA;
    public const byte HEADER_MAGIC1 = 0x55;
    private const int AckTimeoutMs = 750; // generous: a sector transfer alone can
                                          // take ~550ms at 9600 baud, plus disk time
    private const int MaxRetries = 10;

    private Stream? _stream;
    private object _streamWriterLock = new();
    private object _streamReaderLock = new();
    private SerialPort? _port;
    private volatile bool _running = false;
    private Thread? _thread;
    private BinaryReader _reader = null!;
    private BinaryWriter _writer = null!;
    private uint currentSentMessageNum = 1;
    private readonly object _pendingLock = new();
    private Dictionary<uint, PendingMessage> sentMessages = new();
    private System.Threading.Timer? _retryTimer;

    public event Action<string>? Log;
    public event Action<uint, byte, bool>? SectorReceived;
    public event Action<StatusReply>? StatusReceived;

    public bool IsConnected => _stream != null;

    private void SendMagic()
    {
        lock (_streamWriterLock)
        {
            _writer.Write(HEADER_MAGIC0);
            _writer.Write(HEADER_MAGIC1);
        }
    }
    private void SendAck(uint num)
    {
        lock (_streamWriterLock)
        {
            SendMagic();
            _writer.Write(Opcode.Ack);
            _writer.Write(num);
        }
    }

    private void SendNack(uint num)
    {
        lock (_streamWriterLock)
        {
            SendMagic();
            _writer.Write(Opcode.Nack);
            _writer.Write(num);
        }
    }

    /// <summary>
    /// retransmits a message
    /// </summary>
    /// <param name="num"></param>
    private void Retransmit(uint num)
    {
        byte[]? bytes = null;
        lock (_pendingLock)
        {
            if (sentMessages.TryGetValue(num, out var pending))
            {
                bytes = pending.Bytes;
                pending.SentAt = DateTime.UtcNow;
                pending.Attempts++;
            }
        }
        if (bytes == null) { Log?.Invoke($"Tried to retransmit not existing message with number {num}"); return; }
        lock (_streamWriterLock) { SendMagic(); _stream!.Write(bytes); }
    }

    /// <summary>
    /// removes the message out of the sent messages cause it was confirmed that the other side received it
    /// </summary>
    /// <param name="num"></param>
    private void ConfirmReceived(uint num)
    {
        lock (_pendingLock) { sentMessages.Remove(num); }
    }

    private void SendMessage(uint num, byte[] bytes)
    {
        lock (_streamWriterLock) { SendMagic(); _stream!.Write(bytes); }
        Log?.Invoke($"Sent message with num {num}");
        lock (_pendingLock)
        {
            sentMessages[num] = new PendingMessage { Bytes = bytes, SentAt = DateTime.UtcNow };
        }
    }

    private uint GetNextMessageNum()
    {
        return Interlocked.Increment(ref currentSentMessageNum);
    }

    private byte[] AssembleMessage(uint num, Opcode opcode)
    {
        using var ms = new MemoryStream();
        var writer = new BinaryWriter(ms, Encoding.ASCII, true);

        writer.Write(opcode);
        writer.Write(num);

        return ms.ToArray();
    }

    private byte[] AssembleMessage<T>(uint num, Opcode opcode, T body) where T : unmanaged
    {
        using var ms = new MemoryStream();
        var writer = new BinaryWriter(ms, Encoding.ASCII, true);


        writer.Write(opcode);
        writer.Write(num);
        writer.WriteStruct(body);

        return ms.ToArray();
    }

    public void Connect(string portName, int baudRate = 9600)
    {
        Disconnect();
        _port = new SerialPort(portName, baudRate, Parity.None, 8, StopBits.One)
        {
            WriteTimeout = 5000,
            ReadTimeout = 5000,
            Handshake = Handshake.RequestToSend
        };
        _port.Open();
        _stream = _port.BaseStream;
        _reader = new BinaryReader(_stream, Encoding.ASCII, true);
        _writer = new BinaryWriter(_stream, Encoding.ASCII, true);
        _port.DiscardInBuffer();
        StartReceiving();
        _retryTimer = new System.Threading.Timer(_ => CheckRetries(), null, 100, 100);
        Log?.Invoke($"Connected to {portName} @ {baudRate}");
    }

    public void Disconnect()
    {
        _retryTimer?.Dispose();
        _retryTimer = null;
        StopReceiving();
        _stream?.Close();
        _stream = null;
        _port?.Dispose();
        _port = null;
        try
        {
            _reader?.Dispose();
            _writer?.Dispose();
        }
        catch
        {
            //ignore, they shouldnt close the underlying stream, but they still try to dispose it
        }
        Log?.Invoke("Disconnected");
    }

    public void SendPing()
    {
        var num = GetNextMessageNum();
        var body = AssembleMessage(num, Opcode.Ping);
        SendMessage(num, body);
    }

    public void SendStart()
    {
        var num = GetNextMessageNum();
        var body = AssembleMessage(num, Opcode.Start);
        SendMessage(num, body);
    }

    public void SendStop()
    {
        var num = GetNextMessageNum();
        var body = AssembleMessage(num, Opcode.Stop);
        SendMessage(num, body);
    }

    struct SeekBody
    {
        public uint lba;
    }

    public void SendSeek(uint lba)
    {
        var bytes = new SeekBody
        {
            lba = lba,
        };
        var num = GetNextMessageNum();
        var body = AssembleMessage(num, Opcode.Seek, bytes);
        SendMessage(num, body);
    }

    public void QueryStatus()
    {
        var num = GetNextMessageNum();
        var body = AssembleMessage(num, Opcode.SendStatus);
        SendMessage(num, body);
    }

    struct HeadMaskBody
    {
        public byte mask;
    }
    public void SendHeadMask(byte mask)
    {
        var msk = new HeadMaskBody() { mask = mask };
        var num = GetNextMessageNum();
        var body = AssembleMessage(num, Opcode.HeadMask, msk);
        SendMessage(num, body);
    }

    struct RetriesBody
    {
        public byte retries;
    }
    public void SendRetries(byte retries)
    {
        var bdy = new RetriesBody() { retries = retries };
        var num = GetNextMessageNum();
        var body = AssembleMessage(num, Opcode.Retries, bdy);
        SendMessage(num, body);
    }

    public void StartReceiving()
    {
        StopReceiving();
        _running = true;
        _thread = new Thread(ReadLoop);
        _thread.IsBackground = true;
        _thread.Start();
    }

    public void StopReceiving()
    {
        _running = false;
        if (_thread != null)
        {
            _thread.Interrupt();
            _thread.Join();
            _thread = null;
        }
    }

    private bool VerifyMagic()
    {
        try
        {
            lock (_streamReaderLock)
            {
                // resync scanner: consume bytes until we see the 0xAA 0x55
                // pair. a stray byte just gets eaten and we re-lock on the
                // next real magic instead of reading from a shifted position.
                bool gotMagic0 = false;
                while (true)
                {
                    byte magic = _reader.ReadByte();
                    if (!gotMagic0)
                    {
                        if (magic == HEADER_MAGIC0)
                        {
                            gotMagic0 = true;
                        }
                        continue;
                    }
                    if (magic == HEADER_MAGIC1)
                    {
                        return true;
                    }
                    if (magic == HEADER_MAGIC0)
                    {
                        continue; // back-to-back magics: this one may start the real pair
                    }
                    gotMagic0 = false;
                }
            }
        }
        catch (TimeoutException)
        {
            return false; //no data came in time
        }
    }

    private void ReadLoop()
    {
        /*
        Message structure:
        Byte Magic 0
        Byte Magic 1
        Byte opcode
        Uint packetNumber
        [optional body of packet, depends on opcode]
        */
        while (_running)
        {
            try
            {
                lock (_streamReaderLock)
                {
                    if (!VerifyMagic())
                    {
                        continue;
                    }
                    var opcode = _reader.ReadOpcode();
                    uint packetNumber = _reader.ReadUInt32Exactly();
                    if (opcode == Opcode.Ack)
                    {
                        Log?.Invoke($"ack received  {packetNumber}");
                        ConfirmReceived(packetNumber);
                        continue;
                    }
                    if (opcode == Opcode.Nack)
                    {
                        Log?.Invoke($"nack received {packetNumber}");
                        Retransmit(packetNumber);
                        continue;
                    }
                    if (processPacket(opcode))
                    {
                        SendAck(packetNumber);
                    }
                    else
                    {
                        Log?.Invoke($"failed processing {opcode} {packetNumber}");
                        SendNack(packetNumber);
                    }
                }
            }
            catch (ThreadInterruptedException)
            {
                break;
            }
            catch (Exception ex)
            {
                Log?.Invoke($"exception in read loop: {ex}");
            }
        }
    }

    private bool ProcessSectorPacket()
    {
        SectorHeader sh = _reader.ReadStruct<SectorHeader>();
        byte[]? data = null;
        if (sh.HasData)
        {
            data = _reader.ReadBytesExactly(512);
            if (!sh.VerifyDataCrc(data))
            {
                Log?.Invoke($"failed to verify data for sector {sh.lba}");
                return false;
            }
        }

        SaveSector(sh, data);
        SectorReceived?.Invoke(sh.lba, sh.status, data != null);
        return true;
    }

    private bool ProcessStatusPacket()
    {
        StatusReply reply = _reader.ReadStruct<StatusReply>();
        StatusReceived?.Invoke(reply!);
        return true;
    }

    private bool processPacket(Opcode opcode)
    {
        switch (opcode)
        {
            case Opcode.Pong:
                Log?.Invoke("Pong Received");
                return true;
            case Opcode.Sector:
                return ProcessSectorPacket();
            case Opcode.Status:
                return ProcessStatusPacket();
        }
        return false;
    }

    private void SaveSector(SectorHeader header, byte[]? data)
    {
        using var ctx = new HddSaverContext();
        var sector = new Sector
        {
            Lba = header.lba,
            Status = header.status,
            Data = data,
            ReceivedAt = DateTime.Now
        };
        ctx.Sectors.Add(sector);
        ctx.SaveChanges();
    }

    public void Dispose()
    {
        Disconnect();
        GC.SuppressFinalize(this);
    }

    private void CheckRetries()
    {
        var toResend = new List<(uint num, byte[] bytes)>();
        var toGiveUp = new List<uint>();

        lock (_pendingLock)
        {
            var now = DateTime.UtcNow;
            foreach (var kvp in sentMessages)
            {
                if ((now - kvp.Value.SentAt).TotalMilliseconds < AckTimeoutMs)
                    continue;

                if (kvp.Value.Attempts >= MaxRetries)
                {
                    toGiveUp.Add(kvp.Key);
                    continue;
                }

                kvp.Value.Attempts++;
                kvp.Value.SentAt = now;
                toResend.Add((kvp.Key, kvp.Value.Bytes));
            }
            foreach (var num in toGiveUp) sentMessages.Remove(num);
        }

        foreach (var num in toGiveUp)
            Log?.Invoke($"Giving up on message {num} after {MaxRetries} retries - no ack received");

        foreach (var (num, bytes) in toResend)
        {
            lock (_streamWriterLock) { SendMagic(); _stream!.Write(bytes); }
            Log?.Invoke($"Resending message {num} (no ack yet)");
        }
    }
}
