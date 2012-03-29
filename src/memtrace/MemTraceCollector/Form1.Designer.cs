namespace MemTraceCollector
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.btnExit = new System.Windows.Forms.Button();
            this.tbFromClients = new System.Windows.Forms.TextBox();
            this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
            this.labelConnectionStatus = new System.Windows.Forms.Label();
            this.labelCurrAllocated = new System.Windows.Forms.Label();
            this.labelMessagesInfo = new System.Windows.Forms.Label();
            this.flowLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // btnExit
            // 
            this.btnExit.AutoSize = true;
            this.btnExit.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.btnExit.Location = new System.Drawing.Point(0, 239);
            this.btnExit.Margin = new System.Windows.Forms.Padding(0);
            this.btnExit.Name = "btnExit";
            this.btnExit.Size = new System.Drawing.Size(284, 23);
            this.btnExit.TabIndex = 0;
            this.btnExit.Text = "Exit";
            this.btnExit.UseVisualStyleBackColor = true;
            this.btnExit.Click += new System.EventHandler(this.btnExit_Click);
            // 
            // tbFromClients
            // 
            this.tbFromClients.BackColor = System.Drawing.SystemColors.ActiveCaption;
            this.tbFromClients.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tbFromClients.Location = new System.Drawing.Point(0, 0);
            this.tbFromClients.Margin = new System.Windows.Forms.Padding(0);
            this.tbFromClients.Multiline = true;
            this.tbFromClients.Name = "tbFromClients";
            this.tbFromClients.ReadOnly = true;
            this.tbFromClients.Size = new System.Drawing.Size(284, 239);
            this.tbFromClients.TabIndex = 2;
            // 
            // flowLayoutPanel1
            // 
            this.flowLayoutPanel1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.flowLayoutPanel1.AutoSize = true;
            this.flowLayoutPanel1.Controls.Add(this.labelConnectionStatus);
            this.flowLayoutPanel1.Controls.Add(this.labelCurrAllocated);
            this.flowLayoutPanel1.Controls.Add(this.labelMessagesInfo);
            this.flowLayoutPanel1.FlowDirection = System.Windows.Forms.FlowDirection.TopDown;
            this.flowLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.flowLayoutPanel1.Name = "flowLayoutPanel1";
            this.flowLayoutPanel1.Size = new System.Drawing.Size(221, 39);
            this.flowLayoutPanel1.TabIndex = 3;
            // 
            // labelConnectionStatus
            // 
            this.labelConnectionStatus.AutoSize = true;
            this.labelConnectionStatus.Dock = System.Windows.Forms.DockStyle.Top;
            this.labelConnectionStatus.Location = new System.Drawing.Point(3, 0);
            this.labelConnectionStatus.Name = "labelConnectionStatus";
            this.labelConnectionStatus.Size = new System.Drawing.Size(174, 13);
            this.labelConnectionStatus.TabIndex = 2;
            this.labelConnectionStatus.Text = "Waiting for connection form client...";
            // 
            // labelCurrAllocated
            // 
            this.labelCurrAllocated.AutoSize = true;
            this.labelCurrAllocated.Location = new System.Drawing.Point(3, 13);
            this.labelCurrAllocated.Name = "labelCurrAllocated";
            this.labelCurrAllocated.Size = new System.Drawing.Size(97, 13);
            this.labelCurrAllocated.TabIndex = 3;
            this.labelCurrAllocated.Text = "Currently allocated:";
            // 
            // labelMessagesInfo
            // 
            this.labelMessagesInfo.AutoSize = true;
            this.labelMessagesInfo.Location = new System.Drawing.Point(3, 26);
            this.labelMessagesInfo.Name = "labelMessagesInfo";
            this.labelMessagesInfo.Size = new System.Drawing.Size(58, 13);
            this.labelMessagesInfo.TabIndex = 4;
            this.labelMessagesInfo.Text = "Messages:";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.GradientInactiveCaption;
            this.ClientSize = new System.Drawing.Size(284, 262);
            this.Controls.Add(this.flowLayoutPanel1);
            this.Controls.Add(this.tbFromClients);
            this.Controls.Add(this.btnExit);
            this.Name = "Form1";
            this.Text = "MemTraceCollector";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.flowLayoutPanel1.ResumeLayout(false);
            this.flowLayoutPanel1.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button btnExit;
        private System.Windows.Forms.TextBox tbFromClients;
        private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
        private System.Windows.Forms.Label labelConnectionStatus;
        private System.Windows.Forms.Label labelCurrAllocated;
        private System.Windows.Forms.Label labelMessagesInfo;

    }
}

