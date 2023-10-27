#include "stdafx.h"
#include "intrin.h"
#include <Windows.h>
#include "d3d8.h"
#include "d3dx8.h"
#include "SADXModLoader.h"

FunctionPointer(NJS_TEXMEMLIST*, njSearchEmptyTexMemList, (int gbix), 0x0077F5B0);

// Code decompiled by Exant

// Locked surface struct from sega of china api, imma use it cuz its easy to decompile like this
struct LockedSurface
{
	void* m_pBaseAddr;
	int m_Pitch;
	IDirect3DTexture8* m_pLockedTexture;
	int m_MipmapLevel;
	bool m_IsLocked;
};

unsigned char PAKFormatLookupTable[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x15, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x16, 0x00,
  0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00,
  0x05, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x06, 0x00,
  0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
  0x1A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x1C, 0x00,
  0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00,
  0x0A, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x0B, 0x00,
  0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
  0x24, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x32, 0x00,
  0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x44, 0x58, 0x54, 0x31,
  0x0F, 0x00, 0x00, 0x00, 0x44, 0x58, 0x54, 0x32, 0x10, 0x00,
  0x00, 0x00, 0x44, 0x58, 0x54, 0x33, 0x11, 0x00, 0x00, 0x00,
  0x44, 0x58, 0x54, 0x34, 0x12, 0x00, 0x00, 0x00, 0x44, 0x58,
  0x54, 0x35, 0x13, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00,
  0x14, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x15, 0x00,
  0x00, 0x00, 0x4B, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
  0x4D, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x50, 0x00,
  0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00,
  0x19, 0x00, 0x00, 0x00, 0x6F, 0x00, 0x00, 0x00, 0x1A, 0x00,
  0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00,
  0x71, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x72, 0x00,
  0x00, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x1E, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00
};

NJS_TEXSURFACE* sub_4307A0(NJS_TEXMEMLIST* a1, int a2)
{
	NJS_TEXSURFACE* result; // eax
	unsigned int v4; // ecx
	unsigned int v5; // edx
	Uint32 v6; // ecx
	Uint32 v7; // ecx

	result = &a1->texinfo.texsurface;
	result->BitDepth = 0;
	v4 = (*(_DWORD*)(a2 + 8));
	v5 = (((v4 << 16) | v4 & 0xFF00) << 8) | ((HIWORD(v4) | v4 & 0xFF0000) >> 8);
	v6 = HIBYTE(v4);
	result->Type = ((v4 >> 8) >> 8) >> 8;
	//PrintDebug("type %00x \n", result->Type);
	switch (result->Type)
	{
	case 5u:
		result->PixelFormat = 5;
		break;
	case 8u:
		result->PixelFormat = 8;
		break;
	case 14u:
		result->PixelFormat = 14;
		break;
	}
	result->fSurfaceFlags = 0x80000000;
	if ((v5 & 0x200) != 0)
	{
		result->fSurfaceFlags = 0x80008000;
	}
	result->nWidth = (unsigned __int16)_byteswap_ushort(*(__int16*)(a2 + 0xC));
	result->nHeight = (unsigned __int16)_byteswap_ushort(*(__int16*)(a2 + 14));
	v7 = *(_DWORD*)(a2 + 4) - 8;
	result->pSurface = (Uint32*)(a2 + 16);
	result->TextureSize = v7;
	//PrintDebug("width %d height %d \n", result->nWidth, result->nHeight);
	return result;
}

int njLoadTextureTexMemListAnalize(NJS_TEXMEMLIST* a1, int a2)
{
	int* v2; // esi
	int v3; // edx

	v2 = (int*)a2;
	if (!a2)
	{
		return 0;
	}
	v3 = *(_DWORD*)(a2 + 4) + a2 + 8;
	while (*v2 != 'TRVG')
	{
		if (*v2 == 'XIBG')
		{
			a1->globalIndex = (((v2[2] << 16) | v2[2] & 0xFF00) << 8) | ((unsigned int)(HIWORD(v2[2]) | v2[2] & 0xFF0000) >> 8);
		}
		v2 = (int*)v3;
		if (v3)
		{
			v3 += *(_DWORD*)(v3 + 4) + 8;
		}
		if (!v2)
		{
			return 0;
		}
	}
	sub_4307A0(a1, (int)v2);
	return *v2;
}

int sub_85F0E0(int a1, LockedSurface* a2, IDirect3DTexture8* textureData, int a4)
{
	char v4; // al
	int v5; // ecx
	void* v6; // eax
	void* v8; // [esp+18h] [ebp-8h] BYREF
	int v9; // [esp+1Ch] [ebp-4h] BYREF
	D3DLOCKED_RECT rect;
	if (!a2)
	{
		return 0;
	}
	v8 = 0;
	v9 = 0;
	v4 = textureData->LockRect(0, &rect, 0, 0);
	v5 = v9;
	v6 = v8;
	a2->m_pLockedTexture = textureData;
	a2->m_pBaseAddr = rect.pBits;
	a2->m_Pitch = rect.Pitch;
	a2->m_MipmapLevel = a1;
	return 1;
}

signed int sub_85E7E0(int a1)
{
	signed int result; // eax

	result = 0;
	switch (a1)
	{
	case 1:
		result = 24;
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
		result = 32;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
	case 19:
	case 23:
	case 25:
		result = 16;
		break;
	case 8:
	case 13:
	case 15:
	case 16:
	case 17:
	case 18:
		result = 8;
		break;
	case 12:
	case 27:
	case 29:
		result = 64;
		break;
	case 14:
		result = 4;
		break;
	case 30:
		result = 128;
		break;
	default:
		return result;
	}
	return result;
}

int GetLockedTexelOffset(int a1, int a2, int a3, int a4, int a5)
{
	signed int v5; // eax
	unsigned int v6; // edx
	int v7; // eax

	v5 = sub_85E7E0(a1);
	if (v5 > 4)
	{
		v7 = a2 * ((v5 + 7) / 8);
	}
	else
	{
		v7 = a2 * v5 / 8;
	}
	//v7 = a2 * ((v5 + 7) / 8);
	return a4 * a3 + v7;
}

int  sub_433320(int format, signed int a1, void* pp, LockedSurface* a3, signed int a4)
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
			+ GetLockedTexelOffset(
				format,
				0,
				2 * v13,
				v4->m_Pitch,
				v4->m_MipmapLevel);
		v8 = (unsigned char*)v4->m_pBaseAddr
			+ GetLockedTexelOffset(
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
	return result;
}

void sub_433730(char* a2, char* result, unsigned int a3, char* a1, int a5)
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

void sub_433770(signed int a1, int a2, int a3, int a4, signed int a5)
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

int __fastcall sub_4331F0(__int16 a1)
{
	unsigned __int16 v1; // ax
	unsigned int result; // eax

	v1 = _byteswap_ushort(a1);
	if ((a1 & 0x80u) == 0)
	{
		((uint8_t*)&result)[3] = (uint8_t)(((v1 >> 12) & 0x07) * 0xFF / 0x07);
		((uint8_t*)&result)[2] = (uint8_t)(((v1 >> 8) & 0x0F) * 0xFF / 0x0F);
		((uint8_t*)&result)[1] = (uint8_t)(((v1 >> 4) & 0x0F) * 0xFF / 0x0F);
		((uint8_t*)&result)[0] = (uint8_t)(((v1 >> 0) & 0x0F) * 0xFF / 0x0F);
		//result = (unsigned __int8)((char)-(v1 & 0xF) / 15) | (((unsigned __int8)((char)-((unsigned __int8)v1 >> 4) / 15) | (((unsigned __int8)((char)-(HIBYTE(v1) & 0xF) / 15) | ((unsigned __int8)((char)-((v1 >> 12) & 7) / 7) << 8)) << 8)) << 8);
	}
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

/*
int __fastcall sub_4331F0(__int16 a1)
{
	unsigned __int16 v1; // ax
	int result; // eax

	v1 = _byteswap_ushort(a1);
	if ((a1 & 0x80u) == 0)
	{
		result = (unsigned __int8)((char)-(v1 & 0xF) / 15) | (((unsigned __int8)((char)-((unsigned __int8)v1 >> 4) / 15) | (((unsigned __int8)((char)-(HIBYTE(v1) & 0xF) / 15) | ((unsigned __int8)((char)-((v1 >> 12) & 7) / 7) << 8)) << 8)) << 8);
	}
	else
	{
		result = (unsigned __int8)((char)-(v1 & 0x1F) / 31) | (((unsigned __int8)((char)-((v1 >> 5) & 0x1F) / 31) | ((((char)-((v1 >> 10) & 0x1F) / 31) | 0xFFFFFF00) << 8)) << 8);
	}
	return result;
}
*/

int sub_433580(int format, signed int a1, __int16* a2, int a3, signed int a4)
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
						v19 = GetLockedTexelOffset(
							format,
							v7 + i,
							v16 - 2,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v9 = sub_4331F0(*a2++);
						*(uint32_t*)(v19 + *(uint32_t*)a3) = v9;
					}
					for (j = 0; j < 4; ++j)
					{
						v20 = GetLockedTexelOffset(
							format,
							v7 + j,
							v16 - 1,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v11 = sub_4331F0(*a2++);
						*(uint32_t*)(v20 + *(uint32_t*)a3) = v11;
					}
					for (k = 0; k < 4; ++k)
					{
						v21 = GetLockedTexelOffset(
							format,
							v7 + k,
							v16,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v13 = sub_4331F0(*a2++);
						*(uint32_t*)(v21 + *(uint32_t*)a3) = v13;
					}
					for (l = 0; l < 4; ++l)
					{
						v22 = GetLockedTexelOffset(
							format,
							v7 + l,
							v16 + 1,
							*(uint32_t*)(a3 + 4),
							*(uint32_t*)(a3 + 12));
						v15 = sub_4331F0(*a2++);
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
	return result;
}

IDirect3DTexture8* __cdecl sub_433930(NJS_TEXMEMLIST* texture, int data, int pixelformat, D3DFORMAT dataformatmaybe)
{
	IDirect3DTexture8* d3dtex;
	NJS_TEXINFO* v4; // eax
	bool v8; // edi
	int v9; // eax
	_BOOL1 v10; // dl
	signed int v11; // ecx
	int width; // [esp+24h] [ebp-40h]
	signed int height; // [esp+28h] [ebp-3Ch]
	int v15; // [esp+2Ch] [ebp-38h] BYREF
	int v16; // [esp+30h] [ebp-34h] BYREF
	int a2a; // [esp+34h] [ebp-30h] BYREF
	int v18; // [esp+38h] [ebp-2Ch] BYREF
	int v19; // [esp+3Ch] [ebp-28h] BYREF
	int v20; // [esp+40h] [ebp-24h] BYREF
	int v21; // [esp+44h] [ebp-20h] BYREF
	LockedSurface surface; // [esp+48h] [ebp-1Ch] BYREF
	int v23; // [esp+60h] [ebp-4h]
	int v6, v5;
	v4 = &texture->texinfo;
	v6 = 0;
	d3dtex = 0;
	int formatmaybe = 0;
	width = v4->texsurface.nWidth;
	height = v4->texsurface.nHeight;
	if (!v4->texsurface.pSurface)
	{

		v23 = -1;
		v5 = v6;
		v8 = (texture->texinfo.texsurface.fSurfaceFlags & 0x80000000) == 0;
		if (width > 0 && height > 0)
		{
			v16 = height;
			v15 = width;
			v19 = v8;
			a2a = 0;
			v20 = 1;
			v21 = 1;
			//v19 = levelcount
			D3DXCreateTexture(*(LPDIRECT3DDEVICE8*)0x03D128B0, v15, height, 0, 0, dataformatmaybe, D3DPOOL_MANAGED, &d3dtex);
			//(*(LPDIRECT3DDEVICE8*)0x03D128B0)->CreateTexture(v15, height, 1, 0, dataformatmaybe, D3DPOOL_MANAGED, &d3dtex);
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
			formatmaybe = ((int*)PAKFormatLookupTable)[2 * v25];
			//PrintDebug("PLEASE GIVE ME FORMAT %d \n", formatmaybe);
		}
	}
	surface.m_pBaseAddr = 0;
	surface.m_Pitch = 0;
	surface.m_pLockedTexture = 0;
	if (sub_85F0E0(0, &surface, d3dtex, 1))
	{
		//PrintDebug("locked rect\n");
		switch (pixelformat)
		{
		case 14:
			sub_433320(formatmaybe, height, (void*)data, &surface, width);
			break;
		case 5:
			sub_433580(formatmaybe, height, (__int16*)data, (int)&surface, width);
			break;
		case 6:
			//case 14:

			sub_433730((char*)data, (char*)width, surface.m_Pitch, (char*)surface.m_pBaseAddr, height);
			break;
		case 8:
			sub_433770(height, data, surface.m_Pitch, (int)surface.m_pBaseAddr, width);
			break;
		}

		d3dtex->UnlockRect(0);
	}
	if ((texture->texinfo.texsurface.fSurfaceFlags & 0x80000000) != 0)
	{
		D3DXFilterTexture(d3dtex, 0, 0, -1);
	}
	return d3dtex;
}

void sub_433BD0(NJS_TEXMEMLIST* a1, int a2)
{
	NJS_TEXINFO* v2; // eax
	int v3; // ebx
	D3DFORMAT v4; // edi
	double v5; // st7
	Uint32 v6; // ebp
	float v7; // [esp+10h] [ebp-8h]

	if (a1)
	{
		v2 = &a1->texinfo;
		v3 = v2->texsurface.PixelFormat;
		v7 = 0.0;
		v4 = (D3DFORMAT)0;
		switch (v3)
		{
		case 14:
			v5 = 0.5;
			v4 = D3DFMT_DXT1;
			break;
		case 5:
			v5 = 4.0;
			v4 = D3DFMT_A8R8G8B8;
			break;
		case 6:
			v5 = 4.0;
			v4 = D3DFMT_A8R8G8B8;
			break;
		case 8:
			v5 = 1.0;
			v4 = D3DFMT_A8;
			break;
		default:
			goto LABEL_11;
		}
		v7 = v5;
	LABEL_11:
		v6 = (int)((double)(v2->texsurface.nWidth * v2->texsurface.nHeight) * v7);
		if (v4)
		{
			a1->texinfo.texsurface.pSurface = (Uint32*)sub_433930(a1, a2, v3, v4);
			a1->texinfo.texsurface.TextureSize = v6;
		}
	}
}

NJS_TEXMEMLIST* gjLoadTextureTexMemList(void* pFile, int globalindex)
{
	signed int v3; // edi

	Uint32* v5; // ecx
	NJS_TEXSURFACE* v6; // esi

	int v10; // ecx
	int v12; // [esp+14h] [ebp-48h]
	NJS_TEXMEMLIST* result_1; // ebx
	NJS_TEXMEMLIST local; // [esp+4h] [ebp-44h]

	local.globalIndex = globalindex;
	local.bank = -1;
	local.texaddr = 0;
	local.count = 0;
	local.texinfo.texaddr = pFile;

	v12 = njLoadTextureTexMemListAnalize(&local, (int)pFile);
	NJS_TEXMEMLIST* cached = njSearchEmptyTexMemList(local.globalIndex);
	result_1 = cached;
	if (cached)
	{
		if (!cached->count)
		{
			memcpy(cached, &local, sizeof(NJS_TEXMEMLIST));
			Uint32* backup = cached->texinfo.texsurface.pSurface;
			cached->texinfo.texsurface.pSurface = 0;
			sub_433BD0(cached, (int)backup);
			//cached->texinfo.texsurface.pSurface = backup;

		}
		++result_1->count;
	}
	return result_1;
}

unsigned int njGetGlobalIndexFromPVMH(int a1, unsigned __int16 a2)
{
	__int16 v2; // cx
	int v3; // eax
	unsigned int v4; // ecx

	if (a2 >= _byteswap_ushort(*(__int16*)(a1 + 10)))
	{
		return -1;
	}
	v2 = *(__int16*)(a1 + 8);
	if ((v2 & 8) == 0)
	{
		return -1;
	}
	v3 = 6;
	if ((v2 & 1) != 0)
	{
		v3 = 34;
	}
	if ((v2 & 2) != 0)
	{
		v3 += 2;
	}
	if ((v2 & 4) != 0)
	{
		v3 += 2;
	}
	v4 = *(_DWORD*)(v3 * (a2 + 1) + a1 + 8);
	return (((v4 << 16) | v4 & 0xFF00) << 8) | ((HIWORD(v4) | v4 & 0xFF0000) >> 8);
}

signed int njLoadTextureGvmMemory(int* a1, NJS_TEXLIST* a2)
{
	int* v2; // edi
	int* dataOffset_; // ecx
	unsigned __int16 v4; // si
	int v5; // ebp
	int v6; // eax
	int v7; // eax
	unsigned __int16 v8; // ax
	int v9; // eax
	NJS_TEXMEMLIST* v10; // eax
	int v11; // ecx
	int* dataOffset; // [esp+14h] [ebp-8h]
	int* testtest;
	testtest = 0;
	v2 = a1;
	if (a1 == 0 || a2 == 0)
	{
		return -1;
	}
	dataOffset_ = a1;
	dataOffset = a1;
	if (a1)
	{
		dataOffset = (int*)((char*)a1 + a1[1] + 8);
		dataOffset_ = dataOffset;
	}
	v4 = 0;
	v5 = 0;
	if (!a1)
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
			testtest = v2;
			a2->nbTexture = v8;
		}
		goto LABEL_13;
	}
LABEL_11:
	v9 = njGetGlobalIndexFromPVMH((int)testtest, v4);
	v10 = gjLoadTextureTexMemList(v2, v9);
	v11 = v4++;
	a2->textures[v11].texaddr = (Uint32)v10;
	if (v4 != a2->nbTexture)
	{
		dataOffset_ = dataOffset;
		goto LABEL_13;
	}
	return 1;
}