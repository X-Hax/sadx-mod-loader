#include "stdafx.h"
#include <DShow.h>
#include <d3d8.h>
#include <chrono>
#include <thread>
#include "bass_vgmstream.h"
#include "sadx-ffmpeg-player.h"

DataPointer(IDirect3DDevice8*, _st_d3d_device_, 0x03D128B0);
DataPointer(IMediaControl*, g_pMC, 0x3C600F8);
DataPointer(Sint32, nWidth, 0x10F1D68);
DataPointer(Sint32, nHeight, 0x10F1D6C);
DataPointer(Uint8*, video_tex, 0x3C600E8);
DataPointer(NJS_TEXNAME, video_texname, 0x3C600DC);
DataPointer(NJS_TEXLIST, video_texlist, 0x3C60094);
DataPointer(Sint32, video_frame, 0x3C5FFF0);

static bool b_ffmpeg = false;
static int paused_frame = -1;
static bool draw_border = false;
static bool border_image = false;
static std::wstring border_image_path;

FunctionHook<void, Sint32, Sint32> DrawMovieTex_h(DrawMovieTex);
FunctionHook<Bool> StartDShowTextureRenderer_h(StartDShowTextureRenderer);
FunctionHook<Bool> EndDShowTextureRenderer_h(EndDShowTextureRenderer);

LPDIRECT3DTEXTURE8 pBorderTexture = NULL;

#pragma region Utilities
Bool PauseVideo()
{
	if (b_ffmpeg)
	{
		ffPlayerPause();
		return TRUE;
	}
	else
	{
		paused_frame = video_frame;
		return g_pMC && SUCCEEDED(g_pMC->Pause());
	}
}

Bool ResumeVideo()
{
	if (b_ffmpeg)
	{
		ffPlayerPlay();
		return TRUE;
	}
	else
	{
		paused_frame = -1;
		return g_pMC && SUCCEEDED(g_pMC->Run());
	}
}
#pragma endregion

#pragma region Video border
void DrawBorders(Sint32 video_width, Sint32 video_height)
{
	if (!draw_border)
	{
		return;
	}

	float video_w = video_width;
	float video_h = video_height;
	float screen_w = HorizontalResolution;
	float screen_h = VerticalResolution;

	float window_aspect_ratio = screen_w / screen_h;
	float video_aspect_ratio = video_w / video_h;

	if (window_aspect_ratio == video_aspect_ratio)
	{
		return;
	}

	float ratio_w = screen_w / video_w;
	float ratio_h = screen_h / video_h;

	float size_x, size_y, offset_x, offset_y;
	if (window_aspect_ratio > video_aspect_ratio)
	{
		size_x = (screen_w - (video_w * ratio_h)) / 2;
		size_y = screen_h;
		offset_x = screen_w - size_x;
		offset_y = 0.0f;
	}
	else
	{
		size_x = screen_w;
		size_y = (screen_h - (video_h * ratio_w)) / 2;
		offset_x = 0.0f;
		offset_y = screen_h - size_y;
	}

	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_ZFUNC, D3DCMP_ALWAYS);
	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_ALPHABLENDENABLE, FALSE);
	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_LIGHTING, FALSE);

	if (pBorderTexture)
	{
		float u2, v2;
		if (window_aspect_ratio > video_aspect_ratio)
		{
			u2 = 1.0f - offset_x / screen_w;
			v2 = 1.0f;
		}
		else
		{
			u2 = 1.0f;
			v2 = 1.0f - offset_y / screen_h;
		}

		IDirect3DDevice8_SetVertexShader(_st_d3d_device_, D3DFVF_DIFFUSE | D3DFVF_XYZRHW | D3DFVF_TEX1);
		IDirect3DDevice8_SetTexture(_st_d3d_device_, 0, pBorderTexture);

		struct VERTEX {
			D3DVECTOR position;
			Float rhw;
			Uint32 diffuse;
			Float u, v;
		} pool[4];

		pool[0].position = { 0.0f, 0.0f, 0.0f };
		pool[0].diffuse = 0xFFFFFFFF;
		pool[0].rhw = 1.0f;
		pool[0].u = 0.0f;
		pool[0].v = 0.0f;

		pool[1].position = { size_x, 0.0f, 0.0f };
		pool[1].diffuse = 0xFFFFFFFF;
		pool[1].rhw = 1.0f;
		pool[1].u = u2;
		pool[1].v = 0.0f;

		pool[2].position = { 0.0f, size_y, 0.0f };
		pool[2].diffuse = 0xFFFFFFFF;
		pool[2].rhw = 1.0f;
		pool[2].u = 0.0f;
		pool[2].v = v2;

		pool[3].position = { size_x, size_y, 0.0f };
		pool[3].diffuse = 0xFFFFFFFF;
		pool[3].rhw = 1.0f;
		pool[3].u = u2;
		pool[3].v = v2;

		IDirect3DDevice8_DrawPrimitiveUP(_st_d3d_device_, D3DPT_TRIANGLESTRIP, 2, pool, sizeof(struct VERTEX));

		pool[0].position = { offset_x, offset_y, 0.0f };
		pool[0].diffuse = 0xFFFFFFFF;
		pool[0].rhw = 1.0f;
		pool[0].u = 1.0f - u2;
		pool[0].v = 1.0f - v2;

		pool[1].position = { offset_x + size_x, offset_y, 0.0f };
		pool[1].diffuse = 0xFFFFFFFF;
		pool[1].rhw = 1.0f;
		pool[1].u = 1.0f;
		pool[1].v = 1.0f - v2;

		pool[2].position = { offset_x, offset_y + size_y, 0.0f };
		pool[2].diffuse = 0xFFFFFFFF;
		pool[2].rhw = 1.0f;
		pool[2].u = 1.0f - u2;
		pool[2].v = 1.0f;

		pool[3].position = { offset_x + size_x, offset_y + size_y, 0.0f };
		pool[3].diffuse = 0xFFFFFFFF;
		pool[3].rhw = 1.0f;
		pool[3].u = 1.0f;
		pool[3].v = 1.0f;

		IDirect3DDevice8_DrawPrimitiveUP(_st_d3d_device_, D3DPT_TRIANGLESTRIP, 2, pool, sizeof(struct VERTEX));
	}
	else
	{
		IDirect3DDevice8_SetVertexShader(_st_d3d_device_, D3DFVF_DIFFUSE | D3DFVF_XYZRHW);

		struct VERTEX {
			D3DVECTOR position;
			Float rhw;
			Uint32 diffuse;
		} pool[4];

		pool[0].position = { 0.0f, 0.0f, 0.0f };
		pool[0].diffuse = 0xFF000000;
		pool[0].rhw = 1.0f;

		pool[1].position = { size_x, 0.0f, 0.0f };
		pool[1].diffuse = 0xFF000000;
		pool[1].rhw = 1.0f;

		pool[2].position = { 0.0f, size_y, 0.0f };
		pool[2].diffuse = 0xFF000000;
		pool[2].rhw = 1.0f;

		pool[3].position = { size_x, size_y, 0.0f };
		pool[3].diffuse = 0xFF000000;
		pool[3].rhw = 1.0f;

		IDirect3DDevice8_DrawPrimitiveUP(_st_d3d_device_, D3DPT_TRIANGLESTRIP, 2, pool, sizeof(struct VERTEX));

		pool[0].position = { offset_x, offset_y, 0.0f };
		pool[0].diffuse = 0xFF000000;
		pool[0].rhw = 1.0f;

		pool[1].position = { offset_x + size_x, offset_y, 0.0f };
		pool[1].diffuse = 0xFF0000000;
		pool[1].rhw = 1.0f;

		pool[2].position = { offset_x, offset_y + size_y, 0.0f };
		pool[2].diffuse = 0xFF000000;
		pool[2].rhw = 1.0f;

		pool[3].position = { offset_x + size_x, offset_y + size_y, 0.0f };
		pool[3].diffuse = 0xFF000000;
		pool[3].rhw = 1.0f;

		IDirect3DDevice8_DrawPrimitiveUP(_st_d3d_device_, D3DPT_TRIANGLESTRIP, 2, pool, sizeof(struct VERTEX));
	}

	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_ZFUNC, D3DCMP_LESS);
}

void LoadBorderTexture()
{
	if (!pBorderTexture)
	{
		D3DXCreateTextureFromFileW(_st_d3d_device_, border_image_path.c_str(), &pBorderTexture);
	}
}

void FreeBorderTexture()
{
	if (pBorderTexture)
	{
		pBorderTexture->Release();
		pBorderTexture = nullptr;
	}
}
#pragma endregion

StdcallFunctionPointer(float, stCalcRHWtoZ, (float rhw), 0x78DBE0);

#pragma region ffmpeg
void DrawVideo()
{
	float screen_w = (float)HorizontalResolution;
	float screen_h = (float)VerticalResolution;
	float screen_ratio = screen_w / screen_h;

	float w = (float)ffPlayerWidth();
	float h = (float)ffPlayerHeight();
	float ratio = w / h;

	float x1, x2, y1, y2;

	if (ratio == screen_ratio)
	{
		x1 = 0.0f;
		y1 = 0.0f;
		x2 = screen_w;
		y2 = screen_h;
	}
	else if (ratio < screen_ratio)
	{
		float scaled_w = w * (screen_h / h);
		float gap = (screen_w - scaled_w) / 2.0f;
		x1 = gap;
		y1 = 0.0f;
		x2 = scaled_w + gap;
		y2 = screen_h;
	}
	else
	{
		float scaled_h = h * (screen_w / w);
		float gap = (screen_h - scaled_h) / 2.0f;
		x1 = 0.0f;
		y1 = gap;
		x2 = screen_w;
		y2 = scaled_h + gap;
	}

	struct VERTEX {
		D3DVECTOR position;
		Float rhw;
		Uint32 diffuse;
		Float u;
		Float v;
	} pool[4];

	float depth = stCalcRHWtoZ(-1.0f / -1000.0f);

	pool[0].position = { x1, y1, depth };
	pool[0].diffuse = 0xFFFFFFFF;
	pool[0].rhw = 1.0f;
	pool[0].u = 0.0f;
	pool[0].v = 0.0f;

	pool[1].position = { x2, y1, depth };
	pool[1].diffuse = 0xFFFFFFFF;
	pool[1].rhw = 1.0f;
	pool[1].u = 1.0f;
	pool[1].v = 0.0f;

	pool[2].position = { x1, y2, depth };
	pool[2].diffuse = 0xFFFFFFFF;
	pool[2].rhw = 1.0f;
	pool[2].u = 0.0f;
	pool[2].v = 1.0f;

	pool[3].position = { x2, y2, depth };
	pool[3].diffuse = 0xFFFFFFFF;
	pool[3].rhw = 1.0f;
	pool[3].u = 1.0f;
	pool[3].v = 1.0f;

	IDirect3DDevice8_SetTexture(_st_d3d_device_, 0, (IDirect3DBaseTexture8*)((NJS_TEXMEMLIST*)video_texlist.textures[0].texaddr)->texinfo.texsurface.pSurface);
	IDirect3DDevice8_SetVertexShader(_st_d3d_device_, D3DFVF_DIFFUSE | D3DFVF_XYZRHW | D3DFVF_TEX1);
	IDirect3DDevice8_DrawPrimitiveUP(_st_d3d_device_, D3DPT_TRIANGLESTRIP, 2, pool, sizeof(struct VERTEX));
}

void FreeVideo()
{
	ffPlayerClose();

	if (video_tex)
	{
		syFree(video_tex);
		video_tex = NULL;
		njReleaseTexture(&video_texlist);
	}
}

Bool PlayDShowTextureRenderer_r()
{
	ffPlayerPlay();
	return TRUE;
}

Bool CheckMovieStatus_r()
{
	return !ffPlayerFinished();
}

Bool GetNJTexture_r()
{
	if (ffPlayerGetFrameBuffer((uint8_t*)video_tex))
	{
		njReLoadTextureNumG(0xD00000D0, (Uint16*)video_tex, NJD_TEXATTR_GLOBALINDEX | NJD_TEXATTR_TYPE_MEMORY, 0);
	}
	return TRUE;
}

Bool LoadDShowTextureRenderer_r(const char* filename)
{
	bool sfd = false; // If the original MPG doesn't exist, use the SFD file
	bool dlc = false; // If the file is RE-US or RE-JP (DLC folder in Steam)

	std::string fullpath = filename;
	std::string basename = GetBaseName(filename); // Filename without extension
	StripExtension(basename);

	if (!lstrcmpiA(basename.c_str(), "re-us") || !lstrcmpiA(basename.c_str(), "re-jp"))
	{
		dlc = true;
	}

	// If the file doesn't exist, use the SFD version
	if (!Exists(fullpath))
	{
		if (dlc)
			fullpath = "DLC\\" + basename + "2.sfd";
		else
			ReplaceFileExtension(fullpath, ".sfd");
		sfd = true;
	}

	PrintDebug("[video] Playing movie: %s.\n", fullpath.c_str());

	if (!ffPlayerOpen(fullpath.c_str(), sfd))
	{
		PrintDebug("[video] Failed to play movie.\n");
		return FALSE;
	}

	nWidth = ffPlayerWidth();
	nHeight = ffPlayerHeight();

	if (video_tex)
	{
		syFree(video_tex);
		njReleaseTexture(&video_texlist);
	}

	video_tex = (Uint8*)syMalloc(nWidth * nHeight * 4);

	NJS_TEXINFO texinfo;
	njSetTextureInfo(&texinfo, (Uint16*)video_tex, NJD_TEXFMT_RECTANGLE | NJD_TEXFMT_ARGB_8888, nWidth, nHeight);
	njSetTextureName(&video_texname, &texinfo, 0xD00000D0, NJD_TEXATTR_GLOBALINDEX | NJD_TEXATTR_TYPE_MEMORY);
	video_texlist.textures = &video_texname;
	video_texlist.nbTexture = 1;
	njLoadTexture(&video_texlist);

	return TRUE;
}
#pragma endregion

void DrawMovieTex_r(Sint32 max_width, Sint32 max_height)
{
	// Prevent fade timer from continuing while game is paused
	if (paused_frame >= 0)
	{
		video_frame = paused_frame;
	}

	DrawBorders(max_width, max_height);

	if (b_ffmpeg)
	{
		DrawVideo();
	}
	else
	{
		DrawMovieTex_h.Original(max_width, max_height);
	}
}

Bool StartDShowTextureRenderer_r()
{
	if (border_image)
	{
		LoadBorderTexture();
	}

	if (b_ffmpeg)
	{
		return TRUE;
	}
	else
	{
		return StartDShowTextureRenderer_h.Original();
	}
}

Bool EndDShowTextureRenderer_r()
{
	if (border_image)
	{
		FreeBorderTexture();
	}

	if (b_ffmpeg)
	{
		FreeVideo();
		return TRUE;
	}
	else
	{
		return EndDShowTextureRenderer_h.Original();
	}
}

void Video_Init(const LoaderSettings& settings, const std::wstring& borderpath)
{
	draw_border = settings.FmvFillMode == uiscale::FillMode_Fit;
	border_image = !settings.DisableBorderImage;
	b_ffmpeg = settings.EnableFFMPEG;

	DrawMovieTex_h.Hook(DrawMovieTex_r);
	StartDShowTextureRenderer_h.Hook(StartDShowTextureRenderer_r);
	EndDShowTextureRenderer_h.Hook(EndDShowTextureRenderer_r);

	if (border_image)
	{
		border_image_path = borderpath;
	}

	if (b_ffmpeg)
	{
		WriteJump((void*)0x513850, StartDShowTextureRenderer_r);
		WriteJump((void*)0x513990, GetNJTexture_r);
		WriteJump((void*)0x5139F0, DrawMovieTex_r);
		WriteJump((void*)0x513C50, EndDShowTextureRenderer_r);
		WriteJump((void*)0x513D30, PlayDShowTextureRenderer_r);
		WriteJump((void*)0x513D50, CheckMovieStatus_r);
		WriteJump((void*)0x513ED0, LoadDShowTextureRenderer_r);
	}
}