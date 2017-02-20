using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;

namespace SADXModManager
{
	public enum ModManifestState
	{
		Unmodified,
		Moved,
		Changed,
		Added,
		Removed
	}

	public class ModManifestDiff
	{
		public ModManifestState State;
		public ModManifest Manifest;

		public ModManifestDiff(ModManifestState state, ModManifest manifest)
		{
			State = state;
			Manifest = manifest;
		}
	}

	public class ModManifest
	{
		public string FilePath;
		public long FileSize;
		public string Checksum;

		private ModManifest(string line)
		{
			string[] fields = line.Split('\t');
			if (fields.Length != 3)
			{
				throw new ArgumentException($"Manifest line must have 3 fields. Provided: { fields.Length }", nameof(line));
			}

			FilePath = fields[0];
			FileSize = long.Parse(fields[1]);
			Checksum = fields[2];
		}

		private ModManifest() {}

		public static List<ModManifest> FromFile(string filePath)
		{
			string[] lines = File.ReadAllLines(filePath);
			return lines.Select(line => new ModManifest(line)).ToList();
		}

		public static void ToFile(IEnumerable<ModManifest> manifest, string path)
		{
			File.WriteAllLines(path, manifest.Select(x => x.ToString()));
		}

		public static List<ModManifest> Generate(string modPath)
		{
			if (!Directory.Exists(modPath))
			{
				throw new DirectoryNotFoundException();
			}

			var result = new List<ModManifest>();

			foreach (var f in Directory.EnumerateFiles(modPath, "*", SearchOption.AllDirectories))
			{
				var name = Path.GetFileName(f);
				if (string.Compare(name, "mod.manifest", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					continue;
				}

				if (string.Compare(name, "mod.version", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					continue;
				}

				var relativePath = f.Substring(modPath.Length + 1);
				var file = new FileInfo(f);

				byte[] hash;

				using (var sha = new SHA256Cng())
				{
					using (FileStream stream = File.OpenRead(f))
					{
						hash = sha.ComputeHash(stream);
					}
				}

				 result.Add(new ModManifest
				 {
					 FilePath = relativePath,
					 FileSize = file.Length,
					 Checksum = string.Concat(hash.Select(x => x.ToString("x2")))
				 });
			}

			return result;
		}

		public static List<ModManifestDiff> Diff(List<ModManifest> newManifest, List<ModManifest> oldManifest)
		{
			var result = new List<ModManifestDiff>();
			var old = new List<ModManifest>(oldManifest);

			foreach (ModManifest entry in newManifest)
			{
				var exact = old.FirstOrDefault(x => Equals(x, entry));
				if (exact != null)
				{
					old.Remove(exact);
					result.Add(new ModManifestDiff(ModManifestState.Unmodified, entry));
					continue;
				}

				List<ModManifest> checksum = old.Where(x => x.Checksum == entry.Checksum).ToList();
				if (checksum.Count > 0)
				{
					var name = Path.GetFileName(entry.FilePath);

					foreach (var c in checksum)
					{
						old.Remove(c);
					}

					if (checksum.All(x => Path.GetFileName(x.FilePath) != name))
					{
						result.Add(new ModManifestDiff(ModManifestState.Moved, entry));
						continue;
					}
				}

				var nameMatch = old.FirstOrDefault(x => x.FilePath == entry.FilePath);
				if (nameMatch != null)
				{
					old.Remove(nameMatch);
					result.Add(new ModManifestDiff(ModManifestState.Changed, entry));
					continue;
				}

				result.Add(new ModManifestDiff(ModManifestState.Added, entry));
			}

			if (old.Count > 0)
			{
				result.AddRange(old.Select(x => new ModManifestDiff(ModManifestState.Removed, x)));
			}

			return result;
		}

		public override string ToString()
		{
			return $"{ FilePath }\t{ FileSize }\t{ Checksum }";
		}

		public override bool Equals(object obj)
		{
			if (ReferenceEquals(this, obj))
			{
				return true;
			}

			var m = obj as ModManifest;
			if (m == null)
			{
				return false;
			}

			return FileSize == m.FileSize && FilePath == m.FilePath && Checksum == m.Checksum;
		}

		public override int GetHashCode()
		{
			return 1;
		}
	}
}
