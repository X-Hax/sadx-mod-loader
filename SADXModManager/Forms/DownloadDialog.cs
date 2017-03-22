using System;
using System.Collections.Generic;
using System.Linq;
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
			DialogResult = DialogResult.OK;

			using (var client = new UpdaterWebClient())
			{
				CancellationToken token = tokenSource.Token;

				void DownloadProgress(object o, DownloadProgressEventArgs args)
				{
					SetProgress((int)args.BytesReceived / 1024);
					SetStep($"Downloading: {args.BytesReceived} / {args.TotalBytesToReceive}");
					args.Cancel = token.IsCancellationRequested;
				}

				foreach (ModDownload update in updates)
				{
					DialogResult result;

					SetTaskAndStep($"Updating mod: { update.Info.Name }", "Starting download...");

					update.Extracting       += (o, args) => { SetStep("Extracting..."); };
					update.ParsingManifest  += (o, args) => { SetStep("Parsing manifest..."); };
					update.ApplyingManifest += (o, args) => { SetStep("Applying manifest..."); };
					update.DownloadProgress += DownloadProgress;

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
		}

		protected override void Dispose(bool disposing)
		{
			tokenSource.Dispose();
			base.Dispose(disposing);
		}
	}
}
