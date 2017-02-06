using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SADXModManager.Forms
{
	class DownloadDialog : ProgressDialog
	{
		private readonly List<ModDownload> updates;
		private readonly string updatePath;

		public DownloadDialog(List<ModDownload> updates, string updatePath)
			: base("Update Progress", updates.Select(x => (int)x.Size / 1024).ToArray())
		{
			this.updates    = updates;
			this.updatePath = updatePath;

			Shown += OnShown;
		}

		private void OnShown(object sender, EventArgs eventArgs)
		{
			using (var client = new UpdaterWebClient())
			{
				DownloadProgressChangedEventHandler progressChanged = (o, args) =>
				{
					SetProgress((int)args.BytesReceived / 1024);
					SetStep($"Downloading: {args.BytesReceived} / {args.TotalBytesToReceive}");
				};

				client.DownloadProgressChanged += progressChanged;

				foreach (ModDownload update in updates)
				{
					DialogResult result;

					SetTaskAndStep($"Updating mod: {update.Info.Name}", "Starting download...");

					update.Extracting       += (o, args) => { SetStep("Extracting..."); };
					update.ParsingManifest  += (o, args) => { SetStep("Parsing manifest..."); };
					update.ApplyingManifest += (o, args) => { SetStep("Applying manifest..."); };

					do
					{
						result = DialogResult.Cancel;

						try
						{
							// poor man's await Task.Run (not available in .net 4.0)
							using (var task = new Task(() => update.Download(client, updatePath)))
							{
								task.Start();

								while (!task.IsCompleted)
								{
									Application.DoEvents();
								}

								task.Wait();
							}
						}
						catch (AggregateException ae)
						{
							ae.Handle(ex =>
							{
								result = MessageBox.Show(this, $"Failed to update mod {update.Info.Name}:\r\n" + ex.Message
									+ "\r\n\r\nPress Retry to try again, or Cancel to skip this mod.",
									"Update Failed", MessageBoxButtons.RetryCancel, MessageBoxIcon.Error);
								return true;
							});
						}
					} while (result == DialogResult.Retry);

					NextTask();
				}
			}
		}
	}
}
