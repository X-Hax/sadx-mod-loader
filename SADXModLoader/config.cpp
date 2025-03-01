#include "stdafx.h"
#include "json.hpp"
#include "IniFile.hpp"

#include <ShlObj.h>

using json = nlohmann::json;
using std::string;
using std::wstring;
using std::vector;
using std::unique_ptr;

wstring currentProfilePath; // Used for crash dumps
vector<std::string> ModList;
vector<std::string> GamePatchList;

void DisplaySettingsLoadError(wstring file)
{
	wstring error = L"Mod Loader settings could not be read. Please run the Mod Manager, save settings and try again.\n\nThe following file was missing: " + file;
	MessageBox(nullptr, error.c_str(), L"SADX Mod Loader", MB_ICONERROR);
	OnExit(0, 0, 0);
	ExitProcess(0);
}

void LoadModLoaderSettings(LoaderSettings* loaderSettings, wstring gamePath)
{
	// Get paths for Mod Loader settings and libraries, normally located in 'Sonic Adventure DX\mods\.modloader'

	wstring loaderDataPath = gamePath + L"\\mods\\.modloader\\";
	loaderSettings->ExtLibPath = loaderDataPath + L"extlib\\";
	wstring profilesFolderPath = loaderDataPath + L"profiles\\";
	wstring profilesJsonPath = profilesFolderPath + L"Profiles.json";

	// If Profiles.json isn't found, assume the old paths system
	if (!Exists(profilesJsonPath))
	{
		// Check 'Sonic Adventure DX\SAManager\SADX' (portable mode) first
		profilesJsonPath = gamePath + L"\\SAManager\\SADX\\Profiles.json";
		if (Exists(profilesJsonPath))
		{
			loaderDataPath = gamePath + L"\\SAManager\\";
		}
		// If that doesn't exist either, assume the settings are in 'AppData\Local\SAManager'
		else
		{
			WCHAR appDataLocalBuf[MAX_PATH];
			// Get the LocalAppData folder and check if it has the profiles json
			if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataLocalBuf)))
			{
				wstring appDataLocalPath(appDataLocalBuf);
				profilesJsonPath = appDataLocalPath + L"\\SAManager\\SADX\\Profiles.json";
				if (Exists(profilesJsonPath))
				{
					loaderDataPath = appDataLocalPath + L"\\SAManager\\";
				}
				// If it still can't be found, display an error message
				else
					DisplaySettingsLoadError(gamePath + L"\\mods\\.modloader\\Profiles.json");
			}
			else
			{
				MessageBox(nullptr, L"Unable to retrieve local AppData path.", L"SADX Mod Loader", MB_ICONERROR);
				OnExit(0, 0, 0);
				ExitProcess(0);
			}
		}
		// If Profiles.json was found, set old paths
		loaderSettings->ExtLibPath = loaderDataPath + L"extlib\\";
		profilesFolderPath = loaderDataPath + L"SADX\\";
	}

	// Load profiles JSON file
	std::ifstream ifs(profilesJsonPath);
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
	currentProfilePath = profilesFolderPath + profname_w;
	if (!Exists(currentProfilePath))
	{
		DisplaySettingsLoadError(currentProfilePath);
	}

	std::ifstream ifs_p(currentProfilePath);
	json json_config = json::parse(ifs_p);
	int settingsVersion = json_config.value("SettingsVersion", 0);

	// Graphics settings
	json json_graphics = json_config["Graphics"];
	loaderSettings->ScreenNum = json_graphics.value("SelectedScreen", 0);
	loaderSettings->HorizontalResolution = json_graphics.value("HorizontalResolution", 640);
	loaderSettings->VerticalResolution = json_graphics.value("VerticalResolution", 480);
	loaderSettings->ForceAspectRatio = json_graphics.value("Enable43ResolutionRatio", false);
	loaderSettings->EnableVsync = json_graphics.value("EnableVsync", true);
	loaderSettings->Antialiasing = json_graphics.value("Antialiasing", 0);
	loaderSettings->Anisotropic = json_graphics.value("Anisotropic", 0);
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
	loaderSettings->RenderBackend = json_graphics.value("RenderBackend", 0);
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

	// Game Patches:
	// V1: `Patches` as a struct with bools that have hardcoded names (converted to bool values in LoaderSettings)
	// V2: `EnabledGamePatches` as a list of enabled patch names as strings
	// V3: `Patches` as a dictionary of strings and bools
	// V1 is compatible with V3 because it's stored with the same formatting in the JSON file
	
	// Process V2 first
	if (json_config.contains("EnabledGamePatches"))
	{
		json json_patches_list = json_config["EnabledGamePatches"];
		for (unsigned int i = 1; i <= json_patches_list.size(); i++)
		{
			std::string patch_name = json_patches_list.at(i - 1);
			GamePatchList.push_back(patch_name);
		}
	}
	// Process V1/V3
	if (json_config.contains("Patches")) 
	{
		json json_patches = json_config["Patches"];
		for (json::iterator it = json_patches.begin(); it != json_patches.end(); ++it)
		{
			std::string patch_name = it.key();
			if (it.value() == true)
			{
				// Check if it isn't on the list already (legacy patches can be there)
				if (std::find(std::begin(GamePatchList), std::end(GamePatchList), patch_name) == std::end(GamePatchList));
				GamePatchList.push_back(patch_name);
			}
		}
		// Update the old LoaderSettings values for compatibility
		loaderSettings->HRTFSound = json_patches.value("HRTFSound", false);
		loaderSettings->CCEF = json_patches.value("KeepCamSettings", true);
		loaderSettings->PolyBuff = json_patches.value("FixVertexColorRendering", true);
		loaderSettings->MaterialColorFix = json_patches.value("MaterialColorFix", true);
		loaderSettings->NodeLimit = json_patches.value("NodeLimit", true);
		loaderSettings->FovFix = json_patches.value("FOVFix", true);
		loaderSettings->SCFix = json_patches.value("SkyChaseResolutionFix", true);
		loaderSettings->Chaos2CrashFix = json_patches.value("Chaos2CrashFix", true);
		loaderSettings->ChunkSpecFix = json_patches.value("ChunkSpecularFix", true);
		loaderSettings->E102PolyFix = json_patches.value("E102NGonFix", true);
		loaderSettings->ChaoPanelFix = json_patches.value("ChaoPanelFix", true);
		loaderSettings->PixelOffsetFix = json_patches.value("PixelOffSetFix", true);
		loaderSettings->LightFix = json_patches.value("LightFix", true);
		loaderSettings->KillGbix = json_patches.value("KillGBIX", false);
		loaderSettings->DisableCDCheck = json_patches.value("DisableCDCheck", true);
		loaderSettings->ExtendedSaveSupport = json_patches.value("ExtendedSaveSupport", true);
		loaderSettings->CrashGuard = json_patches.value("CrashGuard", true);
		loaderSettings->XInputFix = json_patches.value("XInputFix", false);
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

void ListGamePatches()
{
	PrintDebug("Enabling %d game patches:\n", GamePatchList.size());
	for (std::string s : GamePatchList)
	{
		PrintDebug("Enabled game patch: %s\n", s.c_str());
	}
}