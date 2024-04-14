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

	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_ZFUNC, D3DCMP_ALWAYS);
	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_ALPHABLENDENABLE, FALSE);
	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_LIGHTING, FALSE);
	IDirect3DDevice8_SetVertexShader(_st_d3d_device_, D3DFVF_DIFFUSE | D3DFVF_XYZRHW);

	float screen_w = (float)HorizontalResolution;
	float screen_h = (float)VerticalResolution;
	float size_x, size_y, offset_x, offset_y;

	if (screen_w / screen_h >= 1.333333333f)
	{
		size_x = (screen_w - (video_width * ScreenRaitoY)) / 2;
		size_y = screen_h;
		offset_x = screen_w - size_x;
		offset_y = 0.0f;
	}
	else
	{
		size_x = screen_w;
		size_y = (screen_h - (video_height * ScreenRaitoX)) / 2;
		offset_x = 0.0f;
		offset_y = screen_h - size_y;
	}

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
	pool[1].diffuse = 0xFF000000;
	pool[1].rhw = 1.0f;
	
	pool[2].position = { offset_x, offset_y + size_y, 0.0f };
	pool[2].diffuse = 0xFF000000;
	pool[2].rhw = 1.0f;
	
	pool[3].position = { offset_x + size_x, offset_y + size_y, 0.0f };
	pool[3].diffuse = 0xFF000000;
	pool[3].rhw = 1.0f;
	
	IDirect3DDevice8_DrawPrimitiveUP(_st_d3d_device_, D3DPT_TRIANGLESTRIP, 2, pool, sizeof(struct VERTEX));

	IDirect3DDevice8_SetRenderState(_st_d3d_device_, D3DRS_ZFUNC, D3DCMP_LESS);
}

void DrawMovieTex_r(Sint32 max_width, Sint32 max_height)
{
	if (paused_frame >= 0)
	{
		video_frame = paused_frame;
	}
	DrawBorders(max_width, max_height);
	DrawMovieTex(max_width, max_height);
}

void Video_Init(const LoaderSettings& settings)
{
	draw_border = settings.FmvFillMode == uiscale::FillMode_Fit;
	WriteCall((void*)0x51330A, DrawMovieTex_r);
}