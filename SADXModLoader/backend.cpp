#include "stdafx.h"
#include "d3d8types.h"

enum RenderBackend: int
{
	DirectX8 = 0,
	DirectX9 = 1,
	DirectX11 = 2
};

static std::string BackendName; // Title of the backend, e.g. "d3d8to9"
static std::wstring WrapperDllFilename; // Filename of the backend DLL, e.g. "d3d8to11.dll"
static std::wstring WrapperCheckFilename; // Filename of DLL to check if the backend is already loaded, e.g. "d3d9.dll" or "d3d11.dll"
static std::wstring WrapperDllFolderLocation; // Full path to the backend folder, e.g. "C:\SADX\SAManager\extlib\d3d8m"
static std::wstring WrapperDllFullPath; // Full path to the backend DLL file e.g. "C:\SADX\SAManager\extlib\d3d8m\d3d8m.dll"

IDirect3D8* __stdcall CreateDirect3D8Hook(UINT SDKVersion)
{
	HMODULE handle = GetModuleHandle(WrapperDllFilename.c_str());
	if (!handle)
		return 0;
	typedef IDirect3D8* (WINAPI* Direct3DCreate8Ptr)(UINT);
	Direct3DCreate8Ptr Direct3DCreate8Original = (Direct3DCreate8Ptr)GetProcAddress(handle, "Direct3DCreate8");
	if (!Direct3DCreate8Original)
		return 0;
	IDirect3D8* result = Direct3DCreate8Original(SDKVersion);
	if (!result)
	{
		PrintDebug("%s backend: Error creating Direct3D object - IDirect3D8 pointer is null \n", BackendName.c_str());
		MessageBox(nullptr, L"Error creating Direct3D object: IDirect3D8 pointer is null.", L"Direct3D Object Error", MB_OK | MB_ICONERROR);
		OnExit(0, 0, 0);
		ExitProcess(0);
	}
	return result;
}

void __cdecl InitRenderBackend(int mode, std::wstring gamePath, std::wstring extLibPath)
{
	bool loadWrapper = false;
	switch ((RenderBackend)mode)
	{
	case DirectX8:
	default:
		break;
	case DirectX9:
		loadWrapper = true;
		BackendName = "D3D8to9";
		WrapperDllFilename = L"d3d8m.dll";
		WrapperCheckFilename = L"d3d9.dll";
		WrapperDllFolderLocation = extLibPath + L"D3D8M";
		WrapperDllFullPath = extLibPath + L"D3D8M\\d3d8m.dll";
		break;
	case DirectX11:
		loadWrapper = true;
		BackendName = "D3D8to11";
		WrapperDllFilename = L"d3d8to11.dll";
		WrapperCheckFilename = L"d3d11.dll";
		WrapperDllFolderLocation = extLibPath + L"D3D8to11";
		WrapperDllFullPath = extLibPath + L"D3D8to11\\d3d8to11.dll";
		// Set environment variables
		SetEnvironmentVariable(L"D3D8TO11_CONFIG_DIR", WrapperDllFolderLocation.c_str());
		SetEnvironmentVariable(L"D3D8TO11_SHADER_SOURCE_DIR", WrapperDllFolderLocation.c_str());
		break;
	}
	if (loadWrapper)
	{
		if (GetModuleHandle(WrapperCheckFilename.c_str()) != nullptr)
		{
			PrintDebug("%s: Direct3D 9 is already loaded, skipping D3D8to9 hook\n", BackendName.c_str());
			return;
		}
		// Attempt to load the DLL
		HMODULE handle = LoadLibraryEx(WrapperDllFullPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

		if (handle)
			PrintDebug("%s: Backend DLL loaded successfully\n", BackendName.c_str());
		else
		{
			int err = GetLastError();
			PrintDebug("%s: Error loading backend DLL %X (%d)\n", BackendName.c_str(), err, err);
			MessageBox(nullptr, L"Error loading rendering backend.\n\n"
				L"Make sure the Mod Loader is installed properly.",
				L"Backend Load Error", MB_OK | MB_ICONERROR);
			return;
		}
		// Hook Direct3DCreate8
		WriteJump((void*)0x007C235E, CreateDirect3D8Hook);
	}
}