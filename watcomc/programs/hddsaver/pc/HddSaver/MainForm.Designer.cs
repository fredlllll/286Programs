namespace HddSaver
{

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
            components = new System.ComponentModel.Container();
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
            btnSaveBin = new Button();
            lblRetries = new Label();
            txtRetries = new TextBox();
            lblProgress = new Label();
            lblSectorsReceived = new Label();
            lblErrors = new Label();
            lblStatus = new Label();
            txtLog = new TextBox();
            mainLayout = new TableLayoutPanel();
            connPanel = new FlowLayoutPanel();
            ctrlPanel = new FlowLayoutPanel();
            lblSeekLba = new Label();
            cfgPanel = new FlowLayoutPanel();
            lblHeads = new Label();
            progPanel = new FlowLayoutPanel();
            progressTimer = new System.Windows.Forms.Timer(components);
            mainLayout.SuspendLayout();
            connPanel.SuspendLayout();
            ctrlPanel.SuspendLayout();
            cfgPanel.SuspendLayout();
            progPanel.SuspendLayout();
            SuspendLayout();
            // 
            // cmbPort
            // 
            cmbPort.Location = new Point(3, 3);
            cmbPort.Name = "cmbPort";
            cmbPort.Size = new Size(100, 23);
            cmbPort.TabIndex = 0;
            // 
            // cmbBaud
            // 
            cmbBaud.Items.AddRange(new object[] { "9600", "19200", "38400", "57600", "115200" });
            cmbBaud.Location = new Point(109, 3);
            cmbBaud.Name = "cmbBaud";
            cmbBaud.Size = new Size(80, 23);
            cmbBaud.TabIndex = 1;
            // 
            // btnConnect
            // 
            btnConnect.Location = new Point(195, 3);
            btnConnect.Name = "btnConnect";
            btnConnect.Size = new Size(80, 23);
            btnConnect.TabIndex = 6;
            btnConnect.Text = "Connect";
            btnConnect.Click += BtnConnect_Click;
            // 
            // lblConnStatus
            // 
            lblConnStatus.AutoSize = true;
            lblConnStatus.Location = new Point(288, 6);
            lblConnStatus.Margin = new Padding(10, 6, 0, 0);
            lblConnStatus.Name = "lblConnStatus";
            lblConnStatus.Size = new Size(79, 15);
            lblConnStatus.TabIndex = 7;
            lblConnStatus.Text = "Disconnected";
            // 
            // btnStart
            // 
            btnStart.Enabled = false;
            btnStart.Location = new Point(3, 3);
            btnStart.Name = "btnStart";
            btnStart.Size = new Size(70, 23);
            btnStart.TabIndex = 0;
            btnStart.Text = "Start";
            btnStart.Click += BtnStart_Click;
            // 
            // btnStop
            // 
            btnStop.Enabled = false;
            btnStop.Location = new Point(79, 3);
            btnStop.Name = "btnStop";
            btnStop.Size = new Size(70, 23);
            btnStop.TabIndex = 1;
            btnStop.Text = "Stop";
            btnStop.Click += BtnStop_Click;
            // 
            // txtSeekLba
            // 
            txtSeekLba.Location = new Point(223, 3);
            txtSeekLba.Name = "txtSeekLba";
            txtSeekLba.Size = new Size(100, 23);
            txtSeekLba.TabIndex = 3;
            txtSeekLba.Text = "0";
            // 
            // btnSeek
            // 
            btnSeek.Enabled = false;
            btnSeek.Location = new Point(329, 3);
            btnSeek.Name = "btnSeek";
            btnSeek.Size = new Size(60, 23);
            btnSeek.TabIndex = 4;
            btnSeek.Text = "Seek";
            btnSeek.Click += BtnSeek_Click;
            // 
            // btnPing
            // 
            btnPing.Enabled = false;
            btnPing.Location = new Point(395, 3);
            btnPing.Name = "btnPing";
            btnPing.Size = new Size(60, 23);
            btnPing.TabIndex = 5;
            btnPing.Text = "Ping";
            btnPing.Click += BtnPing_Click;
            // 
            // btnStatus
            // 
            btnStatus.Enabled = false;
            btnStatus.Location = new Point(461, 3);
            btnStatus.Name = "btnStatus";
            btnStatus.Size = new Size(60, 23);
            btnStatus.TabIndex = 6;
            btnStatus.Text = "Status";
            btnStatus.Click += BtnStatus_Click;
            // 
            // chkHead0
            // 
            chkHead0.AutoSize = true;
            chkHead0.Checked = true;
            chkHead0.CheckState = CheckState.Checked;
            chkHead0.Location = new Point(49, 6);
            chkHead0.Margin = new Padding(6, 6, 0, 0);
            chkHead0.Name = "chkHead0";
            chkHead0.Size = new Size(32, 19);
            chkHead0.TabIndex = 1;
            chkHead0.Text = "0";
            // 
            // chkHead1
            // 
            chkHead1.AutoSize = true;
            chkHead1.Checked = true;
            chkHead1.CheckState = CheckState.Checked;
            chkHead1.Location = new Point(87, 6);
            chkHead1.Margin = new Padding(6, 6, 0, 0);
            chkHead1.Name = "chkHead1";
            chkHead1.Size = new Size(32, 19);
            chkHead1.TabIndex = 2;
            chkHead1.Text = "1";
            // 
            // chkHead2
            // 
            chkHead2.AutoSize = true;
            chkHead2.Checked = true;
            chkHead2.CheckState = CheckState.Checked;
            chkHead2.Location = new Point(125, 6);
            chkHead2.Margin = new Padding(6, 6, 0, 0);
            chkHead2.Name = "chkHead2";
            chkHead2.Size = new Size(32, 19);
            chkHead2.TabIndex = 3;
            chkHead2.Text = "2";
            // 
            // chkHead3
            // 
            chkHead3.AutoSize = true;
            chkHead3.Checked = true;
            chkHead3.CheckState = CheckState.Checked;
            chkHead3.Location = new Point(163, 6);
            chkHead3.Margin = new Padding(6, 6, 0, 0);
            chkHead3.Name = "chkHead3";
            chkHead3.Size = new Size(32, 19);
            chkHead3.TabIndex = 4;
            chkHead3.Text = "3";
            // 
            // chkHead4
            // 
            chkHead4.AutoSize = true;
            chkHead4.Checked = true;
            chkHead4.CheckState = CheckState.Checked;
            chkHead4.Location = new Point(201, 6);
            chkHead4.Margin = new Padding(6, 6, 0, 0);
            chkHead4.Name = "chkHead4";
            chkHead4.Size = new Size(32, 19);
            chkHead4.TabIndex = 5;
            chkHead4.Text = "4";
            // 
            // chkHead5
            // 
            chkHead5.AutoSize = true;
            chkHead5.Checked = true;
            chkHead5.CheckState = CheckState.Checked;
            chkHead5.Location = new Point(239, 6);
            chkHead5.Margin = new Padding(6, 6, 0, 0);
            chkHead5.Name = "chkHead5";
            chkHead5.Size = new Size(32, 19);
            chkHead5.TabIndex = 6;
            chkHead5.Text = "5";
            // 
            // btnApplyConfig
            // 
            btnApplyConfig.Enabled = false;
            btnApplyConfig.Location = new Point(375, 3);
            btnApplyConfig.Name = "btnApplyConfig";
            btnApplyConfig.Size = new Size(100, 23);
            btnApplyConfig.TabIndex = 9;
            btnApplyConfig.Text = "Apply Config";
            btnApplyConfig.Click += BtnApplyConfig_Click;
            // 
            // btnBadMap
            // 
            btnBadMap.Location = new Point(370, 3);
            btnBadMap.Name = "btnBadMap";
            btnBadMap.Size = new Size(116, 23);
            btnBadMap.TabIndex = 7;
            btnBadMap.Text = "Create Bad Map";
            btnBadMap.Click += BtnBadMap_Click;
            // 
            // btnSaveBin
            // 
            btnSaveBin.Location = new Point(492, 3);
            btnSaveBin.Name = "btnSaveBin";
            btnSaveBin.Size = new Size(116, 23);
            btnSaveBin.TabIndex = 8;
            btnSaveBin.Text = "Save Bin Image";
            btnSaveBin.Click += BtnSaveBin_Click;
            // 
            // lblRetries
            // 
            lblRetries.AutoSize = true;
            lblRetries.Location = new Point(281, 6);
            lblRetries.Margin = new Padding(10, 6, 0, 0);
            lblRetries.Name = "lblRetries";
            lblRetries.Size = new Size(45, 15);
            lblRetries.TabIndex = 7;
            lblRetries.Text = "Retries:";
            // 
            // txtRetries
            // 
            txtRetries.Location = new Point(329, 3);
            txtRetries.Name = "txtRetries";
            txtRetries.Size = new Size(40, 23);
            txtRetries.TabIndex = 8;
            txtRetries.Text = "5";
            // 
            // lblProgress
            // 
            lblProgress.AutoSize = true;
            lblProgress.Location = new Point(3, 0);
            lblProgress.Name = "lblProgress";
            lblProgress.Size = new Size(49, 15);
            lblProgress.TabIndex = 0;
            lblProgress.Text = "LBA: -/-";
            // 
            // lblSectorsReceived
            // 
            lblSectorsReceived.AutoSize = true;
            lblSectorsReceived.Location = new Point(58, 0);
            lblSectorsReceived.Name = "lblSectorsReceived";
            lblSectorsReceived.Size = new Size(66, 15);
            lblSectorsReceived.TabIndex = 1;
            lblSectorsReceived.Text = "Received: 0";
            // 
            // lblErrors
            // 
            lblErrors.AutoSize = true;
            lblErrors.Location = new Point(130, 0);
            lblErrors.Name = "lblErrors";
            lblErrors.Size = new Size(49, 15);
            lblErrors.TabIndex = 2;
            lblErrors.Text = "Errors: 0";
            // 
            // lblStatus
            // 
            lblStatus.AutoSize = true;
            lblStatus.Location = new Point(185, 0);
            lblStatus.Name = "lblStatus";
            lblStatus.Size = new Size(0, 15);
            lblStatus.TabIndex = 3;
            // 
            // txtLog
            // 
            txtLog.Dock = DockStyle.Fill;
            txtLog.Location = new Point(3, 163);
            txtLog.Multiline = true;
            txtLog.Name = "txtLog";
            txtLog.ReadOnly = true;
            txtLog.ScrollBars = ScrollBars.Vertical;
            txtLog.Size = new Size(894, 534);
            txtLog.TabIndex = 4;
            // 
            // mainLayout
            // 
            mainLayout.ColumnCount = 1;
            mainLayout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
            mainLayout.Controls.Add(connPanel, 0, 0);
            mainLayout.Controls.Add(ctrlPanel, 0, 1);
            mainLayout.Controls.Add(cfgPanel, 0, 2);
            mainLayout.Controls.Add(progPanel, 0, 3);
            mainLayout.Controls.Add(txtLog, 0, 4);
            mainLayout.Dock = DockStyle.Fill;
            mainLayout.Location = new Point(0, 0);
            mainLayout.Name = "mainLayout";
            mainLayout.RowCount = 5;
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40F));
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40F));
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40F));
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 40F));
            mainLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
            mainLayout.Size = new Size(900, 700);
            mainLayout.TabIndex = 0;
            // 
            // connPanel
            // 
            connPanel.Controls.Add(cmbPort);
            connPanel.Controls.Add(cmbBaud);
            connPanel.Controls.Add(btnConnect);
            connPanel.Controls.Add(lblConnStatus);
            connPanel.Controls.Add(btnBadMap);
            connPanel.Controls.Add(btnSaveBin);
            connPanel.Dock = DockStyle.Fill;
            connPanel.Location = new Point(3, 3);
            connPanel.Name = "connPanel";
            connPanel.Size = new Size(894, 34);
            connPanel.TabIndex = 0;
            connPanel.WrapContents = false;
            // 
            // ctrlPanel
            // 
            ctrlPanel.Controls.Add(btnStart);
            ctrlPanel.Controls.Add(btnStop);
            ctrlPanel.Controls.Add(lblSeekLba);
            ctrlPanel.Controls.Add(txtSeekLba);
            ctrlPanel.Controls.Add(btnSeek);
            ctrlPanel.Controls.Add(btnPing);
            ctrlPanel.Controls.Add(btnStatus);
            ctrlPanel.Dock = DockStyle.Fill;
            ctrlPanel.Location = new Point(3, 43);
            ctrlPanel.Name = "ctrlPanel";
            ctrlPanel.Size = new Size(894, 34);
            ctrlPanel.TabIndex = 1;
            ctrlPanel.WrapContents = false;
            // 
            // lblSeekLba
            // 
            lblSeekLba.AutoSize = true;
            lblSeekLba.Location = new Point(162, 6);
            lblSeekLba.Margin = new Padding(10, 6, 0, 0);
            lblSeekLba.Name = "lblSeekLba";
            lblSeekLba.Size = new Size(58, 15);
            lblSeekLba.TabIndex = 2;
            lblSeekLba.Text = "Seek LBA:";
            // 
            // cfgPanel
            // 
            cfgPanel.Controls.Add(lblHeads);
            cfgPanel.Controls.Add(chkHead0);
            cfgPanel.Controls.Add(chkHead1);
            cfgPanel.Controls.Add(chkHead2);
            cfgPanel.Controls.Add(chkHead3);
            cfgPanel.Controls.Add(chkHead4);
            cfgPanel.Controls.Add(chkHead5);
            cfgPanel.Controls.Add(lblRetries);
            cfgPanel.Controls.Add(txtRetries);
            cfgPanel.Controls.Add(btnApplyConfig);
            cfgPanel.Dock = DockStyle.Fill;
            cfgPanel.Location = new Point(3, 83);
            cfgPanel.Name = "cfgPanel";
            cfgPanel.Size = new Size(894, 34);
            cfgPanel.TabIndex = 2;
            cfgPanel.WrapContents = false;
            // 
            // lblHeads
            // 
            lblHeads.AutoSize = true;
            lblHeads.Location = new Point(0, 6);
            lblHeads.Margin = new Padding(0, 6, 0, 0);
            lblHeads.Name = "lblHeads";
            lblHeads.Size = new Size(43, 15);
            lblHeads.TabIndex = 0;
            lblHeads.Text = "Heads:";
            // 
            // progPanel
            // 
            progPanel.Controls.Add(lblProgress);
            progPanel.Controls.Add(lblSectorsReceived);
            progPanel.Controls.Add(lblErrors);
            progPanel.Controls.Add(lblStatus);
            progPanel.Dock = DockStyle.Fill;
            progPanel.Location = new Point(3, 123);
            progPanel.Name = "progPanel";
            progPanel.Size = new Size(894, 34);
            progPanel.TabIndex = 3;
            progPanel.WrapContents = false;
            // 
            // progressTimer
            // 
            progressTimer.Interval = 5000;
            // 
            // MainForm
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(900, 700);
            Controls.Add(mainLayout);
            MinimumSize = new Size(700, 500);
            Name = "MainForm";
            Text = "HDD Saver 3.1";
            mainLayout.ResumeLayout(false);
            mainLayout.PerformLayout();
            connPanel.ResumeLayout(false);
            connPanel.PerformLayout();
            ctrlPanel.ResumeLayout(false);
            ctrlPanel.PerformLayout();
            cfgPanel.ResumeLayout(false);
            cfgPanel.PerformLayout();
            progPanel.ResumeLayout(false);
            progPanel.PerformLayout();
            ResumeLayout(false);
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
        private Button btnSaveBin;
        private CheckBox chkHead0;
        private CheckBox chkHead1;
        private CheckBox chkHead2;
        private CheckBox chkHead3;
        private CheckBox chkHead4;
        private CheckBox chkHead5;
        private Label lblRetries;
        private TextBox txtRetries;
        private Button btnApplyConfig;
        private Label lblProgress;
        private Label lblSectorsReceived;
        private Label lblErrors;
        private Label lblStatus;
        private TextBox txtLog;
        private TableLayoutPanel mainLayout;
        private FlowLayoutPanel connPanel;
        private FlowLayoutPanel ctrlPanel;
        private Label lblSeekLba;
        private FlowLayoutPanel cfgPanel;
        private Label lblHeads;
        private FlowLayoutPanel progPanel;
        private System.Windows.Forms.Timer progressTimer;
    }
}