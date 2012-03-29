using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using System.Threading;
using System.IO;

namespace NamedPipe
{
    class Server
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern SafeFileHandle CreateNamedPipe(
           String pipeName, uint dwOpenMode, uint dwPipeMode,
           uint nMaxInstances, uint nOutBufferSize, uint nInBufferSize,
           uint nDefaultTimeOut, IntPtr lpSecurityAttributes);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern int ConnectNamedPipe(SafeFileHandle hNamedPipe, IntPtr lpOverlapped);

        public const uint DUPLEX = 0x00000003;
        public const uint FILE_FLAG_OVERLAPPED = 0x40000000;

        public delegate void ClientConnectedHandler(SafeFileHandle fileHandle);

        public event ClientConnectedHandler ClientConnected;
        public const int BUFFER_SIZE = 4096;

        string  pipeName;
        Thread  listenThread;
        bool    running = false;
        bool    requestedCancel = false;

        public string PipeName
        {
            get { return this.pipeName; }
            set { this.pipeName = value; }
        }

        public bool Running
        {
            get { return this.running; }
        }

        public Server()
        {
        }

        public void Start()
        {
            this.listenThread = new Thread(new ThreadStart(ListenForClientsWrapper));
            this.listenThread.Start();

            this.running = true;
        }

        public void Stop()
        {
            requestedCancel = true;
            // make ConnectNamedPipe() quit
            try
            {
                File.Delete(PipeName);
            }
            catch (IOException)
            {
            }
        }

        private void ListenForClientsWrapper()
        {
            try
            {
                ListenForClients();
            }
            catch (ThreadAbortException)
            {
            }
        }

        private void ListenForClients()
        {
            while (true)
            {
                SafeFileHandle clientHandle = CreateNamedPipe(
                     this.pipeName,
                     DUPLEX | FILE_FLAG_OVERLAPPED, 0, 255,
                     BUFFER_SIZE, BUFFER_SIZE, 0, IntPtr.Zero);

                if (clientHandle.IsInvalid)
                    return;

                int success = ConnectNamedPipe(clientHandle, IntPtr.Zero);
                if (requestedCancel)
                    return;

                // could not connect client
                if (success == 0)
                    return;

                ClientConnected(clientHandle);
            }
        }
    }
}
