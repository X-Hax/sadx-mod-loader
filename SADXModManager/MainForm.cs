﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using IniFile;
using Newtonsoft.Json;
using SADXModManager.Forms;

namespace SADXModManager
{
	public partial class MainForm : Form
	{
		public MainForm()
		{
			InitializeComponent();
		}

		private bool checkedForUpdates;

		const string datadllpath = "system/CHRMODELS.dll";
		const string datadllorigpath = "system/CHRMODELS_orig.dll";
		const string loaderinipath = "mods/SADXModLoader.ini";
		const string loaderdllpath = "mods/SADXModLoader.dll";
		LoaderInfo loaderini;
		Dictionary<string, ModInfo> mods;
		const string codexmlpath = "mods/Codes.xml";
		const string codedatpath = "mods/Codes.dat";
		const string patchdatpath = "mods/Patches.dat";
		CodeList mainCodes;
		List<Code> codes;
		bool installed;
		bool suppressEvent;

		BackgroundWorker updateChecker;

		private static bool Elapsed(UpdateUnit unit, int amount, DateTime start)
		{
			if (unit == UpdateUnit.Always)
			{
				return true;
			}

			TimeSpan span = DateTime.UtcNow - start;

			switch (unit)
			{
				case UpdateUnit.Hours:
					return span.TotalHours >= amount;

				case UpdateUnit.Days:
					return span.TotalDays >= amount;

				case UpdateUnit.Weeks:
					return span.TotalDays / 7.0 >= amount;

				default:
					throw new ArgumentOutOfRangeException(nameof(unit), unit, null);
			}
		}

		private void MainForm_Load(object sender, EventArgs e)
		{
			loaderini = File.Exists(loaderinipath) ? IniSerializer.Deserialize<LoaderInfo>(loaderinipath) : new LoaderInfo();

			if (CheckForUpdates())
				return;

			try { mainCodes = CodeList.Load(codexmlpath); }
			catch { mainCodes = new CodeList() { Codes = new List<Code>() }; }

			LoadModList();

			for (int i = 0; i < Screen.AllScreens.Length; i++)
			{
				Screen s = Screen.AllScreens[i];
				screenNumComboBox.Items.Add($"{ i + 1 } { s.DeviceName } ({ s.Bounds.Location.X },{ s.Bounds.Y })"
					+ $" { s.Bounds.Width }x{ s.Bounds.Height } { s.BitsPerPixel } bpp { (s.Primary ? "Primary" : "") }");
			}

			consoleCheckBox.Checked             = loaderini.DebugConsole;
			screenCheckBox.Checked              = loaderini.DebugScreen;
			fileCheckBox.Checked                = loaderini.DebugFile;
			disableCDCheckCheckBox.Checked      = loaderini.DisableCDCheck;
			useCustomResolutionCheckBox.Checked = verticalResolution.Enabled = forceAspectRatioCheckBox.Enabled = nativeResolutionButton.Enabled = loaderini.UseCustomResolution;
			checkVsync.Checked                  = loaderini.EnableVsync;
			horizontalResolution.Enabled        = loaderini.UseCustomResolution && !loaderini.ForceAspectRatio;
			horizontalResolution.Value          = Math.Max(horizontalResolution.Minimum, Math.Min(horizontalResolution.Maximum, loaderini.HorizontalResolution));
			verticalResolution.Value            = Math.Max(verticalResolution.Minimum, Math.Min(verticalResolution.Maximum, loaderini.VerticalResolution));
			checkUpdateStartup.Checked          = loaderini.UpdateCheck;
			checkUpdateModsStartup.Checked      = loaderini.ModUpdateCheck;
			comboUpdateFrequency.SelectedIndex  = (int)loaderini.UpdateUnit;
			numericUpdateFrequency.Value        = loaderini.UpdateFrequency;

			suppressEvent = true;
			forceAspectRatioCheckBox.Checked = loaderini.ForceAspectRatio;
			checkScaleHud.Checked = loaderini.ScaleHud;
			suppressEvent = false;

			windowedFullscreenCheckBox.Checked = loaderini.WindowedFullscreen;
			forceMipmappingCheckBox.Checked    = loaderini.AutoMipmap;
			forceTextureFilterCheckBox.Checked = loaderini.TextureFilter;
			pauseWhenInactiveCheckBox.Checked  = loaderini.PauseWhenInactive;
			stretchFullscreenCheckBox.Checked  = loaderini.StretchFullscreen;

			int screenNum = Math.Min(Screen.AllScreens.Length, loaderini.ScreenNum);

			screenNumComboBox.SelectedIndex  = screenNum;
			customWindowSizeCheckBox.Checked = windowHeight.Enabled = maintainWindowAspectRatioCheckBox.Enabled = loaderini.CustomWindowSize;
			windowWidth.Enabled              = loaderini.CustomWindowSize && !loaderini.MaintainWindowAspectRatio;
			System.Drawing.Rectangle rect    = Screen.PrimaryScreen.Bounds;

			foreach (Screen screen in Screen.AllScreens)
				rect = System.Drawing.Rectangle.Union(rect, screen.Bounds);

			windowWidth.Maximum  = rect.Width;
			windowWidth.Value    = Math.Max(windowWidth.Minimum, Math.Min(rect.Width, loaderini.WindowWidth));
			windowHeight.Maximum = rect.Height;
			windowHeight.Value   = Math.Max(windowHeight.Minimum, Math.Min(rect.Height, loaderini.WindowHeight));

			suppressEvent = true;
			maintainWindowAspectRatioCheckBox.Checked = loaderini.MaintainWindowAspectRatio;
			suppressEvent = false;

			CheckForModUpdates();

			// If we've checked for updates, save the modified
			// last update times without requiring the user to
			// click the save button.
			if (checkedForUpdates)
			{
				IniSerializer.Serialize(loaderini, loaderinipath);
			}

			if (!File.Exists(datadllpath))
			{
				MessageBox.Show(this, "CHRMODELS.dll could not be found.\n\nCannot determine state of installation.",
					Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
				installButton.Hide();
			}
			else if (File.Exists(datadllorigpath))
			{
				installed = true;
				installButton.Text = "Uninstall loader";
				using (MD5 md5 = MD5.Create())
				{
					byte[] hash1 = md5.ComputeHash(File.ReadAllBytes(loaderdllpath));
					byte[] hash2 = md5.ComputeHash(File.ReadAllBytes(datadllpath));

					if (hash1.SequenceEqual(hash2))
					{
						return;
					}
				}

				DialogResult result = MessageBox.Show(this, "Installed loader DLL differs from copy in mods folder."
					+ "\n\nDo you want to overwrite the installed copy?", Text, MessageBoxButtons.YesNo, MessageBoxIcon.Warning);

				if (result == DialogResult.Yes)
					File.Copy(loaderdllpath, datadllpath, true);
			}
		}

		private void LoadModList()
		{
			modListView.Items.Clear();
			mods = new Dictionary<string, ModInfo>();
			codes = new List<Code>(mainCodes.Codes);
			string modDir = Path.Combine(Environment.CurrentDirectory, "mods");

			foreach (string filename in ModInfo.GetModFiles(new DirectoryInfo(modDir)))
			{
				mods.Add(Path.GetDirectoryName(filename).Substring(modDir.Length + 1), IniSerializer.Deserialize<ModInfo>(filename));
			}

			modListView.BeginUpdate();

			foreach (string mod in new List<string>(loaderini.Mods))
			{
				if (mods.ContainsKey(mod))
				{
					ModInfo inf = mods[mod];
					suppressEvent = true;
					modListView.Items.Add(new ListViewItem(new[] { inf.Name, inf.Author, inf.Version }) { Checked = true, Tag = mod });
					suppressEvent = false;
					if (!string.IsNullOrEmpty(inf.Codes))
						codes.AddRange(CodeList.Load(Path.Combine(Path.Combine(modDir, mod), inf.Codes)).Codes);
				}
				else
				{
					MessageBox.Show(this, "Mod \"" + mod + "\" could not be found.\n\nThis mod will be removed from the list.",
						Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
					loaderini.Mods.Remove(mod);
				}
			}

			foreach (KeyValuePair<string, ModInfo> inf in mods)
			{
				if (!loaderini.Mods.Contains(inf.Key))
					modListView.Items.Add(new ListViewItem(new[] { inf.Value.Name, inf.Value.Author, inf.Value.Version }) { Tag = inf.Key });
			}

			modListView.EndUpdate();

			loaderini.EnabledCodes = new List<string>(loaderini.EnabledCodes.Where(a => codes.Any(c => c.Name == a)));
			foreach (Code item in codes.Where(a => a.Required && !loaderini.EnabledCodes.Contains(a.Name)))
				loaderini.EnabledCodes.Add(item.Name);

			codesCheckedListBox.BeginUpdate();
			codesCheckedListBox.Items.Clear();

			foreach (Code item in codes)
				codesCheckedListBox.Items.Add(item.Name, loaderini.EnabledCodes.Contains(item.Name));

			codesCheckedListBox.EndUpdate();
		}

		private bool CheckForUpdates(bool force = false)
		{
			if (!force && !Elapsed(loaderini.UpdateUnit, loaderini.UpdateFrequency, DateTime.FromFileTimeUtc(loaderini.UpdateTime)))
			{
				return false;
			}

			checkedForUpdates = true;
			loaderini.UpdateTime = DateTime.UtcNow.ToFileTimeUtc();

			if (!File.Exists("sadxmlver.txt"))
			{
				return false;
			}

			using (var wc = new WebClient())
			{
				try
				{
					string msg = wc.DownloadString("http://mm.reimuhakurei.net/toolchangelog.php?tool=sadxml&rev=" + File.ReadAllText("sadxmlver.txt"));

					if (msg.Length > 0)
					{
						using (var dlg = new UpdateMessageDialog(msg.Replace("\n", "\r\n")))
						{
							if (dlg.ShowDialog(this) == DialogResult.Yes)
							{
								Process.Start("http://mm.reimuhakurei.net/sadxmods/SADXModLoader.7z");
								Close();
								return true;
							}
						}
					}
				}
				catch
				{
					MessageBox.Show(this, "Unable to retrieve update information.", "SADX Mod Manager");
				}
			}

			return false;
		}

		private void CheckForModUpdates(bool force = false)
		{
			if (updateChecker == null)
			{
				updateChecker = new BackgroundWorker { WorkerSupportsCancellation = true };
				updateChecker.DoWork += UpdateChecker_DoWork;
				updateChecker.RunWorkerCompleted += UpdateChecker_RunWorkerCompleted;
			}

			if (!force && !Elapsed(loaderini.UpdateUnit, loaderini.UpdateFrequency, DateTime.FromFileTimeUtc(loaderini.ModUpdateTime)))
			{
				return;
			}

			checkedForUpdates = true;
			loaderini.ModUpdateTime = DateTime.UtcNow.ToFileTimeUtc();
			updateChecker.RunWorkerAsync(mods.ToList());
			buttonCheckForUpdates.Enabled = false;
		}

		private void UpdateChecker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
		{
			buttonCheckForUpdates.Enabled = true;

			if (e.Cancelled)
			{
				return;
			}

			var data = e.Result as Tuple<List<ModDownload>, List<string>>;
			if (data == null)
			{
				return;
			}

			List<string> errors = data.Item2;
			if (errors.Count != 0)
			{
				MessageBox.Show(this, "The following errors occurred while checking for updates:\n\n" + string.Join("\n", errors),
					"Update Errors", MessageBoxButtons.OK, MessageBoxIcon.Warning);
			}

			List<ModDownload> updates = data.Item1;
			if (updates.Count == 0)
			{
				return;
			}

			using (var dialog = new ModUpdatesDialog(updates))
			{
				if (dialog.ShowDialog(this) != DialogResult.OK)
				{
					return;
				}

				updates = dialog.SelectedMods;
			}

			if (updates.Count == 0)
			{
				return;
			}

			DialogResult result;
			string updatePath = Path.Combine("mods", ".updates");

			do
			{
				try
				{
					result = DialogResult.Cancel;
					if (!Directory.Exists(updatePath))
					{
						Directory.CreateDirectory(updatePath);
					}
				}
				catch (Exception ex)
				{
					result = MessageBox.Show(this, "Failed to create temporary update directory:\n" + ex.Message
						+ "\n\nWould you like to retry?", "Directory Creation Failed", MessageBoxButtons.RetryCancel);
				}
			} while (result == DialogResult.Retry);

			using (var progress = new DownloadDialog(updates, updatePath))
			{
				progress.ShowDialog(this);
			}

			do
			{
				try
				{
					result = DialogResult.Cancel;
					Directory.Delete(updatePath, true);
				}
				catch (Exception ex)
				{
					result = MessageBox.Show(this, "Failed to remove temporary update directory:\n" + ex.Message
						+ "\n\nWould you like to retry? You can remove the directory manually later.",
						"Directory Deletion Failed", MessageBoxButtons.RetryCancel);
				}
			} while (result == DialogResult.Retry);

			LoadModList();
		}

		private static void UpdateChecker_DoWork(object sender, DoWorkEventArgs e)
		{
			var worker = sender as BackgroundWorker;

			if (worker == null)
			{
				throw new Exception("what");
			}

			var updatableMods = e.Argument as List<KeyValuePair<string, ModInfo>>;
			if (updatableMods == null || updatableMods.Count == 0)
			{
				return;
			}

			var cache = new Dictionary<string, List<GitHubRelease>>();
			var updates = new List<ModDownload>();
			var errors = new List<string>();

			using (var client = new UpdaterWebClient())
			{
				foreach (KeyValuePair<string, ModInfo> info in updatableMods)
				{
					if (worker.CancellationPending)
					{
						e.Cancel = true;
						break;
					}

					ModInfo mod = info.Value;
					if (!string.IsNullOrEmpty(mod.GitHubRepo))
					{
						if (string.IsNullOrEmpty(mod.GitHubAsset))
						{
							errors.Add($"[{ mod.Name }] GitHubRepo specified, but GitHubAsset is missing.");
							continue;
						}

						ModDownload d = GetGitHubReleases(mod, info.Key, client, cache, errors);
						if (d != null)
						{
							updates.Add(d);
						}
					}
					else if (!string.IsNullOrEmpty(mod.UpdateUrl))
					{
						ModDownload d = CheckModularVersion(mod, info.Key, client, errors);
						if (d != null)
						{
							updates.Add(d);
						}
					}
				}
			}

			e.Result = new Tuple<List<ModDownload>, List<string>>(updates, errors);
		}

		private static ModDownload GetGitHubReleases(ModInfo mod, string folder, 
			UpdaterWebClient client, Dictionary<string, List<GitHubRelease>> cache, List<string> errors)
		{
			List<GitHubRelease> releases;
			var url = "https://api.github.com/repos/" + mod.GitHubRepo + "/releases";
			if (!cache.ContainsKey(url))
			{
				try
				{
					var text = client.DownloadString(url);
					releases = JsonConvert.DeserializeObject<List<GitHubRelease>>(text)
						.Where(x => !x.Draft && !x.PreRelease)
						.ToList();

					if (releases.Count > 0)
					{
						cache[url] = releases;
					}
				}
				catch (Exception ex)
				{
					errors.Add($"[{mod.Name}] Error checking for updates at {url}: {ex.Message}");
					return null;
				}
			}
			else
			{
				releases = cache[url];
			}

			// No releases available.
			if (releases == null || releases.Count == 0)
			{
				return null;
			}

			string versionPath = Path.Combine("mods", folder, "mod.version");
			DateTime? localVersion = null;

			if (File.Exists(versionPath))
			{
				localVersion = DateTime.Parse(File.ReadAllText(versionPath).Trim());
			}
			else
			{
				var info = new FileInfo(Path.Combine("mods", folder, "mod.manifest"));
				if (info.Exists)
				{
					localVersion = info.LastWriteTimeUtc;
				}
			}

			GitHubRelease latestRelease = null;
			GitHubAsset latestAsset = null;

			foreach (GitHubRelease release in releases)
			{
				GitHubAsset asset = release.Assets
					.FirstOrDefault(x => x.Name.Equals(mod.GitHubAsset, StringComparison.OrdinalIgnoreCase));

				if (asset == null)
				{
					continue;
				}

				latestRelease = release;

				// No updates available.
				if (localVersion.HasValue)
				{
					DateTime uploaded = DateTime.Parse(asset.Uploaded);
					if (localVersion >= uploaded)
					{
						break;
					}
				}

				latestAsset = asset;
				break;
			}

			if (latestRelease == null)
			{
				errors.Add($"[{mod.Name}] No releases with matching asset \"{mod.GitHubAsset}\" could be found in {releases.Count} release(s).");
				return null;
			}

			if (latestAsset == null)
			{
				return null;
			}

			var body = Regex.Replace(latestRelease.Body, "(?<!\r)\n", "\r\n");

			return new ModDownload(mod, Path.Combine("mods", folder), latestAsset.DownloadUrl, body, latestAsset.Size)
			{
				HomePage   = "https://github.com/" + mod.GitHubRepo,
				Name       = latestRelease.Name,
				Version    = latestRelease.TagName,
				Published  = latestRelease.Published,
				Updated    = latestAsset.Uploaded,
				ReleaseUrl = latestRelease.HtmlUrl
			};
		}

		private static ModDownload CheckModularVersion(ModInfo mod, string folder,
			UpdaterWebClient client, List<string> errors)
		{
			if (!mod.UpdateUrl.StartsWith("http://", StringComparison.InvariantCulture)
			    && !mod.UpdateUrl.StartsWith("https://", StringComparison.InvariantCulture))
			{
				mod.UpdateUrl = "http://" + mod.UpdateUrl;
			}

			if (!mod.UpdateUrl.EndsWith("/", StringComparison.InvariantCulture))
			{
				mod.UpdateUrl += "/";
			}

			var url = new Uri(mod.UpdateUrl);
			url = new Uri(url, "mod.ini");

			ModInfo remoteInfo;

			try
			{
				var dict = IniFile.IniFile.Load(client.OpenRead(url));
				remoteInfo = IniSerializer.Deserialize<ModInfo>(dict);
			}
			catch (Exception ex)
			{
				errors.Add($"[{mod.Name}] Error pulling mod.ini from \"{mod.UpdateUrl}\": {ex.Message}");
				return null;
			}

			if (remoteInfo.Version == mod.Version)
			{
				return null;
			}

			string manString;

			try
			{
				manString = client.DownloadString(new Uri(new Uri(mod.UpdateUrl), "mod.manifest"));
			}
			catch (Exception ex)
			{
				errors.Add($"[{mod.Name}] Error pulling mod.manifest from \"{mod.UpdateUrl}\": {ex.Message}");
				return null;
			}

			List<ModManifest> remoteManifest;

			try
			{
				remoteManifest = ModManifest.FromString(manString);
			}
			catch (Exception ex)
			{
				errors.Add($"[{mod.Name}] Error parsing remote manifest from \"{mod.UpdateUrl}\": {ex.Message}");
				return null;
			}

			var manPath = Path.Combine("mods", folder, "mod.manifest");
			List<ModManifest> localManifest = null;

			if (File.Exists(manPath))
			{
				try
				{
					localManifest = ModManifest.FromFile(manPath);
				}
				catch (Exception ex)
				{
					errors.Add($"[{mod.Name}] Error parsing local manifest: {ex.Message}");
					return null;
				}
			}

			List<ModManifestDiff> diff = ModManifestGenerator.Diff(remoteManifest, localManifest);

			if (diff.Count < 1 || diff.All(x => x.State == ModManifestState.Unchanged))
			{
				return null;
			}

			return new ModDownload(mod, Path.Combine("mods", folder), mod.UpdateUrl, diff);
		}

		private void modListView_SelectedIndexChanged(object sender, EventArgs e)
		{
			int count = modListView.SelectedIndices.Count;
			if (count == 0)
			{
				modUpButton.Enabled = modDownButton.Enabled = false;
				modDescription.Text = "Description: No mod selected.";
			}
			else if (count == 1)
			{
				modDescription.Text = "Description: " + mods[(string)modListView.SelectedItems[0].Tag].Description;
				modUpButton.Enabled = modListView.SelectedIndices[0] > 0;
				modDownButton.Enabled = modListView.SelectedIndices[0] < modListView.Items.Count - 1;
			}
			else if (count > 1)
			{
				modDescription.Text = "Description: Multiple mods selected.";
				modUpButton.Enabled = modDownButton.Enabled = true;
			}
		}

		private void modUpButton_Click(object sender, EventArgs e)
		{
			if (modListView.SelectedItems.Count < 1)
				return;

			modListView.BeginUpdate();

			for (int i = 0; i < modListView.SelectedItems.Count; i++)
			{
				int index = modListView.SelectedItems[i].Index;

				if (index-- > 0 && !modListView.Items[index].Selected)
				{
					ListViewItem item = modListView.SelectedItems[i];
					modListView.Items.Remove(item);
					modListView.Items.Insert(index, item);
				}
			}

			modListView.SelectedItems[0].EnsureVisible();
			modListView.EndUpdate();
		}

		private void modDownButton_Click(object sender, EventArgs e)
		{
			if (modListView.SelectedItems.Count < 1)
				return;

			modListView.BeginUpdate();

			for (int i = modListView.SelectedItems.Count - 1; i >= 0; i--)
			{
				int index = modListView.SelectedItems[i].Index + 1;

				if (index != modListView.Items.Count && !modListView.Items[index].Selected)
				{
					ListViewItem item = modListView.SelectedItems[i];
					modListView.Items.Remove(item);
					modListView.Items.Insert(index, item);
				}
			}

			modListView.SelectedItems[modListView.SelectedItems.Count - 1].EnsureVisible();
			modListView.EndUpdate();
		}

		private void Save()
		{
			loaderini.Mods.Clear();

			foreach (ListViewItem item in modListView.CheckedItems)
			{
				loaderini.Mods.Add((string)item.Tag);
			}

			loaderini.DebugConsole              = consoleCheckBox.Checked;
			loaderini.DebugScreen               = screenCheckBox.Checked;
			loaderini.DebugFile                 = fileCheckBox.Checked;
			loaderini.DisableCDCheck            = disableCDCheckCheckBox.Checked;
			loaderini.UseCustomResolution       = useCustomResolutionCheckBox.Checked;
			loaderini.HorizontalResolution      = (int)horizontalResolution.Value;
			loaderini.VerticalResolution        = (int)verticalResolution.Value;
			loaderini.ForceAspectRatio          = forceAspectRatioCheckBox.Checked;
			loaderini.ScaleHud                  = checkScaleHud.Checked;
			loaderini.EnableVsync               = checkVsync.Checked;
			loaderini.WindowedFullscreen        = windowedFullscreenCheckBox.Checked;
			loaderini.AutoMipmap                = forceMipmappingCheckBox.Checked;
			loaderini.TextureFilter             = forceTextureFilterCheckBox.Checked;
			loaderini.PauseWhenInactive         = pauseWhenInactiveCheckBox.Checked;
			loaderini.StretchFullscreen         = stretchFullscreenCheckBox.Checked;
			loaderini.ScreenNum                 = screenNumComboBox.SelectedIndex;
			loaderini.CustomWindowSize          = customWindowSizeCheckBox.Checked;
			loaderini.WindowWidth               = (int)windowWidth.Value;
			loaderini.WindowHeight              = (int)windowHeight.Value;
			loaderini.MaintainWindowAspectRatio = maintainWindowAspectRatioCheckBox.Checked;
			loaderini.UpdateCheck               = checkUpdateStartup.Checked;
			loaderini.ModUpdateCheck            = checkUpdateModsStartup.Checked;
			loaderini.UpdateUnit                = (UpdateUnit)comboUpdateFrequency.SelectedIndex;
			loaderini.UpdateFrequency           = (int)numericUpdateFrequency.Value;

			IniSerializer.Serialize(loaderini, loaderinipath);

			List<Code> selectedCodes = new List<Code>();
			List<Code> selectedPatches = new List<Code>();

			foreach (Code item in codesCheckedListBox.CheckedIndices.OfType<int>().Select(a => codes[a]))
			{
				if (item.Patch)
					selectedPatches.Add(item);
				else
					selectedCodes.Add(item);
			}

			using (FileStream fs = File.Create(patchdatpath))
			using (BinaryWriter bw = new BinaryWriter(fs, System.Text.Encoding.ASCII))
			{
				bw.Write(new[] { 'c', 'o', 'd', 'e', 'v', '5' });
				bw.Write(selectedPatches.Count);
				foreach (Code item in selectedPatches)
				{
					if (item.IsReg)
						bw.Write((byte)CodeType.newregs);
					WriteCodes(item.Lines, bw);
				}
				bw.Write(byte.MaxValue);
			}
			using (FileStream fs = File.Create(codedatpath))
			using (BinaryWriter bw = new BinaryWriter(fs, System.Text.Encoding.ASCII))
			{
				bw.Write(new[] { 'c', 'o', 'd', 'e', 'v', '5' });
				bw.Write(selectedCodes.Count);
				foreach (Code item in selectedCodes)
				{
					if (item.IsReg)
						bw.Write((byte)CodeType.newregs);
					WriteCodes(item.Lines, bw);
				}
				bw.Write(byte.MaxValue);
			}
		}

		private void WriteCodes(IEnumerable<CodeLine> codeList, BinaryWriter writer)
		{
			foreach (CodeLine line in codeList)
			{
				writer.Write((byte)line.Type);
				uint address;
				if (line.Address.StartsWith("r"))
					address = uint.Parse(line.Address.Substring(1), System.Globalization.NumberStyles.None, System.Globalization.NumberFormatInfo.InvariantInfo);
				else
					address = uint.Parse(line.Address, System.Globalization.NumberStyles.HexNumber);
				if (line.Pointer)
					address |= 0x80000000u;
				writer.Write(address);
				if (line.Pointer)
					if (line.Offsets != null)
					{
						writer.Write((byte)line.Offsets.Count);
						foreach (int off in line.Offsets)
							writer.Write(off);
					}
					else
						writer.Write((byte)0);
				if (line.Type == CodeType.ifkbkey)
					writer.Write((int)(Keys)Enum.Parse(typeof(Keys), line.Value));
				else
					switch (line.ValueType)
					{
						case ValueType.@decimal:
							switch (line.Type)
							{
								case CodeType.writefloat:
								case CodeType.addfloat:
								case CodeType.subfloat:
								case CodeType.mulfloat:
								case CodeType.divfloat:
								case CodeType.ifeqfloat:
								case CodeType.ifnefloat:
								case CodeType.ifltfloat:
								case CodeType.iflteqfloat:
								case CodeType.ifgtfloat:
								case CodeType.ifgteqfloat:
									writer.Write(float.Parse(line.Value, System.Globalization.NumberStyles.Float, System.Globalization.NumberFormatInfo.InvariantInfo));
									break;
								default:
									writer.Write(unchecked((int)long.Parse(line.Value, System.Globalization.NumberStyles.Integer, System.Globalization.NumberFormatInfo.InvariantInfo)));
									break;
							}
							break;
						case ValueType.hex:
							writer.Write(uint.Parse(line.Value, System.Globalization.NumberStyles.HexNumber, System.Globalization.NumberFormatInfo.InvariantInfo));
							break;
					}
				writer.Write(line.RepeatCount ?? 1);
				if (line.IsIf)
				{
					WriteCodes(line.TrueLines, writer);
					if (line.FalseLines.Count > 0)
					{
						writer.Write((byte)CodeType.@else);
						WriteCodes(line.FalseLines, writer);
					}
					writer.Write((byte)CodeType.endif);
				}
			}
		}

		private void saveAndPlayButton_Click(object sender, EventArgs e)
		{
			if (updateChecker?.IsBusy == true)
			{
				var result = MessageBox.Show(this, "Mods are still being checked for updates. Continue anyway?",
					"Busy", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);

				if (result == DialogResult.No)
				{
					return;
				}

				Enabled = false;

				updateChecker.CancelAsync();
				while (updateChecker.IsBusy)
				{
					Application.DoEvents();
				}

				Enabled = true;
			}

			Save();
			Process process = Process.Start(loaderini.Mods.Select((item) => mods[item].EXEFile)
				                                .FirstOrDefault((item) => !string.IsNullOrEmpty(item)) ?? "sonic.exe");
			process?.WaitForInputIdle(10000);
			Close();
		}

		private void saveButton_Click(object sender, EventArgs e)
		{
			Save();
			LoadModList();
		}

		private void installButton_Click(object sender, EventArgs e)
		{
			if (installed)
			{
				File.Delete(datadllpath);
				File.Move(datadllorigpath, datadllpath);
				installButton.Text = "Install loader";
			}
			else
			{
				File.Move(datadllpath, datadllorigpath);
				File.Copy(loaderdllpath, datadllpath);
				installButton.Text = "Uninstall loader";
			}
			installed = !installed;
		}

		private void useCustomResolutionCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			if (nativeResolutionButton.Enabled = forceAspectRatioCheckBox.Enabled =
				verticalResolution.Enabled = useCustomResolutionCheckBox.Checked)
				horizontalResolution.Enabled = !forceAspectRatioCheckBox.Checked;
			else
				horizontalResolution.Enabled = false;
		}

		const decimal ratio = 4 / 3m;
		private void forceAspectRatioCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			if (forceAspectRatioCheckBox.Checked)
			{
				horizontalResolution.Enabled = false;
				horizontalResolution.Value = Math.Round(verticalResolution.Value * ratio);
			}
			else if (!suppressEvent)
				horizontalResolution.Enabled = true;
		}

		private void verticalResolution_ValueChanged(object sender, EventArgs e)
		{
			if (forceAspectRatioCheckBox.Checked)
				horizontalResolution.Value = Math.Round(verticalResolution.Value * ratio);
		}

		private void nativeResolutionButton_Click(object sender, EventArgs e)
		{
			System.Drawing.Rectangle rect = Screen.PrimaryScreen.Bounds;
			if (screenNumComboBox.SelectedIndex > 0)
				rect = Screen.AllScreens[screenNumComboBox.SelectedIndex - 1].Bounds;
			else
				foreach (Screen screen in Screen.AllScreens)
					rect = System.Drawing.Rectangle.Union(rect, screen.Bounds);
			verticalResolution.Value = rect.Height;
			if (!forceAspectRatioCheckBox.Checked)
				horizontalResolution.Value = rect.Width;
		}

		private void configEditorButton_Click(object sender, EventArgs e)
		{
			using (ConfigEditDialog dlg = new ConfigEditDialog())
				dlg.ShowDialog(this);
		}

		private void buttonRefreshModList_Click(object sender, EventArgs e)
		{
			LoadModList();
		}

		private void buttonNewMod_Click(object sender, EventArgs e)
		{
			using (var ModDialog = new NewModDialog())
			{
				if (ModDialog.ShowDialog() == DialogResult.OK)
					LoadModList();
			}
		}

		private void customWindowSizeCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			if (maintainWindowAspectRatioCheckBox.Enabled = windowHeight.Enabled = customWindowSizeCheckBox.Checked)
				windowWidth.Enabled = !maintainWindowAspectRatioCheckBox.Checked;
			else
				windowWidth.Enabled = false;

		}

		private void maintainWindowAspectRatioCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			if (maintainWindowAspectRatioCheckBox.Checked)
			{
				windowWidth.Enabled = false;
				windowWidth.Value = Math.Round(windowHeight.Value * (horizontalResolution.Value / verticalResolution.Value));
			}
			else if (!suppressEvent)
				windowWidth.Enabled = true;
		}

		private void windowHeight_ValueChanged(object sender, EventArgs e)
		{
			if (maintainWindowAspectRatioCheckBox.Checked)
				windowWidth.Value = Math.Round(windowHeight.Value * (horizontalResolution.Value / verticalResolution.Value));
		}

		private void codesCheckedListBox_ItemCheck(object sender, ItemCheckEventArgs e)
		{
			Code code = codes[e.Index];
			if (code.Required)
				e.NewValue = CheckState.Checked;
			if (e.NewValue == CheckState.Unchecked)
			{
				if (loaderini.EnabledCodes.Contains(code.Name))
					loaderini.EnabledCodes.Remove(code.Name);
			}
			else
			{
				if (!loaderini.EnabledCodes.Contains(code.Name))
					loaderini.EnabledCodes.Add(code.Name);
			}
		}

		private void modListView_ItemCheck(object sender, ItemCheckEventArgs e)
		{
			if (suppressEvent) return;
			codes = new List<Code>(mainCodes.Codes);
			string modDir = Path.Combine(Environment.CurrentDirectory, "mods");
			List<string> modlist = new List<string>();
			foreach (ListViewItem item in modListView.CheckedItems)
				modlist.Add((string)item.Tag);
			if (e.NewValue == CheckState.Unchecked)
				modlist.Remove((string)modListView.Items[e.Index].Tag);
			else
				modlist.Add((string)modListView.Items[e.Index].Tag);
			foreach (string mod in modlist)
				if (mods.ContainsKey(mod))
				{
					ModInfo inf = mods[mod];
					if (!string.IsNullOrEmpty(inf.Codes))
						codes.AddRange(CodeList.Load(Path.Combine(Path.Combine(modDir, mod), inf.Codes)).Codes);
				}
			loaderini.EnabledCodes = new List<string>(loaderini.EnabledCodes.Where(a => codes.Any(c => c.Name == a)));
			foreach (Code item in codes.Where(a => a.Required && !loaderini.EnabledCodes.Contains(a.Name)))
				loaderini.EnabledCodes.Add(item.Name);
			codesCheckedListBox.BeginUpdate();
			codesCheckedListBox.Items.Clear();
			foreach (Code item in codes)
				codesCheckedListBox.Items.Add(item.Name, loaderini.EnabledCodes.Contains(item.Name));
			codesCheckedListBox.EndUpdate();
		}

		private void modListView_MouseClick(object sender, MouseEventArgs e)
		{
			if (e.Button != MouseButtons.Right)
			{
				return;
			}

			if (modListView.FocusedItem.Bounds.Contains(e.Location))
			{
				modContextMenu.Show(Cursor.Position);
			}
		}

		private void openFolderToolStripMenuItem_Click(object sender, EventArgs e)
		{
			foreach (ListViewItem item in modListView.SelectedItems)
			{
				Process.Start(Path.Combine("mods", (string)item.Tag));
			}
		}

		private void uninstallToolStripMenuItem_Click(object sender, EventArgs e)
		{
			DialogResult result = MessageBox.Show(this, "This will uninstall all selected mods."
				+ "\n\nAre you sure you wish to continue?", "Warning", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);

			if (result != DialogResult.Yes)
			{
				return;
			}

			result = MessageBox.Show(this, "Would you like to keep mod user data where possible? (Save files, config files, etc)",
				"User Data", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);

			if (result == DialogResult.Cancel)
			{
				return;
			}

			foreach (ListViewItem item in modListView.SelectedItems)
			{
				var dir = (string)item.Tag;
				var modDir = Path.Combine("mods", dir);
				var manpath = Path.Combine(modDir, "mod.manifest");

				try
				{
					if (result == DialogResult.Yes && File.Exists(manpath))
					{
						List<ModManifest> manifest = ModManifest.FromFile(manpath);
						foreach (var entry in manifest)
						{
							var path = Path.Combine(modDir, entry.FilePath);
							if (File.Exists(path))
							{
								File.Delete(path);
							}
						}

						File.Delete(manpath);
						var version = Path.Combine(modDir, "mod.version");
						if (File.Exists(version))
						{
							File.Delete(version);
						}
					}
					else
					{
						if (result == DialogResult.Yes)
						{
							var retain = MessageBox.Show(this, $"The mod \"{ mods[dir].Name }\" (\"mods\\{ dir }\") does not have a manifest, so mod user data cannot be retained."
								+ " Do you want to uninstall it anyway?", "Cannot Retain User Data", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);

							if (retain == DialogResult.No)
							{
								continue;
							}
						}

						Directory.Delete(modDir, true);
					}
				}
				catch (Exception ex)
				{
					MessageBox.Show(this, $"Failed to uninstall mod \"{ mods[dir].Name }\" from \"{ dir }\": { ex.Message }", "Failed",
						MessageBoxButtons.OK, MessageBoxIcon.Error);
				}
			}

			LoadModList();
		}
		
		private void generateManifestToolStripMenuItem_Click(object sender, EventArgs e)
		{
			DialogResult result = MessageBox.Show(this, "This can cause MOD USER DATA (SAVE FILES, CONFIG FILES) TO BE LOST upon next update!"
				+ " To prevent this, you should never run this on mods you did not develop."
				+ "\n\nAre you sure you wish to continue?",
				"Warning", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);

			if (result != DialogResult.Yes)
			{
				return;
			}

			foreach (ListViewItem item in modListView.SelectedItems)
			{
				var modPath = Path.Combine("mods", (string)item.Tag);
				var manifestPath = Path.Combine(modPath, "mod.manifest");

				List<ModManifest> manifest;
				List<ModManifestDiff> diff;

				using (var progress = new ManifestDialog(modPath, $"Generating manifest: {(string)item.Tag}", true))
				{
					progress.SetTask("Generating file index...");
					if (progress.ShowDialog(this) == DialogResult.Cancel)
					{
						continue;
					}

					manifest = progress.manifest;
					diff = progress.diff;
				}

				if (diff == null)
				{
					continue;
				}

				if (diff.Count(x => x.State != ModManifestState.Unchanged) <= 0)
				{
					continue;
				}

				using (var dialog = new ManifestDiffDialog(diff))
				{
					if (dialog.ShowDialog(this) == DialogResult.Cancel)
					{
						continue;
					}

					manifest = dialog.MakeNewManifest();
				}

				ModManifest.ToFile(manifest, manifestPath);
			}
		}
		
		private void checkForUpdatesToolStripMenuItem_Click(object sender, EventArgs e)
		{
			updateChecker?.RunWorkerAsync(modListView.SelectedItems.Cast<ListViewItem>()
				.Select(x => (string)x.Tag).Select(x => new KeyValuePair<string, ModInfo>(x, mods[x])).ToList());
		}

		private void comboUpdateFrequency_SelectedIndexChanged(object sender, EventArgs e)
		{
			numericUpdateFrequency.Enabled = comboUpdateFrequency.SelectedIndex > 0;
		}

		private void buttonCheckForUpdates_Click(object sender, EventArgs e)
		{
			buttonCheckForUpdates.Enabled = false;

			if (CheckForUpdates(true))
			{
				return;
			}

			CheckForModUpdates(true);
		}
	}
}
