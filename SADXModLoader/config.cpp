#include "stdafx.h"
#include "json.hpp"
#include "IniFile.hpp"
#include "GamePatchesLegacy.h"

using json = nlohmann::json;
using std::string;
using std::wstring;
using std::vector;
using std::unique_ptr;

wstring currentProfilePath; // Used for crash dumps
vector<std::string> ModList;
vector<std::string> GamePatchList;

void LoadModLoaderSettings(LoaderSettings* loaderSettings, std::wstring appPath)
{	
	std::wstring profilesPath = appPath + L"\\SADX\\Profiles.json";
	// If the profiles path exists, assume SA Mod Manager
	if (Exists(profilesPath))
	{
		// Load profiles JSON file
		std::ifstream ifs(profilesPath);
		json json_profiles = json::parse(ifs);
		ifs.close();

		// Get current profile index
		int ind_profile = json_profiles.value("ProfileIndex", 0);

		// Get current profile filename
		json proflist = json_profiles["ProfilesList"];
		std::string profname = proflist.at(ind_profile)["Filename"];

		// Convert profile name from UTF8 stored in JSON to wide string
		int count = MultiByteToWideChar(CP_UTF8, 0, profname.c_str(), profname.length(), NULL, 0);
		std::wstring profname_w(count, 0);
		MultiByteToWideChar(CP_UTF8, 0, profname.c_str(), profname.length(), &profname_w[0], count);

		// Load the current profile
		currentProfilePath = appPath + L"\\SADX\\" + profname_w;
		std::ifstream ifs_p(appPath + L"\\SADX\\" + profname_w);
		json json_config = json::parse(ifs_p);
		int settingsVersion = json_config.value("SettingsVersion", 0);

		// Graphics settings
		json json_graphics = json_config["Graphics"];
		loaderSettings->ScreenNum = json_graphics.value("SelectedScreen", 0);
		loaderSettings->HorizontalResolution = json_graphics.value("HorizontalResolution", 640);
		loaderSettings->VerticalResolution = json_graphics.value("VerticalResolution", 480);
		loaderSettings->ForceAspectRatio = json_graphics.value("Enable43ResolutionRatio", false);
		loaderSettings->EnableVsync = json_graphics.value("EnableVsync", true);
		loaderSettings->PauseWhenInactive = json_graphics.value("EnablePauseOnInactive", true);
		//loaderSettings->WindowedFullscreen = json_graphics.value("EnableBorderless", true);
		//loaderSettings->CustomWindowSize = json_graphics.value("EnableCustomWindow", false);
		//loaderSettings->StretchFullscreen = json_graphics.value("EnableScreenScaling", true);
		loaderSettings->WindowWidth = json_graphics.value("CustomWindowWidth", 640);
		loaderSettings->WindowHeight = json_graphics.value("CustomWindowHeight", 480);
		loaderSettings->MaintainWindowAspectRatio = json_graphics.value("EnableKeepResolutionRatio", false);
		loaderSettings->ResizableWindow = json_graphics.value("EnableResizableWindow", true);
		loaderSettings->BackgroundFillMode = json_graphics.value("FillModeBackground", 2);
		loaderSettings->FmvFillMode = json_graphics.value("FillModeFMV", 1);
		// ModeTextureFiltering ?
		// ModeUIFiltering ?
		loaderSettings->ScaleHud = json_graphics.value("EnableUIScaling", true);
		loaderSettings->AutoMipmap = json_graphics.value("EnableForcedMipmapping", true);
		loaderSettings->TextureFilter = json_graphics.value("EnableForcedTextureFilter", true);
		loaderSettings->ScreenMode = json_graphics.value("ScreenMode", 0);
		loaderSettings->ShowMouseInFullscreen = json_graphics.value("ShowMouseInFullscreen", false);
		loaderSettings->DisableBorderImage = json_graphics.value("DisableBorderImage", false);

		// Controller settings
		json json_controller = json_config["Controller"];
		loaderSettings->InputMod = json_controller.value("EnabledInputMod", true);

		// Sound settings
		json json_sound = json_config["Sound"];
		loaderSettings->EnableBassMusic = json_sound.value("EnableBassMusic", true);
		loaderSettings->EnableBassSFX = json_sound.value("EnableBassSFX", true);
		loaderSettings->SEVolume = json_sound.value("SEVolume", 100);

		// Patches settings (for compatibility)
		json json_oldpatches = json_config["Patches"];
		for (int p = 0; p < LengthOfArray(LegacyGamePatchList); p++)
		{
			if (json_oldpatches.value(LegacyGamePatchList[p].PatchName, LegacyGamePatchList[p].DefaultValue))
			{
				GamePatchList.push_back(LegacyGamePatchList[p].PatchName);
			}
		}

		// Debug settings
		json json_debug = json_config["DebugSettings"];
		loaderSettings->DebugConsole = json_debug.value("EnableDebugConsole", false);
		loaderSettings->DebugScreen = json_debug.value("EnableDebugScreen", false);
		loaderSettings->DebugFile = json_debug.value("EnableDebugFile", false);
		loaderSettings->DebugCrashLog = json_debug.value("EnableDebugCrashLog", true);

		// Testspawn settings (These don't really belong in testspawn imo - PkR)
		json json_testspawn = json_config["TestSpawn"];
		loaderSettings->TextLanguage = json_testspawn.value("GameTextLanguage", 1);
		loaderSettings->VoiceLanguage = json_testspawn.value("GameVoiceLanguage", 1);
		// EnableShowConsole ?
		// Mods
		json json_mods = json_config["EnabledMods"];
		for (unsigned int i = 1; i <= json_mods.size(); i++)
		{
			std::string mod_fname = json_mods.at(i - 1);
			ModList.push_back(mod_fname);
		}
		// Game Patches (current system)
		json json_patches = json_config["EnabledPatches"];
		for (unsigned int i = 1; i <= json_patches.size(); i++)
		{
			std::string patch_name = json_patches.at(i - 1);
			// Check if it isn't on the list already (legacy patches can be there)
			if (std::find(std::begin(GamePatchList), std::end(GamePatchList), patch_name) == std::end(GamePatchList));
				GamePatchList.push_back(patch_name);
		}
	}
	// If the profiles path doesn't exist, assume old SADX Mod Manager
	else
	{
		FILE* f_ini = _wfopen(L"mods\\SADXModLoader.ini", L"r");
		if (!f_ini)
		{
			MessageBox(nullptr, L"Mod Loader settings could not be read. Please run the Mod Manager, save settings and try again.", L"SADX Mod Loader", MB_ICONWARNING);
			return;
		}
		unique_ptr<IniFile> ini(new IniFile(f_ini));
		fclose(f_ini);

		// Process the main Mod Loader settings.
		const IniGroup* setgrp = ini->getGroup("");

		loaderSettings->DebugConsole = setgrp->getBool("DebugConsole");
		loaderSettings->DebugScreen = setgrp->getBool("DebugScreen");
		loaderSettings->DebugFile = setgrp->getBool("DebugFile");
		loaderSettings->DebugCrashLog = setgrp->getBool("DebugCrashLog", true);
		loaderSettings->HorizontalResolution = setgrp->getInt("HorizontalResolution", 640);
		loaderSettings->VerticalResolution = setgrp->getInt("VerticalResolution", 480);
		loaderSettings->ForceAspectRatio = setgrp->getBool("ForceAspectRatio");
		//loaderSettings->WindowedFullscreen = (setgrp->getBool("Borderless") || setgrp->getBool("WindowedFullscreen"));
		loaderSettings->EnableVsync = setgrp->getBool("EnableVsync", true);
		loaderSettings->AutoMipmap = setgrp->getBool("AutoMipmap", true);
		loaderSettings->TextureFilter = setgrp->getBool("TextureFilter", true);
		loaderSettings->PauseWhenInactive = setgrp->getBool("PauseWhenInactive", true);
		//loaderSettings->StretchFullscreen = setgrp->getBool("StretchFullscreen", true);
		loaderSettings->ScreenNum = setgrp->getInt("ScreenNum", 1);
		loaderSettings->VoiceLanguage = setgrp->getInt("VoiceLanguage", 1);
		loaderSettings->TextLanguage = setgrp->getInt("TextLanguage", 1);
		//loaderSettings->CustomWindowSize = setgrp->getBool("CustomWindowSize");
		loaderSettings->WindowWidth = setgrp->getInt("WindowWidth", 640);
		loaderSettings->WindowHeight = setgrp->getInt("WindowHeight", 480);
		loaderSettings->MaintainWindowAspectRatio = setgrp->getBool("MaintainWindowAspectRatio");
		loaderSettings->ResizableWindow = setgrp->getBool("ResizableWindow");
		loaderSettings->ScaleHud = setgrp->getBool("ScaleHud", true);
		loaderSettings->BackgroundFillMode = setgrp->getInt("BackgroundFillMode", uiscale::FillMode_Fill);
		loaderSettings->FmvFillMode = setgrp->getInt("FmvFillMode", uiscale::FillMode_Fit);
		loaderSettings->EnableBassMusic = setgrp->getBool("EnableBassMusic", true);
		loaderSettings->EnableBassSFX = setgrp->getBool("EnableBassSFX", false);
		loaderSettings->SEVolume = setgrp->getInt("SEVolume", 100);

		//Patches
		loaderSettings->HRTFSound = setgrp->getBool("HRTFSound", true);
		loaderSettings->CCEF = setgrp->getBool("CCEF", true);
		loaderSettings->PolyBuff = setgrp->getBool("PolyBuff", true);
		loaderSettings->MaterialColorFix = setgrp->getBool("MaterialColorFix", true);
		loaderSettings->NodeLimit = setgrp->getBool("NodeLimit", true);
		loaderSettings->FovFix = setgrp->getBool("FovFix", true);
		loaderSettings->SCFix = setgrp->getBool("SCFix", true);
		loaderSettings->Chaos2CrashFix = setgrp->getBool("Chaos2CrashFix", true);
		loaderSettings->ChunkSpecFix = setgrp->getBool("ChunkSpecFix", true);
		loaderSettings->E102PolyFix = setgrp->getBool("E102PolyFix", true);
		loaderSettings->ChaoPanelFix = setgrp->getBool("ChaoPanelFix ", true);
		loaderSettings->PixelOffsetFix = setgrp->getBool("PixelOffsetFix ", true);
		loaderSettings->LightFix = setgrp->getBool("LightFix", true);
		loaderSettings->KillGbix = setgrp->getBool("KillGbix", true);
		loaderSettings->DisableCDCheck = setgrp->getBool("DisableCDCheck", false);
		loaderSettings->ExtendedSaveSupport = setgrp->getBool("ExtendedSaveSupport", true);
		loaderSettings->CrashGuard = setgrp->getBool("CrashGuard", true);
		loaderSettings->XInputFix = setgrp->getBool("XInputFix", false);

		for (unsigned int i = 1; i <= 999; i++)
		{
			char key[8];
			snprintf(key, sizeof(key), "Mod%u", i);
			if (!setgrp->hasKey(key))
				break;
			ModList.push_back(setgrp->getString(key));
		}
	}
}

std::string GetModName(int index)
{
	return ModList.at(index - 1);
}

unsigned int GetModCount()
{
	return ModList.size();
}

bool IsGamePatchEnabled(const char* patchName)
{
	return (std::find(std::begin(GamePatchList), std::end(GamePatchList), patchName) != std::end(GamePatchList));
}

void ListPatches()
{
	PrintDebug("Enabling %d game patches:\n", GamePatchList.size());
	for (std::string s : GamePatchList)
	{
		PrintDebug("Enabled game patch: %s\n", s.c_str());
	}
}