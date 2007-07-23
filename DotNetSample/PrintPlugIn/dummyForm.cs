using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace PrintPlugIn
{
    public partial class dummyForm : Form
    {
        public dummyForm()
        {
            InitializeComponent();
 //           this.Hide();
        }
        public void Print(string pdfFileName)
        {
            //this.axPdf1.Visible = false;
            this.axPdf1.LoadFile(pdfFileName);
            //this.axPdf1.printAll();
            //this.Close();
        }
    }
}