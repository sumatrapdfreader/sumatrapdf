using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32.SafeHandles;
using NamedPipe;

namespace MemTraceCollector
{
    public partial class Form1 : Form
    {
        Server pipeServer;
        const string PIPE_NAME = "\\\\.\\pipe\\MemTraceCollectorPipe";
        const int BUFFER_SIZE = 4096;
        string textFromClients = "";
        int messagesSize = 0;
        int messagesCount = 0;

        public delegate void NewMessageHandler(byte[] msg);
        public delegate void ClientDisconnectedHandler(PipeClient client);

        public class PipeClient
        {
            public SafeFileHandle                   FileHandle;
            public FileStream                       Stream;
            public event NewMessageHandler          NewMessage;
            public event ClientDisconnectedHandler  ClientDisconnected;

            public void NotifyNewMessage(byte[] msg)
            {
                NewMessage(msg);
            }

            public void NotifyClientDisconnected()
            {
                ClientDisconnected(this);
            }
        }

        private List<PipeClient> clients;

        public Form1()
        {
            InitializeComponent();
            FormClosed += new FormClosedEventHandler(Form1_FormClosed);
            clients = new List<PipeClient>();
        }

        void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            pipeServer.Stop();
        }

        // called from the background thread
        void pipeServer_ClientConnected(SafeFileHandle fileHandle)
        {
            this.Invoke(
                new Server.ClientConnectedHandler(ClientConnected), 
                new object[] { fileHandle });
        }

        void ClientConnected(SafeFileHandle fileHandle)
        {
            PipeClient client = new PipeClient();
            client.FileHandle = fileHandle;
            client.Stream = new FileStream(fileHandle, FileAccess.ReadWrite, BUFFER_SIZE, true);
            client.NewMessage += pipeClient_NewMessage;
            client.ClientDisconnected += pipeClient_ClientDisconnected;
            clients.Add(client);
            label1.Text = String.Format("Connected {0} clients", clients.Count);
            Thread readThread = new Thread(new ParameterizedThreadStart(ClientReadThread));
            readThread.Start(client);
        }

        void pipeClient_ClientDisconnected(PipeClient client)
        {
            this.Invoke(
                new ClientDisconnectedHandler(ClientDisconnected),
                new object[] { client });
        }

        void ClientDisconnected(PipeClient client)
        {
            clients.Remove(client);
            label1.Text = String.Format("Connected {0} clients", clients.Count);
        }

        void pipeClient_NewMessage(byte[] msg)
        {
            this.Invoke(
                new NewMessageHandler(NewMessage),
                new object[] { msg });
        }

        void NewMessage(byte[] msg)
        {
            messagesCount += 1;
            messagesSize += msg.Length;
            textFromClients = String.Format("{0} bytes sent in {1} messages", messagesSize, messagesCount);
            tbFromClients.Text = textFromClients;
        }

        void ClientReadThread(object clientObj)
        {
            PipeClient client = (PipeClient)clientObj;
            FileStream stream = client.Stream;
            byte[] buf = new byte[BUFFER_SIZE];
            while (true)
            {
                int bytesRead = 0;
                int msgLen = 0;
                try
                {
                    bytesRead = stream.Read(buf, 0, 2);
                    if (2 != bytesRead)
                    {
                        bytesRead = 0;
                        break;
                    }
                    msgLen = buf[0] + buf[1] * 256;
                    if (msgLen > BUFFER_SIZE)
                    {
                        bytesRead = 0;
                        break;
                    }
                    bytesRead = stream.Read(buf, 0, msgLen);
                    if (bytesRead != msgLen)
                    {
                        bytesRead = 0;
                        break;
                    }
                }
                catch
                {
                    //read error has occurred
                    break;
                }

                //client has disconnected
                if (bytesRead == 0)
                    break;

                byte[] msg = new byte[msgLen];
                Array.Copy(buf, msg, msgLen);
                client.NotifyNewMessage(msg);
            }

            stream.Close();
            client.FileHandle.Close();
            client.NotifyClientDisconnected();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            tbFromClients.Text = "Nothing received from clients yet";

            pipeServer = new Server();
            pipeServer.ClientConnected += pipeServer_ClientConnected;
            pipeServer.PipeName = PIPE_NAME;
            pipeServer.Start();
        }

        private void btnExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }
    }
}
