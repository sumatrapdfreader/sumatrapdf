using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using NamedPipe;

namespace MemTraceCollector
{
    public partial class Form1 : Form
    {
        private Server pipeServer;
        private const string PIPE_NAME = "\\\\.\\pipe\\MemTraceCollectorPipe";
        private string textFromClients = "";
        public Form1()
        {
            InitializeComponent();
            FormClosed += new FormClosedEventHandler(Form1_FormClosed);
            tbFromClients.Text = "Nothing received from clients yet";

            pipeServer = new Server();
            pipeServer.MessageReceived +=
                new Server.MessageReceivedHandler(pipeServer_MessageReceived);
            pipeServer.PipeName = PIPE_NAME;
            pipeServer.Start();
        }

        void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            pipeServer.Stop();
        }

        void pipeServer_MessageReceived(Server.Client client, string message)
        {
            this.Invoke(new Server.MessageReceivedHandler(DisplayMessageReceived),
                new object[] { client, message });
        }

        void DisplayMessageReceived(Server.Client client, string message)
        {
            textFromClients += message + "\r\n";
            tbFromClients.Text = textFromClients;
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        private void btnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }
    }
}
