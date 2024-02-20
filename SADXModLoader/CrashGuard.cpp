#include "stdafx.h"
#include <SADXModLoader.h>

// TODO: Figure out how to make 2D sprites appear when the texture is missing

DataArray(Sint32, TEXTURE_ALPHA_TBL, 0x389D458, 4);
DataPointer(NJS_TEXLIST*, nj_current_texlist, 0x03D0FA24);
DataPointer(NJS_TEXMEMLIST*, nj_texture_current_memlist_, 0x03CE7128);

FunctionPointer(void, stApplyPalette, (NJS_TEXMEMLIST*), 0x78CDC0);
FunctionPointer(void, ghGetPvrTextureSize, (NJS_TEXLIST* texlist, Sint32 index, Sint32* width, Sint32* height), 0x004332B0);
FastcallFunctionPointer(void, stLoadTexture, (NJS_TEXMEMLIST* tex, IDirect3DTexture8* surface), 0x0078CBD0);
FastcallFunctionPointer(Sint32, njSetTexture_real, (NJS_TEXLIST* a1), 0x0077F3D0); // The original njSetTexture (Direct3D_SetTexlist in old disasm)
FastcallFunctionPointer(Sint32, stSetTexture, (NJS_TEXMEMLIST* a1), 0x0078CF20);

// Texture loading hooks
FastcallFunctionHook<Sint32, NJS_TEXLIST*> njSetTexture_real_h(njSetTexture_real);
FastcallFunctionHook<Sint32, NJS_TEXMEMLIST*> stSetTexture_h(stSetTexture);
FastcallFunctionHook<Sint32, Sint32> njSetTextureNum_h(njSetTextureNum_);

// File loading hooks
FunctionHook<Sint8*, const char*> njOpenBinary_h(njOpenBinary);
FunctionHook<void, const char*, LPVOID> LoadFile_h(LoadFile);

// Variables
static Uint8 checker_texturedata[2048] = { 0 }; // Raw texture data
static NJS_TEXINFO checker_texinfo;
static NJS_TEXNAME checker_textures[300] = { 0 }; // Textures array (some functions access individual entries directly so no escape with just one)
static NJS_TEXLIST checker_texlist = { arrayptrandlength(checker_textures) }; // Used in direct3d.cpp
NJS_TEXMEMLIST checker_memlist = { 0 }; // Used in TextureReplacement.cpp

static char errormsg[1024];
static bool IgnoreFileLoadErrors = false; // Do not show the error dialog if true

// Hack to check filename before loading a file
void LoadFile_r(const char* str, LPVOID lpBuffer)
{
	char buf[MAX_PATH];
	sprintf_s(buf, "system\\%s", str);
	if (!FileExists(sadx_fileMap.replaceFile(buf)))
	{
		PrintDebug("LoadFile_r: Failed to load %s\n", buf);
		if (!IgnoreFileLoadErrors)
		{
			sprintf(errormsg, "Unable to load binary file %s. This is a critical error and the game may not work properly.\n\nCheck game health in the Mod Manager and try again.\n\nTry to continue running? Select Cancel to ignore further errors.", buf);
			int result = MessageBoxA(nullptr, errormsg, "SADX Mod Loader Error", MB_ICONERROR | MB_YESNOCANCEL);
			switch (result)
			{
			case IDNO:
				OnExit(0, 0, 0);
				ExitProcess(1);
				break;
			case IDCANCEL:
				IgnoreFileLoadErrors = true;
				break;
			default:
				break;
			}
		}
	}
	return LoadFile_h.Original(str, lpBuffer);
}

// Hack to check filename before loading a file (njOpenBinary version)
Sint8* __cdecl njOpenBinary_r(const char* str)
{
	char buf[MAX_PATH];
	sprintf_s(buf, "system\\%s", str);
	if (!FileExists(sadx_fileMap.replaceFile(buf)))
	{
		PrintDebug("njOpenBinary_r: Failed to load %s\n", buf);
		if (!IgnoreFileLoadErrors)
		{
			sprintf(errormsg, "Unable to load the binary file %s. This is a critical error and the game may not work properly.\n\nCheck game health in the Mod Manager and try again.\n\nTry to continue running? Select Cancel to ignore further errors.", buf);
			int result = MessageBoxA(nullptr, errormsg, "SADX Mod Loader Error", MB_ICONERROR | MB_YESNOCANCEL);
			switch (result)
			{
			case IDNO:
				OnExit(0, 0, 0);
				ExitProcess(1);
				break;
			case IDCANCEL:
				IgnoreFileLoadErrors = true;
				break;
			default:
				break;
			}
		}
	}
	return njOpenBinary_h.Original(str);
}

// Hack to set texmemlist when a specific texid is used
Sint32 __fastcall njSetTextureNum_r(Sint32 num)
{
	if (nj_current_texlist && (Uint32)num < nj_current_texlist->nbTexture && nj_current_texlist->textures[num].texaddr)
	{
		return njSetTextureNum_h.Original(num);
	}
	else
	{
		nj_texture_current_memlist_ = &checker_memlist;
		stSetTexture(nj_texture_current_memlist_);
		return 1;
	}
}

// Hack to get texture dimensions when textures aren't loaded
void __cdecl ghGetPvrTextureSize_r(NJS_TEXLIST* texlist, Sint32 index, Sint32* width, Sint32* height)
{
	NJS_TEXLIST* _texlist; // ecx
	NJS_TEXSURFACE* p_texsurface; // eax

	_texlist = texlist;
	if (!texlist || !texlist->textures || (Sint32)texlist->nbTexture <= index || !texlist->textures[index].texaddr)
	{
		_texlist = nj_current_texlist;
	}
	p_texsurface = &((NJS_TEXMEMLIST*)_texlist->textures[index].texaddr)->texinfo.texsurface;
	if (width)
	{
		*width = p_texsurface->nWidth;
	}
	if (height)
	{
		*height = p_texsurface->nHeight;
	}
}

// Initialize checkerboard texture
void InitDefaultTexture()
{
	// Fill raw color data for VQ
	for (int i = 8; i < 16; i += 2)
	{
		checker_texturedata[i] = 0x1F;
		checker_texturedata[i + 1] = 0xF8;
	}
	for (int i = 0; i < 256; i++)
	{
		checker_texturedata[1024 + i] = 0x01u;
		checker_texturedata[1792 + i] = 0x01u;
	}
	// Set global index and init info
	checker_memlist.globalIndex = 0xFFFFFFFA;
	njSetTextureInfo(&checker_texinfo, (Uint16*)&checker_texturedata, NJD_TEXFMT_SMALLVQ | NJD_TEXFMT_RGB_565, 64, 64);
	// Generate texture
	{
		checker_texinfo.texsurface.pSurface = 0;
		checker_memlist.texinfo.texsurface.Type = 0x10010000;
		checker_memlist.texinfo.texsurface.PixelFormat = NJD_PIXELFORMAT_RGB565;
		checker_memlist.texinfo.texsurface.TextureSize = 0x2000;
		checker_memlist.texinfo.texsurface.nWidth = 0x40;
		checker_memlist.texinfo.texsurface.nHeight = 0x40;
		stLoadTexture(&checker_memlist, (IDirect3DTexture8*)checker_texinfo.texaddr);
	}
	// Set the same texture for 300 slots
	for (int i = 0;i < 300;i++)
	{
		checker_textures[i].attr = NJD_TEXATTR_GLOBALINDEX | NJD_TEXATTR_TYPE_MEMORY;
		checker_textures[i].filename = &checker_texinfo;
		checker_textures[i].texaddr = (Uint32)&checker_memlist;
	}
	checker_texlist.nbTexture = 300;
	checker_texlist.textures = checker_textures;
}

// Set default texture, called on failure
Sint32 __fastcall SetDefaultTexture()
{
	stApplyPalette(&checker_memlist);
	if (SetTexture_CurrentGBIX != checker_memlist.globalIndex)
	{
		Direct3D_Device->SetTexture(0, (IDirect3DBaseTexture8*)checker_memlist.texinfo.texsurface.pSurface);
		SetTexture_CurrentGBIX = checker_memlist.globalIndex;
		SetTexture_CurrentMemTexture = &checker_memlist;
	}
	return TEXTURE_ALPHA_TBL[(checker_memlist.texinfo.texsurface.PixelFormat >> 27) & 3];
}

// Set texture function that handles failure
Sint32 __fastcall stSetTexture_r(NJS_TEXMEMLIST* t)
{
	if (!t)
		return SetDefaultTexture();

	stApplyPalette(t);
	if (SetTexture_CurrentGBIX != t->globalIndex)
	{
		if (t->texinfo.texsurface.pSurface)
		{
			if (FAILED(Direct3D_Device->SetTexture(0, (IDirect3DBaseTexture8*)t->texinfo.texsurface.pSurface)))
			{
				return SetDefaultTexture();
			}
			else
			{
				SetTexture_CurrentGBIX = t->globalIndex;
				SetTexture_CurrentMemTexture = t;
			}
		}
	}

	return TEXTURE_ALPHA_TBL[(t->texinfo.texsurface.PixelFormat >> 27) & 3];
}

// Set texlist function with additional failchecks, compatible with Lantern Engine
Sint32 __fastcall njSetTexture_real_r(NJS_TEXLIST* a1)
{
	if (!a1 || !a1->textures || !a1->textures->texaddr)
	{
		nj_current_texlist = &checker_texlist;
		njds_texList = &checker_texlist;
		nj_texture_current_memlist_ = (NJS_TEXMEMLIST*)checker_texlist.textures->texaddr;
		CurrentTextureNum = 0;
		stSetTexture(nj_texture_current_memlist_);
		return 1;
	}
	return njSetTexture_real_h.Original(a1);
}

// Hack to get a global index value on invalid texlists without crashing
Uint32 __cdecl GetGlobalIndex_r(NJS_TEXLIST* a1, Sint32 texIndex)
{
	NJS_TEXLIST* texlist = a1;

	// If anything is wrong with the texlist, set the current one
	if (!a1 || !a1->textures || (Uint32)texIndex >= a1->nbTexture || !a1->textures[texIndex].texaddr)
		texlist = nj_current_texlist;
	// The default one can be broken too
	if (texlist && texlist->textures)
	{
		NJS_TEXMEMLIST* memlist = (NJS_TEXMEMLIST*)texlist->textures[texIndex].texaddr;
		if (memlist)
			return memlist->globalIndex;
	}
	// If the TEXMEMLIST is null, return 0
	return 0;
}

void CrashGuard_Init()
{
	// Binary file checks
	njOpenBinary_h.Hook(njOpenBinary_r);
	LoadFile_h.Hook(LoadFile_r);
	// Load the checkerboard texture after Direct3D is initialized
	WriteCall((void*)0x004209B2, InitDefaultTexture);
	// Main hooks for the texture
	WriteJump(ghGetPvrTextureSize, ghGetPvrTextureSize_r);
	njSetTexture_real_h.Hook(njSetTexture_real_r);
	stSetTexture_h.Hook(stSetTexture_r);
	njSetTextureNum_h.Hook(njSetTextureNum_r);
	WriteJump(GetGlobalIndex, GetGlobalIndex_r);
	// Patch IsTextureNG to prevent models from becoming invisible
	WriteData<1>((Uint8*)0x00403255, 0i8);
	WriteData<1>((Uint8*)0x00403265, 0i8);
}