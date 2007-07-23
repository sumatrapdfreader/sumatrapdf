using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace PlugInInterface
{
    public partial class frmAbout : Form
    {
        public string PluginName 
        {
            set { lblName.Text = "Name:" + value; }
        }
        public string PluginDescription
        {
            set { lblDescription.Text = "Description:" + value; }
        }
        public string PluginAuthor
        {
            set { lblAuthor.Text = "Author:" + value; }
        }
        public string PluginVersion
        {
            set { lblVersion.Text = "Version:" + value; }
        }
        public Image PluginIcon
        {
            set { pictureBox1.Image = value; }
        }
        public frmAbout()
        {
            InitializeComponent();
        }

        private void btnClose_Click(object sender, EventArgs e)
        {
            this.Close();
        }
    }
}