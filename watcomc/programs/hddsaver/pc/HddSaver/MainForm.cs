using HddSaver.Data;
using HddSaver.Protocol;
using HddSaver.Services;

namespace HddSaver
{

    public partial class MainForm : Form
    {
        private readonly SerialConnection _serial = new();
        
        private const int MaxLogLines = 1000;

        public MainForm()
        {
            InitializeComponent();
            cmbPort.Items.AddRange(System.IO.Ports.SerialPort.GetPortNames());
            if (cmbPort.Items.Count > 0)
            {
                cmbPort.SelectedIndex = 0;
            }
            cmbBaud.SelectedIndex = 0;
            WireEvents();
        }

        private void WireEvents()
        {
            _serial.Log += msg => Invoke(() => AppendLog(msg));
            _serial.SectorReceived += (lba, status, hasData) => Invoke(() =>
            {
                var txt = $"LBA {lba} {(status == 0 ? "OK" : $"0x{status:X2}")} {(hasData ? "[data]" : "")}";
                AppendLog(txt);
                lblStatus.Text = txt;
            });
            _serial.StatusReceived += s => Invoke(() =>
            {
                AppendLog($"Status: {s.totalCyls}cyl {s.totalHeads}hd {s.totalSpt}spt LBA={s.currentLba} mask=0x{s.headMask:X2} retries={s.retries}");
                lblProgress.Text = $"LBA {s.currentLba}/{s.totalSectors} ({s.totalCyls}cyl {s.totalHeads}hd {s.totalSpt}spt)";
                lblSectorsReceived.Text = $"Mask: 0x{s.headMask:X2} Retries: {s.retries}";
            });

            progressTimer.Tick += (s, e) => RefreshProgress();
            progressTimer.Start();
        }

        private async void BtnConnect_Click(object? sender, EventArgs e)
        {
            await ToggleConnection();
        }
        private async void BtnStart_Click(object? sender, EventArgs e)
        {
            _serial.SendStart();
            AppendLog("Start sent");
        }
        private async void BtnStop_Click(object? sender, EventArgs e)
        {
            _serial.SendStop();
            AppendLog("Stop sent");
        }
        private async void BtnSeek_Click(object? sender, EventArgs e)
        {
            if (uint.TryParse(txtSeekLba.Text, out uint lba))
            {
                _serial.SendSeek(lba);
                AppendLog($"Seek to LBA {lba}");
            }
        }
        private async void BtnPing_Click(object? sender, EventArgs e)
        {
            _serial.SendPing();
            AppendLog("Ping sent");
        }
        private async void BtnStatus_Click(object? sender, EventArgs e)
        {
            _serial.QueryStatus();
            AppendLog("Status query sent");
        }
        private async void BtnApplyConfig_Click(object? sender, EventArgs e)
        {
            var mask = GetHeadMask();
            _serial.SendHeadMask(mask);
            AppendLog("Head mask sent");
            if (byte.TryParse(txtRetries.Text, out byte retries))
            {
                _serial.SendRetries(retries);
                AppendLog("Retries sent");
            }
        }
        private void BtnBadMap_Click(object? sender, EventArgs e)
        {
            GenerateBadMap();
        }

        private async Task ToggleConnection()
        {
            if (_serial.IsConnected)
            {
                _serial.Disconnect();
                btnConnect.Text = "Connect";
                SetControlsEnabled(false);
                lblConnStatus.Text = "Disconnected";
            }
            else
            {
                try
                {
                    var baud = int.Parse(cmbBaud.Text);
                    _serial.Connect(cmbPort.Text, baud);
                    lblConnStatus.Text = $"Serial {cmbPort.Text} @ {baud}";
                    btnConnect.Text = "Disconnect";
                    SetControlsEnabled(true);
                    await Task.Delay(1000);
                    _serial.SendPing();
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

        private void RefreshProgress()
        {
            if (!_serial.IsConnected) return;
            try
            {
                using var ctx = new HddSaverContext();
                var count = ctx.Sectors.Count();
                var errors = ctx.Sectors.Count(s => !SectorStatus.HasData(s.Status) && s.Status != SectorStatus.HeadSkip);
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
            _serial.Dispose();
            base.OnFormClosing(e);
        }
    }
}