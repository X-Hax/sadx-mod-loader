﻿using System.Collections.Generic;
using System.ComponentModel;
using IniSerializer;

namespace SADXModManager
{
	class LoaderInfo
	{
		public bool DebugConsole { get; set; }
		public bool DebugScreen { get; set; }
		public bool DebugFile { get; set; }
		public bool? ShowConsole { get { return null; } set { if (value.HasValue) DebugConsole = value.Value; } }
		public bool DisableCDCheck { get; set; }
		public bool UseCustomResolution { get; set; }
		[DefaultValue(640)]
		public int HorizontalResolution { get; set; }
		[DefaultValue(480)]
		public int VerticalResolution { get; set; }
		public bool ForceAspectRatio { get; set; }
		[DefaultValue(false)]
		public bool ScaleHud { get; set; }
		public bool WindowedFullscreen { get; set; }
		[DefaultValue(true)]
		public bool EnableVsync { get; set; }
		[DefaultValue(true)]
		public bool AutoMipmap { get; set; }
		[DefaultValue(true)]
		public bool TextureFilter { get; set; }
		[DefaultValue(true)]
		public bool PauseWhenInactive { get; set; }
		[DefaultValue(true)]
		public bool StretchFullscreen { get; set; }
		[DefaultValue(1)]
		public int ScreenNum { get; set; }
		public bool CustomWindowSize { get; set; }
		[DefaultValue(640)]
		public int WindowWidth { get; set; }
		[DefaultValue(480)]
		public int WindowHeight { get; set; }
		public bool MaintainWindowAspectRatio { get; set; }
		[IniName("Mod")]
		[IniCollection(IniCollectionMode.NoSquareBrackets, StartIndex = 1)]
		public List<string> Mods { get; set; }
		[IniName("Code")]
		[IniCollection(IniCollectionMode.NoSquareBrackets, StartIndex = 1)]
		public List<string> EnabledCodes { get; set; }

		public LoaderInfo()
		{
			Mods = new List<string>();
			EnabledCodes = new List<string>();
		}
	}
}