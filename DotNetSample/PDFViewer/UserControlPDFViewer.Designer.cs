namespace PDFViewer
{
    partial class UserControlPDFViewer
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UserControlPDFViewer));
            this.toolStrip = new System.Windows.Forms.ToolStrip();
            this.btnOpen = new System.Windows.Forms.ToolStripButton();
            this.btnClose = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator5 = new System.Windows.Forms.ToolStripSeparator();
            this.btnFirst = new System.Windows.Forms.ToolStripButton();
            this.btnPrevious = new System.Windows.Forms.ToolStripButton();
            this.txtCurrent = new System.Windows.Forms.ToolStripTextBox();
            this.txtbOfPages = new System.Windows.Forms.ToolStripLabel();
            this.btnNext = new System.Windows.Forms.ToolStripButton();
            this.btnLast = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.bntZoomOut = new System.Windows.Forms.ToolStripButton();
            this.txtCurrentZoom = new System.Windows.Forms.ToolStripTextBox();
            this.btnZoomIn = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
            this.btnPrint = new System.Windows.Forms.ToolStripButton();
            this.btnPrintDialog = new System.Windows.Forms.ToolStripButton();
            this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
            this.btnThumbnailsOnOff = new System.Windows.Forms.ToolStripButton();
            this.dropDownButtonDisplayMode = new System.Windows.Forms.ToolStripDropDownButton();
            this.toolStripMenuItemSinglePage = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItemFacing = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItemContinuous = new System.Windows.Forms.ToolStripMenuItem();
            this.tooltripMenuItemContinuousFacing = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator4 = new System.Windows.Forms.ToolStripSeparator();
            this.btnCustom1 = new System.Windows.Forms.ToolStripButton();
            this.splitContainer = new System.Windows.Forms.SplitContainer();
            this.toolStrip.SuspendLayout();
            this.splitContainer.SuspendLayout();
            this.SuspendLayout();
            // 
            // toolStrip
            // 
            this.toolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.btnOpen,
            this.btnClose,
            this.toolStripSeparator5,
            this.btnFirst,
            this.btnPrevious,
            this.txtCurrent,
            this.txtbOfPages,
            this.btnNext,
            this.btnLast,
            this.toolStripSeparator1,
            this.bntZoomOut,
            this.txtCurrentZoom,
            this.btnZoomIn,
            this.toolStripSeparator2,
            this.btnPrint,
            this.btnPrintDialog,
            this.toolStripSeparator3,
            this.btnThumbnailsOnOff,
            this.dropDownButtonDisplayMode,
            this.toolStripSeparator4});
            this.toolStrip.Location = new System.Drawing.Point(0, 0);
            this.toolStrip.Name = "toolStrip";
            this.toolStrip.Size = new System.Drawing.Size(558, 25);
            this.toolStrip.TabIndex = 0;
            this.toolStrip.Text = "toolStrip1";
            // 
            // btnOpen
            // 
            this.btnOpen.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnOpen.Image = ((System.Drawing.Image)(resources.GetObject("btnOpen.Image")));
            this.btnOpen.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnOpen.Name = "btnOpen";
            this.btnOpen.Size = new System.Drawing.Size(23, 22);
            this.btnOpen.Text = "toolStripButton1";
            this.btnOpen.ToolTipText = "Open a PDF file";
            this.btnOpen.Click += new System.EventHandler(this.btnOpen_Click);
            // 
            // btnClose
            // 
            this.btnClose.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnClose.Image = ((System.Drawing.Image)(resources.GetObject("btnClose.Image")));
            this.btnClose.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnClose.Name = "btnClose";
            this.btnClose.Size = new System.Drawing.Size(23, 22);
            this.btnClose.Text = "toolStripButton1";
            this.btnClose.ToolTipText = "Close this file";
            this.btnClose.Click += new System.EventHandler(this.btnClose_Click);
            // 
            // toolStripSeparator5
            // 
            this.toolStripSeparator5.Name = "toolStripSeparator5";
            this.toolStripSeparator5.Size = new System.Drawing.Size(6, 25);
            // 
            // btnFirst
            // 
            this.btnFirst.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnFirst.Image = ((System.Drawing.Image)(resources.GetObject("btnFirst.Image")));
            this.btnFirst.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnFirst.Name = "btnFirst";
            this.btnFirst.Size = new System.Drawing.Size(23, 22);
            this.btnFirst.Text = "toolStripButton1";
            this.btnFirst.ToolTipText = "Go to first page";
            this.btnFirst.Click += new System.EventHandler(this.btnFirst_Click);
            // 
            // btnPrevious
            // 
            this.btnPrevious.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnPrevious.Image = ((System.Drawing.Image)(resources.GetObject("btnPrevious.Image")));
            this.btnPrevious.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnPrevious.Name = "btnPrevious";
            this.btnPrevious.Size = new System.Drawing.Size(23, 22);
            this.btnPrevious.Text = "toolStripButton2";
            this.btnPrevious.ToolTipText = "Go to previous page";
            this.btnPrevious.Click += new System.EventHandler(this.btnPrevious_Click);
            // 
            // txtCurrent
            // 
            this.txtCurrent.Name = "txtCurrent";
            this.txtCurrent.Size = new System.Drawing.Size(50, 25);
            this.txtCurrent.ToolTipText = "Enter page";
            this.txtCurrent.Click += new System.EventHandler(this.txtCurrent_Click);
            // 
            // txtbOfPages
            // 
            this.txtbOfPages.Name = "txtbOfPages";
            this.txtbOfPages.Size = new System.Drawing.Size(31, 22);
            this.txtbOfPages.Text = "of 10";
            // 
            // btnNext
            // 
            this.btnNext.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnNext.Image = ((System.Drawing.Image)(resources.GetObject("btnNext.Image")));
            this.btnNext.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnNext.Name = "btnNext";
            this.btnNext.Size = new System.Drawing.Size(23, 22);
            this.btnNext.Text = "toolStripButton3";
            this.btnNext.ToolTipText = "Go to next page";
            this.btnNext.Click += new System.EventHandler(this.btnNext_Click);
            // 
            // btnLast
            // 
            this.btnLast.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnLast.Image = ((System.Drawing.Image)(resources.GetObject("btnLast.Image")));
            this.btnLast.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnLast.Name = "btnLast";
            this.btnLast.Size = new System.Drawing.Size(23, 22);
            this.btnLast.Text = "toolStripButton4";
            this.btnLast.ToolTipText = "Go to last page";
            this.btnLast.Click += new System.EventHandler(this.btnLast_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(6, 25);
            // 
            // bntZoomOut
            // 
            this.bntZoomOut.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.bntZoomOut.Image = ((System.Drawing.Image)(resources.GetObject("bntZoomOut.Image")));
            this.bntZoomOut.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.bntZoomOut.Name = "bntZoomOut";
            this.bntZoomOut.Size = new System.Drawing.Size(23, 22);
            this.bntZoomOut.Text = "toolStripButton5";
            this.bntZoomOut.ToolTipText = "Zoom out";
            this.bntZoomOut.Click += new System.EventHandler(this.bntZoomOut_Click);
            // 
            // txtCurrentZoom
            // 
            this.txtCurrentZoom.Name = "txtCurrentZoom";
            this.txtCurrentZoom.Size = new System.Drawing.Size(50, 25);
            this.txtCurrentZoom.ToolTipText = "Enter zoom level";
            this.txtCurrentZoom.Click += new System.EventHandler(this.txtCurrentZoom_Click);
            // 
            // btnZoomIn
            // 
            this.btnZoomIn.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnZoomIn.Image = ((System.Drawing.Image)(resources.GetObject("btnZoomIn.Image")));
            this.btnZoomIn.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnZoomIn.Name = "btnZoomIn";
            this.btnZoomIn.Size = new System.Drawing.Size(23, 22);
            this.btnZoomIn.Text = "toolStripButton6";
            this.btnZoomIn.ToolTipText = "Zoom in";
            this.btnZoomIn.Click += new System.EventHandler(this.btnZoomIn_Click);
            // 
            // toolStripSeparator2
            // 
            this.toolStripSeparator2.Name = "toolStripSeparator2";
            this.toolStripSeparator2.Size = new System.Drawing.Size(6, 25);
            // 
            // btnPrint
            // 
            this.btnPrint.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnPrint.Image = ((System.Drawing.Image)(resources.GetObject("btnPrint.Image")));
            this.btnPrint.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnPrint.Name = "btnPrint";
            this.btnPrint.Size = new System.Drawing.Size(23, 22);
            this.btnPrint.Text = "toolStripButton1";
            this.btnPrint.ToolTipText = "Print";
            this.btnPrint.Click += new System.EventHandler(this.btnPrint_Click);
            // 
            // btnPrintDialog
            // 
            this.btnPrintDialog.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnPrintDialog.Image = ((System.Drawing.Image)(resources.GetObject("btnPrintDialog.Image")));
            this.btnPrintDialog.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnPrintDialog.Name = "btnPrintDialog";
            this.btnPrintDialog.Size = new System.Drawing.Size(23, 22);
            this.btnPrintDialog.Text = "toolStripButton1";
            this.btnPrintDialog.ToolTipText = "Print dialog";
            this.btnPrintDialog.Click += new System.EventHandler(this.btnPrintDialog_Click);
            // 
            // toolStripSeparator3
            // 
            this.toolStripSeparator3.Name = "toolStripSeparator3";
            this.toolStripSeparator3.Size = new System.Drawing.Size(6, 25);
            // 
            // btnThumbnailsOnOff
            // 
            this.btnThumbnailsOnOff.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnThumbnailsOnOff.Enabled = false;
            this.btnThumbnailsOnOff.Image = ((System.Drawing.Image)(resources.GetObject("btnThumbnailsOnOff.Image")));
            this.btnThumbnailsOnOff.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnThumbnailsOnOff.Name = "btnThumbnailsOnOff";
            this.btnThumbnailsOnOff.Size = new System.Drawing.Size(23, 22);
            this.btnThumbnailsOnOff.Text = "toolStripButton1";
            this.btnThumbnailsOnOff.ToolTipText = "Toggle thumbnails on/off";
            this.btnThumbnailsOnOff.Click += new System.EventHandler(this.btnThumbnailsOnOff_Click);
            // 
            // dropDownButtonDisplayMode
            // 
            this.dropDownButtonDisplayMode.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.dropDownButtonDisplayMode.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.toolStripMenuItemSinglePage,
            this.toolStripMenuItemFacing,
            this.toolStripMenuItemContinuous,
            this.tooltripMenuItemContinuousFacing});
            this.dropDownButtonDisplayMode.Image = ((System.Drawing.Image)(resources.GetObject("dropDownButtonDisplayMode.Image")));
            this.dropDownButtonDisplayMode.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.dropDownButtonDisplayMode.Name = "dropDownButtonDisplayMode";
            this.dropDownButtonDisplayMode.Size = new System.Drawing.Size(29, 22);
            this.dropDownButtonDisplayMode.ToolTipText = "Display mode";
            // 
            // toolStripMenuItemSinglePage
            // 
            this.toolStripMenuItemSinglePage.Name = "toolStripMenuItemSinglePage";
            this.toolStripMenuItemSinglePage.Size = new System.Drawing.Size(162, 22);
            this.toolStripMenuItemSinglePage.Text = "Single page";
            this.toolStripMenuItemSinglePage.Click += new System.EventHandler(this.toolStripMenuItemSinglePage_Click);
            // 
            // toolStripMenuItemFacing
            // 
            this.toolStripMenuItemFacing.Name = "toolStripMenuItemFacing";
            this.toolStripMenuItemFacing.Size = new System.Drawing.Size(162, 22);
            this.toolStripMenuItemFacing.Text = "Facing";
            this.toolStripMenuItemFacing.Click += new System.EventHandler(this.toolStripMenuItemFacing_Click);
            // 
            // toolStripMenuItemContinuous
            // 
            this.toolStripMenuItemContinuous.Name = "toolStripMenuItemContinuous";
            this.toolStripMenuItemContinuous.Size = new System.Drawing.Size(162, 22);
            this.toolStripMenuItemContinuous.Text = "Continuous";
            this.toolStripMenuItemContinuous.Click += new System.EventHandler(this.toolStripMenuItemContinuous_Click);
            // 
            // tooltripMenuItemContinuousFacing
            // 
            this.tooltripMenuItemContinuousFacing.Name = "tooltripMenuItemContinuousFacing";
            this.tooltripMenuItemContinuousFacing.Size = new System.Drawing.Size(162, 22);
            this.tooltripMenuItemContinuousFacing.Text = "Continuous facing";
            this.tooltripMenuItemContinuousFacing.Click += new System.EventHandler(this.tooltripMenuItemContinuousFacing_Click);
            // 
            // toolStripSeparator4
            // 
            this.toolStripSeparator4.Name = "toolStripSeparator4";
            this.toolStripSeparator4.Size = new System.Drawing.Size(6, 25);
            // 
            // btnCustom1
            // 
            this.btnCustom1.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
            this.btnCustom1.Image = ((System.Drawing.Image)(resources.GetObject("btnCustom1.Image")));
            this.btnCustom1.ImageTransparentColor = System.Drawing.Color.Magenta;
            this.btnCustom1.Name = "btnCustom1";
            this.btnCustom1.Size = new System.Drawing.Size(23, 22);
            this.btnCustom1.Text = "toolStripButton1";
            this.btnCustom1.ToolTipText = "Something 1";
            this.btnCustom1.Click += new System.EventHandler(this.btnCustom1_Click);
            // 
            // splitContainer
            // 
            this.splitContainer.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer.Location = new System.Drawing.Point(0, 25);
            this.splitContainer.Name = "splitContainer";
            // 
            // splitContainer.Panel1
            // 
            this.splitContainer.Panel1.Resize += new System.EventHandler(this.splitContainer_Panel1_Resize);
            this.splitContainer.Panel2Collapsed = true;
            this.splitContainer.Size = new System.Drawing.Size(558, 340);
            this.splitContainer.SplitterDistance = 186;
            this.splitContainer.TabIndex = 1;
            // 
            // UserControlPDFViewer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.splitContainer);
            this.Controls.Add(this.toolStrip);
            this.Name = "UserControlPDFViewer";
            this.Size = new System.Drawing.Size(558, 365);
            this.toolStrip.ResumeLayout(false);
            this.toolStrip.PerformLayout();
            this.splitContainer.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ToolStrip toolStrip;
        private System.Windows.Forms.ToolStripButton btnFirst;
        private System.Windows.Forms.ToolStripButton btnPrevious;
        private System.Windows.Forms.ToolStripButton btnNext;
        private System.Windows.Forms.ToolStripButton btnLast;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripButton bntZoomOut;
        private System.Windows.Forms.ToolStripTextBox txtCurrentZoom;
        private System.Windows.Forms.ToolStripButton btnZoomIn;
        private System.Windows.Forms.ToolStripTextBox txtCurrent;
        private System.Windows.Forms.ToolStripLabel txtbOfPages;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
        private System.Windows.Forms.ToolStripButton btnPrint;
        private System.Windows.Forms.ToolStripButton btnPrintDialog;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
        private System.Windows.Forms.ToolStripButton btnCustom1;
        private System.Windows.Forms.SplitContainer splitContainer;
        private System.Windows.Forms.ToolStripButton btnThumbnailsOnOff;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator4;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator5;
        private System.Windows.Forms.ToolStripButton btnOpen;
        private System.Windows.Forms.ToolStripButton btnClose;
        private System.Windows.Forms.ToolStripDropDownButton dropDownButtonDisplayMode;
        private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemSinglePage;
        private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemFacing;
        private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemContinuous;
        private System.Windows.Forms.ToolStripMenuItem tooltripMenuItemContinuousFacing;
    }
}
