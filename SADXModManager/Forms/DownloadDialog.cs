using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SADXModManager.Forms
{
	class DownloadDialog : ProgressDialog
	{
		private readonly List<ModDownload> updates;
		private readonly string updatePath;
		private readonly CancellationTokenSource tokenSource = new CancellationTokenSource();

		public DownloadDialog(List<ModDownload> updates, string updatePath)
			: base("Update Progress", updates.Select(x => (int)x.Size / 1024).ToArray(), true)
		{
			this.updates    = updates;
			this.updatePath = updatePath;

			Shown += OnShown;
			CancelEvent += OnCancelEvent;
		}

		private void OnCancelEvent(object sender, EventArgs eventArgs)
		{
			tokenSource.Cancel();
		}

		private void OnShown(object sender, EventArgs eventArgs)
		{
			using (var client = new UpdaterWebClient())
			{
				CancellationToken token = tokenSource.Token;

				void ProgressChanged(object o, DownloadProgressChangedEventArgs args)
				{
					SetProgress((int)args.BytesReceived / 1024);
					SetStep($"Downloading: {args.BytesReceived} / {args.TotalBytesToReceive}");

					if (token.IsCancellationRequested)
					{
						client.CancelAsync();
					}
				}

				client.DownloadProgressChanged += ProgressChanged;

				foreach (ModDownload update in updates)
				{
					DialogResult result;

					SetTaskAndStep($"Updating mod: { update.Info.Name }", "Starting download...");

					update.Extracting       += (o, args) => { SetStep("Extracting..."); };
					update.ParsingManifest  += (o, args) => { SetStep("Parsing manifest..."); };
					update.ApplyingManifest += (o, args) => { SetStep("Applying manifest..."); };

					do
					{
						result = DialogResult.Cancel;

						try
						{
							// poor man's await Task.Run (not available in .net 4.0)
							using (var task = new Task(() => update.Download(client, updatePath), token))
							{
								task.Start();

								while (!task.IsCompleted && !task.IsCanceled)
								{
									Application.DoEvents();
								}

								task.Wait(token);
							}
						}
						catch (AggregateException ae)
						{
							ae.Handle(ex =>
							{
								result = MessageBox.Show(this, $"Failed to update mod { update.Info.Name }:\r\n{ ex.Message }"
									+ "\r\n\r\nPress Retry to try again, or Cancel to skip this mod.",
									"Update Failed", MessageBoxButtons.RetryCancel, MessageBoxIcon.Error);
								return true;
							});
						}
					} while (result == DialogResult.Retry);

					NextTask();
				}
			}

			DialogResult = DialogResult.OK;
		}

		protected override void Dispose(bool disposing)
		{
			tokenSource.Dispose();
			base.Dispose(disposing);
		}
	}
}
