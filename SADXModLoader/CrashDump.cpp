#include "stdafx.h"
#include <dbghelp.h> 
#include <windows.h>
#include <direct.h>
#include <Psapi.h>
#include <shlwapi.h>
#include <time.h>
#include "util.h"
#include <filesystem>  // For std::filesystem on C++17 and above
#include <iostream>
#include <iomanip>
#include "git.h"
#include <ShlObj.h>
#include <shellapi.h>
#include <locale>
#include <sstream>
#include <ctime>

using namespace std;
extern HelperFunctions helperFunctions;
extern bool useTestSpawn;

static const unordered_map<int16_t, wstring> level_map
{
	{ 0, L"HH"},
	{ 1, L"EC"},
	{ 2, L"WV"},
	{ 3, L"TP"},
	{ 4, L"SH"},
	{ 5, L"RM"},
	{ 6, L"SD"},
	{ 7, L"LW"},
	{ 8, L"IC"},
	{ 9, L"CAS"},
	{ 10, L"FE"},
	{ 12, L"HS"},
	{ 15, L"C0"},
	{ 16, L"C2"},
	{ 17, L"C4"},
	{ 18, L"C6"},
	{ 19, L"PC"},
	{ 20, L"EH"},
	{ 21, L"EW"},
	{ 22, L"EV"},
	{ 23, L"ZERO"},
	{24, L"E101"},
	{25, L"E101R"},
	{26, L"SS"},
	{29, L"ECO"},
	{32, L"ECI"},
	{33, L"MR"},
	{34, L"PAST"},
	{35, L"TC"},
	{36, L"SC1"},
	{37, L"SC2"},
	{38, L"SH"},
	{39, L"SGarden"},
	{40, L"EGarden"},
	{41, L"MGarden"},
	{42, L"CRace"}
};

static const unordered_map<uint8_t, wstring> char_map
{
	{ 0, L"Sonic"},
	{ 1, L"Eggman"},
	{ 2, L"Tails"},
	{ 3, L"Knuckles"},
	{ 4, L"Tikal"},
	{ 5, L"Amy"},
	{ 6, L"E102"},
	{ 7, L"Big"},
};

static wstring GetLevelName()
{
	if (GameState != 0)
	{
		auto it = level_map.find(CurrentLevel);
		if (it != level_map.cend()) 
		{
			return it->second;
		}
		else
		{
			return L"Unknown";
		}
	}

	return L"";
}

static wstring GetCharName()
{
	if (GameState > 0)
	{
		auto it = char_map.find(CurrentCharacter);
		if (it != char_map.cend()) 
		{
			if (MetalSonicFlag && CurrentCharacter == Characters_Sonic)
				return L"Metal";
			else
				return it->second;
		}
		else
		{
			return L"Unknown";
		}
	}

	return L"";
}

const string texCrashMsg = "Texture error: the game failed to apply one or more textures. This could be a mod conflict.\nIf you are making a mod, make sure all your textures are loaded.";
const string charCrashMsg = "Character Crash: The game crashed in one of the character's main function.\nYou most likely have a mod order conflict.\nMods that edit gameplay should be loaded last.";
const string weldCrashMsg = "Welds Crash: The game crashed in the function that processes vertex welding.\nMake sure your character welds are correct and try again.";

static const unordered_map<intptr_t, string> crashes_addresses_map = 
{
	{ 0x78CF24, texCrashMsg},
	{ 0x78D149, texCrashMsg },
	{ 0x56FC1C, texCrashMsg }, // E101R init
	{ 0x7B293A85, "DirectX error: You most likely reached a tex ID out of range."},
	{ 0x40E380, "Font loading error: the game failed to load FONTDATA files.\nCheck game health in the Mod Manager and make sure the game has no missing files."},
	{ 0x434614, "Camera error: the game failed to load a cam file for the stage."},
	{ 0x787148, "Model error: The game crashed on the eval flag check.\nIf you are making a level mod, make sure all your meshes have the flag \"Skip Children\" checked."},
	{ 0x644EB1, "Write file error: The game crashed trying to write a save file."},
	{ 0x78CDC9, "stApplyPalette error: The game crashed trying to set a texture.\nThis can happen when there are multiple mods replacing the same character model, or when a mod doesn't load textures properly."}
};

struct addressRange
{
	intptr_t start = 0;
	intptr_t end = 0;
	string crashMsg = "";
};

static const addressRange Range_Addresses_list[] =
{
	{ (intptr_t)SonicTheHedgehog, 0x49BFBB, charCrashMsg },
	{ (intptr_t)Eggman, 0x7B52DE, charCrashMsg} ,
	{ (intptr_t)MilesTalesPrower, 0x4624CB, charCrashMsg },
	{ (intptr_t)KnucklesTheEchidna, 0x47B504, charCrashMsg },
	{ (intptr_t)Tikal, 0x7B43E0, charCrashMsg },
	{ (intptr_t)AmyRose, 0x48B5B6, charCrashMsg },
	{ (intptr_t)E102, 0x4841CA, charCrashMsg },
	{ (intptr_t)BigTheCat, 0x491438, charCrashMsg },
	{ (intptr_t)ProcessVertexWelds, 0x440342, weldCrashMsg },
};

static const string GetRangeAddressesCrash(const intptr_t address)
{
	for (uint8_t i = 0; i < LengthOfArray(Range_Addresses_list); i++)
	{
		if (address >= Range_Addresses_list[i].start && address <= Range_Addresses_list[i].end)
		{
			return Range_Addresses_list[i].crashMsg;
		}
	}

	return "NULL";
}


static string getErrorMSG(intptr_t address)
{
	if ((crashes_addresses_map.find(address) == crashes_addresses_map.end()))
	{
		return "NULL";
	}

	return crashes_addresses_map.find(address)->second; //return a custom error message if the address is known
}

static void SetErrorMessage(string& fullMsg, const string address, const string dllName, const intptr_t crashID, const string what)
{
	if (crashID == -1 && address == "")
	{
		fullMsg = "SADX has crashed, but the crash information couldn't be obtained.\n\n";
		fullMsg += "If you still want to report this crash, please share your mod list and the 'extra_info' text file from your game's CrashDumps folder.\n";
		if (what != "")
			fullMsg += "Advanced Error:\n" + what;

		return;
	}

	string errorCommon = getErrorMSG(crashID); //get error message if the crash address is common
	fullMsg = "SADX has crashed at " + address + " (" + dllName + ").\n";

	if (errorCommon != "NULL")
	{
		fullMsg += "\n" + errorCommon + "\n"; //add the common error message if it exists
	}
	else
	{
		//if the crash isn't in the list, check if it's a common crash from addresses from a whole function...
		auto charcrash = GetRangeAddressesCrash(crashID);

		if (charcrash != "NULL")
		{
			fullMsg += "\n" + charcrash + "\n";
		}
	}

	if (dllName.find("chrmodels") != std::string::npos)
		fullMsg += "\nThis is a crash in the Mod Loader, if you are making a mod, there is a chance that you did something very wrong.";

	fullMsg += "\nA crash dump and a mod list have been added to your game's CrashDumps folder.\n\nIf you want to report this crash, please include the dump (.dmp file) and the mod list (.json file) in your report.\n";
}

//extra info
static void CreateExtraInfoFile(wstring curCrashFolder, wstring timeStr, string fullMsg)
{
	std::wstring chara = GetCharName();
	std::wstring lvl = GetLevelName();

	std::wstring filename = timeStr + L"_extra_info_" + chara + L"_" + lvl + L".txt";
	std::filesystem::path directory = curCrashFolder;
	std::filesystem::path destinationPath = directory / filename;

	// Create and open the file
	std::ofstream outfile(destinationPath);

	if (!outfile)
		return;

	outfile << "Mod Loader Git Version: " << MODLOADER_GIT_TMP_SHAID  << std::endl;
	outfile << "Mod Loader API Version: " << ModLoaderVer << std::endl;
	outfile << "Use TestSpawn: " << (useTestSpawn ? "Yes" : "No") << std::endl;
	outfile << "\n" << std::endl;
	outfile << "[Full Message]\n" << fullMsg << "\n" << std::endl;
	// Write information to the file
	outfile << "[Mods]" << std::endl;

	auto mod = helperFunctions.Mods;

	if (mod)
	{
		for (uint16_t i = 0; i < mod->size(); i++)
			outfile << mod->at(i).Name << " - " << mod->at(i).Version << std::endl;
	}

	outfile << "\n" << std::endl;
	outfile << "[Game Values]" << std::endl;
	outfile << "GameMode: " << GameMode << std::endl;
	outfile << "GameState: " << GameState << std::endl;
	outfile << "Current Level: " << CurrentLevel << std::endl;
	outfile << "Current Act: " << CurrentAct << std::endl;
	outfile << "Last Level: " << LastLevel << std::endl;
	outfile << "Last Act: " << LastAct << std::endl;
	outfile << "Current Character: " << CurrentCharacter << std::endl;
	if (EV_MainThread_ptr)
		outfile << "Event: " << evInfo.no << std::endl;
	outfile << "\n" << std::endl;

	if (playertwp[0])
	{
		for (uint8_t i = 0; i < 8; i++)
		{
			auto p = playertwp[i];
			auto pwp = playerpwp[i];
			if (p)
			{
				if (i > 0)
					outfile << "\n" << std::endl;
				else
					outfile << "[Players]" << std::endl;

				outfile << "Player ID: " << static_cast<int>(p->counter.b[0]) << std::endl;
				outfile << "Char ID: " << static_cast<int>(p->counter.b[1]) << std::endl;
				outfile << "Action: " << static_cast<int>(p->mode) << std::endl;
				outfile << "Next Action: " << static_cast<int>(p->smode) << std::endl;
				outfile << "Position: " << p->pos.x << ", " << p->pos.y << ", " << p->pos.z << std::endl;
				outfile << "Rotation: " << p->ang.x << ", " << p->ang.y << ", " << p->ang.z << std::endl;
				outfile << "Is on Ground: " << ((p->flag & 3) ? "Yes" : "No") << std::endl;
				outfile << "Is Holding Obj: " << ((p->flag & Status_HoldObject) ? "Yes" : "No") << std::endl;
				outfile << "Is on Path: " << ((p->flag & Status_OnPath) ? "Yes" : "No") << std::endl;
			}

			if (pwp)
			{
				outfile << "Is in Super Form: " << ((pwp->equipment & Upgrades_SuperSonic) ? "Yes" : "No") << std::endl;
				outfile << "Anim: " << pwp->mj.reqaction << std::endl;
				outfile << "Frame: " << pwp->mj.nframe << std::endl;
				outfile << "Next Anim: " << pwp->mj.plactptr[pwp->mj.reqaction].next << std::endl;
			}
			else
			{
				if (i == 0)
					outfile << "playerpwp pointer was null.";
			}
		}
	}
	else
	{
		outfile << "playertwp pointer was null.";
	}


	outfile << "\n" << std::endl;


	if (pCurSequence)
	{
		outfile << "[Story]" << std::endl;
		outfile << "Time: " << static_cast<int>(pCurSequence->time) << std::endl;
		outfile << "Emerald: " << static_cast<int>(pCurSequence->emerald) << std::endl;
		outfile << "Seq no: " << pCurSequence->seqno << std::endl;
		outfile << "Sec: " << pCurSequence->sec << std::endl;
		outfile << "Next Sec: " << pCurSequence->nextsec << std::endl;
		outfile << "Stage and Act: " << pCurSequence->stage << std::endl;
		outfile << "Destination: " << pCurSequence->destination << std::endl;
	}

	// Close the file
	outfile.close();
}

static void CopyAndRename_SADXLoaderProfile(std::wstring crashDumpFolder, std::wstring timeStr)
{
	std::filesystem::path directory = crashDumpFolder;
	std::filesystem::path sourcePath = currentProfilePath;

	std::wstring chara = GetCharName();
	std::wstring lvl = GetLevelName();
	std::wstring fileName = timeStr + L"_ModList_" + chara + L"_" + lvl + L".json";

	std::filesystem::path destinationPath = directory / (fileName);

	try 
	{
		std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::overwrite_existing);
		PrintDebug("CrashDump: Successfully copied and renamed SADX Profile.\n");
	}
	catch (const std::exception& e) {
		PrintDebug("CrashDump: Failed to copy and rename SADX Profile. Error: %s\n", e.what());
	}
}

static std::wstring getCurrentDate() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	std::tm* ptm = std::localtime(&now_c);

	// Format date with '_' separators
	std::wostringstream ss;
	ss << std::setfill(L'0') << std::setw(2) << ptm->tm_mday << L"_"
		<< std::setfill(L'0') << std::setw(2) << (ptm->tm_mon + 1) << L"_"
		<< (ptm->tm_year + 1900);

	return ss.str();
}

static std::wstring getCurrentTime() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	std::tm* ptm = std::localtime(&now_c);

	// Format time with '_' separators
	std::wostringstream ss;
	ss << std::setfill(L'0') << std::setw(2) << ptm->tm_hour << L"_"
		<< std::setfill(L'0') << std::setw(2) << ptm->tm_min << L"_"
		<< std::setfill(L'0') << std::setw(2) << ptm->tm_sec;

	return ss.str();
}

#pragma comment(lib, "dbghelp.lib") 
#pragma comment(lib, "Psapi.lib")
LONG WINAPI HandleException(struct _EXCEPTION_POINTERS* apExceptionInfo)
{
	std::wstring dateStr = getCurrentDate();
	std::wstring timeStr = getCurrentTime();

	wstring curCrashDumpFolder = L"CrashDump\\" + dateStr;

	std::filesystem::create_directories(curCrashDumpFolder);

	std::wstring chara = GetCharName();
	std::wstring lvl = GetLevelName();
	std::wstring fileName = timeStr + L"_" + L"Dump_" + chara + L"_" + lvl + L".dmp";

	wstring crashDumpFile = curCrashDumpFolder + L"\\" + fileName;

	MINIDUMP_EXCEPTION_INFORMATION info = { NULL, NULL, NULL };
	string exception = "";
	try
	{
		//generate crash dump
		HANDLE hFile = CreateFileW(
			crashDumpFile.c_str(),
			GENERIC_WRITE | GENERIC_READ,
			0,
			NULL,
			CREATE_ALWAYS,
			0,
			NULL
		);

		HANDLE hProcess = GetCurrentProcess();
		PrintDebug("SADX HAS CRASHED!\n");

		if (hFile != NULL)
		{
			PrintDebug("Generating Crash Dump file...\n");
			info =
			{
			 GetCurrentThreadId(),
			 apExceptionInfo,
			 TRUE
			};

			MiniDumpWriteDump(hProcess, GetCurrentProcessId(),
				hFile, MiniDumpWithIndirectlyReferencedMemory,
				&info, NULL, NULL
			);

			CloseHandle(hFile);

			PrintDebug("Done.\n");
		}
	}
	catch (const std::exception& e)
	{
		PrintDebug("CrashDump: Failed to generate a Crash Dump. Error: %s\n", e.what());
		exception = e.what();
	}

	intptr_t crashID = -1;
	string address = "";
	string dllName = "";

	if (info.ThreadId != NULL) 		//get crash address
		crashID = (intptr_t)info.ExceptionPointers->ExceptionRecord->ExceptionAddress;

	if (crashID != -1)
	{
		char hex[MAX_PATH];
		sprintf_s(hex, "%x", crashID);
		address = hex;

		PrintDebug("Get fault module name...\n");

		HMODULE handle;

		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)crashID, &handle))
		{
			char path[MAX_PATH];
			if (GetModuleFileNameA(handle, path, MAX_PATH))
			{
				auto filename = PathFindFileNameA(path);
				dllName = filename;
			}
		}
	}

	string fullMsg = "";
	SetErrorMessage(fullMsg, address, dllName, crashID, exception);
	CopyAndRename_SADXLoaderProfile(curCrashDumpFolder, timeStr); //copy JSON Profile file to the Crash Dump folder so we know what mods and cheats were used
	CreateExtraInfoFile(curCrashDumpFolder, timeStr, fullMsg); //add extra game info like variable states
	string text = "Crash Address: " + address + "\n";
	PrintDebug("\nFault module name: %s \n", dllName.c_str());
	PrintDebug(text.c_str());

	PrintDebug("Crash Dump Done.\n");
	MessageBoxA(0, fullMsg.c_str(), "SADX Error", MB_ICONERROR);
	ShellExecute(NULL, L"open", curCrashDumpFolder.c_str(), NULL, NULL, SW_SHOW);

	return EXCEPTION_EXECUTE_HANDLER;
}

void initCrashDump()
{
	SetUnhandledExceptionFilter(HandleException);
}