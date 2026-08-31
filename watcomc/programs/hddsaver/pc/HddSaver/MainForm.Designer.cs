namespace HddSaver;

partial class MainForm
{
    private System.ComponentModel.IContainer components = null;

    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }
        base.Dispose(disposing);
    }

    private void InitializeComponent()
    {
        cmbPort = new ComboBox();
        cmbBaud = new ComboBox();
        btnConnect = new Button();
        lblConnStatus = new Label();
        btnStart = new Button();
        btnStop = new Button();
        txtSeekLba = new TextBox();
        btnSeek = new Button();
        btnPing = new Button();
        btnStatus = new Button();
        cmbHeadMask = new ComboBox();
        btnApplyConfig = new Button();
        lblProgress = new Label();
        lblSectorsReceived = new Label();
        lblErrors = new Label();
        lblStatus = new Label();
        txtLog = new TextBox();
        dgvSectors = new DataGridView();

        var mainLayout = new TableLayoutPanel();
        var connPanel = new FlowLayoutPanel();
        var ctrlPanel = new FlowLayoutPanel();
        var cfgPanel = new FlowLayoutPanel();
        var progPanel = new FlowLayoutPanel();
        var bottomSplit = new SplitContainer();
        var lblSeekLba = new Label();
        var lblHeadMask = new Label();

        ((System.ComponentModel.ISupportInitialize)bottomSplit).BeginInit();
        bottomSplit.Panel1.SuspendLayout();
        bottomSplit.Panel2.SuspendLayout();
        bottomSplit.SuspendLayout();
        ((System.ComponentModel.ISupportInitialize)dgvSectors).BeginInit();
        mainLayout.SuspendLayout();
        connPanel.SuspendLayout();
        ctrlPanel.SuspendLayout();
        cfgPanel.SuspendLayout();
        progPanel.SuspendLayout();
        SuspendLayout();

        // mainLayout
        mainLayout.Dock = DockStyle.Fill;
        mainLayout.ColumnCount = 1;
        mainLayout.RowCount = 5;
        mainLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        // connPanel
        connPanel.Dock = DockStyle.Fill;
        connPanel.FlowDirection = FlowDirection.LeftToRight;
        connPanel.WrapContents = false;
        cmbPort.Width = 100;
        cmbPort.DropDownStyle = ComboBoxStyle.DropDown;
        cmbPort.Items.AddRange(System.IO.Ports.SerialPort.GetPortNames());
        if (cmbPort.Items.Count > 0) cmbPort.SelectedIndex = 0;
        cmbBaud.Width = 80;
        cmbBaud.DropDownStyle = ComboBoxStyle.DropDown;
        cmbBaud.Items.AddRange(new object[] { "9600", "19200", "38400", "57600", "115200" });
        cmbBaud.SelectedIndex = 0;
        btnConnect.Text = "Connect";
        btnConnect.Width = 80;
        btnConnect.Click += btnConnect_Click;
        lblConnStatus.Text = "Disconnected";
        lblConnStatus.Width = 200;
        lblConnStatus.AutoSize = true;
        lblConnStatus.Margin = new Padding(10, 6, 0, 0);
        connPanel.Controls.AddRange(new Control[] { cmbPort, cmbBaud, btnConnect, lblConnStatus });

        // ctrlPanel
        ctrlPanel.Dock = DockStyle.Fill;
        ctrlPanel.FlowDirection = FlowDirection.LeftToRight;
        ctrlPanel.WrapContents = false;
        btnStart.Text = "Start";
        btnStart.Width = 70;
        btnStart.Enabled = false;
        btnStart.Click += btnStart_Click;
        btnStop.Text = "Stop";
        btnStop.Width = 70;
        btnStop.Enabled = false;
        btnStop.Click += btnStop_Click;
        txtSeekLba.Width = 100;
        txtSeekLba.Text = "0";
        btnSeek.Text = "Seek";
        btnSeek.Width = 60;
        btnSeek.Enabled = false;
        btnSeek.Click += btnSeek_Click;
        btnPing.Text = "Ping";
        btnPing.Width = 60;
        btnPing.Enabled = false;
        btnPing.Click += btnPing_Click;
        btnStatus.Text = "Status";
        btnStatus.Width = 60;
        btnStatus.Enabled = false;
        btnStatus.Click += btnStatus_Click;
        lblSeekLba.Text = "Seek LBA:";
        lblSeekLba.AutoSize = true;
        lblSeekLba.Margin = new Padding(10, 6, 0, 0);
        ctrlPanel.Controls.AddRange(new Control[] { btnStart, btnStop, lblSeekLba, txtSeekLba, btnSeek, btnPing, btnStatus });

        // cfgPanel
        cfgPanel.Dock = DockStyle.Fill;
        cfgPanel.FlowDirection = FlowDirection.LeftToRight;
        cfgPanel.WrapContents = false;
        cmbHeadMask.Width = 60;
        cmbHeadMask.DropDownStyle = ComboBoxStyle.DropDown;
        cmbHeadMask.Items.AddRange(new object[] { "FF", "01", "02", "04", "08", "10", "20", "40", "80" });
        cmbHeadMask.SelectedIndex = 0;
        btnApplyConfig.Text = "Apply Config";
        btnApplyConfig.Width = 100;
        btnApplyConfig.Enabled = false;
        btnApplyConfig.Click += btnApplyConfig_Click;
        lblHeadMask.Text = "Head mask:";
        lblHeadMask.AutoSize = true;
        lblHeadMask.Margin = new Padding(0, 6, 0, 0);
        cfgPanel.Controls.AddRange(new Control[] { lblHeadMask, cmbHeadMask, btnApplyConfig });

        // progPanel
        progPanel.Dock = DockStyle.Fill;
        progPanel.FlowDirection = FlowDirection.LeftToRight;
        progPanel.WrapContents = false;
        lblProgress.Text = "LBA: -/-";
        lblProgress.Width = 250;
        lblProgress.AutoSize = true;
        lblSectorsReceived.Text = "Received: 0";
        lblSectorsReceived.Width = 150;
        lblSectorsReceived.AutoSize = true;
        lblErrors.Text = "Errors: 0";
        lblErrors.Width = 120;
        lblErrors.AutoSize = true;
        lblStatus.Text = "";
        lblStatus.Width = 300;
        lblStatus.AutoSize = true;
        progPanel.Controls.AddRange(new Control[] { lblProgress, lblSectorsReceived, lblErrors, lblStatus });

        // bottomSplit
        bottomSplit.Dock = DockStyle.Fill;
        bottomSplit.Orientation = Orientation.Horizontal;
        bottomSplit.SplitterDistance = 200;
        bottomSplit.FixedPanel = FixedPanel.None;

        // txtLog
        txtLog.Dock = DockStyle.Fill;
        txtLog.Multiline = true;
        txtLog.ReadOnly = true;
        txtLog.ScrollBars = ScrollBars.Vertical;

        // dgvSectors
        dgvSectors.Dock = DockStyle.Fill;
        dgvSectors.ReadOnly = true;
        dgvSectors.AllowUserToAddRows = false;
        dgvSectors.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
        dgvSectors.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
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

        // MainForm
        AutoScaleDimensions = new SizeF(7F, 15F);
        AutoScaleMode = AutoScaleMode.Font;
        ClientSize = new Size(900, 700);
        MinimumSize = new Size(700, 500);
        Text = "HDD Saver 3.1";

        ((System.ComponentModel.ISupportInitialize)bottomSplit).EndInit();
        bottomSplit.Panel1.ResumeLayout(false);
        bottomSplit.Panel2.ResumeLayout(false);
        bottomSplit.ResumeLayout(false);
        ((System.ComponentModel.ISupportInitialize)dgvSectors).EndInit();
        mainLayout.ResumeLayout(false);
        connPanel.ResumeLayout(false);
        connPanel.PerformLayout();
        ctrlPanel.ResumeLayout(false);
        ctrlPanel.PerformLayout();
        cfgPanel.ResumeLayout(false);
        cfgPanel.PerformLayout();
        progPanel.ResumeLayout(false);
        progPanel.PerformLayout();
        ResumeLayout(false);
        PerformLayout();
    }

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
