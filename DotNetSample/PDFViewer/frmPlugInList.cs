using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using PlugInInterface;
using System.Reflection;
using System.IO;

namespace PDFViewer
{
    public partial class frmPlugInList : Form
    {
        // Declare the list of plugin classes we're going to read in from the libraries
        List<IPlugin> plugins = new List<IPlugin>();

        public frmPlugInList()
        {
            InitializeComponent();
            LoadPlugIns();
        }

        private void LoadPlugIns()
        {
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
            // List all of the plugins
            foreach (IPlugin plugin in plugins)
            {
                listBox1.Items.Add(plugin.PluginName);
            }
        }



        private void btnClose_Click(object sender, EventArgs e)
        {
            this.Close();
        }


        private void btnAbout_Click(object sender, EventArgs e)
        {
            // List all of the plugins
            foreach (IPlugin plugin in plugins)
            {
                if (listBox1.SelectedItem.ToString() == plugin.PluginName)
                {
                    plugin.AboutPlugIn();
                }
            }
        }

        private void listBox1_DoubleClick(object sender, EventArgs e)
        {
            btnAbout_Click(sender, e);
        }
    }
}