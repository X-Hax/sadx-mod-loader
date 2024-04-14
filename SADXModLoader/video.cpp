#include "stdafx.h"
#include <DShow.h>
#include <d3d8.h>
#include "video.h"

DataPointer(IDirect3DDevice8*, _st_d3d_device_, 0x03D128B0);
DataPointer(IMediaControl*, g_pMC, 0x3C600F8);
DataPointer(Sint32, video_frame, 0x3C5FFF0);

static int paused_frame = -1;

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

void DrawMovieTex_r(Sint32 max_width, Sint32 max_height)
{
	if (paused_frame >= 0)
	{
		video_frame = paused_frame;
	}
	DrawMovieTex(max_width, max_height);
}

void Video_Init()
{
	WriteCall((void*)0x51330A, DrawMovieTex_r);
}