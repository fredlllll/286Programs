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
        chkHead0 = new CheckBox();
        chkHead1 = new CheckBox();
        chkHead2 = new CheckBox();
        chkHead3 = new CheckBox();
        chkHead4 = new CheckBox();
        chkHead5 = new CheckBox();
        btnApplyConfig = new Button();
        btnBadMap = new Button();
        lblProgress = new Label();
        lblSectorsReceived = new Label();
        lblErrors = new Label();
        lblStatus = new Label();
        txtLog = new TextBox();

        var mainLayout = new TableLayoutPanel();
        var connPanel = new FlowLayoutPanel();
        var ctrlPanel = new FlowLayoutPanel();
        var cfgPanel = new FlowLayoutPanel();
        var progPanel = new FlowLayoutPanel();
        var lblSeekLba = new Label();
        var lblHeads = new Label();

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
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
        mainLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        // connPanel
        connPanel.Dock = DockStyle.Fill;
        connPanel.FlowDirection = FlowDirection.LeftToRight;
        connPanel.WrapContents = false;
        cmbPort.Width = 100;
        cmbPort.DropDownStyle = ComboBoxStyle.DropDown;
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
        btnBadMap.Text = "Bad Map";
        btnBadMap.Width = 80;
        btnBadMap.Click += btnBadMap_Click;
        lblSeekLba.Text = "Seek LBA:";
        lblSeekLba.AutoSize = true;
        lblSeekLba.Margin = new Padding(10, 6, 0, 0);
        ctrlPanel.Controls.AddRange(new Control[] { btnStart, btnStop, lblSeekLba, txtSeekLba, btnSeek, btnPing, btnStatus, btnBadMap });

        // cfgPanel
        cfgPanel.Dock = DockStyle.Fill;
        cfgPanel.FlowDirection = FlowDirection.LeftToRight;
        cfgPanel.WrapContents = false;
        lblHeads.Text = "Heads:";
        lblHeads.AutoSize = true;
        lblHeads.Margin = new Padding(0, 6, 0, 0);
        chkHead0.Text = "0";
        chkHead0.AutoSize = true;
        chkHead0.Checked = true;
        chkHead0.Margin = new Padding(6, 6, 0, 0);
        chkHead1.Text = "1";
        chkHead1.AutoSize = true;
        chkHead1.Checked = true;
        chkHead1.Margin = new Padding(6, 6, 0, 0);
        chkHead2.Text = "2";
        chkHead2.AutoSize = true;
        chkHead2.Checked = true;
        chkHead2.Margin = new Padding(6, 6, 0, 0);
        chkHead3.Text = "3";
        chkHead3.AutoSize = true;
        chkHead3.Checked = true;
        chkHead3.Margin = new Padding(6, 6, 0, 0);
        chkHead4.Text = "4";
        chkHead4.AutoSize = true;
        chkHead4.Checked = true;
        chkHead4.Margin = new Padding(6, 6, 0, 0);
        chkHead5.Text = "5";
        chkHead5.AutoSize = true;
        chkHead5.Checked = true;
        chkHead5.Margin = new Padding(6, 6, 0, 0);
        btnApplyConfig.Text = "Apply Config";
        btnApplyConfig.Width = 100;
        btnApplyConfig.Enabled = false;
        btnApplyConfig.Click += btnApplyConfig_Click;
        cfgPanel.Controls.AddRange(new Control[] { lblHeads, chkHead0, chkHead1, chkHead2, chkHead3, chkHead4, chkHead5, btnApplyConfig });

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

        // txtLog
        txtLog.Dock = DockStyle.Fill;
        txtLog.Multiline = true;
        txtLog.ReadOnly = true;
        txtLog.ScrollBars = ScrollBars.Vertical;

        mainLayout.Controls.Add(connPanel, 0, 0);
        mainLayout.Controls.Add(ctrlPanel, 0, 1);
        mainLayout.Controls.Add(cfgPanel, 0, 2);
        mainLayout.Controls.Add(progPanel, 0, 3);
        mainLayout.Controls.Add(txtLog, 0, 4);

        Controls.Add(mainLayout);

        // MainForm
        AutoScaleDimensions = new SizeF(7F, 15F);
        AutoScaleMode = AutoScaleMode.Font;
        ClientSize = new Size(900, 700);
        MinimumSize = new Size(700, 500);
        Text = "HDD Saver 3.1";

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
    private Button btnBadMap;
    private CheckBox chkHead0;
    private CheckBox chkHead1;
    private CheckBox chkHead2;
    private CheckBox chkHead3;
    private CheckBox chkHead4;
    private CheckBox chkHead5;
    private Button btnApplyConfig;
    private Label lblProgress;
    private Label lblSectorsReceived;
    private Label lblErrors;
    private Label lblStatus;
    private TextBox txtLog;
}
