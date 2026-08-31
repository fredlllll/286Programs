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

    public MainForm()
    {
        SetupUI();
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

    private void SetupUI()
    {
        Text = "HDD Saver 3.1";
        Size = new Size(900, 700);
        MinimumSize = new Size(700, 500);

        var mainLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            RowCount = 5
        };
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));  // Connection
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));  // Control
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));  // Config
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));  // Progress
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));  // Log + list

        // Connection panel
        var connPanel = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
        cmbPort = new ComboBox { Width = 100, DropDownStyle = ComboBoxStyle.DropDown };
        cmbPort.Items.AddRange(System.IO.Ports.SerialPort.GetPortNames());
        if (cmbPort.Items.Count > 0) cmbPort.SelectedIndex = 0;
        cmbBaud = new ComboBox { Width = 80, DropDownStyle = ComboBoxStyle.DropDown };
        cmbBaud.Items.AddRange(new object[] { "9600", "19200", "38400", "57600", "115200" });
        cmbBaud.SelectedIndex = 0;
        btnConnect = new Button { Text = "Connect", Width = 80 };
        btnConnect.Click += async (s, e) => await ToggleConnection();
        lblConnStatus = new Label { Text = "Disconnected", Width = 200, AutoSize = true };
        connPanel.Controls.AddRange(new Control[] { cmbPort, cmbBaud, btnConnect, lblConnStatus });

        // Control panel
        var ctrlPanel = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
        btnStart = new Button { Text = "Start", Width = 70, Enabled = false };
        btnStart.Click += async (s, e) => await StartDump();
        btnStop = new Button { Text = "Stop", Width = 70, Enabled = false };
        btnStop.Click += async (s, e) => await StopDump();
        txtSeekLba = new TextBox { Width = 100, Text = "0" };
        btnSeek = new Button { Text = "Seek", Width = 60, Enabled = false };
        btnSeek.Click += async (s, e) => await SeekToLba();
        btnPing = new Button { Text = "Ping", Width = 60, Enabled = false };
        btnPing.Click += async (s, e) => await PingDevice();
        btnStatus = new Button { Text = "Status", Width = 60, Enabled = false };
        btnStatus.Click += async (s, e) => await QueryStatus();
        ctrlPanel.Controls.AddRange(new Control[] { btnStart, btnStop, new Label { Text = "Seek LBA:", AutoSize = true, Margin = new Padding(10, 6, 0, 0) }, txtSeekLba, btnSeek, btnPing, btnStatus });

        // Config panel
        var cfgPanel = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
        cmbHeadMask = new ComboBox { Width = 60, DropDownStyle = ComboBoxStyle.DropDown };
        cmbHeadMask.Items.AddRange(new object[] { "FF", "01", "02", "04", "08", "10", "20", "40", "80" });
        cmbHeadMask.SelectedIndex = 0;
        btnApplyConfig = new Button { Text = "Apply Config", Width = 100, Enabled = false };
        btnApplyConfig.Click += async (s, e) => await ApplyConfig();
        cfgPanel.Controls.AddRange(new Control[] { new Label { Text = "Head mask:", AutoSize = true, Margin = new Padding(0, 6, 0, 0) }, cmbHeadMask, btnApplyConfig });

        // Progress panel
        var progPanel = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight };
        lblProgress = new Label { Text = "LBA: -/-", Width = 250, AutoSize = true };
        lblSectorsReceived = new Label { Text = "Received: 0", Width = 150, AutoSize = true };
        lblErrors = new Label { Text = "Errors: 0", Width = 120, AutoSize = true };
        lblStatus = new Label { Text = "", Width = 300, AutoSize = true };
        progPanel.Controls.AddRange(new Control[] { lblProgress, lblSectorsReceived, lblErrors, lblStatus });

        // Log + sector list (split)
        var bottomSplit = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, SplitterDistance = 200 };
        txtLog = new TextBox { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical };
        dgvSectors = new DataGridView
        {
            Dock = DockStyle.Fill,
            ReadOnly = true,
            AllowUserToAddRows = false,
            AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
            SelectionMode = DataGridViewSelectionMode.FullRowSelect
        };
        dgvSectors.Columns.Add("Lba", "LBA");
        dgvSectors.Columns.Add("Status", "Status");
        dgvSectors.Columns.Add("HasData", "Data");
        dgvSectors.Columns.Add("ReceivedAt", "Received");
        bottomSplit.Panel1.Controls.Add(txtLog);
        bottomSplit.Panel2.Controls.Add(dgvSectors);

        mainLayout.Controls.Add(connPanel, 0, 0);
        mainLayout.Controls.Add(ctrlPanel, 0, 1);
        mainLayout.Controls.Add(cfgPanel, 0, 2);
        mainLayout.Controls.Add(progPanel, 0, 3);
        mainLayout.Controls.Add(bottomSplit, 0, 4);

        Controls.Add(mainLayout);
    }

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
            // Status reply is handled by the read loop
            AppendLog("Status query sent");
        }
        catch (Exception ex)
        {
            AppendLog($"Status query failed: {ex.Message}");
        }
    }

    private async Task ApplyConfig()
    {
        if (byte.TryParse(cmbHeadMask.Text, System.Globalization.NumberStyles.HexNumber, null, out byte mask))
        {
            await _receiver.SetHeadMask(mask);
        }
    }

    private async Task RefreshProgress()
    {
        if (!_receiver.IsConnected) return;
        try
        {
            using var ctx = new HddSaverContext();
            var session = ctx.Sessions.OrderByDescending(s => s.Id).FirstOrDefault();
            if (session != null)
            {
                var count = ctx.Sectors.Count(s => s.SessionId == session.Id);
                var errors = ctx.Sectors.Count(s => s.SessionId == session.Id && s.Status != 0 && s.Status != 0x11);
                lblProgress.Text = $"Session #{session.Id}: {count} sectors";
                lblSectorsReceived.Text = $"Received: {count}";
                lblErrors.Text = $"Errors: {errors}";
            }
        }
        catch { }
    }

    private void AppendLog(string message)
    {
        var timestamp = DateTime.Now.ToString("HH:mm:ss");
        txtLog.AppendText($"[{timestamp}] {message}\r\n");
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _receiver.Dispose();
        base.OnFormClosing(e);
    }

    // Controls
    private ComboBox cmbPort;
    private ComboBox cmbBaud;
    private Button btnConnect;
    private Label lblConnStatus;
    private Button btnStart;
    private Button btnStop;
    private TextBox txtSeekLba;
    private Button btnSeek;
    private Button btnPing;
    private Button btnStatus;
    private ComboBox cmbHeadMask;
    private Button btnApplyConfig;
    private Label lblProgress;
    private Label lblSectorsReceived;
    private Label lblErrors;
    private Label lblStatus;
    private TextBox txtLog;
    private DataGridView dgvSectors;
}
