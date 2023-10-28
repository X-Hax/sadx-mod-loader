#include "stdafx.h"
#include "intrin.h"
#include <Windows.h>
#include "d3d8.h"
#include "d3dx8.h"
#include "SADXModLoader.h"
#include "gvm.h"
#include "AutoMipmap.h"

DataPointer(IDirect3DDevice8*, _st_d3d_device_, 0x3D128B0); // Direct3D device

// Original code decompiled by Exant, cleaned up by PkR
// TODO: 
// Figure out what's wrong with Index4 texture decoding
// Load built-in mipmaps instead of generating them
// Test ARGB8888 support
// Add Index8 support

// Retrieves global index for the specified texture ID, requires the GVMH chunk pointer
Uint32 gjGetGlobalIndexFromGVMH(sStChunkPVMH* chunk, Uint16 n)
{
	Uint16 flags; // GVM flags
	Uint32 start; // Start offset

	if (n >= chunk->nbTexture)
	{
		return -1;
	}
	flags = chunk->flag;
	if ((flags & 8) == 0)
	{
		return -1;
	}
	start = 6;
	if ((flags & GVMH_NAMES) != 0)
	{
		start = 34;
	}
	if ((flags & GVMH_FORMATS) != 0)
	{
		start += 2;
	}
	if ((flags & GVMH_DIMENSIONS) != 0)
	{
		start += 2;
	}
	Uint32 result = *(_DWORD*)((int)chunk + 8 + start * (n + 1));
	return _byteswap_ulong(result);
}

// Sets up the texture surface in NJS_TEXMEMLIST using specified texture data pointer
void gjLoadTextureGVRTAnalize(NJS_TEXMEMLIST* texmemlist, Uint8* addr)
{
	NJS_TEXSURFACE* result; // eax

	result = &texmemlist->texinfo.texsurface;
	result->BitDepth = 0;

	Uint32 value = _byteswap_ulong(*(Uint32*)(addr + 8));
	Uint16 flags = (value & 0xFF00) >> 8;
	Uint16 surfacetype = value & 0xFF;
	result->Type = surfacetype;
	switch (surfacetype)
	{
	case GJD_TEXFMT_ARGB_5A3:
	case GJD_TEXFMT_ARGB_8888:
		result->PixelFormat = GJD_PIXELFORMAT_OTHER;
		break;
	case GJD_TEXFMT_PALETTIZE4:
		result->PixelFormat = GJD_PIXELFORMAT_PAL_4BPP;
		break;
	case GJD_TEXFMT_PALETTIZE8:
		result->PixelFormat = GJD_PIXELFORMAT_PAL_8BPP;
		break;
	case GJD_TEXFMT_DXT1:
		result->PixelFormat = GJD_PIXELFORMAT_DXT1;
		break;
	}
	result->fSurfaceFlags = 0;
	if ((flags & GVR_FLAG_PALETTE) != 0)
	{
		result->fSurfaceFlags |= GJD_SURFACEFLAGS_PALETTIZED;
	}
	// Looks like this needs to be on always? Otherwise non-mipmapped textures fade to black
	//if ((flags & GVR_FLAG_MIPMAP) != 0) 
	//{
		result->fSurfaceFlags |= GJD_SURFACEFLAGS_MIPMAPED;
	//}
	result->nWidth = _byteswap_ushort(*(Uint16*)(addr + 0xC));
	result->nHeight = _byteswap_ushort(*(Uint16*)(addr + 14));
	result->pSurface = (Uint32*)(addr + 16);
	result->TextureSize = *(Uint32*)(addr + 4) - 8;
	//PrintDebug("Value: %08X, Type: %X, Flags: %X\n", value, surfacetype, flags);
	//PrintDebug("Width %d, Height %d\n", result->nWidth, result->nHeight);
}

// Parses binary data and sets GBIX or reads the texture
void gjLoadTextureTexMemListAnalize(NJS_TEXMEMLIST* memlist)
{
	Uint8* addr = (Uint8*)memlist->texinfo.texaddr;
	while (1)
	{
		Uint32 value = *(int*)addr;

		if (value == 'TRVG')
		{
			gjLoadTextureGVRTAnalize(memlist, (Uint8*)addr);
			break;
		}
		else if (value == 'XIBG')
		{
			memlist->globalIndex = _byteswap_ulong(*(Uint32*)(addr + 4));
		}
		if (!value)
		{
			break;
		}
		addr += 1;
	}
}

// Gets texel offset based on D3D format
int GetLockedTexelAddress(D3DFORMAT format, int start, int x, int y)
{
	Uint32 result; // eax
	Uint32 offset;
	switch (format)
	{
	case D3DFMT_R8G8B8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_A8:
		offset = 32;
		break;
	case D3DFMT_R5G6B5:
	case D3DFMT_A1R5G5B5:
		offset = 16;
		break;
	case D3DFMT_R3G3B2:
	case D3DFMT_A8R3G3B2:
		offset = 64;
		break;
	case D3DFMT_X4R4G4B4:
		offset = 128;
		break;
	case D3DFMT_DXT1:
	default:
		offset = 0;
	}
	if (offset > 4)
	{
		result = start * ((offset + 7) / 8);
	}
	else
	{
		result = start * offset / 8;
	}
	if (y == 0)
		return x / 2 + result;
	else
		return y * x + result;
}

// Decodes RGB555 or ARGB4333 pixels
int GvrDecodePixelArgb5a3(Uint16 bytes)
{
	Uint16 data = _byteswap_ushort(bytes);
	Uint32 result; // eax

	// ARGB3444
	if ((bytes & 0x80u) == 0)
	{
		((uint8_t*)&result)[3] = (uint8_t)(((data >> 12) & 0x07) * 0xFF / 0x07);
		((uint8_t*)&result)[2] = (uint8_t)(((data >> 8) & 0x0F) * 0xFF / 0x0F);
		((uint8_t*)&result)[1] = (uint8_t)(((data >> 4) & 0x0F) * 0xFF / 0x0F);
		((uint8_t*)&result)[0] = (uint8_t)(((data >> 0) & 0x0F) * 0xFF / 0x0F);
		//result = (unsigned __int8)((char)-(v1 & 0xF) / 15) | (((unsigned __int8)((char)-((unsigned __int8)v1 >> 4) / 15) | (((unsigned __int8)((char)-(HIBYTE(v1) & 0xF) / 15) | ((unsigned __int8)((char)-((v1 >> 12) & 7) / 7) << 8)) << 8)) << 8);
	}
	// RGB555
	else
	{
		((uint8_t*)&result)[3] = 0xFF;
		((uint8_t*)&result)[2] = (uint8_t)(((data >> 10) & 0x1F) * 0xFF / 0x1F);
		((uint8_t*)&result)[1] = (uint8_t)(((data >> 5) & 0x1F) * 0xFF / 0x1F);
		((uint8_t*)&result)[0] = (uint8_t)(((data >> 0) & 0x1F) * 0xFF / 0x1F);
		//result = (unsigned __int8)((char)-(v1 & 0x1F) / 31) | (((unsigned __int8)((char)-((v1 >> 5) & 0x1F) / 31) | ((((char)-((v1 >> 10) & 0x1F) / 31) | 0xFFFFFF00) << 8)) << 8);
	}
	return result;
}

// Pixel decoding function: ARGB5A3
void GvrDecodeArgb5a3(void* ptr, Sint32 width, Sint32 height, void* pBaseAddr, Sint32 mPitch)
{
	int xpos; // ebx
	int ypos; // [esp+50h] [ebp-14h]
	int texelAddr; // [esp+5Ch] [ebp-8h]
	int pixel; // eax
	int tex_x; // eax
	int tex_y; // [esp+58h] [ebp-Ch]
	int tex_x_cur; // [esp+54h] [ebp-10h]
	Uint16* data = (Uint16*)ptr;
	// Must be divisible by 4
	if (height / 4 > 0)
	{
		tex_x = width / 4;
		tex_y = height / 4;
		ypos = 2;
		do
		{
			if (tex_x > 0)
			{
				xpos = 0;
				tex_x_cur = tex_x;
				do
				{
					for (int i = 0; i < 4; ++i)
					{
						texelAddr = GetLockedTexelAddress(
							D3DFMT_A8R8G8B8,
							xpos + i,
							ypos - 2,
							mPitch);
						pixel = GvrDecodePixelArgb5a3(*data++);
						*(uint32_t*)((char*)pBaseAddr + texelAddr) = pixel;
					}
					for (int j = 0; j < 4; ++j)
					{
						texelAddr = GetLockedTexelAddress(
							D3DFMT_A8R8G8B8,
							xpos + j,
							ypos - 1,
							mPitch);
						pixel = GvrDecodePixelArgb5a3(*data++);
						*(uint32_t*)((char*)pBaseAddr + texelAddr) = pixel;
					}
					for (int k = 0; k < 4; ++k)
					{
						texelAddr = GetLockedTexelAddress(
							D3DFMT_A8R8G8B8,
							xpos + k,
							ypos,
							mPitch);
						pixel = GvrDecodePixelArgb5a3(*data++);
						*(uint32_t*)((char*)pBaseAddr + texelAddr) = pixel;
					}
					for (int l = 0; l < 4; ++l)
					{
						texelAddr = GetLockedTexelAddress(
							D3DFMT_A8R8G8B8,
							xpos + l,
							ypos + 1,
							mPitch);
						pixel = GvrDecodePixelArgb5a3(*data++);
						*(uint32_t*)((char*)pBaseAddr + texelAddr) = pixel;
					}
					xpos += 4;
					--tex_x_cur;
				} while (tex_x_cur);
			}
			ypos += 4;
			--tex_y;
		} while (tex_y);
	}
	//return result;
}

// Pixel decoding function: DXT1
void GvrDecodeDXT1(void* data, Sint32 width, Sint32 height, void* pBaseAddr, Sint32 mPitch)
{
	int result; // eax
	unsigned __int8* v7; // ebp
	unsigned __int8* v8; // edi
	unsigned __int8* v10; // ebp
	unsigned __int8* v12; // edi
	Uint32 pos = 0; // [esp+28h] [ebp-10h]
	int i; // [esp+34h] [ebp-4h]
	int a2 = (int)data;
	result = height / 8;
	for (i = result; pos < i; ++pos)
	{
		v7 = (unsigned char*)pBaseAddr
			+ GetLockedTexelAddress(
				D3DFMT_DXT1,
				0,
				2 * pos,
				mPitch);
		v8 = (unsigned char*)pBaseAddr
			+ GetLockedTexelAddress(
				D3DFMT_DXT1,
				0,
				2 * pos + 1,
				mPitch);
		if (width / 8 > 0)
		{
			int v14 = width / 8;
			do
			{
				int v9 = 2;
				do
				{
					*v7 = *(unsigned __int8*)(a2 + 1);
					v7[1] = *(unsigned __int8*)a2;
					v10 = v7 + 1;
					*++v10 = *(unsigned __int8*)(a2 + 3);
					*++v10 = *(unsigned __int8*)(a2 + 2);
					*++v10 = (4 * ((16 * *(unsigned __int8*)(a2 + 4)) | *(unsigned __int8*)(a2 + 4) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 4) >> 4) | *(unsigned __int8*)(a2 + 4) & 0x30) >> 2);
					*++v10 = (4 * ((16 * *(unsigned __int8*)(a2 + 5)) | *(unsigned __int8*)(a2 + 5) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 5) >> 4) | *(unsigned __int8*)(a2 + 5) & 0x30) >> 2);
					v10[1] = (4 * ((16 * *(unsigned __int8*)(a2 + 6)) | *(unsigned __int8*)(a2 + 6) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 6) >> 4) | *(unsigned __int8*)(a2 + 6) & 0x30) >> 2);
					v10 += 2;
					*v10 = (4 * ((16 * *(unsigned __int8*)(a2 + 7)) | *(unsigned __int8*)(a2 + 7) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 7) >> 4) | *(unsigned __int8*)(a2 + 7) & 0x30) >> 2);
					v7 = v10 + 1;
					a2 += 8;
					--v9;
				} while (v9);
				int v11 = 2;
				do
				{
					*v8 = *(unsigned __int8*)(a2 + 1);
					v8[1] = *(unsigned __int8*)a2;
					v12 = v8 + 1;
					*++v12 = *(unsigned __int8*)(a2 + 3);
					*++v12 = *(unsigned __int8*)(a2 + 2);
					*++v12 = (4 * ((16 * *(unsigned __int8*)(a2 + 4)) | *(unsigned __int8*)(a2 + 4) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 4) >> 4) | *(unsigned __int8*)(a2 + 4) & 0x30) >> 2);
					*++v12 = (4 * ((16 * *(unsigned __int8*)(a2 + 5)) | *(unsigned __int8*)(a2 + 5) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 5) >> 4) | *(unsigned __int8*)(a2 + 5) & 0x30) >> 2);
					v12[1] = (4 * ((16 * *(unsigned __int8*)(a2 + 6)) | *(unsigned __int8*)(a2 + 6) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 6) >> 4) | *(unsigned __int8*)(a2 + 6) & 0x30) >> 2);
					v12 += 2;
					*v12 = (4 * ((16 * *(unsigned __int8*)(a2 + 7)) | *(unsigned __int8*)(a2 + 7) & 0xC)) | ((unsigned __int8)((*(unsigned __int8*)(a2 + 7) >> 4) | *(unsigned __int8*)(a2 + 7) & 0x30) >> 2);
					v8 = v12 + 1;
					a2 += 8;
					--v11;
				} while (v11);
				--v14;
			} while (v14);
		}
		result = pos + 1;
	}
	//return result;
}

// Pixel decoding function: ARGB8888
void GvrDecodeArgb8888(void* ptr, Sint32 width, Sint32 height, void* pBits, Sint32 pitch)
{
	char* data = (char*)ptr;
	char* bits = (char*)pBits;
	int x;
	int y = height;
	if (height > 0)
	{
		x = 4 * width;
		do
		{
			memcpy(bits, data, pitch);
			data += x;
			bits += pitch;
			--y;
		} while (y);
	}
}

// Pixel decoding function: Indexed4
void GvrDecodeIndex4(void* ptr, Sint32 width, Sint32 height, void* pBits, Sint32 pitch)
{
	int data = (int)ptr;
	int v7; // eax
	unsigned char* v8; // ecx
	int v9; // ebp
	unsigned char* v10; // [esp+4h] [ebp-14h]
	int v11; // [esp+8h] [ebp-10h]
	int v12; // [esp+Ch] [ebp-Ch]
	int v13; // [esp+10h] [ebp-8h]

	if (height / 8 > 0)
	{
		v7 = width / 8;
		v11 = (int)pBits + 6;
		v13 = height / 8;
		do
		{
			if (v7 > 0)
			{
				v10 = (unsigned __int8*)v11;
				v12 = v7;
				do
				{
					v8 = v10;
					v9 = 8;
					do
					{
						*(v8 - 6) = (unsigned char)-(*(unsigned __int8*)data >> 4) / 15;
						*(v8 - 5) = (unsigned char)-(*(unsigned __int8*)data & 0xF) / 15;
						*(v8 - 4) = (unsigned char)-(*(unsigned __int8*)(data + 1) >> 4) / 15;
						*(v8 - 3) = (unsigned char)-(*(unsigned __int8*)(data + 1) & 0xF) / 15;
						*(v8 - 2) = (unsigned char)-(*(unsigned __int8*)(data + 2) >> 4) / 15;
						*(v8 - 1) = (unsigned char)-(*(unsigned __int8*)(data + 2) & 0xF) / 15;
						*v8 = (unsigned char)-(*(unsigned __int8*)(data + 3) >> 4) / 15;
						v8[1] = 255 * (*(unsigned __int8*)(data + 3) & 0xF) / 15;
						data += 4;
						v8 += pitch;
						--v9;
					} while (v9);
					v10 += 8;
					--v12;
				} while (v12);
				v7 = width / 8;
			}
			v11 += 8 * pitch;
			--v13;
		} while (v13);
	}
}

// GVR texture format array (for retrieving the decoding function index)
Uint32 TEX_LOAD_FUNC_INDEX_GVR[] = {
	0,
	GJD_TEXFMT_DXT1,
	GJD_TEXFMT_ARGB_5A3,
	GJD_TEXFMT_ARGB_8888,
	GJD_TEXFMT_PALETTIZE4,
	GJD_TEXFMT_PALETTIZE8
};

// GVR pixel decoding func array
void (*TEX_LOAD_FUNC_GVR[6])(void* ptr, Sint32 width, Sint32 height, void* pBaseAddr, Sint32 mPitch) =
{
	NULL,
	GvrDecodeDXT1,
	GvrDecodeArgb5a3,
	GvrDecodeArgb8888,
	GvrDecodeIndex4,
	NULL // GvrDecodeIndex8 unimplemented
};

// GVR pixel format array (for converting to D3D formats and setting bytes per pixel to calculate texture size)
Uint32 PIXEL_FORMAT_TBL_GVR[] = {
	GJD_PIXELFORMAT_DXT1, D3DFMT_DXT1, 0,
	GJD_PIXELFORMAT_OTHER, D3DFMT_A8R8G8B8, 4, // RGBA8888 and RGB5A3
	GJD_PIXELFORMAT_PAL_4BPP, D3DFMT_P8, 1,
	GJD_PIXELFORMAT_PAL_8BPP, D3DFMT_P8, 1
};

// Retrieves the decoding function index for the specified texture format
Sint32 GetLoadFuncIndexGvr(Sint32 fmt)
{
	Uint32* index = TEX_LOAD_FUNC_INDEX_GVR;
	Sint32 i = 0;
	while (*index != fmt)
	{
		++i;
		++index;
		if (i == 9)
		{
			return 0;
		}
	}
	return i;
}

// Converts NJS_TEXMEMLIST to a D3D texture (no mipmap)
IDirect3DTexture8* __cdecl stConvertSurfaceGvr(NJS_TEXMEMLIST* tex, void* data, D3DFORMAT format, Uint32 njfmt, Uint32 bpp)
{
	NJS_TEXSURFACE* pTexSurface = &tex->texinfo.texsurface;
	IDirect3DTexture8* d3dtex = (IDirect3DTexture8*)pTexSurface->pSurface;

	if (!d3dtex)
	{
		if (IDirect3DDevice8_CreateTexture(_st_d3d_device_, pTexSurface->nWidth, pTexSurface->nHeight,
			1, 0, format, D3DPOOL_MANAGED, &d3dtex) != D3D_OK)
		{
			Exit();
		}
	}

	Sint32 index = GetLoadFuncIndexGvr(njfmt);

	D3DLOCKED_RECT rect;
	if (IDirect3DTexture8_LockRect(d3dtex, 0, &rect, 0, 0) != D3D_OK)
	{
		Exit();
	}

	void (*load_func)(void* data, Sint32 width, Sint32 height, void* pBaseAddr, Sint32 mPitch) = NULL;

	load_func = TEX_LOAD_FUNC_GVR[index];

	if (load_func == NULL)
	{
		//PrintDebug("Index: %d, format: %X\n", index, njfmt);
		//MessageBox(NULL, L"No func", L"F", 0);
		Exit();
	}

	load_func(data, pTexSurface->nWidth, pTexSurface->nHeight, rect.pBits, rect.Pitch);
	IDirect3DTexture8_UnlockRect(d3dtex, 0);

	return d3dtex;
}

// Converts NJS_TEXMEMLIST to a D3D texture (mipmap)
IDirect3DTexture8* __cdecl stConvertSurfaceGvrMM(NJS_TEXMEMLIST* tex, void* data, D3DFORMAT format, Uint32 njfmt, Uint32 bpp)
{
	// Hack: Do the same stuff as stConvertSurfaceGvr but generate mip level 0 instead of 1 + apply filter. 
	// The correct implementation should load the texture's built-in mipmaps instead.
	NJS_TEXSURFACE* pTexSurface = &tex->texinfo.texsurface;
	IDirect3DTexture8* d3dtex = (IDirect3DTexture8*)pTexSurface->pSurface;

	if (!d3dtex)
	{
		if (IDirect3DDevice8_CreateTexture(_st_d3d_device_, pTexSurface->nWidth, pTexSurface->nHeight,
			0, 0, format, D3DPOOL_MANAGED, &d3dtex) != D3D_OK) // Level 0
		{
			Exit();
		}
	}

	Sint32 index = GetLoadFuncIndexGvr(njfmt);

	D3DLOCKED_RECT rect;
	if (IDirect3DTexture8_LockRect(d3dtex, 0, &rect, 0, 0) != D3D_OK)
	{
		Exit();
	}

	void (*load_func)(void* data, Sint32 width, Sint32 height, void* pBaseAddr, Sint32 mPitch) = NULL;

	load_func = TEX_LOAD_FUNC_GVR[index];

	if (load_func == NULL)
	{
		//PrintDebug("Index: %d, format: %X\n", index, njfmt);
		//MessageBox(NULL, L"No func", L"F", 0);
		Exit();
	}

	load_func(data, pTexSurface->nWidth, pTexSurface->nHeight, rect.pBits, rect.Pitch);
	IDirect3DTexture8_UnlockRect(d3dtex, 0);

	// Apply the mipmap filter, otherwise the textures will fade out in the distance. Remove if internal mipmaps are loaded.
	if ((pTexSurface->fSurfaceFlags & 0x80000000) != 0)
	{
		D3DXFilterTexture(d3dtex, 0, 0, -1);
	}
	return d3dtex;

	// Leftovers from the attempt to read built-in mipmaps
	/*
	NJS_TEXSURFACE* pTexSurface = &tex->texinfo.texsurface;
	IDirect3DTexture8* d3dtex = (IDirect3DTexture8*)pTexSurface->pSurface;

	if (!d3dtex)
	{
		if (IDirect3DDevice8_CreateTexture(_st_d3d_device_, pTexSurface->nWidth, pTexSurface->nHeight,
			0, 0, format, D3DPOOL_MANAGED, &d3dtex) != D3D_OK)
		{
			Exit();
		}
	}

	Sint32 index = GetLoadFuncIndexGvr(pixelfmt);

	Uint32 mipmapcount = 0;
	for (Int i = 0; i < 11; ++i)
	{
		const Uint32* m = &MIPMAP_LEVEL_TBL[i * 2];

		if (m[0] == pTexSurface->nWidth)
		{
			mipmapcount = m[1];
		}
	}

	void (*load_func)(void* ptr, Sint32 width, Sint32 height, void* pBaseAddr, Sint32 mPitch, Uint32 level) = NULL;

	load_func = TEX_LOAD_FUNC_GVR_MM[index];

	if (load_func == NULL)
	{
		MessageBox(NULL, L"No func", L"A", 0);
		Exit();
	}

	for (Uint32 i = 1; i <= mipmapcount; ++i)
	{
		LPDIRECT3DSURFACE8 d3dsurface;

		IDirect3DTexture8_GetSurfaceLevel(d3dtex, i - 1, &d3dsurface);
		if (!d3dsurface)
		{
			MessageBox(NULL, L"No surface", L"A", 0);
			Exit();
		}
		D3DLOCKED_RECT rect;
		if (IDirect3DSurface8_LockRect(d3dsurface, &rect, 0, 0) != D3D_OK)
		{
			MessageBox(NULL, L"No rect", L"A", 0);
			Exit();
		}
		if (pixelfmt == GJD_TEXFMT_DXT1)
		{
			PrintDebug("TEX");
			load_func(data, pTexSurface->nWidth, pTexSurface->nHeight, rect.pBits, bpp, mipmapcount - i);
		}
		IDirect3DSurface8_UnlockRect(d3dsurface);
		IDirect3DSurface8_Release(d3dsurface);
	}

	return d3dtex;
	*/
}

// Checks texture format and starts D3D texture conversion
void stLoadTextureGvr(NJS_TEXMEMLIST* tex, void* surface)
{
	float sizeMultiplier = 0.0f; // [esp+10h] [ebp-8h]

	if (tex)
	{
		NJS_TEXSURFACE* pTexSurface = &tex->texinfo.texsurface;
		Uint32 fmt = pTexSurface->Type;
		// Paletted
		if (pTexSurface->PixelFormat == GJD_PIXELFORMAT_PAL_4BPP || pTexSurface->PixelFormat == GJD_PIXELFORMAT_PAL_8BPP)
		{

			Uint32 height = tex->texinfo.texsurface.nHeight;
			Uint32 width = tex->texinfo.texsurface.nWidth;

			// This creates an empty surface to apply the palette to.
			void* mem = MAlloc(width * height);
			if (pTexSurface->PixelFormat == GJD_PIXELFORMAT_PAL_4BPP)
			{
				_stTwiddledToLinear4bpp(mem, surface, width, height, 1);
			}
			else
			{
				_stTwiddledToLinear(mem, surface, width, height, 1);
			}

			tex->texinfo.texsurface.pPhysical = (Uint32*)mem;

			// Decode the indexed texture
			void* surface_d3d = stConvertSurfaceGvr(tex, surface, D3DFMT_A8R8G8B8, fmt, 1);
			if (!surface_d3d)
				surface_d3d = stConvertSurfaceGvr(tex, surface, D3DFMT_A4R4G4B4, fmt, 1);
			pTexSurface->pSurface = (Uint32*)surface_d3d;
			pTexSurface->TextureSize = pTexSurface->nWidth * pTexSurface->nHeight * 1;
		}
		// Non-paletted
		else
		{
			for (int i = 0; i < 4; ++i)
			{
				const Uint32* p = &PIXEL_FORMAT_TBL_GVR[i * 3];
				if (pTexSurface->PixelFormat != p[0])
					continue;

				if (p[1] == D3DFMT_UNKNOWN)
				{
					exit(0);
					return;
				}

				bool mipmap = true; // Since all of them can have mipmaps
				sizeMultiplier = p[2];
				if (p[1] == D3DFMT_DXT1) // Special case for DXT1
					sizeMultiplier = 0.5f;

				void* surface_d3d;
				if (mipmap)
					surface_d3d = stConvertSurfaceGvrMM(tex, surface, (D3DFORMAT)p[1], fmt, p[2]);
				else
					surface_d3d = stConvertSurfaceGvr(tex, surface, (D3DFORMAT)p[1], fmt, p[2]);

				tex->texinfo.texsurface.pSurface = (Uint32*)surface_d3d;
				tex->texinfo.texsurface.TextureSize = pTexSurface->nWidth * pTexSurface->nHeight * sizeMultiplier;

				break;
			}
		}
	}
	else
	{
		Exit();
	}
}

// Checks GBIX cache and loads the specified texture 
NJS_TEXMEMLIST* gjLoadTextureTexMemList(void* addr, int gbix)
{
	NJS_TEXMEMLIST memlist; // [esp+4h] [ebp-44h]

	memlist.globalIndex = gbix;
	memlist.bank = -1;
	memlist.texaddr = 0;
	memlist.count = 0;
	memlist.texinfo.texaddr = addr;

	gjLoadTextureTexMemListAnalize(&memlist);

	NJS_TEXMEMLIST* cached = njSearchEmptyTexMemList(memlist.globalIndex);

	if (cached)
	{
		if (!cached->count)
		{
			memcpy(cached, &memlist, sizeof(NJS_TEXMEMLIST));
			void* surface = cached->texinfo.texsurface.pSurface;
			cached->texinfo.texsurface.pSurface = 0;
			stLoadTextureGvr(cached, surface);
		}
		++cached->count;
	}
	return cached;
}

signed int njLoadTextureGvmMemory(void* data, NJS_TEXLIST* texList)
{
	int* v2; // edi
	int* dataOffset_; // ecx
	unsigned __int16 v4; // si
	int v6; // eax
	int v7; // eax
	unsigned __int16 v8; // ax
	int gbix; // eax
	int v11; // ecx
	int* dataOffset; // [esp+14h] [ebp-8h]
	sStChunkPVMH* pvmchunk = 0;
	v2 = (int*)data;
	if (data == 0 || texList == 0)
	{
		return -1;
	}
	dataOffset_ = (int*)data;
	dataOffset = (int*)data;
	if (data)
	{
		dataOffset = (int*)((char*)data + v2[1] + 8);
		dataOffset_ = dataOffset;
	}
	v4 = 0;
	if (!data)
	{
		return -1;
	}
	while (1)
	{
		v6 = *v2;
		if (*v2 <= (unsigned int)'TRVG')
		{
			break;
		}
		if (v6 == 'TRVP')                     // PVRT
		{
			//PrintDebug("Huh?\n");
			goto LABEL_11;
		}
	LABEL_13:
		v2 = dataOffset_;
		if (dataOffset_)
		{
			dataOffset = (int*)((char*)dataOffset_ + dataOffset_[1] + 8);
			dataOffset_ = dataOffset;
		}
		if (!v2)
		{
			return -1;
		}
	}
	if (*v2 != 'TRVG')                        // 'GVRT'
	{
		v7 = v6 - 'HMVG';                       // 'GVMH'
		if (!v7 || v7 == 9)
		{
			*((Uint16*)v2 + 4) = _byteswap_ushort(*((Uint16*)v2 + 4));
			v8 = _byteswap_ushort(*((Uint16*)v2 + 5));
			*((Uint16*)v2 + 5) = v8;
			pvmchunk = (sStChunkPVMH*)v2;
			texList->nbTexture = v8;
		}
		goto LABEL_13;
	}
LABEL_11:
	gbix = gjGetGlobalIndexFromGVMH(pvmchunk, v4);
	v11 = v4++;
	texList->textures[v11].texaddr = (Uint32)gjLoadTextureTexMemList(v2, gbix);
	if (v4 != texList->nbTexture)
	{
		dataOffset_ = dataOffset;
		goto LABEL_13;
	}
	return 1;
}

// Removed for now
/*
void (*TEX_LOAD_FUNC_GVR_MM[6])(void* ptr, Sint32 width, void* pBaseAddr, Sint32 mPitch, Uint32 level) =
{
	NULL,
	GvrDecodeDXT1_MM,
	GvrDecodeArgb5a3_MM, //GvrDecodeArgb5a3,//GvrDecodeArgb5a3_MM,
	GvrDecodeArgb8888_MM, //GvrDecodeArgb8888,//GvrDecodeArgb8888_MM,
	GvrDecodeIndex4_MM, //GvrDecodeIndex4,//GvrDecodeIndex4_MM,
	GvrDecodeIndex8_MM, //GvrDecodeIndex8,//GvrDecodeIndex8_MM
};
*/