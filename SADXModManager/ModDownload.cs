using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading;
using SharpCompress.Archives.SevenZip;
using SharpCompress.Readers;

namespace SADXModManager
{
	// TODO: actually implement Modular support (download only changed files)
	public enum ModDownloadType
	{
		Archive,
		Modular
	}

	public class ModDownload
	{
		public ModInfo Info { get; private set; }
		public readonly ModDownloadType Type;
		public readonly string Url;
		public readonly string Folder;
		public readonly string Changes;
		public long Size { get; private set; }
		public int FilesToDownload { get; private set; }

		public string HomePage;
		public string Name;
		public string Version;
		public string Published;
		public string Updated;
		public string ReleaseUrl;

		public ModDownload(ModInfo info, ModDownloadType type, string url, string folder, string changes, long size)
		{
			Info    = info;
			Type    = type;
			Url     = url;
			Folder  = folder;
			Changes = changes;
			Size    = size;
		}

		public void CheckFiles()
		{
			switch (Type)
			{
				case ModDownloadType.Archive:
					FilesToDownload = 1;
					break;

				default:
					throw new ArgumentOutOfRangeException();
			}
		}

		public event EventHandler Extracting;
		public event EventHandler ParsingManifest;
		public event EventHandler ApplyingManifest;

		private static void Extract(IReader reader, string outDir)
		{
			var options = new ExtractionOptions
			{
				ExtractFullPath    = true,
				Overwrite          = true,
				PreserveAttributes = true,
				PreserveFileTime   = true
			};

			while (reader.MoveToNextEntry())
			{
				if (reader.Entry.IsDirectory)
				{
					continue;
				}

				reader.WriteEntryToDirectory(outDir, options);
			}
		}

		public void Download(WebClient client, string updatePath)
		{
			switch (Type)
			{
				case ModDownloadType.Archive:
					var uri = new Uri(Url);
					string filePath = Path.Combine(updatePath, uri.Segments.Last());

					var info = new FileInfo(filePath);
					if (!info.Exists || info.Length != Size)
					{
						AsyncCompletedEventHandler downloadComplete = (sender, args) =>
						{
							lock (args.UserState)
							{
								Monitor.Pulse(args.UserState);
							}
						};

						client.DownloadFileCompleted += downloadComplete;

						var sync = new object();
						lock (sync)
						{
							client.DownloadFileAsync(uri, filePath, sync);
							Monitor.Wait(sync);
						}

						client.DownloadFileCompleted -= downloadComplete;
					}

					string dataDir = Path.Combine(updatePath, Path.GetFileNameWithoutExtension(filePath));
					if (!Directory.Exists(dataDir))
					{
						Directory.CreateDirectory(dataDir);
					}

					OnExtracting();
					using (Stream fileStream = File.OpenRead(filePath))
					{
						if (SevenZipArchive.IsSevenZipFile(fileStream))
						{
							using (SevenZipArchive archive = SevenZipArchive.Open(fileStream))
							{
								Extract(archive.ExtractAllEntries(), dataDir);
							}
						}
						else
						{
							using (IReader reader = ReaderFactory.Open(fileStream))
							{
								Extract(reader, dataDir);
							}
						}
					}

					string workDir = Path.GetDirectoryName(ModInfo.GetModFiles(new DirectoryInfo(dataDir)).FirstOrDefault());

					if (string.IsNullOrEmpty(workDir))
					{
						throw new DirectoryNotFoundException("Unable to locate mod.ini in " + dataDir);
					}

					string newManPath = Path.Combine(workDir, "mod.manifest");
					string oldManPath = Path.Combine(Folder, "mod.manifest");

					OnParsingManifest();
					List<ModManifest> newManifest = ModManifest.FromFile(newManPath);

					OnApplyingManifest();

					if (File.Exists(oldManPath))
					{
						List<ModManifest> oldManifest = ModManifest.FromFile(oldManPath);
						IEnumerable<ModManifest> unique = oldManifest.Except(newManifest, new ModManifestCompare());

						foreach (string removed in unique.Select(x => Path.Combine(Folder, x.Path)))
						{
							if (File.Exists(removed))
							{
								File.Delete(removed);
							}
							else if (Directory.Exists(removed))
							{
								Directory.Delete(removed, true);
							}
						}
					}

					foreach (ModManifest file in newManifest)
					{
						string dir = Path.GetDirectoryName(file.Path);
						if (!string.IsNullOrEmpty(dir))
						{
							string newDir = Path.Combine(Folder, dir);
							if (!Directory.Exists(newDir))
							{
								Directory.CreateDirectory(newDir);
							}
						}

						string dest = Path.Combine(Folder, file.Path);

						if (File.Exists(dest))
						{
							File.Delete(dest);
						}

						File.Move(Path.Combine(workDir, file.Path), dest);
					}

					File.Copy(newManPath, oldManPath, true);
					Directory.Delete(dataDir, true);
					File.Delete(filePath);
					break;

				default:
					throw new ArgumentOutOfRangeException();
			}

			File.WriteAllText(Path.Combine(Folder, "mod.version"), Updated);
		}

		protected virtual void OnExtracting()
		{
			Extracting?.Invoke(this, EventArgs.Empty);
		}

		protected virtual void OnParsingManifest()
		{
			ParsingManifest?.Invoke(this, EventArgs.Empty);
		}

		protected virtual void OnApplyingManifest()
		{
			ApplyingManifest?.Invoke(this, EventArgs.Empty);
		}
	}
}
