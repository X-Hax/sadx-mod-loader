#include "stdafx.h"
#include <DShow.h>
#include <chrono>
#include <thread>
#include "bass_vgmstream.h"
#include "sadx-ffmpeg-player.h"

DataPointer(IMediaControl*, g_pMC, 0x3C600F8);
DataPointer(Sint32, nWidth, 0x10F1D68);
DataPointer(Sint32, nHeight, 0x10F1D6C);
DataPointer(Uint8*, video_tex, 0x3C600E8);
DataPointer(NJS_TEXNAME, video_texname, 0x3C600DC);
DataPointer(NJS_TEXLIST, video_texlist, 0x3C60094);

static bool b_ffmpeg = false;

Bool PauseVideo()
{
	if (b_ffmpeg)
	{
		ffPlayerPause();
		return TRUE;
	}
	return g_pMC && SUCCEEDED(g_pMC->Pause());
}

Bool ResumeVideo()
{
	if (b_ffmpeg)
	{
		ffPlayerPlay();
		return TRUE;
	}
	return g_pMC && SUCCEEDED(g_pMC->Run());
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

void DrawMovieTex_r(Sint32 max_width, Sint32 max_height)
{
	NJS_QUAD_TEXTURE quad;

	float screen_w = 640.0f * ScreenRaitoX;
	float screen_h = 480.0f * ScreenRaitoY;
	float screen_ratio = screen_w / screen_h;

	float w = (float)ffPlayerWidth();
	float h = (float)ffPlayerHeight();
	float ratio = w / h;

	if (ratio == screen_ratio)
	{
		quad.x1 = 0;
		quad.y1 = 0;
		quad.x2 = screen_w;
		quad.y2 = screen_h;
	}
	else if (ratio < screen_ratio)
	{
		float scaled_w = w * (screen_h / h);
		float gap = (screen_w - scaled_w) / 2.0f;
		quad.x1 = gap;
		quad.y1 = 0;
		quad.x2 = scaled_w + gap;
		quad.y2 = screen_h;
	}
	else
	{
		float scaled_h = h * (screen_w / w);
		float gap = (screen_h - scaled_h) / 2.0f;
		quad.x1 = 0;
		quad.y1 = gap;
		quad.x2 = screen_w;
		quad.y2 = scaled_h + gap;
	}

	quad.u1 = 0.0f;
	quad.v1 = 0.0f;
	quad.u2 = 1.0f;
	quad.v2 = 1.0f;

	// Draw border/background
	NJS_POINT2COL border;
	NJS_COLOR colors[4];
	NJS_POINT2 points[4];
	points[0] = { 0.0f, 0.0f };
	colors[0].color = 0xFF000000;
	points[1] = { screen_w, 0.0f };
	colors[1].color = 0xFF000000;
	points[2] = { 0.0f, screen_h };
	colors[2].color = 0xFF000000;
	points[3] = { screen_w, screen_h };
	colors[3].color = 0xFF000000;
	border.col = colors;
	border.p = points;
	border.num = 4;
	border.tex = NULL;
	njColorBlendingMode(NJD_SOURCE_COLOR, NJD_COLOR_BLENDING_SRCALPHA);
	njColorBlendingMode(NJD_DESTINATION_COLOR, NJD_COLOR_BLENDING_INVSRCALPHA);
	___SAnjDrawPolygon2D(&border, 4, -1000.0f, NJD_FILL | NJD_DRAW_CONNECTED);

	// Draw video
	njSetTexture(&video_texlist);
	njQuadTextureStart(0);
	njSetQuadTexture(0, 0xFFFFFFFF);
	njDrawQuadTexture(&quad, 1000.0f);
	njQuadTextureEnd();
}

Bool StartDShowTextureRenderer_r()
{
	return TRUE;
}

Bool EndDShowTextureRenderer_r()
{
	ffPlayerClose();

	if (video_tex)
	{
		syFree(video_tex);
		video_tex = NULL;
		njReleaseTexture(&video_texlist);
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

void Video_Init()
{
	WriteJump((void*)0x513850, StartDShowTextureRenderer_r);
	WriteJump((void*)0x513990, GetNJTexture_r);
	WriteJump((void*)0x5139F0, DrawMovieTex_r);
	WriteJump((void*)0x513C50, EndDShowTextureRenderer_r);
	WriteJump((void*)0x513D30, PlayDShowTextureRenderer_r);
	WriteJump((void*)0x513D50, CheckMovieStatus_r);
	WriteJump((void*)0x513ED0, LoadDShowTextureRenderer_r);
	b_ffmpeg = true;
}