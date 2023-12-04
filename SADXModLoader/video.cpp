#include "stdafx.h"
#include <DShow.h>
#include <chrono>
#include <thread>
#include "bass_vgmstream.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
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
	std::thread* pVideoThread = nullptr;

	AVFormatContext* pFormatContext = nullptr;
	AVPacket* pPacket = nullptr;
	AVFrame* pFrame = nullptr;

	AVCodecContext* pVideoCodecContext = nullptr;
	SwsContext* pSwsContext = nullptr;
	AVFrame* pVideoFrame = nullptr;
	uint8_t* framebuffer = nullptr;

	AVCodecContext* pAudioCodecContext = nullptr;
	SwrContext* pSwrContext = nullptr;
	AVFrame* pAudioFrame = nullptr;
	HSTREAM BassHandle = NULL;

	int video_stream_index = -1;
	int audio_stream_index = -1;

	unsigned int width = 0;
	unsigned int height = 0;

	bool opened = false;
	bool play = false;
	bool finished = false;
	bool update = false;

	double time = 0.0;

	void DecodeAudio(AVStream* pStream)
	{
		if (avcodec_send_packet(pAudioCodecContext, pPacket) < 0)
		{
			return;
		}

		if (avcodec_receive_frame(pAudioCodecContext, pFrame) < 0)
		{
			return;
		}

		if (swr_convert_frame(pSwrContext, pAudioFrame, pFrame) < 0)
		{
			PrintDebug("[video] Failed to convert audio frame.\n");
			return;
		}
		
		int size = av_samples_get_buffer_size(&pAudioFrame->linesize[0], pAudioFrame->ch_layout.nb_channels, pAudioFrame->nb_samples, AV_SAMPLE_FMT_FLT, 0);
		if (size < 0)
		{
			PrintDebug("[video] Failed to send audio buffer to BASS.\n");
			return;
		}

		BASS_StreamPutData(BassHandle, pAudioFrame->data[0], size);
	}

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
			pVideoFrame->data,
			pVideoFrame->linesize);

		double new_time = pFrame->best_effort_timestamp * av_q2d(pStream->time_base);
		double wait = new_time - time;
		time = new_time;

		std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now();
		t += std::chrono::milliseconds((int)(wait * 1000.0));
		std::this_thread::sleep_until(t);

		update = false;
		memcpy(framebuffer, pVideoFrame->data[0], width * height * 4);
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
				{
					finished = true;
				}
				break;
			}
			
			if (pPacket->stream_index == video_stream_index)
			{
				DecodeVideo(pFormatContext->streams[video_stream_index]);
				av_packet_unref(pPacket);
				break;
			}

			if (pPacket->stream_index == audio_stream_index)
			{
				DecodeAudio(pFormatContext->streams[audio_stream_index]);
				av_packet_unref(pPacket);
				break;
			}

			av_packet_unref(pPacket);
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
		{
			PrintDebug("[video] Failed to initialize ffmpeg.\n");
			return false;
		}

		if (sfd)
		{
			PrintDebug("[video] SFD compatibility mode.\n");
			pFormatContext->audio_codec_id = AV_CODEC_ID_ADPCM_ADX;
		}

		if (avformat_open_input(&pFormatContext, path, NULL, NULL) != 0 ||
			avformat_find_stream_info(pFormatContext, NULL) < 0)
		{
			PrintDebug("[video] Failed to open %s.\n", path);
			return false;
		}

		avio_seek(pFormatContext->pb, 0, SEEK_SET);

		video_stream_index = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		if (video_stream_index < 0)
		{
			PrintDebug("[video] No video stream found.\n");
			return false;
		}

		const AVCodec* pVideoCodec = avcodec_find_decoder(pFormatContext->streams[video_stream_index]->codecpar->codec_id);
		pFormatContext->video_codec = pVideoCodec;

		pVideoCodecContext = avcodec_alloc_context3(pVideoCodec);
		if (!pVideoCodecContext)
		{
			PrintDebug("[video] Failed to initialize video codec context.\n");
			return false;
		}

		AVStream* pVideoStream = pFormatContext->streams[video_stream_index];

		avformat_seek_file(pFormatContext, 0, 0, 0, pFormatContext->streams[0]->duration, 0);

		if (avcodec_parameters_to_context(pVideoCodecContext, pVideoStream->codecpar) < 0 ||
			avcodec_open2(pVideoCodecContext, pVideoCodec, NULL) < 0)
		{
			PrintDebug("[video] Failed to initialize video codec.\n");
			return false;
		}

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
		{
			PrintDebug("[video] Failed to initialize video conversion.\n");
			return false;
		}

		pPacket = av_packet_alloc();
		if (!pPacket)
		{
			PrintDebug("[video] Failed to allocate packet\n");
			return false;
		}

		pFrame = av_frame_alloc();
		if (!pFrame)
		{
			PrintDebug("[video] Failed to allocate packet frame.\n");
			return false;
		}

		pVideoFrame = av_frame_alloc();
		if (!pVideoFrame)
		{
			PrintDebug("[video] Failed to allocate video output frame.\n");
			return false;
		}

		pVideoFrame->format = AV_PIX_FMT_BGRA;
		pVideoFrame->width = width;
		pVideoFrame->height = height;

		if (av_frame_get_buffer(pVideoFrame, 0) < 0)
		{
			PrintDebug("[video] Failed to allocate video output frame buffer.\n");
			return false;
		}

		framebuffer = (uint8_t*)av_malloc(width * height * 4);
		if (!framebuffer)
		{
			PrintDebug("[video] Failed to allocate frame buffer.\n");
			return false;
		}

		audio_stream_index = av_find_best_stream(pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
		if (audio_stream_index >= 0)
		{
			const AVCodec* pAudioCodec = avcodec_find_decoder(pFormatContext->streams[audio_stream_index]->codecpar->codec_id);
			pFormatContext->audio_codec = pAudioCodec;

			pAudioCodecContext = avcodec_alloc_context3(pAudioCodec);
			if (!pAudioCodecContext)
			{
				PrintDebug("[video] Failed to initialize audio codec context.\n");
				return false;
			}

			AVStream* pAudoStream = pFormatContext->streams[audio_stream_index];

			if (avcodec_parameters_to_context(pAudioCodecContext, pAudoStream->codecpar) < 0 ||
				avcodec_open2(pAudioCodecContext, pAudioCodec, NULL) < 0)
			{
				PrintDebug("[video] failed to initialize audio codec.\n");
				return false;
			}

			// Initialize resampler
			if (swr_alloc_set_opts2(&pSwrContext, &pAudoStream->codecpar->ch_layout, AV_SAMPLE_FMT_FLT, pAudoStream->codecpar->sample_rate,
				&pAudoStream->codecpar->ch_layout, (AVSampleFormat)pAudoStream->codecpar->format, pAudoStream->codecpar->sample_rate, 0, nullptr) < 0)
			{
				PrintDebug("[video] Failed to initialize audio conversion.\n");
				return false;
			}

			pAudioFrame = av_frame_alloc();
			if (!pAudioFrame)
			{
				PrintDebug("[video] Failed to allocate audio output frame.\n");
				return false;
			}

			pAudioFrame->sample_rate = pAudioCodecContext->sample_rate;
			pAudioFrame->ch_layout = pAudioCodecContext->ch_layout;
			pAudioFrame->format = AV_SAMPLE_FMT_FLT;

			BassHandle = BASS_StreamCreate(pAudioCodecContext->sample_rate, pAudioCodecContext->ch_layout.nb_channels, BASS_SAMPLE_FLOAT, STREAMPROC_PUSH, NULL);
			if (!BassHandle)
			{
				PrintDebug("[video] Failed to initialize audio library.");
				return false;
			}
			
			BASS_ChannelPlay(BassHandle, FALSE);
		}

		time = pVideoStream->start_time * av_q2d(pVideoStream->time_base);
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
			if (pPacket) av_packet_free(&pPacket);
			if (pFrame) av_frame_free(&pFrame);

			if (pVideoCodecContext) avcodec_free_context(&pVideoCodecContext);
			if (pSwsContext) sws_freeContext(pSwsContext);
			if (pVideoFrame) av_frame_free(&pVideoFrame);
			if (framebuffer) av_free(framebuffer);

			if (pAudioCodecContext) avcodec_free_context(&pAudioCodecContext);
			if (pSwrContext) swr_free(&pSwrContext);
			if (pAudioFrame) av_frame_free(&pAudioFrame);
			if (BassHandle) { BASS_StreamFree(BassHandle); BassHandle = NULL; };
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

	PrintDebug("[video] Playing movie: %s.\n", fullpath.c_str());

	if (!player.Open(fullpath.c_str(), sfd))
	{
		PrintDebug("[video] Failed to play movie.\n");
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