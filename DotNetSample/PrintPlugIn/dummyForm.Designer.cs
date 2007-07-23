namespace PrintPlugIn
{
    partial class dummyForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(dummyForm));
            this.axPdf1 = new AxPdfLib.AxPdf();
            ((System.ComponentModel.ISupportInitialize)(this.axPdf1)).BeginInit();
            this.SuspendLayout();
            // 
            // axPdf1
            // 
            this.axPdf1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.axPdf1.Enabled = true;
            this.axPdf1.Location = new System.Drawing.Point(0, 0);
            this.axPdf1.Name = "axPdf1";
            this.axPdf1.OcxState = ((System.Windows.Forms.AxHost.State)(resources.GetObject("axPdf1.OcxState")));
            this.axPdf1.Size = new System.Drawing.Size(606, 687);
            this.axPdf1.TabIndex = 0;
            // 
            // dummyForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(606, 687);
            this.Controls.Add(this.axPdf1);
            this.Name = "dummyForm";
            this.Text = "dummyForm";
            ((System.ComponentModel.ISupportInitialize)(this.axPdf1)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private AxPdfLib.AxPdf axPdf1;
    }
}