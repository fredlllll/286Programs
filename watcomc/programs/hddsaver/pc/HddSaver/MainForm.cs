using HddSaver.Data;
using HddSaver.Models;
using HddSaver.Protocol;
using HddSaver.Services;
using Microsoft.EntityFrameworkCore;

namespace HddSaver;

public partial class MainForm : Form
{
    private readonly SerialReceiver _receiver = new();
    private System.Windows.Forms.Timer _progressTimer;
    private const int MaxLogLines = 1000;

    public MainForm()
    {
        InitializeComponent();
        cmbPort.Items.AddRange(System.IO.Ports.SerialPort.GetPortNames());
        if (cmbPort.Items.Count > 0) cmbPort.SelectedIndex = 0;
        WireEvents();
    }

    private void WireEvents()
    {
        _receiver.Log += msg => Invoke(() => AppendLog(msg));
        _receiver.SectorReceived += (lba, status, hasData) => Invoke(() =>
        {
            lblStatus.Text = $"LBA {lba} {(status == 0 ? "OK" : $"0x{status:X2}")} {(hasData ? "[data]" : "")}";
        });
        _receiver.SectorError += (lba, err) => Invoke(() => AppendLog($"LBA {lba} error: {err}"));
        _receiver.ProgressChanged += count => Invoke(() =>
        {
            lblSectorsReceived.Text = $"Received: {count}";
        });
        _receiver.DumpComplete += () => Invoke(() => AppendLog("Dump complete!"));

        _progressTimer = new System.Windows.Forms.Timer { Interval = 5000 };
        _progressTimer.Tick += async (s, e) => await RefreshProgress();
        _progressTimer.Start();
    }

    private async void btnConnect_Click(object? sender, EventArgs e) => await ToggleConnection();
    private async void btnStart_Click(object? sender, EventArgs e) => await StartDump();
    private async void btnStop_Click(object? sender, EventArgs e) => await StopDump();
    private async void btnSeek_Click(object? sender, EventArgs e) => await SeekToLba();
    private async void btnPing_Click(object? sender, EventArgs e) => await PingDevice();
    private async void btnStatus_Click(object? sender, EventArgs e) => await QueryStatus();
    private async void btnApplyConfig_Click(object? sender, EventArgs e) => await ApplyConfig();
    private void btnBadMap_Click(object? sender, EventArgs e) => GenerateBadMap();

    private async Task ToggleConnection()
    {
        if (_receiver.IsConnected)
        {
            _receiver.Disconnect();
            btnConnect.Text = "Connect";
            SetControlsEnabled(false);
            lblConnStatus.Text = "Disconnected";
        }
        else
        {
            try
            {
                var baud = int.Parse(cmbBaud.Text);
                _receiver.Connect(cmbPort.Text, baud);
                btnConnect.Text = "Disconnect";
                SetControlsEnabled(true);
                lblConnStatus.Text = $"Connected @ {baud}";
                await Task.Delay(1000);
                await PingDevice();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Connection failed: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }

    private void SetControlsEnabled(bool enabled)
    {
        btnStart.Enabled = enabled;
        btnStop.Enabled = enabled;
        btnSeek.Enabled = enabled;
        btnPing.Enabled = enabled;
        btnStatus.Enabled = enabled;
        btnApplyConfig.Enabled = enabled;
    }

    private byte GetHeadMask()
    {
        byte mask = 0;
        if (chkHead0.Checked) mask |= 0x01;
        if (chkHead1.Checked) mask |= 0x02;
        if (chkHead2.Checked) mask |= 0x04;
        if (chkHead3.Checked) mask |= 0x08;
        if (chkHead4.Checked) mask |= 0x10;
        if (chkHead5.Checked) mask |= 0x20;
        return mask;
    }

    private async Task StartDump()
    {
        await _receiver.StartDump();
        lblConnStatus.Text = "Receiving...";
    }

    private async Task StopDump()
    {
        await _receiver.StopDump();
        lblConnStatus.Text = $"Connected @ {cmbBaud.Text}";
    }

    private async Task SeekToLba()
    {
        if (uint.TryParse(txtSeekLba.Text, out uint lba))
        {
            await _receiver.SeekTo(lba);
            AppendLog($"Seek to LBA {lba}");
        }
    }

    private async Task PingDevice()
    {
        try
        {
            await _receiver.Ping();
        }
        catch (Exception ex)
        {
            AppendLog($"Ping failed: {ex.Message}");
        }
    }

    private async Task QueryStatus()
    {
        try
        {
            await _receiver.SendCommand(Command.STATUS);
            AppendLog("Status query sent");
        }
        catch (Exception ex)
        {
            AppendLog($"Status query failed: {ex.Message}");
        }
    }

    private async Task ApplyConfig()
    {
        var mask = GetHeadMask();
        await _receiver.SetHeadMask(mask);
        if (byte.TryParse(txtRetries.Text, out byte retries))
        {
            await _receiver.SetRetries(retries);
        }
    }

    private void GenerateBadMap()
    {
        using var dlg = new SaveFileDialog
        {
            Filter = "SVG files (*.svg)|*.svg",
            DefaultExt = "svg",
            FileName = "badmap.svg"
        };
        if (dlg.ShowDialog() != DialogResult.OK) return;

        try
        {
            // Tandon geometry: 820 cyl, 6 heads, 26 spt
            BadMapGenerator.Generate(dlg.FileName, 820, 6, 26);
            AppendLog($"Bad map saved to {dlg.FileName}");
        }
        catch (Exception ex)
        {
            AppendLog($"Bad map failed: {ex.Message}");
        }
    }

    private async Task RefreshProgress()
    {
        if (!_receiver.IsConnected) return;
        try
        {
            using var ctx = new HddSaverContext();
            var count = ctx.Sectors.Count();
            var errors = ctx.Sectors.Count(s => s.Status != 0 && s.Status != 0x11);
            lblProgress.Text = $"Sectors: {count}";
            lblSectorsReceived.Text = $"Received: {count}";
            lblErrors.Text = $"Errors: {errors}";
        }
        catch { }
    }

    private void AppendLog(string message)
    {
        var timestamp = DateTime.Now.ToString("HH:mm:ss");
        txtLog.AppendText($"[{timestamp}] {message}\r\n");

        // Cap line count to avoid memory growth
        var lines = txtLog.Lines;
        if (lines.Length > MaxLogLines)
        {
            txtLog.Text = string.Join("\r\n", lines.Skip(lines.Length - MaxLogLines));
            txtLog.SelectionStart = txtLog.TextLength;
            txtLog.ScrollToCaret();
        }
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _receiver.Dispose();
        base.OnFormClosing(e);
    }
}
