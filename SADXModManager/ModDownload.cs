using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;

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

		public void Download(WebClient client, string updatePath)
		{
			switch (Type)
			{
				case ModDownloadType.Archive:
					var uri = new Uri(Url);
					var filePath = Path.Combine(updatePath, uri.Segments.Last());

					var info = new FileInfo(filePath);
					if (!info.Exists || info.Length != Size)
					{
						client.DownloadFile(Url, filePath);
					}

					var dataDir = Path.Combine(updatePath, Path.GetFileNameWithoutExtension(filePath));
					if (!Directory.Exists(dataDir))
					{
						Directory.CreateDirectory(dataDir);
					}

					// HACK: hard-coded garbage
					using (Process process = Process.Start(@"C:\Program Files\7-Zip\7z.exe", $"x -O\"{dataDir}\" -y \"{filePath}\""))
					{
						if (process == null)
						{
							throw new Exception("Failed to start 7z.exe");
						}

						process.WaitForExit();
					}

					string workDir = Path.GetDirectoryName(ModInfo.GetModFiles(new DirectoryInfo(dataDir)).FirstOrDefault());

					if (string.IsNullOrEmpty(workDir))
					{
						throw new DirectoryNotFoundException("Unable to locate mod.ini in " + dataDir);
					}

					var newManPath = Path.Combine(workDir, "mod.manifest");
					var oldManPath = Path.Combine(Folder, "mod.manifest");

					List<ModManifest> newManifest = ModManifest.FromFile(newManPath);

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
						var dir = Path.GetDirectoryName(file.Path);
						if (!string.IsNullOrEmpty(dir))
						{
							var newDir = Path.Combine(Folder, dir);
							if (!Directory.Exists(newDir))
							{
								Directory.CreateDirectory(newDir);
							}
						}

						var dest = Path.Combine(Folder, file.Path);

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
	}
}
