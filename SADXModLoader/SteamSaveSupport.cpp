#include "stdafx.h"
#include <windows.h>
#include <shlobj.h>

#include <SADXModLoader.h>
#include "FunctionHook.h"
#include <time.h>

// Not sure if these match X360
ThiscallFunctionPointer(void, AsynDoWithNowLoading, (void* param), 0x00643EA7);
VoidFunc(dsVMSsgcore_WriteSaveFile, 0x00421FD0);
FunctionPointer(void, dsVMSEraseGame_do, (), 0x00421EC0);
FunctionPointer(FILE*, fopen_, (LPCSTR lpFileName, const char* a2), 0x00644DE6); // SADX version of fopen

FunctionHook<Uint8> CountSaveNum_h(CountSaveNum);
FunctionHook<void> dsVMSEraseGame_do_h(dsVMSEraseGame_do);

static WCHAR DocumentsPath[MAX_PATH];
static WCHAR WorkingDirSteam[FILENAME_MAX]; // Like `C:\Users\Player\Documents\SEGA\Sonic Adventure DX`
static WCHAR WorkingDir2004[FILENAME_MAX]; // Like `C:\Games\Sonic Adventure DX`
static char* DeleteSavePath = nullptr; // Pointer to the string used in the file delete function
static char* ChaoSavePath = nullptr; // Pointer to the string used in Chao file load/save functions
static bool SteamSave = false; // Global variable indicating whether the Steam version's save is being used

// Save delete hook
static void dsVMSEraseGame_do_r()
{
	// Change the current folder (Unicode compatible) then use the game's original ANSI functions with relative paths
	if (SteamSave)
		_wchdir(WorkingDirSteam);
	dsVMSEraseGame_do_h.Original(); // Also runs with patched name
	_wchdir(WorkingDir2004); // Set working folder back to the original
}

// Edited CRC calculation function from the X360 disasm
static Uint16 __cdecl createCRC_r(Uint8* c, bool steam)
{
	Sint16 v1; // r11
	Sint32 v2; // r9
	Sint32 v3; // ctr
	Uint16 v4; // r11
	Sint32 v5; // r6
	Uint16 v6; // r11
	Sint32 v7; // r10
	Uint16 v8; // r11
	Sint32 v9; // r10
	Uint16 v10; // r11
	Sint32 v11; // r10
	Uint16 v12; // r11
	Sint32 v13; // r10
	Uint16 v14; // r11
	Sint32 v15; // r10
	Uint16 v16; // r11
	Sint32 v17; // r10
	Uint16 v18; // r11
	Sint32 v19; // r10

	v1 = -1;
	v2 = 4;
	v3 = steam ? 1404 : 1388;
	do
	{
		v4 = c[v2] ^ v1;
		v5 = v4 & 1;
		v6 = v4 >> 1;
		if (v5)
			v6 ^= 0x8408u;
		v7 = v6 & 1;
		v8 = v6 >> 1;
		if (v7)
			v8 ^= 0x8408u;
		v9 = v8 & 1;
		v10 = v8 >> 1;
		if (v9)
			v10 ^= 0x8408u;
		v11 = v10 & 1;
		v12 = v10 >> 1;
		if (v11)
			v12 ^= 0x8408u;
		v13 = v12 & 1;
		v14 = v12 >> 1;
		if (v13)
			v14 ^= 0x8408u;
		v15 = v14 & 1;
		v16 = v14 >> 1;
		if (v15)
			v16 ^= 0x8408u;
		v17 = v16 & 1;
		v18 = v16 >> 1;
		if (v17)
			v18 ^= 0x8408u;
		v19 = v18 & 1;
		v1 = v18 >> 1;
		if (v19)
			v1 ^= 0x8408u;
		++v2;
		--v3;
	} while (v3);
	return (Uint16)~v1;
}

// Adds save files from the Steam or 2004 version to the file select screen
static Uint8 __cdecl CountSaveNum_r(bool steam)
{	
	HANDLE handle; // edi MAPDST
	FILE* file = nullptr; // ebx
	Uint8 i = 0; // [esp+Eh] [ebp-25Ah]
	_WIN32_FIND_DATAA _FindFileData; // [esp-140h] [ebp-3A8h] BYREF
	_WIN32_FIND_DATAA fileSearchData; // [esp+18h] [ebp-250h] BYREF
	char fileName[MAX_PATH]; // [esp+158h] [ebp-110h] BYREF
	char* data = new char[steam ? 1408 : 1392]();

	memset(&fileSearchData, 0, sizeof(fileSearchData));
	memcpy(&_FindFileData, &fileSearchData, sizeof(_FindFileData));

	// Change the current folder (Unicode compatible) then use the game's original ANSI functions with relative paths
	_wchdir(steam ? WorkingDirSteam : WorkingDir2004);
	sprintf_s(fileName, "./SAVEDATA/*.snc");
	if (!steam)
		AddLineList(0, _FindFileData); // Add first item

	handle = FindFirstFileA(fileName, &fileSearchData);
	if ((int)handle == -1)
	{
		PrintDebug("Steam save support: Unable to find any save files.\n", fileName);
		return 0;
	}
	sprintf_s(fileName, "./SAVEDATA/%s", fileSearchData.cFileName);
	fopen_s(&file, fileName, "rb");
	if (fread(data, steam ? 1408u : 1392u, 1u, file))
	{
		if (createCRC_r((Uint8*)data, steam) == ((SAVE_DATA*)data)->code)
		{
			//PrintDebug("Adding %s\n", fileName);
			strncpy_s(fileName, fileSearchData.cFileName, strlen(fileSearchData.cFileName));
			memcpy(&_FindFileData, &fileSearchData, sizeof(_FindFileData));
			AddLineList(fileName, _FindFileData);
			i = 1;
		}
	}
	fclose(file);
	if (FindNextFileA(handle, &fileSearchData))
	{
		do
		{			
			sprintf_s(fileName, "./SAVEDATA/%s", fileSearchData.cFileName);
			fopen_s(&file, fileName, "rb");
			if (fread(data, steam ? 1408u : 1392u, 1u, file))
			{
				if (createCRC_r((Uint8*)data, steam) == ((SAVE_DATA*)data)->code)
				{
					//PrintDebug("Adding %s\n", fileName);
					strncpy_s(fileName, fileSearchData.cFileName, strlen(fileSearchData.cFileName));
					memcpy(&_FindFileData, &fileSearchData, sizeof(_FindFileData));
					AddLineList(fileName, _FindFileData);
					++i;
				}
			}
			fclose(file);
		} while (i <= 98u && FindNextFileA(handle, &fileSearchData));
	}
	FindClose(handle);
	delete data;
	// Reset working folder
	_wchdir(WorkingDir2004);
	return i;
}

// Savegame file load function
static void __cdecl dsVMSLoadGame_do_r()
{
	LIST_DATA* v0; // esi
	Uint16 v1; // di
	Sint32 v2; // eax
	FILE* v3 = nullptr; // esi
	char path[MAX_PATH]; // [esp+0h] [ebp-108h] BYREF
	char path_chao[MAX_PATH]; // [esp+0h] [ebp-108h] BYREF
	Uint32 v5; // [esp+104h] [ebp-4h]
	Uint32 retaddr = 0; // [esp+108h] [ebp+0h]

	v5 = retaddr ^ security_cookie;
	if (GB_vmsdisable)
	{
		AsynDoWithNowLoading((void*)(retaddr ^ v5));
	}
	else
	{
		v0 = listData;
		if (listData)
		{
			v1 = GCMemoca_State.u8FileName;
			InitSaveData();
			if (v1)
			{
				v2 = v1;
				do
				{
					--v2;
					v0 = v0->next;
				} while (v2);
			}
			// Check non-Steam version first
			SteamSave = false;
			_wchdir(WorkingDir2004);
			sprintf_s(path, "./SAVEDATA/%s", v0->name);
			// If it doesn't exist in 2004's savedata, then it's the Steam version
			if (!Exists(path))
			{
				_wchdir(WorkingDirSteam);
				SteamSave = true;
			}
		}
		else
		{
			SteamSave = false;
			_wchdir(WorkingDir2004);
			sprintf_s(path, "./SAVEDATA/%s", GCMemoca_State.string);
			if (!Exists(path))
			{
				_wchdir(WorkingDirSteam);
				SteamSave = true;
			}
		}
		//PrintDebug("Load: %s\n", path);
		fopen_s(&v3, path, "rb");
		fread(&SaveData, sizeof(SAVE_DATA), 1u, v3);
		fclose(v3);

		// Update path pointers
		delete DeleteSavePath;
		DeleteSavePath = new char[strlen(path) + 1]();
		strncpy(DeleteSavePath, path, strlen(path) + 1);
		WriteData((char**)0x421F07, DeleteSavePath); // dsVMSEraseGame_do_h

		// Update path pointers (Chao)
		delete ChaoSavePath;
		if (SteamSave)
			sprintf_s(path_chao, "./SAVEDATA/SonicAdventureChaoGarden.snc");
		else
			sprintf_s(path_chao, "./SAVEDATA/SONICADVENTURE_DX_CHAOGARDEN.snc");
		ChaoSavePath = new char[strlen(path_chao) + 1]();
		strncpy(ChaoSavePath, path_chao, strlen(path_chao) + 1);
		// Restore working dir
		_wchdir(WorkingDir2004);
		// Exit
		AsynDoWithNowLoading((void*)(retaddr ^ v5));
	}
}

// Savegame file write function
static void __cdecl dsVMSsgcore_WriteSaveFile_r()
{
	Uint8 saveNum; // bl
	LIST_DATA* Next; // edi
	Sint32 v3; // eax
	FILE* v4; // edi
	saveNum = 1;
	SaveRetry = 0;
	SaveOK = 0;
	char fileName[MAX_PATH]; // esi
	char savedata[1408] = { 0 };
	time_t rawtime;

	if (!GB_vmsdisable)
	{
		if (SaveCount > 4u)
		{
			now_saving = 0;
			SaveReady = 0;
			SaveExecFlag = 0;
			AlertFlag = 0;
			SaveCount = 0;
		}
		// Only the 2004 savedata folder is created because Steam saves are never created from scratch
		CreateDirectoryA("./SAVEDATA/", 0); 
		if (!FileDeleteFlag)
		{
			CreateSaveData();
		}
		if (FirstCreateFlag)
		{
			FirstCreateFlag = 0;
			if (GCMemoca_State.u8FileNum > 98u)
			{
				return;
			}
			Next = listData->next;
			sprintf(fileName, "SonicDX%02d.snc", 1);
			while (Next)
			{
				if (CompareStringA(9u, 1u, fileName, -1, Next->name, -1) == 2)
				{
					Next = listData->next;
					if (++saveNum > 99u)
					{
						return;
					}
					sprintf(fileName, "SonicDX%02d.snc", saveNum);
				}
				else
				{
					Next = Next->next;
				}
			}
			if (GCMemoca_State.string)
			{
				SOCFree(GCMemoca_State.string);
				GCMemoca_State.string = nullptr;
			}
			GCMemoca_State.string = (char*)malloc_0(0xEu);
			++GCMemoca_State.u8FileNum;
			GCMemoca_State.u8FileName = saveNum;
			sprintf(GCMemoca_State.string, "SonicDX%02d.snc", saveNum);
			sprintf(fileName, "./SAVEDATA/SonicDX%02d.snc", saveNum);
		}
		else
		{
			if (SteamSave)
				_wchdir(WorkingDirSteam);
			v3 = lstrlenA(GCMemoca_State.string);
			sprintf(fileName, "./SAVEDATA/%s", GCMemoca_State.string);
		}
		v4 = fopen(fileName, "wb");
		memcpy(savedata, SaveData, 0x570u);
		// Add date/time for the Steam save
		if (SteamSave)
		{
			time(&rawtime);
			tm* timeinfo = localtime(&rawtime);
			Uint16 year = (Uint16)timeinfo->tm_year + 1900;
			Uint16 month = (Uint16)timeinfo->tm_mon + 1;
			Uint16 dayweek = (Uint16)timeinfo->tm_wday;
			Uint16 day = (Uint16)timeinfo->tm_mday;
			Uint16 hour = (Uint16)timeinfo->tm_hour;
			Uint16 minute = (Uint16)timeinfo->tm_min;
			Uint16 second = (Uint16)timeinfo->tm_sec;
			memcpy(savedata + 0x570, &year, 2);
			memcpy(savedata + 0x572, &month, 2);
			memcpy(savedata + 0x574, &dayweek, 2);
			memcpy(savedata + 0x576, &day, 2);
			memcpy(savedata + 0x578, &hour, 2);
			memcpy(savedata + 0x57A, &minute, 2);
			memcpy(savedata + 0x57C, &second, 2);
			// Recalculate CRC with new length + time
			Uint32 crc = createCRC_r((Uint8*)&savedata, true);
			memcpy(savedata, &crc, 4);
		}
		fwrite(&savedata, SteamSave ? 0x580u : 0x570u, 1, v4);
		GCMemoca_State.MsgFlag = 0;
		input_init();
		SaveCount = 0;
		now_saving = 0;
		SaveRetry = 0;
		SaveReady = 0;
		SaveExecFlag = 0;
		AlertFlag = 0;
		fclose(v4);
		SaveOK = 1;
		// Restore working dir
		_wchdir(WorkingDir2004);
	}
}

FILE* __cdecl ChaoFileOpenHook(LPCSTR lpFileName, const char* a2)
{
	//PrintDebug("1 Chao open game: %s\n", ChaoSavePath);
	if (SteamSave)
		_wchdir(WorkingDirSteam);
	FILE* result = fopen_(ChaoSavePath, a2); // Using the SADX version of the function because the regular one made it write 0 bytes
	_wchdir(WorkingDir2004);
	return result;
}

HANDLE __stdcall ChaoCreateFileHook(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	//PrintDebug("Steam %d Chao create game: %s\n", SteamSave, ChaoSavePath);
	if (SteamSave)
		_wchdir(WorkingDirSteam);
	HANDLE result = CreateFileA(ChaoSavePath, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	_wchdir(WorkingDir2004);
	return result;
}

static Uint8 CountSaveNum_r()
{
	// 'return CountSaveNum_Custom(false) + CountSaveNum_Custom(true);' made it run in the wrong order
	Uint8 res_1 = CountSaveNum_r(false);
	Uint8 res_2 = CountSaveNum_r(true);
	return res_1 + res_2;
}

void SteamSaveSupport_Init()
{
	HRESULT docs_result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, DocumentsPath);
	if (docs_result != S_OK)
	{
		PrintDebug("Steam save support: Unable to get the path to the Documents folder.\n");
		return;
	}
	// Get folders
	swprintf_s(WorkingDirSteam, L"%s\\SEGA\\Sonic Adventure DX", DocumentsPath); // Base folder for Steam saves
	_wgetcwd(WorkingDir2004, FILENAME_MAX); // Original SADX folder
	// Set up code
	WriteJump(dsVMSsgcore_WriteSaveFile, dsVMSsgcore_WriteSaveFile_r);
	WriteJump(dsVMSLoadGame_do, dsVMSLoadGame_do_r);
	CountSaveNum_h.Hook(CountSaveNum_r);
	dsVMSEraseGame_do_h.Hook(dsVMSEraseGame_do_r);
	WriteCall((void*)0x0071AA73, ChaoFileOpenHook); // al_confirmsave
	WriteCall((void*)0x007163F9, ChaoFileOpenHook); // ALMC_Read
	WriteCall((void*)0x0071ADC9, ChaoFileOpenHook); // al_confirmload
	WriteCall((void*)0x0071ACE2, ChaoCreateFileHook); // al_confirmload
	WriteData<1>((char*)0x0071ACE7, 0x90u); // al_confirmload (extra nop because the original call is 6 bytes)
}