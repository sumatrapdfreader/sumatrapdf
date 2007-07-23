namespace PDFViewer
{
    partial class MainPDFForm
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
            this.userControlPDFViewer = new PDFViewer.UserControlPDFViewer();
            this.SuspendLayout();
            // 
            // userControlPDFViewer
            // 
            this.userControlPDFViewer.Dock = System.Windows.Forms.DockStyle.Fill;
            this.userControlPDFViewer.PdfFileName = null;
            this.userControlPDFViewer.Location = new System.Drawing.Point(0, 0);
            this.userControlPDFViewer.Name = "userControlPDFViewer";
            this.userControlPDFViewer.Size = new System.Drawing.Size(603, 626);
            this.userControlPDFViewer.TabIndex = 0;
            // 
            // MainPDFForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(603, 626);
            this.Controls.Add(this.userControlPDFViewer);
            this.Name = "MainPDFForm";
            this.Text = "PDF Viewer";
            this.ResumeLayout(false);

        }

        #endregion

        private UserControlPDFViewer userControlPDFViewer;
    }
}

