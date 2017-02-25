using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SADXModManager.Forms
{
	class ManifestDialog : ProgressDialog
	{
		private string modPath;
		private string manifestPath;

		public List<ModManifest> manifest;
		public List<ModManifestDiff> diff;

		public ManifestDialog(string path, string title, bool allowCancel) : base(title, allowCancel)
		{
			modPath = path;
			manifestPath = Path.Combine(path, "mod.manifest");
			Shown += OnShown;
		}

		private void OnShown(object sender, EventArgs e)
		{
			var generator = new ModManifestGenerator();
			generator.FilesIndexed += (o, args) =>
			{
				SetTask("Manifest generation complete!");
				SetTaskSteps(new int[] { args.FileCount });
			};

			generator.FileHashStart += (o, args) =>
			{
				SetTask($"Hashing file: {args.FileIndex}/{args.FileCount}");
				SetStep(args.FileName);
			};

			generator.FileHashEnd += (o, args) =>
			{
				StepProgress();
			};

			using (var task = new Task(() =>
			{
				manifest = generator.Generate(modPath);
				diff = generator.Diff(manifest, File.Exists(manifestPath) ? ModManifest.FromFile(manifestPath) : null);
			}))
			{
				task.Start();

				while (!task.IsCompleted)
				{
					Application.DoEvents();
				}

				task.Wait();
			}
		}
	}
}
