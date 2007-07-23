using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace PDFViewer
{
    public partial class MainPDFForm : Form
    {

        public MainPDFForm()
        {
            InitializeComponent();
            userControlPDFViewer.LoadPlugIns();
        }

        public void LoadDocument(string pfdFileName)
        {
            userControlPDFViewer.LoadFile(pfdFileName);
        }

    }
}