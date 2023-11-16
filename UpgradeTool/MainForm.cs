using ModManagerCommon;
using ModManagerCommon.Forms;
using Newtonsoft.Json;
using System;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UpgradeTool
{
	public partial class MainForm : Form
	{
		public MainForm()
		{
			InitializeComponent();
		}

		const string updatePath = "mods/.updates";
		const string datadllorigpath = "system/CHRMODELS_orig.dll";
		const string datadllpath = "system/CHRMODELS.dll";

		public static async Task<GitHubAsset> GetLatestReleaseNewManager(HttpClient httpClient)
		{
			try
			{

				httpClient.DefaultRequestHeaders.Add("User-Agent", "SADXModLoader");

				HttpResponseMessage response = await httpClient.GetAsync("https://api.github.com/repos/X-Hax/SA-Mod-manager/releases/latest");

				if (response.IsSuccessStatusCode)
				{
					string responseBody = await response.Content.ReadAsStringAsync();
					var release = JsonConvert.DeserializeObject<GitHubRelease>(responseBody);
					if (release != null && release.Assets != null)
					{
						var targetAsset = release.Assets.FirstOrDefault(asset => asset.Name.Contains(Environment.Is64BitOperatingSystem ? "x64" : "x86"));

						if (targetAsset != null)
						{
							return targetAsset;
						}
					}
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine("Error fetching latest release: " + ex.Message);
			}

			return null;
		}

		private async void button1_Click(object sender, EventArgs e)
		{
			using (var wc = new HttpClient())
			{
				try
				{
					var release = await GetLatestReleaseNewManager(wc);

					if (release != null)
					{
						DialogResult result = DialogResult.OK;
						do
						{
							try
							{
								if (!Directory.Exists(updatePath))
								{
									Directory.CreateDirectory(updatePath);
								}
							}
							catch (Exception ex)
							{
								result = MessageBox.Show(this, "Failed to create temporary update directory:\n" + ex.Message
															   + "\n\nWould you like to retry?", "Directory Creation Failed", MessageBoxButtons.RetryCancel);
								if (result == DialogResult.Cancel)
									return;
							}
						} while (result == DialogResult.Retry);


						using (var dlg2 = new WPFDownloadDialog(release.DownloadUrl, updatePath))
							if (dlg2.ShowDialog(this) == DialogResult.OK)
							{
								if (File.Exists(datadllorigpath)) //remove the mod loader since we will use a new one.
								{
									File.Delete(datadllpath);
									File.Move(datadllorigpath, datadllpath);
								}
								Close();
							}
					}
				}
				catch
				{
					MessageBox.Show(this, "Unable to retrieve update information.", "SADX Mod Manager");
				}
			}
		}

		private void button2_Click(object sender, EventArgs e)
		{
			DialogResult result = DialogResult.OK;
			do
			{
				try
				{
					if (!Directory.Exists(updatePath))
					{
						Directory.CreateDirectory(updatePath);
					}
				}
				catch (Exception ex)
				{
					result = MessageBox.Show(this, "Failed to create temporary update directory:\n" + ex.Message
												   + "\n\nWould you like to retry?", "Directory Creation Failed", MessageBoxButtons.RetryCancel);
					if (result == DialogResult.Cancel) return;
				}
			} while (result == DialogResult.Retry);

			using (var dlg2 = new LoaderDownloadDialog("http://mm.reimuhakurei.net/sadxmods/SADXModLoaderLegacy.7z", updatePath))
				if (dlg2.ShowDialog(this) == DialogResult.OK)
				{
					Close();
				}

		}
	}
}
