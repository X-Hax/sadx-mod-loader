#include "stdafx.h"
#include "intrin.h"
#include <Windows.h>
#include "d3d8.h"
#include "d3dx8.h"
#include "SADXModLoader.h"
#include "gvm.h"
#include "AutoMipmap.h"

// Original code decompiled by Exant, cleaned up by PkR

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
	unsigned int result = *(_DWORD*)((int)chunk + 8 + start * (n + 1));
	return _byteswap_ulong(result);
}

// Sets up the texture surface in NJS_TEXMEMLIST using specified texture data pointer
void gjLoadTextureGVRTAnalize(NJS_TEXMEMLIST* texmemlist, Uint8* addr)
{
	NJS_TEXSURFACE* result; // eax

	result = &texmemlist->texinfo.texsurface;
	result->BitDepth = 0;

	Uint32 value = _byteswap_ulong(*(Uint32*)(addr + 8));
	Uint16 flags = value & 0xFF00;
	Uint16 surfacetype = value & 0xFF;
	result->Type = surfacetype;
	switch (surfacetype)
	{
	case GJD_TEXFMT_ARGB_5A3:
		result->PixelFormat = GJD_TEXFMT_ARGB_5A3;
		break;
	case GJD_TEXFMT_PALETTIZE4:
		result->PixelFormat = GJD_TEXFMT_PALETTIZE4;
		break;
	case GJD_TEXFMT_DXT1:
		result->PixelFormat = GJD_TEXFMT_DXT1;
		break;
	}
	result->fSurfaceFlags = GJD_SURFACEFLAGS_MIPMAPED;
	if ((flags & 0x200) != 0)
	{
		result->fSurfaceFlags |= GJD_SURFACEFLAGS_PALETTIZED;
	}
	result->nWidth = _byteswap_ushort(*(Uint16*)(addr + 0xC));
	result->nHeight = _byteswap_ushort(*(Uint16*)(addr + 14));
	result->pSurface = (Uint32*)(addr + 16);
	result->TextureSize = *(Uint32*)(addr + 4) - 8;
	//PrintDebug("Orig: %08X, Value: %08X, Type: %X, Flags: %X\n", *(Uint32*)(addr + 8), v4, surfacetype, flags);
	//PrintDebug("Width %d, Height %d\n", result->nWidth, result->nHeight);
}

// Parses binary data and sets GBIX or reads the texture
void gjLoadTextureTexMemListAnalize(NJS_TEXMEMLIST* memlist)
{
	Uint8* addr = (Uint8*)memlist->texinfo.texaddr;
	while (1)
	{
		int value = *(int*)addr;

		if (value == 'TRVG')
		{
			gjLoadTextureGVRTAnalize(memlist, (Uint8*)addr);
			break;
		}
		else if (value == 'XIBG')
		{
			memlist->globalIndex = _byteswap_ulong(*(int*)(addr + 4));
		}
		if (!value)
		{
			break;
		}
		addr += 1;
	}
}

int GetLockedTexelAddress(int format, int a2, int a3, int a4, int a5)
{
	int result; // eax
	signed int offset = 0;

	switch (format)
	{
	case 1:
		offset = 24;
		break;
	case 2:
	case 3:
	case 9:
	case 10:
	case 11:
	case 20:
	case 21:
	case 22:
	case 24:
	case 26:
	case 28:
		offset = 32;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
	case 19:
	case 23:
	case 25:
		offset = 16;
		break;
	case 8:
	case 13:
	case 15:
	case 16:
	case 17:
	case 18:
		offset = 8;
		break;
	case 12:
	case 27:
	case 29:
		offset = 64;
		break;
	case 14:
		offset = 4;
		break;
	case 30:
		offset = 128;
		break;
	}
	if (offset > 4)
	{
		result = a2 * ((offset + 7) / 8);
	}
	else
	{
		result = a2 * offset / 8;
	}
	return a4 * a3 + result;
}

// Decodes RGB555 or ARGB4333 pixels
int GvrDecodePixelArgb5a3(__int16 a1)
{
	unsigned __int16 v1; // ax
	unsigned int result; // eax

	v1 = _byteswap_ushort(a1);
	// ARGB3444
	if ((a1 & 0x80u) == 0)
	{
		((uint8_t*)&result)[3] = (uint8_t)(((v1 >> 12) & 0x07) * 0xFF / 0x07);
		((uint8_t*)&result)[2] = (uint8_t)(((v1 >> 8) & 0x0F) * 0xFF / 0x0F);
		((uint8_t*)&result)[1] = (uint8_t)(((v1 >> 4) & 0x0F) * 0xFF / 0x0F);
		((uint8_t*)&result)[0] = (uint8_t)(((v1 >> 0) & 0x0F) * 0xFF / 0x0F);
		//result = (unsigned __int8)((char)-(v1 & 0xF) / 15) | (((unsigned __int8)((char)-((unsigned __int8)v1 >> 4) / 15) | (((unsigned __int8)((char)-(HIBYTE(v1) & 0xF) / 15) | ((unsigned __int8)((char)-((v1 >> 12) & 7) / 7) << 8)) << 8)) << 8);
	}
	// RGB555
	else
	{
		((uint8_t*)&result)[3] = 0xFF;
		((uint8_t*)&result)[2] = (uint8_t)(((v1 >> 10) & 0x1F) * 0xFF / 0x1F);
		((uint8_t*)&result)[1] = (uint8_t)(((v1 >> 5) & 0x1F) * 0xFF / 0x1F);
		((uint8_t*)&result)[0] = (uint8_t)(((v1 >> 0) & 0x1F) * 0xFF / 0x1F);
		//result = (unsigned __int8)((char)-(v1 & 0x1F) / 31) | (((unsigned __int8)((char)-((v1 >> 5) & 0x1F) / 31) | ((((char)-((v1 >> 10) & 0x1F) / 31) | 0xFFFFFF00) << 8)) << 8);
	}
	return result;
}

// Pixel decoding functions

void GvrDecodeArgb5a3(int format, signed int a1, __int16* a2, int a3, signed int a4)
{
	int result; // eax
	int v7; // ebx
	int i; // ebp
	int v9; // eax
	int j; // ebp
	int v11; // eax
	int k; // ebp
	int v13; // eax
	int l; // ebp
	int v15; // eax
	int v16; // [esp+50h] [ebp-14h]
	int v17; // [esp+54h] [ebp-10h]
	int v18; // [esp+58h] [ebp-Ch]
	int v19; // [esp+5Ch] [ebp-8h]
	int v20; // [esp+5Ch] [ebp-8h]
	int v21; // [esp+5Ch] [ebp-8h]
	int v22; // [esp+5Ch] [ebp-8h]

	result = ((a1 >> 31) & 3) + a1;
	if (a1 / 4 > 0)
	{
		result = a4 / 4;
		v16 = 2;
		v18 = a1 / 4;
		do
		{
			if (result > 0)
			{
				v7 = 0;
				v17 = result;
				do
				{
					for (i = 0; i < 4; ++i)
					{
						v19 = GetLockedTexelAddress(
							format,
							v7 + i,
							v16 - 2,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v9 = GvrDecodePixelArgb5a3(*a2++);
						*(uint32_t*)(v19 + *(uint32_t*)a3) = v9;
					}
					for (j = 0; j < 4; ++j)
					{
						v20 = GetLockedTexelAddress(
							format,
							v7 + j,
							v16 - 1,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v11 = GvrDecodePixelArgb5a3(*a2++);
						*(uint32_t*)(v20 + *(uint32_t*)a3) = v11;
					}
					for (k = 0; k < 4; ++k)
					{
						v21 = GetLockedTexelAddress(
							format,
							v7 + k,
							v16,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v13 = GvrDecodePixelArgb5a3(*a2++);
						*(uint32_t*)(v21 + *(uint32_t*)a3) = v13;
					}
					for (l = 0; l < 4; ++l)
					{
						v22 = GetLockedTexelAddress(
							format,
							v7 + l,
							v16 + 1,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v15 = GvrDecodePixelArgb5a3(*a2++);
						*(uint32_t*)(v22 + *(uint32_t*)a3) = v15;
					}
					v7 += 4;
					--v17;
				} while (v17);
				result = a4 / 4;
			}
			v16 += 4;
			--v18;
		} while (v18);
	}
	//return result;
}

void GvrDecodeDXT1(int format, signed int a1, void* pp, LockedSurface* a3, signed int a4)
{
	LockedSurface* v4; // ebx
	int result; // eax
	unsigned __int8* v7; // ebp
	unsigned __int8* v8; // edi
	int v9; // ecx
	unsigned __int8* v10; // ebp
	int v11; // ecx
	unsigned __int8* v12; // edi
	int v13; // [esp+28h] [ebp-10h]
	int v14; // [esp+2Ch] [ebp-Ch]
	int i; // [esp+34h] [ebp-4h]
	int a2 = (int)pp;
	v4 = a3;
	result = a1 / 8;
	v13 = 0;
	for (i = result; v13 < i; ++v13)
	{
		v7 = (unsigned char*)v4->m_pBaseAddr
			+ GetLockedTexelAddress(
				format,
				0,
				2 * v13,
				v4->m_Pitch,
				v4->m_MipmapLevel);
		v8 = (unsigned char*)v4->m_pBaseAddr
			+ GetLockedTexelAddress(
				format,
				0,
				2 * v13 + 1,
				v4->m_Pitch,
				v4->m_MipmapLevel);
		if (a4 / 8 > 0)
		{
			v14 = a4 / 8;
			do
			{
				v9 = 2;
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
				v11 = 2;
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
			v4 = a3;
		}
		result = v13 + 1;
	}
	//return result;
}

void GvrDecodeArgb8888(char* a2, char* result, unsigned int a3, char* a1, int a5)
{
	int v5; // ebp
	int v8; // [esp+14h] [ebp+8h]

	v5 = a5;
	if (a5 > 0)
	{
		v8 = 4 * (_DWORD)result;
		do
		{
			memcpy(a1, a2, a3);
			a2 += v8;
			a1 += a3;
			--v5;
		} while (v5);
	}
}

void GvrDecodeIndex4(signed int a1, int a2, int a3, int a4, signed int a5)
{
	int v7; // eax
	unsigned char* v8; // ecx
	int v9; // ebp
	unsigned char* v10; // [esp+4h] [ebp-14h]
	int v11; // [esp+8h] [ebp-10h]
	int v12; // [esp+Ch] [ebp-Ch]
	int v13; // [esp+10h] [ebp-8h]

	if (a1 / 8 > 0)
	{
		v7 = a5 / 8;
		v11 = a4 + 6;
		v13 = a1 / 8;
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
						*(v8 - 6) = (unsigned char)-(*(unsigned __int8*)a2 >> 4) / 15;
						*(v8 - 5) = (unsigned char)-(*(unsigned __int8*)a2 & 0xF) / 15;
						*(v8 - 4) = (unsigned char)-(*(unsigned __int8*)(a2 + 1) >> 4) / 15;
						*(v8 - 3) = (unsigned char)-(*(unsigned __int8*)(a2 + 1) & 0xF) / 15;
						*(v8 - 2) = (unsigned char)-(*(unsigned __int8*)(a2 + 2) >> 4) / 15;
						*(v8 - 1) = (unsigned char)-(*(unsigned __int8*)(a2 + 2) & 0xF) / 15;
						*v8 = (unsigned char)-(*(unsigned __int8*)(a2 + 3) >> 4) / 15;
						v8[1] = 255 * (*(unsigned __int8*)(a2 + 3) & 0xF) / 15;
						a2 += 4;
						v8 += a3;
						--v9;
					} while (v9);
					v10 += 8;
					--v12;
				} while (v12);
				v7 = a5 / 8;
			}
			v11 += 8 * a3;
			--v13;
		} while (v13);
	}
}

DataPointer(IDirect3DDevice8*, _st_d3d_device_, 0x3D128B0); // Direct3D device

IDirect3DTexture8* __cdecl stConvertSurfaceGvr(NJS_TEXMEMLIST* tex, int data, int pixelformat, D3DFORMAT dataformatmaybe)
{
	NJS_TEXSURFACE* pTexSurface = &tex->texinfo.texsurface;
	IDirect3DTexture8* d3dtex = (IDirect3DTexture8*)pTexSurface->pSurface;
	Sint32 width = pTexSurface->nWidth;
	Sint32 height = pTexSurface->nHeight;
	D3DFORMAT formatmaybe = (D3DFORMAT)0;
	LockedSurface surface; // [esp+48h] [ebp-1Ch] BYREF
	if (!pTexSurface->pSurface)
	{
		if (width > 0 && height > 0)
		{
			if (IDirect3DDevice8_CreateTexture(_st_d3d_device_, width, height,
				mipmap::auto_mipmaps_enabled() ? 0 : 1, 0,
				dataformatmaybe, D3DPOOL_MANAGED, &d3dtex) != D3D_OK)
			{
				Exit();
			}
			D3DSURFACE_DESC desc;
			d3dtex->GetLevelDesc(0, &desc);
			formatmaybe = desc.Format;

			int v25 = 0;
			int* v26 = (int*)PAKFormatLookupTable + 1;
			while (formatmaybe != *v26)
			{
				++v25;
				v26 += 2;
				if (v25 >= 0x1F)
				{
					break;
				}
			}
			formatmaybe = ((D3DFORMAT*)PAKFormatLookupTable)[2 * v25];
			//PrintDebug("PLEASE GIVE ME FORMAT %d \n", formatmaybe);
		}
	}
	surface.m_pBaseAddr = 0;
	surface.m_Pitch = 0;
	surface.m_pLockedTexture = 0;
	D3DLOCKED_RECT rect;
	if (IDirect3DTexture8_LockRect(d3dtex, 0, &rect, 0, 0) != D3D_OK)
	{
		Exit();
	}
	surface.m_pLockedTexture = d3dtex;
	surface.m_pBaseAddr = rect.pBits;
	surface.m_Pitch = rect.Pitch;
	surface.m_MipmapLevel = 0;
	//PrintDebug("locked rect\n");
	switch (pixelformat)
	{
	case GJD_TEXFMT_DXT1:
		GvrDecodeDXT1(formatmaybe, height, (void*)data, &surface, width);
		break;
	case GJD_TEXFMT_ARGB_5A3:
		GvrDecodeArgb5a3(formatmaybe, height, (__int16*)data, (int)&surface, width);
		break;
	case GJD_TEXFMT_ARGB_8888:
		GvrDecodeArgb8888((char*)data, (char*)width, surface.m_Pitch, (char*)surface.m_pBaseAddr, height);
		break;
	case GJD_TEXFMT_PALETTIZE4:
		GvrDecodeIndex4(height, data, surface.m_Pitch, (int)surface.m_pBaseAddr, width);
		break;
	}

	d3dtex->UnlockRect(0);
	if ((pTexSurface->fSurfaceFlags & 0x80000000) != 0)
	{
		D3DXFilterTexture(d3dtex, 0, 0, -1);
	}
	return d3dtex;
}

// Checks texture format and starts D3D texture conversion
void stLoadTextureGvr(NJS_TEXMEMLIST* tex, void* surface)
{
	D3DFORMAT outfmt = (D3DFORMAT)0; // edi
	float sizeMultiplier = 0.0f; // [esp+10h] [ebp-8h]
	NJS_TEXSURFACE* pTexSurface = &tex->texinfo.texsurface;

	if (tex)
	{
		Uint32 fmt = pTexSurface->PixelFormat;
		switch (fmt)
		{
		case GJD_TEXFMT_DXT1:
			sizeMultiplier = 0.5f;
			outfmt = D3DFMT_DXT1;
			break;
		case GJD_TEXFMT_ARGB_5A3:
			sizeMultiplier = 4.0f;
			outfmt = D3DFMT_A8R8G8B8;
			break;
		case GJD_TEXFMT_ARGB_8888:
			sizeMultiplier = 4.0f;
			outfmt = D3DFMT_A8R8G8B8;
			break;
		case GJD_TEXFMT_PALETTIZE4:
			sizeMultiplier = 1.0f;
			outfmt = D3DFMT_A8;
			break;
		default:
			PrintDebug("Unknown pixel format: %X\n", fmt);
			break;
		}
		if (outfmt)
		{
			tex->texinfo.texsurface.pSurface = (Uint32*)stConvertSurfaceGvr(tex, (int)surface, fmt, outfmt);
			tex->texinfo.texsurface.TextureSize = pTexSurface->nWidth * pTexSurface->nHeight * sizeMultiplier;
		}
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
			PrintDebug("Huh?\n");
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