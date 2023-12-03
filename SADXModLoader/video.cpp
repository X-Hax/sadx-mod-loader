#include "stdafx.h"
#include <DShow.h>
#include <chrono>
#include <thread>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

DataPointer(IMediaControl*, g_pMC, 0x3C600F8);
DataPointer(Sint32, nWidth, 0x10F1D68);
DataPointer(Sint32, nHeight, 0x10F1D6C);
DataPointer(Uint8*, video_tex, 0x3C600E8);
DataPointer(NJS_TEXNAME, video_texname, 0x3C600DC);
DataPointer(NJS_TEXLIST, video_texlist, 0x3C60094);

static bool b_ffmpeg = false;

class VideoPlayer
{
private:
	AVFormatContext* pFormatContext = nullptr;
	AVCodecContext* pVideoCodecContext = nullptr;
	SwsContext* pSwsContext = nullptr;
	AVPacket* pPacket = nullptr;
	AVFrame* pFrame = nullptr;
	AVFrame* pRGBFrame = nullptr;
	uint8_t* framebuffer = nullptr;
	std::thread* pVideoThread = nullptr;

	int video_stream_index = -1;

	unsigned int width = 0;
	unsigned int height = 0;

	bool opened = false;
	bool play = false;
	bool finished = false;
	bool update = false;

	double time = 0.0;

	void DecodeVideo(AVStream* pStream)
	{
		if (avcodec_send_packet(pVideoCodecContext, pPacket) < 0)
		{
			return;
		}

		if (avcodec_receive_frame(pVideoCodecContext, pFrame) < 0)
		{
			return;
		}

		sws_scale(pSwsContext,
			pFrame->data,
			pFrame->linesize,
			0,
			pFrame->height,
			pRGBFrame->data,
			pRGBFrame->linesize);

		double new_time = pFrame->best_effort_timestamp * av_q2d(pStream->time_base);
		double wait = new_time - time;
		time = new_time;

		std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();
		t += std::chrono::milliseconds((int)(wait * 1000.0));
		std::this_thread::sleep_until(t);

		update = false;
		memcpy(framebuffer, pRGBFrame->data[0], width * height * 4);
		update = true;
	}

	void Decode()
	{
		while (1)
		{
			int ret = av_read_frame(pFormatContext, pPacket);

			if (ret < 0)
			{
				if (ret == AVERROR_EOF)
					finished = true;
				break;
			}

			if (pPacket->stream_index == video_stream_index)
			{
				DecodeVideo(pFormatContext->streams[video_stream_index]);
				break;
			}
		}
	}

	static void VideoThread(VideoPlayer* _this)
	{
		while (1)
		{
			if (!_this->opened)
				break;

			if (!_this->play || _this->finished)
				continue;

			_this->Decode();
		}
	}

public:
	unsigned int Width()
	{
		return width;
	}

	unsigned int Height()
	{
		return height;
	}

	bool Finished()
	{
		return finished;
	}

	bool GetFrameBuffer(uint8_t* pBuffer)
	{
		if (opened && update)
		{
			memcpy(pBuffer, framebuffer, width * height * 4);
			update = false;
			return true;
		}
		return false;
	}

	void Play()
	{
		play = true;
	}

	void Pause()
	{
		play = false;
	}

	bool Open(const char* path, bool sfd)
	{
		if (opened)
		{
			Close();
		}

		pFormatContext = avformat_alloc_context();
		if (!pFormatContext)
			return false;
		
		if (sfd)
		{
			PrintDebug("Using ADX audio\n");
			const AVCodec* pAudioCodec = avcodec_find_decoder_by_name("adpcm_adx");
			pFormatContext->audio_codec_id = AV_CODEC_ID_ADPCM_ADX;
			pFormatContext->audio_codec = pAudioCodec;
		}

		if (avformat_open_input(&pFormatContext, path, NULL, NULL) != 0 ||
			avformat_find_stream_info(pFormatContext, NULL) < 0)
			return false;
	
		const AVCodec* pVideoCodec = NULL;

		video_stream_index = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &pVideoCodec, NULL);
		if (video_stream_index < 0)
			return false;

		pVideoCodecContext = avcodec_alloc_context3(pVideoCodec);
		if (!pVideoCodecContext)
			return false;

		AVStream* pStream = pFormatContext->streams[video_stream_index];

		if (avcodec_parameters_to_context(pVideoCodecContext, pStream->codecpar) < 0 ||
			avcodec_open2(pVideoCodecContext, pVideoCodec, NULL) < 0)
			return false;

		width = pVideoCodecContext->width;
		height = pVideoCodecContext->height;

		pSwsContext = sws_getContext(
			pVideoCodecContext->width,
			pVideoCodecContext->height,
			pVideoCodecContext->pix_fmt,
			width,
			height,
			AV_PIX_FMT_BGRA,
			SWS_BICUBIC,
			NULL,
			NULL,
			NULL);

		if (pSwsContext == NULL)
			return false;

		pPacket = av_packet_alloc();
		if (!pPacket)
			return false;

		pFrame = av_frame_alloc();
		if (!pFrame)
			return false;

		pRGBFrame = av_frame_alloc();
		if (!pRGBFrame)
			return false;

		pRGBFrame->format = AV_PIX_FMT_BGRA;
		pRGBFrame->width = width;
		pRGBFrame->height = height;

		if (av_frame_get_buffer(pRGBFrame, 0) < 0)
			return false;

		framebuffer = (uint8_t*)av_malloc(width * height * 4);
		if (!framebuffer)
			return false;

		time = pStream->start_time * av_q2d(pStream->time_base);
		opened = true;

		pVideoThread = new std::thread(VideoThread, this);
		return true;
	}

	void Close()
	{
		if (opened)
		{
			play = false;
			opened = false;
			finished = false;

			if (pVideoThread)
			{
				pVideoThread->join();
				delete pVideoThread;
				pVideoThread = nullptr;
			}

			if (pFormatContext) avformat_close_input(&pFormatContext);
			if (pVideoCodecContext) avcodec_free_context(&pVideoCodecContext);
			if (pSwsContext) sws_freeContext(pSwsContext);
			if (pPacket) av_packet_free(&pPacket);
			if (pFrame) av_frame_free(&pFrame);
			if (pRGBFrame) av_frame_free(&pRGBFrame);
			if (framebuffer) av_free(framebuffer);
		}
	}

	VideoPlayer() = default;
	~VideoPlayer()
	{
		Close();
	}
};

static VideoPlayer player;

Bool PauseVideo()
{
	if (b_ffmpeg)
	{
		player.Pause();
		return TRUE;
	}
	return g_pMC && SUCCEEDED(g_pMC->Pause());
}

Bool ResumeVideo()
{
	if (b_ffmpeg)
	{
		player.Play();
		return TRUE;
	}
	return g_pMC && SUCCEEDED(g_pMC->Run());
}

Bool PlayDShowTextureRenderer_r()
{
	player.Play();
	return TRUE;
}

Bool CheckMovieStatus_r()
{
	return !player.Finished();
}

Bool GetNJTexture_r()
{
	if (player.GetFrameBuffer((uint8_t*)video_tex))
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

	float w = (float)player.Width();
	float h = (float)player.Height();
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
	NJS_COLOR color[4]{ 0 };
	NJS_POINT2 points[4]{ 0 };
	points[1].x = (float)HorizontalResolution;
	points[2].y = (float)VerticalResolution;
	points[3] = { (float)HorizontalResolution, (float)VerticalResolution };
	border.col = color;
	border.p = points;
	border.num = 4;
	border.tex = NULL;
	njColorBlendingMode(0, NJD_COLOR_BLENDING_SRCALPHA);
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
	player.Close();

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

	PrintDebug("Playing movie: %s\n", fullpath.c_str());

	if (!player.Open(fullpath.c_str(), sfd))
	{
		return FALSE;
	}

	nWidth = player.Width();
	nHeight = player.Height();

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