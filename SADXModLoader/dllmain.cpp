#include "stdafx.h"

#include <cassert>

#include <deque>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include "direct3d.h"
#include "bgscale.h"
#include "hudscale.h"
#include "testspawn.h"
#include "json.hpp"
#include "config.h"

using std::deque;
using std::ifstream;
using std::string;
using std::wstring;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using json = nlohmann::json;

// Win32 headers.
#include <DbgHelp.h>
#include <Shlwapi.h>
#include <GdiPlus.h>
#include "resource.h"

#include "git.h"
#include "version.h"
#include <direct.h>

#include "IniFile.hpp"
#include "CodeParser.hpp"
#include "MediaFns.hpp"
#include "TextConv.hpp"
#include "SADXModLoader.h"
#include "LandTableInfo.h"
#include "ModelInfo.h"
#include "AnimationFile.h"
#include "TextureReplacement.h"
#include "FileReplacement.h"
#include "FileSystem.h"
#include "Events.h"
#include "AutoMipmap.h"
#include "uiscale.h"
#include "FixFOV.h"
#include "EXEData.h"
#include "DLLData.h"
#include "ChunkSpecularFix.h"
#include "CrashDump.h"
#include "MaterialColorFixes.h"
#include "sound.h"
#include "InterpolationFixes.h"
#include "MinorGamePatches.h"
#include "jvList.h"
#include "Gbix.h"
#include "input.h"
#include "video.h"
#include <ShlObj.h>
#include "gvm.h"
#include "ExtendedSaveSupport.h"
#include "NodeLimit.h"
#include "CrashGuard.h"
#include "window.h"
#include "XInputFix.h"
#include "dpi.h"
#include "backend.h"

wstring borderimage = L"mods\\Border.png";
HINSTANCE g_hinstDll = nullptr;
wstring iconPathName;
wstring bassFolder;

/**
 * Show an error message indicating that this isn't the 2004 US version.
 * This function also calls ExitProcess(1).
 */
static void ShowNon2004USError()
{
	MessageBox(nullptr, L"This copy of Sonic Adventure DX is not the 2004 US version.\n\n"
		L"Please obtain the EXE file from the 2004 US version and try again.",
		L"SADX Mod Loader", MB_ICONERROR);
	ExitProcess(1);
}

/**
 * Hook SADX's CreateFileA() import.
 */
static void HookCreateFileA()
{
	ULONG ulSize = 0;

	// SADX module handle. (main executable)
	HMODULE hModule = GetModuleHandle(nullptr);

	PROC pNewFunction = (PROC)MyCreateFileA;
	// Get the actual CreateFileA() using GetProcAddress().
	PROC pActualFunction = GetProcAddress(GetModuleHandle(L"Kernel32.dll"), "CreateFileA");
	assert(pActualFunction != nullptr);

	auto pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);

	if (pImportDesc == nullptr)
	{
		return;
	}

	for (; pImportDesc->Name; pImportDesc++)
	{
		// get the module name
		auto pcszModName = (PCSTR)((PBYTE)hModule + pImportDesc->Name);

		// check if the module is kernel32.dll
		if (pcszModName != nullptr && _stricmp(pcszModName, "Kernel32.dll") == 0)
		{
			// get the module
			auto pThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + pImportDesc->FirstThunk);

			for (; pThunk->u1.Function; pThunk++)
			{
				PROC* ppfn = (PROC*)&pThunk->u1.Function;
				if (*ppfn == pActualFunction)
				{
					// Found CreateFileA().
					DWORD dwOldProtect = 0;
					VirtualProtect(ppfn, sizeof(pNewFunction), PAGE_WRITECOPY, &dwOldProtect);
					WriteData(ppfn, pNewFunction);
					VirtualProtect(ppfn, sizeof(pNewFunction), dwOldProtect, &dwOldProtect);
					// FIXME: Would it be listed multiple times?
					break;
				} // Function that we are looking for
			}
		}
	}
}

/**
 * Change write protection of the .rdata section.
 * @param protect True to protect; false to unprotect.
 */
static void SetRDataWriteProtection(bool protect)
{
	// Reference: https://stackoverflow.com/questions/22588151/how-to-find-data-segment-and-code-segment-range-in-program
	// TODO: Does this handle ASLR? (SADX doesn't use ASLR, though...)

	// SADX module handle. (main executable)
	HMODULE hModule = GetModuleHandle(nullptr);

	// Get the PE header.
	const IMAGE_NT_HEADERS* const pNtHdr = ImageNtHeader(hModule);
	// Section headers are located immediately after the PE header.
	const auto* pSectionHdr = reinterpret_cast<const IMAGE_SECTION_HEADER*>(pNtHdr + 1);

	// Find the .rdata section.
	for (unsigned int i = pNtHdr->FileHeader.NumberOfSections; i > 0; i--, pSectionHdr++)
	{
		if (strncmp(reinterpret_cast<const char*>(pSectionHdr->Name), ".rdata", sizeof(pSectionHdr->Name)) != 0)
		{
			continue;
		}

		// Found the .rdata section.
		// Verify that this matches SADX.
		if (pSectionHdr->VirtualAddress != 0x3DB000 ||
			pSectionHdr->Misc.VirtualSize != 0xB6B88)
		{
			// Not SADX, or the wrong version.
			ShowNon2004USError();
			ExitProcess(1);
		}

		const intptr_t vaddr = reinterpret_cast<intptr_t>(hModule) + pSectionHdr->VirtualAddress;
		DWORD flOldProtect;
		DWORD flNewProtect = (protect ? PAGE_READONLY : PAGE_WRITECOPY);
		VirtualProtect(reinterpret_cast<void*>(vaddr), pSectionHdr->Misc.VirtualSize, flNewProtect, &flOldProtect);
		return;
	}

	// .rdata section not found.
	ShowNon2004USError();
	ExitProcess(1);
}

struct message
{
	string text;
	uint32_t time;
};

static deque<message> msgqueue;

static const uint32_t fadecolors[] = {
	0xF7FFFFFF, 0xEEFFFFFF, 0xE6FFFFFF, 0xDDFFFFFF,
	0xD5FFFFFF, 0xCCFFFFFF, 0xC4FFFFFF, 0xBBFFFFFF,
	0xB3FFFFFF, 0xAAFFFFFF, 0xA2FFFFFF, 0x99FFFFFF,
	0x91FFFFFF, 0x88FFFFFF, 0x80FFFFFF, 0x77FFFFFF,
	0x6FFFFFFF, 0x66FFFFFF, 0x5EFFFFFF, 0x55FFFFFF,
	0x4DFFFFFF, 0x44FFFFFF, 0x3CFFFFFF, 0x33FFFFFF,
	0x2BFFFFFF, 0x22FFFFFF, 0x1AFFFFFF, 0x11FFFFFF,
	0x09FFFFFF, 0
};

// Code Parser.
static CodeParser codeParser;

static void __cdecl ProcessCodes()
{
	codeParser.processCodeList();
	RaiseEvents(modFrameEvents);
	uiscale::check_stack_balance();

	const int numrows = (VerticalResolution / (int)DebugFontSize);
	int pos = (int)msgqueue.size() <= numrows - 1 ? numrows - 1 - (msgqueue.size() - 1) : 0;

	if (msgqueue.empty())
	{
		return;
	}

	for (auto iter = msgqueue.begin();
		iter != msgqueue.end(); ++iter)
	{
		int c = -1;

		if (300 - iter->time < LengthOfArray(fadecolors))
		{
			c = fadecolors[LengthOfArray(fadecolors) - (300 - iter->time) - 1];
		}

		SetDebugFontColor((int)c);
		DisplayDebugString(pos++, (char*)iter->text.c_str());
		if (++iter->time >= 300)
		{
			msgqueue.pop_front();

			if (msgqueue.empty())
			{
				break;
			}

			iter = msgqueue.begin();
		}

		if (pos == numrows)
		{
			break;
		}
	}
}

static bool dbgConsole, dbgScreen;
// File for logging debugging output.
static FILE* dbgFile = nullptr;

/**
 * SADX Debug Output function.
 * @param Format Format string.
 * @param ... Arguments.
 * @return Return value from vsnprintf().
 */
static int __cdecl SADXDebugOutput(const char* Format, ...)
{
	va_list ap;
	va_start(ap, Format);
	int result = vsnprintf(nullptr, 0, Format, ap) + 1;
	va_end(ap);
	char* buf = new char[result + 1];
	va_start(ap, Format);
	result = vsnprintf(buf, result + 1, Format, ap);
	va_end(ap);

	// Console output.
	if (dbgConsole)
	{
		char* utf8 = SJIStoUTF8(buf);
		if (utf8)
		{
			fputs(utf8, stdout);
			fflush(stdout);
			delete utf8;
		}
	}

	// Screen output.
	if (dbgScreen)
	{
		message msg = { { buf }, 0 };
			// Remove trailing newlines if present.
			while (!msg.text.empty() &&
				(msg.text[msg.text.size() - 1] == '\n' ||
					msg.text[msg.text.size() - 1] == '\r'))
			{
				msg.text.resize(msg.text.size() - 1);
			}
			msgqueue.push_back(msg);
		}

	// File output.
	if (dbgFile)
	{
		// SADX prints text in Shift-JIS.
		// Convert it to UTF-8 before writing it to the debug file.
		char* utf8 = SJIStoUTF8(buf);
		if (utf8)
		{
			fputs(utf8, dbgFile);
			fflush(dbgFile);
			delete[] utf8;
		}
	}

	delete[] buf;
	return result;
}

unordered_map<unsigned char, unordered_map<int, StartPosition>> StartPositions;
unordered_map<unsigned char, unordered_map<int, FieldStartPosition>> FieldStartPositions;
unordered_map<int, PathDataPtr> Paths;
bool PathsInitialized;
unordered_map<unsigned char, vector<PVMEntry>> CharacterPVMs;
vector<PVMEntry> CommonObjectPVMs;
bool CommonObjectPVMsInitialized;
unordered_map<unsigned char, vector<TrialLevelListEntry>> _TrialLevels;
unordered_map<unsigned char, vector<TrialLevelListEntry>> _TrialSubgames;
const char* mainsavepath = "SAVEDATA";
const char* chaosavepath = "SAVEDATA";
string windowtitle;
vector<SoundList> _SoundLists;
vector<MusicInfo> _MusicList;
vector<__int16> _USVoiceDurationList;
vector<__int16> _JPVoiceDurationList;
extern HelperFunctions helperFunctions;
LoaderSettings loaderSettings = {};

static const char* const dlldatakeys[] = {
	"CHRMODELSData",
	"ADV00MODELSData",
	"ADV01MODELSData",
	"ADV01CMODELSData",
	"ADV02MODELSData",
	"ADV03MODELSData",
	"BOSSCHAOS0MODELSData",
	"CHAOSTGGARDEN02MR_DAYTIMEData",
	"CHAOSTGGARDEN02MR_EVENINGData",
	"CHAOSTGGARDEN02MR_NIGHTData"
};

void __cdecl SetLanguage()
{
	VoiceLanguage = loaderSettings.VoiceLanguage;
	TextLanguage = loaderSettings.TextLanguage;
}

Bool __cdecl FixEKey(int i)
{
	return IsFreeCameraAllowed() == TRUE && GetKey(i) == TRUE;
}

void __cdecl Direct3D_TextureFilterPoint_ForceLinear()
{
	Direct3D_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	Direct3D_Device->SetRenderState(D3DRS_LIGHTING, 0);
	Direct3D_Device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
	Direct3D_Device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	Direct3D_Device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	Direct3D_Device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
	// Maybe use D3DTEXF_ANISOTROPIC here instead? Not sure
	/*
	Direct3D_Device->SetTextureStageState(0, D3DTSS_MAGFILTER, loaderSettings.Anisotropic ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
	Direct3D_Device->SetTextureStageState(0, D3DTSS_MINFILTER, loaderSettings.Anisotropic ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
	Direct3D_Device->SetTextureStageState(0, D3DTSS_MIPFILTER, loaderSettings.Anisotropic ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
	*/
}

void __cdecl SetPreferredFilterOption()
{
	if (loaderSettings.TextureFilter == true)
	{
		Direct3D_TextureFilterPoint_ForceLinear();
	}
	else
	{
		Direct3D_TextureFilterPoint();
	}
}

void __cdecl Direct3D_EnableHudAlpha_Point(bool enable)
{
	Direct3D_EnableHudAlpha(enable);
	Direct3D_TextureFilterPoint();
}

void __cdecl njDrawTextureMemList_NoFilter(NJS_TEXTURE_VTX* polygons, Int count, Uint32 gbix, Int flag)
{
	njAlphaMode(flag);
	Direct3D_TextureFilterPoint();
	njSetTextureNumG(gbix);
	if (uiscale::is_scale_enabled() == true) uiscale::scale_texmemlist(polygons, count);
	DrawRect_TextureVertexTriangleStrip(polygons, count);
	Direct3D_TextureFilterLinear();
}

void __cdecl FixLandTableLightType()
{
	Direct3D_PerformLighting(0);
}

void __cdecl PreserveLightType(int a1)
{
	Direct3D_PerformLighting(a1 * 2); // Bitwise shift left
}

void ProcessVoiceDurationRegisters()
{
	if (!_USVoiceDurationList.empty())
	{
		auto size = _USVoiceDurationList.size();
		auto newlist = new __int16[size];
		memcpy(newlist, _USVoiceDurationList.data(), sizeof(__int16) * size);
		WriteData((__int16**)0x4255A2, newlist);
	}

	if (!_JPVoiceDurationList.empty())
	{
		auto size = _JPVoiceDurationList.size();
		auto newlist = new __int16[size];
		memcpy(newlist, _JPVoiceDurationList.data(), sizeof(__int16) * size);
		WriteData((__int16**)0x42552F, newlist);
	}

	_USVoiceDurationList.clear();
	_JPVoiceDurationList.clear();
}

static void __cdecl InitAudio(wstring extLibPath)
{
	// Load BASS if any BASS options are enabled, or if BGM/SE in the system folder are missing (i.e. this is a converted Steam install)
	if (loaderSettings.EnableBassMusic || loaderSettings.EnableBassSFX || !Exists("system\\sounddata\\bgm") || !Exists("system\\sounddata\\se"))
	{
		bassFolder = extLibPath + L"BASS\\";

		// If the file doesn't exist, assume it's in the game folder like with the old Manager
		if (!FileExists(bassFolder + L"bass_vgmstream.dll"))
			bassFolder = L"";

		bool bassDLL = false; // Handle of the DLL
		wstring fullPath = bassFolder + L"bass_vgmstream.dll"; // Path to the DLL

		// Attempt to load the DLL
		bassDLL = LoadLibraryEx(fullPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		
		if (bassDLL)
			PrintDebug("Loaded Bass DLLs dependencies\n");
		else
		{
			int err = GetLastError();
			PrintDebug("Error loading BASS DLL %X (%d)\n", err, err);
			MessageBox(nullptr, L"Error loading BASS.\n\n"
				L"Make sure the Mod Loader is installed properly.",
				L"BASS Load Error", MB_OK | MB_ICONERROR);
			return;
		}

		// Functions that should be hooked regardless of options because they load or unload the BASS library
		WriteJump((void*)0x40D1EA, WMPInit_r);
		WriteJump((void*)0x40D28A, WMPRelease_r);

		// Music
		if (loaderSettings.EnableBassMusic || !Exists("system\\sounddata\\bgm\\wma"))
		{
			WriteCall((void*)0x42544C, PlayMusicFile_r);
			WriteCall((void*)0x4254F4, PlayVoiceFile_r);
			WriteCall((void*)0x425569, PlayVoiceFile_r);
			WriteCall((void*)0x513187, PlayVideoFile_r);
			WriteCall((void*)0x425488, PlayMusicFile_CD_r);
			WriteCall((void*)0x425591, PlayVoiceFile_CD_r);
			WriteCall((void*)0x42551D, PlayVoiceFile_CD_r);
			WriteJump((void*)0x40CF50, WMPRestartMusic_r);
			WriteJump((void*)0x40D060, PauseMusic_r);
			WriteJump((void*)0x40D0A0, ResumeMusic_r);
			WriteJump((void*)0x40CFF0, WMPClose_r);
			WriteJump((void*)0x40CF20, sub_40CF20_r);
		}

		// SFX
		if (loaderSettings.EnableBassSFX || !Exists("system\\sounddata\\se"))
			Sound_Init(loaderSettings.SEVolume);
	}

	// Allow HRTF 3D sound
	if (IsGamePatchEnabled("HRTFSound"))
	{
		WriteData<uint8_t>(reinterpret_cast<uint8_t*>(0x00402773), 0xEBu);
	}
}

void InitGamePatches()
{
	// Fix the game not saving camera setting properly 
	if (IsGamePatchEnabled("KeepCamSettings"))
	{
		WriteData((int16_t*)0x438330, (int16_t)0x0D81);
		WriteData((int16_t*)0x434870, (int16_t)0x0D81);
	}

	// Fixes N-sided polygons (Gamma's headlight) by using
	// triangle strip vertex buffer initializers.

	if (IsGamePatchEnabled("E102NGonFix"))
	{
		for (size_t i = 0; i < MeshSetInitFunctions.size(); ++i)
		{
			auto& a = MeshSetInitFunctions[i];
			a[NJD_MESHSET_N >> 14] = a[NJD_MESHSET_TRIMESH >> 14];
		}

		for (size_t i = 0; i < PolyBuffDraw_VertexColor.size(); ++i)
		{
			auto& a = PolyBuffDraw_VertexColor[i];
			a[NJD_MESHSET_N >> 14] = a[NJD_MESHSET_TRIMESH >> 14];
		}

		for (size_t i = 0; i < PolyBuffDraw_NoVertexColor.size(); ++i)
		{
			auto& a = PolyBuffDraw_NoVertexColor[i];
			a[NJD_MESHSET_N >> 14] = a[NJD_MESHSET_TRIMESH >> 14];
		}
	}

	if (IsGamePatchEnabled("PixelOffSetFix"))
	{
		// Replaces half-pixel offset addition with subtraction
		WriteData((uint8_t*)0x0077DE1E, (uint8_t)0x25); // njDrawQuadTextureEx
		WriteData((uint8_t*)0x0077DE33, (uint8_t)0x25); // njDrawQuadTextureEx
		WriteData((uint8_t*)0x0078E822, (uint8_t)0x25); // DrawRect_TextureVertexTriangleStrip
		WriteData((uint8_t*)0x0078E83C, (uint8_t)0x25); // DrawRect_TextureVertexTriangleStrip
		WriteData((uint8_t*)0x0078E991, (uint8_t)0x25); // njDrawTriangle2D_List
		WriteData((uint8_t*)0x0078E9AE, (uint8_t)0x25); // njDrawTriangle2D_List
		WriteData((uint8_t*)0x0078EA41, (uint8_t)0x25); // Direct3D_DrawTriangleFan2D
		WriteData((uint8_t*)0x0078EA5E, (uint8_t)0x25); // Direct3D_DrawTriangleFan2D
		WriteData((uint8_t*)0x0078EAE1, (uint8_t)0x25); // njDrawTriangle2D_Strip
		WriteData((uint8_t*)0x0078EAFE, (uint8_t)0x25); // njDrawTriangle2D_Strip
		WriteData((uint8_t*)0x0077DBFC, (uint8_t)0x25); // njDrawPolygon
		WriteData((uint8_t*)0x0077DC16, (uint8_t)0x25); // njDrawPolygon
		WriteData((uint8_t*)0x0078E8F1, (uint8_t)0x25); // njDrawLine2D_Direct3D
		WriteData((uint8_t*)0x0078E90E, (uint8_t)0x25); // njDrawLine2D_Direct3D
	}

	if (IsGamePatchEnabled("ChaoPanelFix"))
	{
		// Chao stat panel screen dimensions fix
		WriteData((float**)0x007377FE, (float*)&_nj_screen_.w);
	}

	if (IsGamePatchEnabled("LightFix"))
	{
		// Fix light incorrectly being applied on LandTables
		WriteCall(reinterpret_cast<void*>(0x0043A6D5), FixLandTableLightType);

		// Enable light type preservation in the draw queue
		WriteCall(reinterpret_cast<void*>(0x004088B1), PreserveLightType);

		// Do not reset light type for queued models
		WriteData<2>(reinterpret_cast<void*>(0x004088A6), 0x90i8);
	}

	if (IsGamePatchEnabled("NodeLimit"))
		IncreaseNodeLimit();
	
	if (IsGamePatchEnabled("ChunkSpecularFix"))
		ChunkSpecularFix_Init();

	if (IsGamePatchEnabled("KillGBIX"))
		Init_NOGbixHack();
}

static vector<string>& split(const string& s, char delim, vector<string>& elems)
{
	std::stringstream ss(s);
	string item;

	while (std::getline(ss, item, delim))
	{
		elems.push_back(item);
	}

	return elems;
}

static vector<string> split(const string& s, char delim)
{
	vector<string> elems;
	split(s, delim, elems);
	return elems;
}

static string trim(const string& s)
{
	auto st = s.find_first_not_of(' ');

	if (st == string::npos)
	{
		st = 0;
	}

	auto ed = s.find_last_not_of(' ');

	if (ed == string::npos)
	{
		ed = s.size() - 1;
	}

	return s.substr(st, (ed + 1) - st);
}

static const unordered_map<string, uint8_t> charnamemap = {
	{ "sonic",    Characters_Sonic },
	{ "eggman",   Characters_Eggman },
	{ "tails",    Characters_Tails },
	{ "knuckles", Characters_Knuckles },
	{ "tikal",    Characters_Tikal },
	{ "amy",      Characters_Amy },
	{ "gamma",    Characters_Gamma },
	{ "big",      Characters_Big }
};

static uint8_t ParseCharacter(const string& str)
{
	string str2 = trim(str);
	transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
	auto ch = charnamemap.find(str2);
	if (ch != charnamemap.end())
		return ch->second;
	else
		return (uint8_t)strtol(str.c_str(), nullptr, 10);
}
extern void RegisterCharacterWelds(const uint8_t character, const char* iniPath);

// Console handler to properly shut down the game when the console window is enabled (since the console closes first)
BOOL WINAPI ConsoleHandler(DWORD dwType)
{
	switch (dwType)
	{
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		OnExit(0, 0, 0);
		PostQuitMessage(0);
		return TRUE;
	default:
		break;
	}
	return FALSE;
}


static void Mod_CheckAndReplaceFiles(const string mod_dirA, const uint16_t i)
{
	const string modSysDirA = mod_dirA + "\\system";
	if (DirectoryExists(modSysDirA))
		sadx_fileMap.scanFolder(modSysDirA, i);

	const string modTexDir = mod_dirA + "\\textures";
	if (DirectoryExists(modTexDir))
		sadx_fileMap.scanTextureFolder(modTexDir, i);

	const string modRepTexDir = mod_dirA + "\\replacetex";
	if (DirectoryExists(modRepTexDir))
		ScanTextureReplaceFolder(modRepTexDir, i);
}

//todo convert to wstring
static string _mainsavepath, _chaosavepath;
static bool saveRedirectFound, chaosaveredirectFound = false;
static void HandleModIniContent(IniFile* ini_mod, const IniGroup* const modinfo, const wstring& mod_dir, const string mod_dirA)
{
	// Check if the mod has EXE data replacements.
	if (modinfo->hasKeyNonEmpty("EXEData"))
	{
		wchar_t filename[MAX_PATH];
		swprintf(filename, LengthOfArray(filename), L"%s\\%s", mod_dir.c_str(), 
			modinfo->getWString("EXEData").c_str());

		ProcessEXEData(filename, mod_dir);
	}

	// Check if the mod has DLL data replacements.
	for (unsigned int j = 0; j < LengthOfArray(dlldatakeys); j++)
	{
		if (modinfo->hasKeyNonEmpty(dlldatakeys[j]))
		{
			wchar_t filename[MAX_PATH];
			swprintf(filename, LengthOfArray(filename), L"%s\\%s",
				mod_dir.c_str(), modinfo->getWString(dlldatakeys[j]).c_str());

			ProcessDLLData(filename, mod_dir);
		}
	}

	if (ini_mod->hasGroup("CharacterWelds"))
	{
		const IniGroup* group = ini_mod->getGroup("CharacterWelds");
		auto data = group->data();
		for (const auto& iter : *data)
			RegisterCharacterWelds(ParseCharacter(iter.first), (mod_dirA + "\\" + iter.second).c_str());
	}

	if (modinfo->getBool("RedirectMainSave") && !saveRedirectFound)
	{
		_mainsavepath = mod_dirA + "\\SAVEDATA";
		if (DirectoryExists(_mainsavepath))
			saveRedirectFound = true;
	}

	if (modinfo->getBool("RedirectChaoSave") && !chaosaveredirectFound)
	{
		_chaosavepath = mod_dirA + "\\SAVEDATA";

		if (DirectoryExists(_chaosavepath))
			chaosaveredirectFound = true;
	}

	const wstring borderPath = mod_dir + L'\\' + modinfo->getWString("BorderImage");
	if (modinfo->hasKeyNonEmpty("BorderImage") && FileExists(borderPath))
		borderimage = borderPath;


	if (modinfo->getBool("SetExeIcon"))
	{
		const wstring mod_icon = mod_dir + L"\\mod.ico";

		if (FileExists(mod_icon))
		{
			iconPathName = mod_icon.c_str();
			PrintDebug("Setting icon from mod folder: %s\n", mod_dir.c_str());
		}
	}
}

static void ModIniProcessFilesCheck(IniFile* ini_mod, const int i, unordered_map<string, string>& filereplaces, vector<std::pair<string, string>>& fileswaps)
{

	if (ini_mod->hasGroup("IgnoreFiles"))
	{
		const IniGroup* group = ini_mod->getGroup("IgnoreFiles");
		auto data = group->data();
		for (const auto& iter : *data)
		{
			sadx_fileMap.addIgnoreFile(iter.first, i);
			PrintDebug("Ignored file: %s\n", iter.first.c_str());
		}
	}

	if (ini_mod->hasGroup("ReplaceFiles"))
	{
		const IniGroup* group = ini_mod->getGroup("ReplaceFiles");
		auto data = group->data();
		for (const auto& iter : *data)
		{
			filereplaces[FileMap::normalizePath(iter.first)] =
				FileMap::normalizePath(iter.second);
		}
	}

	if (ini_mod->hasGroup("SwapFiles"))
	{
		const IniGroup* group = ini_mod->getGroup("SwapFiles");
		auto data = group->data();
		for (const auto& iter : *data)
		{
			fileswaps.emplace_back(FileMap::normalizePath(iter.first),
				FileMap::normalizePath(iter.second));
		}
	}
}

static void HandleRedirectSave()
{
	if (!_mainsavepath.empty())
	{
		if (!saveRedirectFound)
			_mkdir(_mainsavepath.c_str());

		char* buf = new char[_mainsavepath.size() + 1];
		strncpy(buf, _mainsavepath.c_str(), _mainsavepath.size() + 1);
		mainsavepath = buf;
		string tmp = "./" + _mainsavepath + "/";
		WriteData((char*)0x42213D, (char)(tmp.size() + 1)); // Write
		buf = new char[tmp.size() + 1];
		strncpy(buf, tmp.c_str(), tmp.size() + 1);
		WriteData((char**)0x422020, buf); // Write
		tmp = "./" + _mainsavepath + "/%s";
		buf = new char[tmp.size() + 1];
		strncpy(buf, tmp.c_str(), tmp.size() + 1);
		WriteData((char**)0x421E4E, buf); // Load
		WriteData((char**)0x421E6A, buf); // Load
		WriteData((char**)0x421F07, buf); // Delete
		WriteData((char**)0x42214E, buf); // Write
		WriteData((char**)0x5050E5, buf); // Count
		WriteData((char**)0x5051ED, buf); // Count
		tmp = "./" + _mainsavepath + "/SonicDX%02d.snc";
		WriteData((char*)0x422064, (char)(tmp.size() - 1)); // Write
		buf = new char[tmp.size() + 1];
		strncpy(buf, tmp.c_str(), tmp.size() + 1);
		WriteData((char**)0x42210F, buf); // Write
		tmp = "./" + _mainsavepath + "/SonicDX??.snc";
		buf = new char[tmp.size() + 1];
		strncpy(buf, tmp.c_str(), tmp.size() + 1);
		WriteData((char**)0x5050AB, buf); // Count
	}

	if (!_chaosavepath.empty())
	{
		if (!chaosaveredirectFound)
			_mkdir(_chaosavepath.c_str());

		char* buf = new char[_chaosavepath.size() + 1];
		strncpy(buf, _chaosavepath.c_str(), _chaosavepath.size() + 1);
		chaosavepath = buf;
		string tmp = "./" + _chaosavepath + "/SONICADVENTURE_DX_CHAOGARDEN.snc";
		buf = new char[tmp.size() + 1];
		strncpy(buf, tmp.c_str(), tmp.size() + 1);
		WriteData((char**)0x7163EF, buf); // ALMC_Read
		WriteData((char**)0x71AA6F, buf); // al_confirmsave
		WriteData((char**)0x71ACDB, buf); // al_confirmload
		WriteData((char**)0x71ADC5, buf); // al_confirmload
	}
}

std::vector<Mod> modlist;

static void SetManagerConfigFolder(wstring& exepath, wstring& appPath, wstring& extLibPath)
{
	// Get path for Mod Loader settings and libraries, normally located in 'Sonic Adventure DX\mods\.modloader'

	appPath = exepath + L"\\mods\\.modloader\\";
	extLibPath = appPath + L"extlib\\";
	wstring profilesPath = appPath + L"profiles\\Profiles.json";

	// Success
	if (Exists(profilesPath))
		return;

	// If Profiles.json isn't found, assume the old paths system
	else
	{
		// Check 'Sonic Adventure DX\SAManager' (portable mode) first
		wstring checkProfilesPath = exepath + L"\\SAManager\\SADX\\Profiles.json";
		if (Exists(checkProfilesPath))
		{
			appPath = exepath + L"\\SAManager\\";
			extLibPath = appPath + L"extlib\\";
			profilesPath = appPath + L"SADX\\Profiles.json";
			return;
		}
		// If 'checkProfilesPath' doesn't exist either, assume the settings are in 'AppData\Local\SAManager'
		else
		{
			WCHAR appDataLocalBuf[MAX_PATH];
			// Get the LocalAppData folder and check if it has the profiles json
			if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataLocalBuf)))
			{
				wstring appDataLocalPath(appDataLocalBuf);
				checkProfilesPath = appDataLocalPath + L"\\SAManager\\SADX\\Profiles.json";
				if (Exists(checkProfilesPath))
				{
					appPath = appDataLocalPath + L"\\SAManager\\";
					extLibPath = appPath + L"extlib\\";
					profilesPath = appPath + L"SADX\\Profiles.json";
					return;
				}
				// If it still can't be found, display an error message
				else
					DisplaySettingsLoadError(exepath, appPath, profilesPath);
			}
			else
			{
				MessageBox(nullptr, L"Unable to retrieve local AppData path.", L"SADX Mod Loader", MB_ICONERROR);
				OnExit(0, 0, 0);
				ExitProcess(0);
			}
		}
	}
}


static void __cdecl InitMods()
{
	// Set process DPI awareness (silent crash fix on High DPI)
	DPIFix_Init();

	// Hook present function to handle device lost/reset states
	direct3d::init();
	
	// Get full path for sonic.exe
	wchar_t pathbuf[MAX_PATH];
	GetModuleFileName(nullptr, pathbuf, MAX_PATH);

	// Paths used to get external lib location and extra config
	wstring exepath(pathbuf); // "C:\SADX" etc. without trailing slash
	wstring appPath; // "AppData\SAManager\" with trailing slash
	wstring extLibPath; // "AppData\SAManager\extlib\" with trailing slash
	wstring exefilename; // Name of the EXE file such as sonic.exe or other exe name for mods	

	// Get slash position and retrieve the EXE filename
	string::size_type slash_pos = exepath.find_last_of(L"/\\");
	if (slash_pos != string::npos)
	{
		exefilename = exepath.substr(slash_pos + 1);
		if (slash_pos > 0)
			exepath = exepath.substr(0, slash_pos);
	}

	// Convert the EXE filename to lowercase.
	transform(exefilename.begin(), exefilename.end(), exefilename.begin(), ::towlower);

	SetManagerConfigFolder(exepath, appPath, extLibPath);

	// Load Mod Loader settings
	LoadModLoaderSettings(&loaderSettings, appPath, exepath);
	// Process the main Mod Loader settings.
	if (loaderSettings.DebugConsole)
	{
		// Enable the debug console.
		// TODO: setvbuf()?
		AllocConsole();
		SetConsoleTitle(L"SADX Mod Loader output");
		freopen("CONOUT$", "wb", stdout);
		dbgConsole = true;
		// Set console handler for closing the window
		bool res = SetConsoleCtrlHandler(ConsoleHandler, true);
		if (!res)
			PrintDebug("Unable to set console handler routine");
	}

	dbgScreen = loaderSettings.DebugScreen;
	if (loaderSettings.DebugFile)
	{
		// Enable debug logging to a file.
		// dbgFile will be nullptr if the file couldn't be opened.
		dbgFile = _wfopen(L"mods\\SADXModLoader.log", L"a+");
	}

	// Is any debug method enabled?
	if (dbgConsole || dbgScreen || dbgFile)
	{
		// Set console output to UTF-8
		if (dbgConsole)
			SetConsoleOutputCP(CP_UTF8);

		WriteJump((void*)PrintDebug, (void*)SADXDebugOutput);

		PrintDebug("SADX Mod Loader v" VERSION_STRING " (API version %d), built " __TIMESTAMP__ "\n",
			ModLoaderVer);

#ifdef MODLOADER_GIT_VERSION
		PrintDebug("%s\n", MODLOADER_GIT_VERSION); // Old: PrintDebug("%s, %s\n", MODLOADER_GIT_VERSION, MODLOADER_GIT_DESCRIBE);
#endif /* MODLOADER_GIT_VERSION */
	}

	PatchWindow(loaderSettings); // override window creation function
	InitRenderBackend(loaderSettings.RenderBackend, exepath, extLibPath);

	// Other various settings
	if (IsGamePatchEnabled("DisableCDCheck"))
		WriteJump((void*)0x402621, (void*)0x402664);

	if (loaderSettings.AutoMipmap)
		mipmap::enable_auto_mipmaps();

	if (loaderSettings.Anisotropic)
	{
		// gjRender2DReset: set MIN/MIP/MAG filter to anisotropic instead of linear
		WriteData<1>((char*)0x0078B808, (char)D3DTEXF_ANISOTROPIC);
		WriteData<1>((char*)0x0078B81C, (char)D3DTEXF_ANISOTROPIC);
		WriteData<1>((char*)0x0078B830, (char)D3DTEXF_ANISOTROPIC);
	}

	// Hijack a ton of functions in SADX.
	*(void**)0x38A5DB8 = (void*)0x38A5D94; // depth buffer fix
	WriteData((short*)0x50473E, (short)0x00EB); // File select bullshit lol
	WriteCall((void*)0x402614, SetLanguage);
	WriteCall((void*)0x437547, FixEKey);

	InitAudio(extLibPath);
	WriteJump(LoadSoundList, LoadSoundList_r);
	InitGamePatches();

	texpack::init();

	// Unprotect the .rdata section.
	SetRDataWriteProtection(false);

	bool textureFilter = loaderSettings.TextureFilter;
	// Enables GUI texture filtering (D3DTEXF_POINT -> D3DTEXF_LINEAR)
	if (textureFilter == true)
	{
		WriteCall(reinterpret_cast<void*>(0x77DBCA), Direct3D_TextureFilterPoint_ForceLinear); //njDrawPolygon
		WriteCall(reinterpret_cast<void*>(0x77DC79), Direct3D_TextureFilterPoint_ForceLinear); //njDrawTextureMemList
		WriteCall(reinterpret_cast<void*>(0x77DD99), Direct3D_TextureFilterPoint_ForceLinear); //Direct3D_EnableHudAlpha
		WriteCall(reinterpret_cast<void*>(0x77DFA0), Direct3D_TextureFilterPoint_ForceLinear); //njDrawLine2D
		WriteCall(reinterpret_cast<void*>(0x77E032), Direct3D_TextureFilterPoint_ForceLinear); //njDrawCircle2D (?)
		WriteCall(reinterpret_cast<void*>(0x77EA7E), Direct3D_TextureFilterPoint_ForceLinear); //njDrawTriangle2D
		WriteCall(reinterpret_cast<void*>(0x78B074), Direct3D_TextureFilterPoint_ForceLinear); //DrawChaoHudThingB
		WriteCall(reinterpret_cast<void*>(0x78B2F4), Direct3D_TextureFilterPoint_ForceLinear); //Some other Chao thing
		WriteCall(reinterpret_cast<void*>(0x78B4C0), Direct3D_TextureFilterPoint_ForceLinear); //DisplayDebugShape_
		WriteCall(reinterpret_cast<void*>(0x793CDD), Direct3D_EnableHudAlpha_Point); // Debug text still uses point filtering
		WriteCall(reinterpret_cast<void*>(0x6FE9F8), njDrawTextureMemList_NoFilter); // Emulator plane shouldn't be filtered
	}

	direct3d::set_vsync(loaderSettings.EnableVsync);
	direct3d::set_aa(loaderSettings.Antialiasing);
	direct3d::set_af(loaderSettings.Anisotropic);

	if (loaderSettings.ScaleHud)
	{
		uiscale::initialize();
		hudscale::initialize();
	}

	int bgFill = loaderSettings.BackgroundFillMode;
	if (bgFill >= 0 && bgFill <= 3)
	{
		uiscale::bg_fill = static_cast<uiscale::FillMode>(bgFill);
		uiscale::setup_background_scale();
		bgscale::initialize();
	}

	int fmvFill = loaderSettings.FmvFillMode;
	if (fmvFill >= 0 && fmvFill <= 3)
	{
		uiscale::fmv_fill = static_cast<uiscale::FillMode>(fmvFill);
		uiscale::setup_fmv_scale();
	}
	
	if (IsGamePatchEnabled("FixVertexColorRendering"))
	{	
		polybuff::init(); // This is different from the rewrite portion of the polybuff namespace!
		polybuff::rewrite_init();
	}

	if (loaderSettings.DebugCrashLog)
		initCrashDump();

	if (IsGamePatchEnabled("MaterialColorFix"))
		MaterialColorFixes_Init();

	//init interpol fix for helperfunctions
	interpolation::init();
	sadx_fileMap.scanSoundFolder("system\\sounddata\\bgm\\wma");
	sadx_fileMap.scanSoundFolder("system\\sounddata\\voice_jp\\wma");
	sadx_fileMap.scanSoundFolder("system\\sounddata\\voice_us\\wma");

	if (IsGamePatchEnabled("Chaos2CrashFix"))
		MinorGamePatches_Init();

	// Map of files to replace.
	// This is done with a second map instead of sadx_fileMap directly
	// in order to handle multiple mods.
	unordered_map<string, string> filereplaces;
	vector<std::pair<string, string>> fileswaps;

	vector<std::pair<ModInitFunc, string>> initfuncs;
	vector<std::pair<wstring, wstring>> errors;


	// It's mod loading time!
	PrintDebug("Loading mods...\n");
	// Mod list
	for (unsigned int i = 1; i <= GetModCount(); i++)
	{
		std::string mod_fname = GetModName(i);

		int count_m = MultiByteToWideChar(CP_UTF8, 0, mod_fname.c_str(), mod_fname.length(), NULL, 0);
		std::wstring mod_fname_w(count_m, 0);
		MultiByteToWideChar(CP_UTF8, 0, mod_fname.c_str(), mod_fname.length(), &mod_fname_w[0], count_m);

		const string mod_dirA = "mods\\" + mod_fname;
		const wstring mod_dir = L"mods\\" + mod_fname_w;
		const wstring mod_inifile = mod_dir + L"\\mod.ini";
		saveRedirectFound = false;
		chaosaveredirectFound = false;

		FILE* f_mod_ini = _wfopen(mod_inifile.c_str(), L"r");

		if (!f_mod_ini)
		{
			PrintDebug("Could not open file mod.ini in \"%s\".\n", mod_dirA.c_str());
			errors.emplace_back(mod_dir, L"mod.ini missing");
			continue;
		}
		unique_ptr<IniFile> ini_mod(new IniFile(f_mod_ini));
		fclose(f_mod_ini);

		const IniGroup* const modinfo = ini_mod->getGroup("");

		const string mod_nameA = modinfo->getString("Name");
		const wstring mod_name = modinfo->getWString("Name");

		PrintDebug("%u. %s\n", i, mod_nameA.c_str());



		vector<ModDependency> moddeps;

		for (unsigned int j = 1; j <= 999; j++)
		{
			char key2[14];
			snprintf(key2, sizeof(key2), "Dependency%u", j);
			if (!modinfo->hasKey(key2))
				break;
			auto dep = split(modinfo->getString(key2), '|');
			moddeps.push_back({ strdup(dep[0].c_str()), strdup(dep[1].c_str()), strdup(dep[2].c_str()), strdup(dep[3].c_str()) });
		}

		ModDependency* deparr = new ModDependency[moddeps.size()];
		memcpy(deparr, moddeps.data(), moddeps.size() * sizeof(ModDependency));

		Mod modinf = {
			strdup(mod_nameA.c_str()),
			strdup(modinfo->getString("Author").c_str()),
			strdup(modinfo->getString("Description").c_str()),
			strdup(modinfo->getString("Version").c_str()),
			strdup(mod_dirA.c_str()),
			strdup(modinfo->getString("ModID", mod_dirA).c_str()),
			NULL,
			modinfo->getBool("RedirectMainSave"),
			modinfo->getBool("RedirectChaoSave"),
			{
				deparr,
				(int)moddeps.size()
			}
		};

		ModIniProcessFilesCheck(ini_mod.get(), i, filereplaces, fileswaps);


		// Check for SYSTEM replacements.
		// TODO: Convert to WString.
		Mod_CheckAndReplaceFiles(mod_dirA, i);

		// Check if a custom EXE is required.
		if (modinfo->hasKeyNonEmpty("EXEFile"))
		{
			wstring modexe = modinfo->getWString("EXEFile");
			transform(modexe.begin(), modexe.end(), modexe.begin(), ::towlower);

			// Are we using the correct EXE?
			if (modexe != exefilename)
			{
				wchar_t msg[4096];
				swprintf(msg, LengthOfArray(msg),
					L"Mod \"%s\" should be run from \"%s\", but you are running \"%s\".\n\n"
					L"Continue anyway?", mod_name.c_str(), modexe.c_str(), exefilename.c_str());
				if (MessageBox(nullptr, msg, L"SADX Mod Loader", MB_ICONWARNING | MB_YESNO) == IDNO)
					ExitProcess(1);
			}
		}

		// Check if the mod has a DLL file.
		if (modinfo->hasKeyNonEmpty("DLLFile"))
		{
			// Prepend the mod directory.
			// TODO: SetDllDirectory()?
			wstring dll_filename = mod_dir + L'\\' + modinfo->getWString("DLLFile");
			HMODULE module = LoadLibrary(dll_filename.c_str());

			if (module == nullptr)
			{
				DWORD error = GetLastError();
				LPWSTR buffer;
				size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPWSTR)&buffer, 0, nullptr);
				bool allocated = (size != 0);

				if (!allocated)
				{
					static const wchar_t fmterr[] = L"Unable to format error message.";
					buffer = const_cast<LPWSTR>(fmterr);
					size = LengthOfArray(fmterr) - 1;
				}
				wstring message(buffer, size);
				if (allocated)
					LocalFree(buffer);

				const string dll_filenameA = UTF16toMBS(dll_filename, CP_ACP);
				const string messageA = UTF16toMBS(message, CP_ACP);
				PrintDebug("Failed loading mod DLL \"%s\": %s\n", dll_filenameA.c_str(), messageA.c_str());
				errors.emplace_back(mod_name, L"DLL error - " + message);
			}
			else
			{
				const auto info = (const ModInfo*)GetProcAddress(module, "SADXModInfo");
				if (info)
				{
					modinf.DLLHandle = module;
					if (info->Patches)
					{
						for (int j = 0; j < info->PatchCount; j++)
							WriteData(info->Patches[j].address, info->Patches[j].data, info->Patches[j].datasize);
					}

					if (info->Jumps)
					{
						for (int j = 0; j < info->JumpCount; j++)
							WriteJump(info->Jumps[j].address, info->Jumps[j].data);
					}

					if (info->Calls)
					{
						for (int j = 0; j < info->CallCount; j++)
							WriteCall(info->Calls[j].address, info->Calls[j].data);
					}

					if (info->Pointers)
					{
						for (int j = 0; j < info->PointerCount; j++)
							WriteData((void**)info->Pointers[j].address, info->Pointers[j].data);
					}

					if (info->Init)
					{
						// TODO: Convert to Unicode later. (Will require an API bump.)
						initfuncs.emplace_back(info->Init, mod_dirA);
					}

					const auto init = (const ModInitFunc)GetProcAddress(module, "Init");

					if (init)
					{
						initfuncs.emplace_back(init, mod_dirA);
					}

					const auto* const patches = (const PatchList*)GetProcAddress(module, "Patches");

					if (patches)
					{
						for (int j = 0; j < patches->Count; j++)
						{
							WriteData(patches->Patches[j].address, patches->Patches[j].data, patches->Patches[j].datasize);
						}
					}

					const auto* const jumps = (const PointerList*)GetProcAddress(module, "Jumps");

					if (jumps)
					{
						for (int j = 0; j < jumps->Count; j++)
						{
							WriteJump(jumps->Pointers[j].address, jumps->Pointers[j].data);
						}
					}

					const auto* const calls = (const PointerList*)GetProcAddress(module, "Calls");

					if (calls)
					{
						for (int j = 0; j < calls->Count; j++)
						{
							WriteCall(calls->Pointers[j].address, calls->Pointers[j].data);
						}
					}

					const auto* const pointers = (const PointerList*)GetProcAddress(module, "Pointers");

					if (pointers)
					{
						for (int j = 0; j < pointers->Count; j++)
						{
							WriteData((void**)pointers->Pointers[j].address, pointers->Pointers[j].data);
						}
					}

					RegisterEvent(modInitEndEvents, module, "OnInitEnd");
					RegisterEvent(modFrameEvents, module, "OnFrame");
					RegisterEvent(modInputEvents, module, "OnInput");
					RegisterEvent(modControlEvents, module, "OnControl");
					RegisterEvent(modExitEvents, module, "OnExit");
					RegisterEvent(modRenderDeviceLost, module, "OnRenderDeviceLost");
					RegisterEvent(modRenderDeviceReset, module, "OnRenderDeviceReset");
					RegisterEvent(modInitGameLoopEvents, module, "OnInitGameLoop");
					RegisterEvent(onRenderSceneEnd, module, "OnRenderSceneEnd");
					RegisterEvent(onRenderSceneStart, module, "OnRenderSceneStart");

					auto customTextureLoad = reinterpret_cast<TextureLoadEvent>(GetProcAddress(module, "OnCustomTextureLoad"));
					if (customTextureLoad != nullptr)
					{
						modCustomTextureLoadEvents.push_back(customTextureLoad);
					}
				}
				else
				{
					const string dll_filenameA = UTF16toMBS(dll_filename, CP_ACP);
					PrintDebug("File \"%s\" is not a valid mod file.\n", dll_filenameA.c_str());
					errors.emplace_back(mod_name, L"Not a valid mod file.");
				}
			}
		}

		// Set global codepage overrides
		CodepageJapanese = modinfo->getInt("CodepageJapanese", 932);
		CodepageEnglish = modinfo->getInt("CodepageEnglish", 932);
		CodepageFrench = modinfo->getInt("CodepageFrench", 1252);
		CodepageGerman = modinfo->getInt("CodepageGerman", 1252);
		CodepageSpanish = modinfo->getInt("CodepageSpanish", 1252);


		HandleModIniContent(ini_mod.get(), modinfo, mod_dir, mod_dirA);

		if (modinfo->hasKeyNonEmpty("WindowTitle"))
			windowtitle = modinfo->getString("WindowTitle");

		//basic Mod Config, includes file replacement without custom code
		int dirs = ini_mod->getInt("Config", "IncludeDirCount", -1);
		if (dirs != -1)
		{
			for (uint16_t md = 0; md < dirs; md++)
			{
				auto incDirPath = ini_mod->getString("Config", "IncludeDir" + std::to_string(md));
				const string modIncDir = mod_dirA + "\\" + incDirPath;
				const wstring modIncDirW = mod_dir + L"\\" + ini_mod->getWString("Config", "IncludeDir" + std::to_string(md));
				if (DirectoryExists(modIncDir))
				{
					PrintDebug("Mod Config: use path: '%s'\n", modIncDir.c_str());
					Mod_CheckAndReplaceFiles(modIncDir, i);
					HandleModIniContent(ini_mod.get(), modinfo, modIncDirW, modIncDir);
				}
			}
		}

		modlist.push_back(modinf);
	}

	if (!loaderSettings.DisableBorderImage)
	{
		if (!FileExists(borderimage))
			borderimage = L"mods\\Border_Default.png";
		SetBorderImage(borderimage);
	}
	
	Video_Init(loaderSettings, borderimage);

	if (loaderSettings.InputMod)
		SDL2_Init(extLibPath);

	else if (IsGamePatchEnabled("XInputFix"))
		XInputFix_Init();

	if (!errors.empty())
	{
		std::wstringstream message;
		message << L"The following mods didn't load correctly:" << std::endl;

		for (auto& i : errors)
		{
			message << std::endl << i.first << ": " << i.second;
		}

		MessageBox(nullptr, message.str().c_str(), L"Mods failed to load", MB_OK | MB_ICONERROR);

		// Clear the errors vector to free memory.
		errors.clear();
		errors.shrink_to_fit();
	}

	// Replace filenames. ("ReplaceFiles")
	for (const auto& filereplace : filereplaces)
	{
		sadx_fileMap.addReplaceFile(filereplace.first, filereplace.second);
	}
	for (const auto& fileswap : fileswaps)
	{
		sadx_fileMap.swapFiles(fileswap.first, fileswap.second);
	}

	for (auto& initfunc : initfuncs)
		initfunc.first(initfunc.second.c_str(), helperFunctions);

	ProcessTestSpawn(helperFunctions);

	for (const auto& i : StartPositions)
	{
		auto poslist = &i.second;
		auto newlist = new StartPosition[poslist->size() + 1];
		StartPosition* cur = newlist;

		for (const auto& j : *poslist)
		{
			*cur++ = j.second;
		}

		cur->LevelID = LevelIDs_Invalid;
		switch (i.first)
		{
		default:
		case Characters_Sonic:
			WriteData((StartPosition**)0x41491E, newlist);
			break;
		case Characters_Tails:
			WriteData((StartPosition**)0x414925, newlist);
			break;
		case Characters_Knuckles:
			WriteData((StartPosition**)0x41492C, newlist);
			break;
		case Characters_Amy:
			WriteData((StartPosition**)0x41493A, newlist);
			break;
		case Characters_Gamma:
			WriteData((StartPosition**)0x414941, newlist);
			break;
		case Characters_Big:
			WriteData((StartPosition**)0x414933, newlist);
			break;
		}
	}
	StartPositions.clear();

	for (const auto& i : FieldStartPositions)
	{
		auto poslist = &i.second;
		auto newlist = new FieldStartPosition[poslist->size() + 1];
		FieldStartPosition* cur = newlist;

		for (const auto& j : *poslist)
		{
			*cur++ = j.second;
		}

		cur->LevelID = LevelIDs_Invalid;
		StartPosList_FieldReturn[i.first] = newlist;
	}
	FieldStartPositions.clear();

	if (PathsInitialized)
	{
		auto newlist = new PathDataPtr[Paths.size() + 1];
		PathDataPtr* cur = newlist;

		for (const auto& path : Paths)
		{
			*cur++ = path.second;
		}

		cur->LevelAct = 0xFFFF;
		WriteData((PathDataPtr**)0x49C1A1, newlist);
		WriteData((PathDataPtr**)0x49C1AF, newlist);
	}
	Paths.clear();

	for (const auto& pvm : CharacterPVMs)
	{
		const vector<PVMEntry>* pvmlist = &pvm.second;
		auto size = pvmlist->size();
		auto newlist = new PVMEntry[size + 1];
		memcpy(newlist, pvmlist->data(), sizeof(PVMEntry) * size);
		newlist[size].TexList = nullptr;
		CharacterPVMEntries[pvm.first] = newlist;
	}
	CharacterPVMs.clear();

	if (CommonObjectPVMsInitialized)
	{
		auto size = CommonObjectPVMs.size();
		auto newlist = new PVMEntry[size + 1];
		//PVMEntry *cur = newlist;
		memcpy(newlist, CommonObjectPVMs.data(), sizeof(PVMEntry) * size);
		newlist[size].TexList = nullptr;
		TexLists_ObjRegular[0] = newlist;
		TexLists_ObjRegular[1] = newlist;
	}
	CommonObjectPVMs.clear();

	for (const auto& level : _TrialLevels)
	{
		const vector<TrialLevelListEntry>* levellist = &level.second;
		auto size = levellist->size();
		auto newlist = new TrialLevelListEntry[size];
		memcpy(newlist, levellist->data(), sizeof(TrialLevelListEntry) * size);
		TrialLevels[level.first].Levels = newlist;
		TrialLevels[level.first].Count = size;
	}
	_TrialLevels.clear();

	for (const auto& subgame : _TrialSubgames)
	{
		const vector<TrialLevelListEntry>* levellist = &subgame.second;
		auto size = levellist->size();
		auto newlist = new TrialLevelListEntry[size];
		memcpy(newlist, levellist->data(), sizeof(TrialLevelListEntry) * size);
		TrialSubgames[subgame.first].Levels = newlist;
		TrialSubgames[subgame.first].Count = size;
	}
	_TrialSubgames.clear();

	HandleRedirectSave();

	if (!windowtitle.empty())
	{
		char* buf = new char[windowtitle.size() + 1];
		strncpy(buf, windowtitle.c_str(), windowtitle.size() + 1);
		*(char**)0x892944 = buf;
	}

	if (!_MusicList.empty())
	{
		auto size = _MusicList.size();
		auto newlist = new MusicInfo[size];
		memcpy(newlist, _MusicList.data(), sizeof(MusicInfo) * size);
		WriteData((const char***)0x425424, &newlist->Name);
		WriteData((int**)0x425442, &newlist->Loop);
		WriteData((const char***)0x425460, &newlist->Name);
		WriteData((int**)0x42547E, &newlist->Loop);
	}
	_MusicList.clear();

	ProcessVoiceDurationRegisters();

	RaiseEvents(modInitEndEvents);
	PrintDebug("Finished loading mods\n");
#ifdef _DEBUG
	ListGamePatches();
#endif

	// Check for patch-type codes.
	ifstream patches_str("mods\\Patches.dat", ifstream::binary);
	if (patches_str.is_open())
	{
		CodeParser patchParser;
		static const char codemagic[6] = { 'c', 'o', 'd', 'e', 'v', '5' };
		char buf[sizeof(codemagic)];
		patches_str.read(buf, sizeof(buf));
		if (!memcmp(buf, codemagic, sizeof(codemagic)))
		{
			int codecount_header;
			patches_str.read((char*)&codecount_header, sizeof(codecount_header));
			PrintDebug("Loading %d patch-type codes...\n", codecount_header);
			patches_str.seekg(0);
			int codecount = patchParser.readCodes(patches_str);
			if (codecount >= 0)
			{
				PrintDebug("Loaded %d patch-type codes.\n", codecount);
				patchParser.processCodeList();
			}
			else
			{
				PrintDebug("ERROR loading patch-type codes: ");
				switch (codecount)
				{
				case -EINVAL:
					PrintDebug("Patch-type code file is not in the correct format.\n");
					break;
				default:
					PrintDebug("%s\n", strerror(-codecount));
					break;
				}
			}
		}
		else
		{
			PrintDebug("Patch-type code file is not in the correct format.\n");
		}
		patches_str.close();
	}


	// Check for codes.
	ifstream codes_str("mods\\Codes.dat", ifstream::binary);

	if (codes_str.is_open())
	{
		static const char codemagic[6] = { 'c', 'o', 'd', 'e', 'v', '5' };
		char buf[sizeof(codemagic)];
		codes_str.read(buf, sizeof(buf));

		if (!memcmp(buf, codemagic, sizeof(codemagic)))
		{
			int codecount_header;
			codes_str.read((char*)&codecount_header, sizeof(codecount_header));
			PrintDebug("Loading %d codes...\n", codecount_header);
			codes_str.seekg(0);
			int codecount = codeParser.readCodes(codes_str);

			if (codecount >= 0)
			{
				PrintDebug("Loaded %d codes.\n", codecount);
				codeParser.processCodeList();
			}
			else
			{
				PrintDebug("ERROR loading codes: ");
				switch (codecount)
				{
				case -EINVAL:
					PrintDebug("Code file is not in the correct format.\n");
					break;
				default:
					PrintDebug("%s\n", strerror(-codecount));
					break;
				}
			}
		}
		else
		{
			PrintDebug("Code file is not in the correct format.\n");
		}

		codes_str.close();
	}

	// Sets up code/event handling
	WriteJump((void*)0x00426063, (void*)ProcessCodes);
	WriteJump((void*)0x0040FDB3, (void*)OnInput);			// End of first chunk
	WriteJump((void*)0x0042F1C5, (void*)OnInput_MidJump);	// Cutscene stuff - Untested. Couldn't trigger ingame.
	WriteJump((void*)0x0042F1E9, (void*)OnInput);			// Cutscene stuff
	WriteJump((void*)0x0040FF00, (void*)OnControl);

	// Remove "Tails Adventure" gray filter
	WriteData(reinterpret_cast<float*>(0x87CBA8), 0.0f);
	WriteData(reinterpret_cast<float*>(0x87CBAC), 0.0f);

	ApplyTestSpawn();
	GVR_Init();

	if (IsGamePatchEnabled("ExtendedSaveSupport"))
		ExtendedSaveSupport_Init();

	if (IsGamePatchEnabled("CrashGuard"))
		CrashGuard_Init();

	if (FileExists(L"sonic.ico"))
	{
		iconPathName = L"sonic.ico";
		PrintDebug("Setting icon from sonic.ico\n");
	}
}

DataPointer(HMODULE, chrmodelshandle, 0x3AB9170);

void EventGameLoopInit()
{
	RaiseEvents(modInitGameLoopEvents);
	if (IsGamePatchEnabled("CrashGuard"))
		InitDefaultTexture();
}

static void __cdecl LoadChrmodels()
{
	chrmodelshandle = LoadLibrary(L".\\system\\CHRMODELS_orig.dll");
	if (!chrmodelshandle)
	{
		MessageBox(nullptr, L"CHRMODELS_orig.dll could not be loaded!\n\n"
			L"The Mod Loader has not been installed correctly.\nReinstall the game and the Mod Loader and try again.",
			L"SADX Mod Loader", MB_ICONERROR);
		ExitProcess(1);
	}
	SetDLLHandle(L"CHRMODELS", chrmodelshandle);
	SetDLLHandle(L"CHRMODELS_orig", chrmodelshandle);
	if (GetProcAddress(GetModuleHandle(L"CHRMODELS_orig"), "___SONIC_OBJECTS") == nullptr)
	{
		MessageBoxW(nullptr, L"Critical error: data is missing in CHRMODELS_orig.DLL. Please reinstall the game and try again.", L"SADX Mod Loader Error", MB_ICONERROR | MB_OK);
		OnExit(0, 0, 0);
		ExitProcess(1);
	}
	SetDLLHandle(L"CHAOSTGGARDEN02MR_DAYTIME", LoadLibrary(L".\\system\\CHAOSTGGARDEN02MR_DAYTIME.DLL"));
	SetDLLHandle(L"CHAOSTGGARDEN02MR_EVENING", LoadLibrary(L".\\system\\CHAOSTGGARDEN02MR_EVENING.DLL"));
	SetDLLHandle(L"CHAOSTGGARDEN02MR_NIGHT", LoadLibrary(L".\\system\\CHAOSTGGARDEN02MR_NIGHT.DLL"));
	WriteCall((void*)0x402513, (void*)InitMods);
	WriteCall((void*)0x40C045, (void*)EventGameLoopInit);
}

/**
 * DLL entry point.
 * @param hinstDll DLL instance.
 * @param fdwReason Reason for calling DllMain.
 * @param lpvReserved Reserved.
 */
BOOL APIENTRY DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	// US version check.
	static const void* const verchk_addr = (void*)0x789E50;
	static const uint8_t verchk_data[] = { 0x83, 0xEC, 0x28, 0x57, 0x33 };

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		g_hinstDll = hinstDll;
		HookCreateFileA();

		// Make sure this is the correct version of SADX.
		if (memcmp(verchk_data, verchk_addr, sizeof(verchk_data)) != 0)
		{
			ShowNon2004USError();
			ExitProcess(1);
		}

		WriteData((unsigned char*)0x401AE1, (unsigned char)0x90);
		WriteCall((void*)0x401AE2, (void*)LoadChrmodels);

#if !defined(_MSC_VER) || defined(_DLL)
		// Disable thread library calls, since we don't
		// care about thread attachments.
		// NOTE: On MSVC, don't do this if using the static CRT.
		// Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/ms682579(v=vs.85).aspx
		DisableThreadLibraryCalls(hinstDll);
#endif /* !defined(_MSC_VER) || defined(_DLL) */
		break;

	case DLL_PROCESS_DETACH:
		// Make sure the log file is closed.
		if (dbgFile)
		{
			fclose(dbgFile);
			dbgFile = nullptr;
		}
		break;

	default:
		break;
	}

	return TRUE;
}
