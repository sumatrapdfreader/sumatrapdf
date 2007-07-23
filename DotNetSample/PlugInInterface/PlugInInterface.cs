using System;
using System.Collections.Generic;
using System.Text;
using System.Drawing;

namespace PlugInInterface
{
    /// <summary>
    ///  PlugIn interface
    /// </summary>
    public interface IPlugin
	{
		string PluginName {get;}
		string PluginDescription {get;}
		string PluginAuthor {get;}
		string PluginVersion {get;}
        Image PlugInImage { get;}

        IPluginHost PluginHost { get;set;}

        void StartPlugIn(object sender, System.EventArgs e);
        void AboutPlugIn();
    }

    public interface IPluginHost
    {
        string PdfFileName { get;}
    }
}