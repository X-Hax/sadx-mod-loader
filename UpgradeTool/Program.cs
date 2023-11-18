using System;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace UpgradeTool
{
	static class Program
	{
		private const string pipeName = "sadx-mod-manager";
		private static readonly Mutex mutex = new Mutex(true, pipeName);

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main(string[] args)
		{
			try { mutex.WaitOne(); }
			catch (AbandonedMutexException) { }

			if (args.Length > 1 && args[0] == "doupdate")
				File.Delete(args[1] + ".7z");
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
			Application.Run(new MainForm());
		}
	}
}
