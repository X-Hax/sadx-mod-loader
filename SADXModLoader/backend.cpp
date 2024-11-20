#include "stdafx.h"
#include "d3d8types.h"
using std::wstring;

enum RenderBackend: int
{
	DirectX8 = 0,
	DirectX9 = 1,
};

IDirect3D8* __stdcall CreateDirect3D8Hook(UINT SDKVersion)
{
	HMODULE d3d8to9 = GetModuleHandle(L"d3d8m.dll");
	if (!d3d8to9)
		return 0;
	typedef IDirect3D8* (WINAPI* Direct3DCreate8Ptr)(UINT);
	Direct3DCreate8Ptr Direct3DCreate8Original = (Direct3DCreate8Ptr)GetProcAddress(d3d8to9, "Direct3DCreate8");
	if (!Direct3DCreate8Original)
		return 0;
	IDirect3D8* result = Direct3DCreate8Original(SDKVersion);
	if (!result)
	{
		PrintDebug("D3D8to9: Error creating Direct3D object: IDirect3D8 pointer is null \n");
		MessageBox(nullptr, L"Error creating Direct3D object: IDirect3D8 pointer is null.", L"Direct3D Object Error", MB_OK | MB_ICONERROR);
		OnExit(0, 0, 0);
		ExitProcess(0);
	}
	return result;
}

void __cdecl InitRenderBackend(int mode, std::wstring gamePath, std::wstring extLibPath)
{
	switch ((RenderBackend)mode)
	{
	case DirectX8:
		break;
	case DirectX9:
		wstring d3d8oldpath = gamePath + L"\\d3d8.dll";
		if (FileExists(d3d8oldpath))
		{
			PrintDebug("D3D8to9: d3d8.dll found in game directory, aborting D3D8to9 hook\n");
			return;
		}

		bool d3d8to9DLL = false; // Handle of the DLL
		wstring fullPath = extLibPath + L"D3D8M\\d3d8m.dll"; // Path to the DLL

		// Attempt to load the DLL
		d3d8to9DLL = LoadLibraryEx(fullPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

		if (d3d8to9DLL)
			PrintDebug("D3D8to9: Loaded d3d8to9 DLL\n");
		else
		{
			int err = GetLastError();
			PrintDebug("D3D8to9: Error loading D3D8to9 DLL %X (%d)\n", err, err);
			MessageBox(nullptr, L"Error loading D3D8to9.\n\n"
				L"Make sure the Mod Loader is installed properly.",
				L"D3D8to9 Load Error", MB_OK | MB_ICONERROR);
			return;
		}
		// Hook Direct3DCreate8
		WriteJump((void*)0x007C235E, CreateDirect3D8Hook);
		break;
	}
}