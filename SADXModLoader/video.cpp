#include "stdafx.h"
#include <DShow.h>
#include <d3d8.h>
#include "ScaleInfo.h"
#include "video.h"

DataPointer(IDirect3DDevice8*, _st_d3d_device_, 0x03D128B0);
DataPointer(IMediaControl*, g_pMC, 0x3C600F8);
DataPointer(Sint32, video_frame, 0x3C5FFF0);

static int paused_frame = -1;
static bool draw_border = false;
static std::wstring border_image_path;

FunctionHook<void, Sint32, Sint32> DrawMovieTex_h(DrawMovieTex);
FunctionHook<Bool> StartDShowTextureRenderer_h(StartDShowTextureRenderer);
FunctionHook<Bool> EndDShowTextureRenderer_h(EndDShowTextureRenderer);

LPDIRECT3DTEXTURE8 pBorderTexture = NULL;

Bool PauseVideo()
{
	paused_frame = video_frame;
	return g_pMC && SUCCEEDED(g_pMC->Pause());
}

Bool ResumeVideo()
{
	paused_frame = -1;
	return g_pMC && SUCCEEDED(g_pMC->Run());
}

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

		IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_ZFUNC, D3DCMP_LESS);
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

void DrawMovieTex_r(Sint32 max_width, Sint32 max_height)
{
	// Prevent fade timer from continuing while game is paused
	if (paused_frame >= 0)
	{
		video_frame = paused_frame;
	}

	DrawBorders(max_width, max_height);
	DrawMovieTex_h.Original(max_width, max_height);
}

Bool StartDShowTextureRenderer_r()
{
	if (!pBorderTexture)
	{
		D3DXCreateTextureFromFileW(_st_d3d_device_, border_image_path.c_str(), &pBorderTexture);
	}
	return StartDShowTextureRenderer_h.Original();
}

Bool EndDShowTextureRenderer_r()
{
	if (pBorderTexture)
	{
		pBorderTexture->Release();
		pBorderTexture = nullptr;
	}
	return EndDShowTextureRenderer_h.Original();
}

void Video_Init(const LoaderSettings& settings, const std::wstring& borderpath)
{
	draw_border = settings.FmvFillMode == uiscale::FillMode_Fit;
	DrawMovieTex_h.Hook(DrawMovieTex_r);

	if (!settings.DisableBorderImage)
	{
		border_image_path = borderpath;
		StartDShowTextureRenderer_h.Hook(StartDShowTextureRenderer_r);
		EndDShowTextureRenderer_h.Hook(EndDShowTextureRenderer_r);
	}
}