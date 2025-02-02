#include "stdafx.h"
#include "json.hpp"
#include "IniFile.hpp"

using json = nlohmann::json;
using std::string;
using std::wstring;
using std::vector;
using std::unique_ptr;

wstring currentProfilePath; // Used for crash dumps
vector<std::string> ModList;
vector<std::string> GamePatchList;

// List of game patches that may be using the old system
std::string LegacyGamePatchList[] =
{
	"HRTFSound",
	"KeepCamSettings",
	"FixVertexColorRendering",
	"MaterialColorFix",
	"NodeLimit",
	"FOVFix",
	"SkyChaseResolutionFix",
	"Chaos2CrashFix",
	"ChunkSpecularFix",
	"E102NGonFix",
	"ChaoPanelFix",
	"PixelOffSetFix",
	"LightFix",
	"KillGBIX",
	"DisableCDCheck",
	"ExtendedSaveSupport",
	"CrashGuard",
	"XInputFix",
};

void DisplaySettingsLoadError(wstring gamePath, wstring appPath, wstring errorFile)
{
	wstring gamepatherror = L"\n\nGame path: " + gamePath;
	wstring appdataerror = L"\n\nManager data path: " + appPath;
	wstring missingerror = L"\n\nThe following file was missing: " + errorFile;
	wstring error = L"Mod Loader settings could not be read. Please run the Mod Manager, save settings and try again." + gamepatherror + appdataerror + missingerror;
	MessageBox(nullptr, error.c_str(), L"SADX Mod Loader", MB_ICONERROR);
	OnExit(0, 0, 0);
	ExitProcess(0);
}

void LoadModLoaderSettings(LoaderSettings* loaderSettings, std::wstring appPath, std::wstring gamePath)
{
	std::wstring profileFolderName = L"profiles\\";
	std::wstring profilesPath = appPath + profileFolderName + L"Profiles.json";

	if (!Exists(profilesPath)) // If Profiles.json doesn't exist, assume the old paths system that adds "SADX" instead of "profiles"
	{
		profileFolderName = L"SADX\\";
		profilesPath = appPath + profileFolderName + L"Profiles.json";

		// If that doesn't exist either, display an error and quit
		if (!Exists(profilesPath))
		{
			DisplaySettingsLoadError(gamePath, appPath, profilesPath);
		}
	}

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
	currentProfilePath = appPath + profileFolderName + profname_w;
	if (!Exists(currentProfilePath))
	{
		DisplaySettingsLoadError(gamePath, appPath, currentProfilePath);
	}
	

	std::ifstream ifs_p(appPath + profileFolderName + profname_w);
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

	// Game Patches settings (for compatibility)
	if (json_config.contains("Patches"))
	{
		json json_oldpatches = json_config["Patches"];
		loaderSettings->HRTFSound = json_oldpatches.value("HRTFSound", false);
		loaderSettings->CCEF = json_oldpatches.value("KeepCamSettings", true);
		loaderSettings->PolyBuff = json_oldpatches.value("FixVertexColorRendering", true);
		loaderSettings->MaterialColorFix = json_oldpatches.value("MaterialColorFix", true);
		loaderSettings->NodeLimit = json_oldpatches.value("NodeLimit", true);
		loaderSettings->FovFix = json_oldpatches.value("FOVFix", true);
		loaderSettings->SCFix = json_oldpatches.value("SkyChaseResolutionFix", true);
		loaderSettings->Chaos2CrashFix = json_oldpatches.value("Chaos2CrashFix", true);
		loaderSettings->ChunkSpecFix = json_oldpatches.value("ChunkSpecularFix", true);
		loaderSettings->E102PolyFix = json_oldpatches.value("E102NGonFix", true);
		loaderSettings->ChaoPanelFix = json_oldpatches.value("ChaoPanelFix", true);
		loaderSettings->PixelOffsetFix = json_oldpatches.value("PixelOffSetFix", true);
		loaderSettings->LightFix = json_oldpatches.value("LightFix", true);
		loaderSettings->KillGbix = json_oldpatches.value("KillGBIX", false);
		loaderSettings->DisableCDCheck = json_oldpatches.value("DisableCDCheck", true);
		loaderSettings->ExtendedSaveSupport = json_oldpatches.value("ExtendedSaveSupport", true);
		loaderSettings->CrashGuard = json_oldpatches.value("CrashGuard", true);
		loaderSettings->XInputFix = json_oldpatches.value("XInputFix", false);
		// Add old game patches to the new system
		for (int p = 0; p < LengthOfArray(LegacyGamePatchList); p++)
		{
			if (json_oldpatches.value(LegacyGamePatchList[p], false)) // Old game patches are stored in the json regardless of whether they are enabled or not
			{
				GamePatchList.push_back(LegacyGamePatchList[p]);
			}
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
	json json_patches = json_config["EnabledGamePatches"];
	for (unsigned int i = 1; i <= json_patches.size(); i++)
	{
		std::string patch_name = json_patches.at(i - 1);
		// Check if it isn't on the list already (legacy patches can be there)
		if (std::find(std::begin(GamePatchList), std::end(GamePatchList), patch_name) == std::end(GamePatchList));
		GamePatchList.push_back(patch_name);
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