using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;

namespace SADXModManager
{
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
					 FilePath = relativePath, // TODO: relative path
					 FileSize = file.Length,
					 Checksum = string.Concat(hash.Select(x => x.ToString("x2")))
				 });
			}

			return result;
		}

		public override string ToString()
		{
			return $"{ FilePath }\t{ FileSize }\t{ Checksum }";
		}
	}

	public class ModManifestCompare : IEqualityComparer<ModManifest>
	{
		public bool Equals(ModManifest x, ModManifest y)
		{
			return x.FileSize == y.FileSize && x.FilePath == y.FilePath && x.Checksum == y.Checksum;
		}

		public int GetHashCode(ModManifest obj)
		{
			return 1;
		}
	}
}
