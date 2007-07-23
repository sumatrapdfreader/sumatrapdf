using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Text;
using System.Windows.Forms;
using System.IO;
using PlugInInterface;
using System.Reflection;

namespace PDFViewer
{
    public partial class UserControlPDFViewer : UserControl, IPluginHost
    {

        private string mPdfFileName;

        private PdfViewer thumbnails;
        private PdfViewer mainView;

        public string PdfFileName
        {
            get { return mPdfFileName; }
            set { mPdfFileName = value; }
        }

        public void LoadPlugIns()
        {
            // Declare the list of plugin classes we're going to read in from the libraries
            List<IPlugin> plugins = new List<IPlugin>();
            // Loop through all of the DLL files in the startup folder
            foreach (string fileName in Directory.GetFiles(Application.StartupPath, "*.dll"))
            {
                try
                {
                    Assembly assembly = Assembly.LoadFile(fileName);
                    // Loop through all of the types declared in the DLL
                    foreach (Type type in assembly.GetTypes())
                    {
                        // Ignore interfaces, we only want classes
                        if (type.IsClass)
                        {
                            // Is the type based on IPlugin?
                            if (typeof(IPlugin).IsAssignableFrom(type))
                            {
                                // Create a new instance of the type and add it to the list of plugins
                                plugins.Add((IPlugin)Activator.CreateInstance(type));
                            }
                        }
                    }
                }
                catch (BadImageFormatException)
                {
                    // File isn't an executable
                }
                catch (FileLoadException)
                {
                    // Can't load the DLL
                }
            }
            bool plugInPresent = false;
            // List all of the plugins
            foreach (IPlugin plugin in plugins)
            {
                ToolStripButton btnPlugIn = new ToolStripButton();
                btnPlugIn.Click += new System.EventHandler(plugin.StartPlugIn);
                btnPlugIn.Image = plugin.PlugInImage;
                btnPlugIn.ToolTipText = plugin.PluginDescription;
                plugin.PluginHost = this;
                toolStrip.Items.Add(btnPlugIn);
                plugInPresent = true;
            }
            if (plugInPresent)
            {
                ToolStripButton btnPlugIn = new ToolStripButton();
                btnPlugIn.Click += new System.EventHandler(ShowPlugInList);
                btnPlugIn.Image = (System.Drawing.Image)Resource.question.ToBitmap();
                toolStrip.Items.Add(btnPlugIn);
            }
        }

        private void ShowPlugInList(object sender, EventArgs e)
        {
            frmPlugInList pluginList = new frmPlugInList();
            pluginList.ShowDialog();
        }

        public void LoadFile(string fileName)
        {
            if (mainView == null)
            {
                mainView = new PdfViewer();
                mainView.Init(splitContainer.Panel1.Handle);
                mainView.Load(fileName);
            }
            else
            {
                mainView.Close();
                mainView.Load(fileName);
            }
            txtCurrent.Text = mainView.CurrentPage.ToString();
            txtCurrentZoom.Text = mainView.CurrentZoom.ToString();
            txtbOfPages.Text  = "of " + mainView.NumberOfPages.ToString();
            splitContainer.Panel2Collapsed = true;
            PdfFileName = fileName;
        }

        public void Exit()
        {
            if (mainView != null)
            {
                mainView.Exit();
                mainView = null;
            }
            if (thumbnails != null)
            {
                thumbnails.Exit();
                thumbnails = null;
            }
        }

        public UserControlPDFViewer()
        {
            InitializeComponent();
        }

        private void btnOpen_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofn = new OpenFileDialog();
            ofn.Filter = "Pdf Files|*.pdf";
            ofn.Title = "Type File";

            if (ofn.ShowDialog() != DialogResult.Cancel)
            {
                LoadFile(ofn.FileName);
            }
        }

        private void btnClose_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                if (mainView.FileName.Length != 0)
                    mainView.Close();
            }
            if (thumbnails != null)
            {
                if (thumbnails.FileName.Length != 0)
                    thumbnails.Close();
            }
            PdfFileName = "";
        }

        private void btnFirst_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.GoToFirst();
                txtCurrent.Text = mainView.CurrentPage.ToString();
            }
        }

        private void btnPrevious_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.GoToPrevious();
                txtCurrent.Text = mainView.CurrentPage.ToString();
            }
        }

        private void txtCurrent_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.GoToPage(Convert.ToInt32(txtCurrent.Text));
                txtCurrent.Text = mainView.CurrentPage.ToString();
            }

        }

        private void btnNext_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.GoToNext();
                txtCurrent.Text = mainView.CurrentPage.ToString();
            }
        }

        private void btnLast_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.GoToLast();
                txtCurrent.Text = mainView.CurrentPage.ToString();
            }
        }

        private void bntZoomOut_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.ZoomOut();
                txtCurrentZoom.Text = mainView.CurrentZoom.ToString();
            }
        }

        private void txtCurrentZoom_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.SetZoom(Convert.ToInt32(txtCurrentZoom.Text));
                txtCurrentZoom.Text = mainView.CurrentZoom.ToString();
            }
        }

        private void btnZoomIn_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.ZoomIn();
                txtCurrentZoom.Text = mainView.CurrentZoom.ToString();
            }
        }

        private void btnPrint_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.Print(); // TODO add direct print
            }
        }

        private void btnPrintDialog_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.Print();
            }
        }

        private void btnThumbnailsOnOff_Click(object sender, EventArgs e)
        {
            // Disabled
            //if (splitContainer.Panel2Collapsed)
            //{
            //    splitContainer.Panel2Collapsed = false;
            //    if (thumbnails == null)
            //    {
            //        thumbnails = new PdfViewer();
            //        thumbnails.Init(splitContainer.Panel2.Handle);
            //        thumbnails.Load(FileName);
            //        thumbnails.SetZoom(15);
            //    }
            //}
        }

        private void toolStripMenuItemSinglePage_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.SetDisplay(1); // TODO add equates
            }
        }

        private void toolStripMenuItemFacing_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.SetDisplay(2); // TODO add equates
            }
        }

        private void toolStripMenuItemContinuous_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.SetDisplay(3); // TODO add equates
            }
        }

        private void tooltripMenuItemContinuousFacing_Click(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.SetDisplay(4); // TODO add equates
            }
        }

        private void btnCustom1_Click(object sender, EventArgs e)
        {
            MessageBox.Show("Do something...");
        }

        private void btnCustom2_Click(object sender, EventArgs e)
        {
            MessageBox.Show("Do something...Else");
        }

        private void splitContainer_Panel1_Resize(object sender, EventArgs e)
        {
            if (mainView != null)
            {
                mainView.Resize();
            }
        }
    }
}
