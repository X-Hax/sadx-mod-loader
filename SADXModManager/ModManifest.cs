using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace SADXModManager
{
	public class ModManifest
	{
		public string Path;
		public long Size;
		public string Checksum;

		public ModManifest(string line)
		{
			string[] fields = line.Split('\t');
			if (fields.Length != 3)
			{
				throw new ArgumentException($"Manifest line has too few fields. 3 are required. Provided: {fields.Length}", nameof(line));
			}

			Path     = fields[0];
			Size     = long.Parse(fields[1]);
			Checksum = fields[2];
		}

		public static List<ModManifest> FromFile(string filePath)
		{
			string[] lines = File.ReadAllLines(filePath);
			return lines.Select(line => new ModManifest(line)).ToList();
		}
	}

	public class ModManifestCompare : IEqualityComparer<ModManifest>
	{
		public bool Equals(ModManifest x, ModManifest y)
		{
			return x.Size == y.Size && x.Path == y.Path && x.Checksum == y.Checksum;
		}

		public int GetHashCode(ModManifest obj)
		{
			return 1;
		}
	}
}
