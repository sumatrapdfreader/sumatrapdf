using System;
using System.Collections.Generic;
using System.Text;
using PlugInInterface;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace PrintPlugIn
{
    public class PlugIn : IPlugin
    {
        private string mName = "Acrobat reader print plug-in";
        private string mDescription = "This plugin prints a pdf document using acrobat reader";
        private string mAuthor = "Valery Possoz";
        private string mVersion = "1.0";
        private Image mPlugInImage = (System.Drawing.Image)Resources.printer.ToBitmap();
        private IPluginHost mHost = null;

        public string Name
        {
            get { return mName; }
        }
        public string Description
        {
            get { return mDescription; }
        }
        public string Author
        {
            get { return mAuthor; }
        }
        public string Version
        {
            get { return mVersion; }
        }
        public Image PlugInImage
        {
            get { return mPlugInImage; }
        }
        public IPluginHost Host
        {
            get { return mHost; }
            set { mHost = value; }
        }

        public void StartPlugIn(object sender, System.EventArgs e)
        {
            if (Host != null)
            {
                if (Host.PdfFileName != "" & Host.PdfFileName != null )
                {
                    dummyForm dummy = new dummyForm();
                    dummy.Show();
                    dummy.Print(Host.PdfFileName);
//                    dummy.Close();
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
            about.Name = Name;
            about.Description = Description;
            about.Author = Author;
            about.Version = Version;
            about.PluginIcon = PlugInImage;
            about.ShowDialog();
        }

    }
}
