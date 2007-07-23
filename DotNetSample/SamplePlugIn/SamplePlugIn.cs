using System;
using System.Collections.Generic;
using System.Text;
using PlugInInterface;
using System.Drawing;
using System.Windows.Forms;

namespace SamplePlugIn
{
    public class SamplePlugIn : IPlugin
    {
        private string mName = "Sample plug-in";
        private string mDescription = "Sample plug-in description";
        private string mAuthor = "Valery Possoz";
        private string mVersion = "1.0";
        private Image mPlugInImage = (System.Drawing.Image)Resources.INFO.ToBitmap() ;
        private IPluginHost mHost = null;

        public string PluginName
        {
            get { return mName; }
        }
        public string PluginDescription
        {
            get { return mDescription; }
        }
        public string PluginAuthor
        {
            get { return mAuthor; }
        }
        public string PluginVersion
        {
            get { return mVersion; }
        }
        public Image PlugInImage
        {
            get { return mPlugInImage; }
        }
        public IPluginHost PluginHost
        {
            get { return mHost; }
            set { mHost = value; }
        }

        public void StartPlugIn(object sender, System.EventArgs e)
        {
            if (PluginHost != null)
            {
                if (PluginHost.PdfFileName != "" & PluginHost.PdfFileName != null )
                {
                    MessageBox.Show("Do something with the PDF: " + PluginHost.PdfFileName); 

                }
                else
                {
                    MessageBox.Show("No pdf...");
                }                
            }
            else
            {
                MessageBox.Show("No host...");
            }
        }
        public void AboutPlugIn()
        {
            frmAbout about = new frmAbout();
            about.PluginName = PluginName;
            about.PluginDescription = PluginDescription;
            about.PluginAuthor = PluginAuthor;
            about.PluginVersion = PluginVersion;
            about.PluginIcon = PlugInImage;
            about.ShowDialog();
        }

    }
}
