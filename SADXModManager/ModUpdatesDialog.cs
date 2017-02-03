using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;

namespace SADXModManager
{
	public partial class ModUpdatesDialog : Form
	{
		// TODO: changelogs somehow
		private readonly List<ModDownload> mods;

		public List<ModDownload> SelectedMods { get; } = new List<ModDownload>();

		public ModUpdatesDialog(List<ModDownload> mods)
		{
			this.mods = mods;
			InitializeComponent();
		}

		private void ModUpdatesDialog_Load(object sender, EventArgs e)
		{
			listModUpdates.BeginUpdate();
			foreach (ModDownload download in mods)
			{
				download.CheckFiles();

				listModUpdates.Items.Add(new ListViewItem(new[]
				{
					download.Info.Name, download.Info.Version, download.Version, download.Date, download.Size.ToString()
				})
				{
					Checked = true, Tag = download
				});
			}
			listModUpdates.EndUpdate();
		}

		private void buttonInstall_Click(object sender, EventArgs e)
		{
			foreach (ListViewItem item in listModUpdates.Items.Cast<ListViewItem>().Where(item => item.Checked))
			{
				SelectedMods.Add((ModDownload)item.Tag);
			}

			DialogResult = DialogResult.OK;
			Close();
		}

		private void listModUpdates_ItemChecked(object sender, ItemCheckedEventArgs e)
		{
			buttonInstall.Enabled = listModUpdates.Items.Cast<ListViewItem>().Any(x => x.Checked);
		}
	}
}
