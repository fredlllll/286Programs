using System.IO;
using System.IO.Ports;
using System.Net.Sockets;
using HddSaver.Data;
using HddSaver.Models;
using HddSaver.Protocol;
using Microsoft.EntityFrameworkCore;

namespace HddSaver.Services;

public class SerialReceiver : IDisposable
{
    private Stream? _stream;
    private SerialPort? _port;
    private TcpClient? _tcp;
    private CancellationTokenSource? _cts;
    private Task? _readTask;

    public event Action<string>? Log;
    public event Action<uint, byte, bool>? SectorReceived;
    public event Action<uint, string>? SectorError;
    public event Action? DumpComplete;
    public event Action<int>? ProgressChanged;

    private int _sectorsReceived;
    private int _errors;

    public bool IsConnected => _stream != null;
    public bool IsReceiving { get; private set; }

    public void ConnectSerial(string portName, int baudRate = 9600)
    {
        Disconnect();
        _port = new SerialPort(portName, baudRate, Parity.None, 8, StopBits.One)
        {
            ReadTimeout = 5000,
            WriteTimeout = 1000
        };
        _port.Open();
        _stream = _port.BaseStream;
        Log?.Invoke($"Connected to {portName} @ {baudRate}");
    }

    public void ConnectTcp(string host, int port)
    {
        Disconnect();
        _tcp = new TcpClient();
        _tcp.Connect(host, port);
        _tcp.NoDelay = true;
        _stream = _tcp.GetStream();
        _stream.ReadTimeout = 5000;
        _stream.WriteTimeout = 1000;
        Log?.Invoke($"Connected to {host}:{port} (TCP)");
    }

    public void Disconnect()
    {
        StopReceiving();
        _stream?.Close();
        _stream = null;
        _port?.Dispose();
        _port = null;
        _tcp?.Dispose();
        _tcp = null;
        Log?.Invoke("Disconnected");
    }

    public async Task SendCommand(byte cmd, byte[]? payload = null)
    {
        if (_stream == null) return;
        await Task.Run(() =>
        {
            _stream.Write(new[] { cmd }, 0, 1);
            if (payload != null)
                _stream.Write(payload, 0, payload.Length);
        });
    }

    public async Task Ping()
    {
        await SendCommand(Command.PING);
        await Task.Run(() =>
        {
            var buf = new byte[1];
            _stream!.Read(buf, 0, 1);
            if (buf[0] == Command.READY)
                Log?.Invoke("286 is ready");
            else
                Log?.Invoke($"Unexpected response: 0x{buf[0]:X2}");
        });
    }

    public async Task StartDump()
    {
        using var ctx = new HddSaverContext();
        ctx.Database.Migrate();

        _sectorsReceived = 0;
        _errors = 0;
        IsReceiving = true;
        if (_port != null) _port.DiscardInBuffer();
        await SendCommand(Command.START);
        Log?.Invoke("Dump started");
        StartReading();
    }

    public async Task StopDump()
    {
        await SendCommand(Command.STOP);
        IsReceiving = false;
        Log?.Invoke("Stop requested");
    }

    public async Task SeekTo(uint lba)
    {
        var payload = new byte[]
        {
            (byte)(lba & 0xFF),
            (byte)((lba >> 8) & 0xFF),
            (byte)((lba >> 16) & 0xFF)
        };
        await SendCommand(Command.SEEK, payload);
        Log?.Invoke($"Seek to LBA {lba}");
    }

    public async Task SetHeadMask(byte mask)
    {
        await SendCommand(Command.HEAD_MASK, new[] { mask });
        Log?.Invoke($"Head mask set to 0x{mask:X2}");
    }

    public async Task SetRetries(byte retries)
    {
        await SendCommand(Command.RETRIES, new[] { retries });
        Log?.Invoke($"Retries set to {retries}");
    }

    public void StartReading()
    {
        StopReceiving();
        _cts = new CancellationTokenSource();
        _readTask = Task.Run(() => ReadLoop(_cts.Token));
    }

    public void StopReceiving()
    {
        _cts?.Cancel();
        _readTask?.Wait(TimeSpan.FromSeconds(5));
        _cts?.Dispose();
        _cts = null;
    }

    private async Task ReadLoop(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested && _stream != null)
        {
            try
            {
                var headerBuf = new byte[Command.HeaderSize];
                await ReadExactAsync(headerBuf, ct);

                if (!SectorHeader.TryParse(headerBuf, out var header, out var error))
                {
                    Log?.Invoke($"Header error: {error}");
                    await SendCommand(Command.NAK);
                    _errors++;
                    continue;
                }

                byte[]? data = null;
                if (header!.HasData)
                {
                    data = new byte[Command.DataSize];
                    await ReadExactAsync(data, ct);

                    if (!SectorHeader.ValidateDataCrc(data, header.DataCrc))
                    {
                        Log?.Invoke($"Data CRC error for LBA {header.Lba}");
                        await SendCommand(Command.NAK);
                        _errors++;
                        continue;
                    }
                }

                await SaveSector(header, data);
                await SendCommand(Command.ACK);

                _sectorsReceived++;
                SectorReceived?.Invoke(header.Lba, header.Status, data != null);
                ProgressChanged?.Invoke(_sectorsReceived);
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (TimeoutException)
            {
                continue;
            }
            catch (Exception ex)
            {
                Log?.Invoke($"Read error: {ex.Message}");
                break;
            }
        }
    }

    private async Task ReadExactAsync(byte[] buffer, CancellationToken ct)
    {
        int offset = 0;
        while (offset < buffer.Length)
        {
            ct.ThrowIfCancellationRequested();
            int read = await Task.Run(() =>
            {
                try { return _stream!.Read(buffer, offset, buffer.Length - offset); }
                catch (IOException) { return 0; }
                catch (TimeoutException) { return 0; }
            }, ct);
            if (read == 0)
                throw new TimeoutException("Serial read timeout");
            offset += read;
        }
    }

    private async Task SaveSector(SectorHeader header, byte[]? data)
    {
        using var ctx = new HddSaverContext();
        var sector = new Sector
        {
            Lba = header.Lba,
            Status = header.Status,
            Data = data,
            ReceivedAt = DateTime.Now
        };
        ctx.Sectors.Add(sector);
        await ctx.SaveChangesAsync();
    }

    public void Dispose()
    {
        Disconnect();
        GC.SuppressFinalize(this);
    }
}
