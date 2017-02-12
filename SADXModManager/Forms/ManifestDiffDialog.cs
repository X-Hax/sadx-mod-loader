﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;

namespace SADXModManager.Forms
{
	public partial class ManifestDiffDialog : Form
	{
		private readonly List<ModManifestDiff> diff;

		public ManifestDiffDialog(List<ModManifestDiff> diff)
		{
			this.diff = diff;
			InitializeComponent();
		}

		private void ManifestDiff_Load(object sender, EventArgs e)
		{
			listDiff.BeginUpdate();

			foreach (ModManifestDiff entry in diff.Where(x => x.State != ModManifestState.Unmodified))
			{
				listDiff.Items.Add(new ListViewItem(new[]
				{
					entry.Manifest.FilePath, entry.State.ToString()
				})
				{
					Tag = entry, Checked = entry.State != ModManifestState.Removed
				});
			}

			listDiff.EndUpdate();
		}

		public List<ModManifest> MakeNewManifest()
		{
			var result = new List<ModManifest>();

			result.AddRange(listDiff.Items.Cast<ListViewItem>().Where(x => x.Checked).Select(x => ((ModManifestDiff)x.Tag).Manifest));
			result.AddRange(diff.Where(x => x.State == ModManifestState.Unmodified).Select(x => x.Manifest));

			return result;
		}

		private void buttonOK_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.OK;
			Close();
		}
	}
}
