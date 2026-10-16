#include "Codec.h"

Codec::Codec(FrameQueue& frameQueue, std::atomic<int>& quit) : isQuit(quit), videoFrameQueue(frameQueue)
{
	audioBuffer = (uint8_t*)SDL_malloc(audioCallbackBufferSize);
}

Codec::~Codec()
{
	if (formatContext)
		avformat_close_input(&formatContext);


	if (audioCodecContext)
		avcodec_free_context(&audioCodecContext);
	if (videoCodecContext)
		avcodec_free_context(&videoCodecContext);

	//thread
	SDL_WaitThread(PushPacketThrdad, NULL);
	SDL_WaitThread(PushVideoFrameThrdad, NULL);
	SDL_WaitThread(PushFrameThread, NULL);

	SDL_free(audioBuffer);

	sws_freeContext(swsContext);
	swr_free(&swrContext);
}

bool Codec::OpenFile(const char* fileName)
{
#ifdef _DEBUG
	av_log_set_level(AV_LOG_DEBUG);
#endif // DEBUG
	int ret = -1;

	ret = avformat_open_input(&formatContext, fileName, nullptr, nullptr);
	if (ret)
	{
		av_log(nullptr, AV_LOG_ERROR, "open media file error");
		return false;
	}


	return true;
}

bool Codec::GetStreams() const
{
	int ret = -1;

	if (!formatContext)
	{
		av_log(nullptr, AV_LOG_ERROR, "Not Open file yet");
		return false;
	}

	ret = avformat_find_stream_info(formatContext, nullptr);
	if (ret < 0)
		return false;

	return true;;
}

bool Codec::GetCodec()
{
	int ret = -1;

	//find streams
	audioSreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &audioCodec, 0);
	if (audioSreamIndex == AVERROR_STREAM_NOT_FOUND)
	{
		av_log(nullptr, AV_LOG_ERROR, "Can't find audio streams");
		return false;
	}
	else if (audioSreamIndex == AVERROR_DECODER_NOT_FOUND)
	{
		av_log(nullptr, AV_LOG_ERROR, "Can't find audio decoder");
		return false;
	}

	videoSreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
	if (videoSreamIndex == AVERROR_STREAM_NOT_FOUND)
	{
		av_log(nullptr, AV_LOG_ERROR, "Can't find video streams");
		return false;
	}
	else if (videoSreamIndex == AVERROR_DECODER_NOT_FOUND)
	{
		av_log(nullptr, AV_LOG_ERROR, "Can't find video decoder");
		return false;
	}

	//codec context
	audioCodecContext = avcodec_alloc_context3(audioCodec);
	videoCodecContext = avcodec_alloc_context3(videoCodec);
	if (!audioCodecContext || !videoCodecContext)
	{
		av_log(nullptr, AV_LOG_ERROR, "No memory");
		return false;
	}

	ret = avcodec_parameters_to_context(audioCodecContext, formatContext->streams[audioSreamIndex]->codecpar);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "audio parameters_to_context failed");
		return false;
	}
	
	ret = avcodec_parameters_to_context(videoCodecContext, formatContext->streams[videoSreamIndex]->codecpar);
	if (ret < 0)
	{
		av_log(nullptr, AV_LOG_ERROR, "video parameters_to_context failed");
		return false;
	}

	ret = avcodec_open2(audioCodecContext, audioCodec, nullptr);
	if (ret)
	{
		av_log(nullptr, AV_LOG_ERROR, "audio codec open failed");
		return false;
	}

	int a = videoCodecContext->width;
	ret = avcodec_open2(videoCodecContext, videoCodec, nullptr);
	if (ret)
	{
		av_log(nullptr, AV_LOG_ERROR, "video codec open failed");
		return false;
	}

	videoWidth = videoCodecContext->width;
	videoHeight = videoCodecContext->height;

	return true;
}

bool Codec::InitAudio()
{
	SDL_AudioSpec audioSpec = {};
	audioSpec.freq = audioCodecContext->sample_rate;
	audioSpec.format = AUDIO_S16SYS;
	audioSpec.channels = audioCodecContext->ch_layout.nb_channels;
	audioSpec.callback = AudioCallback;
	audioSpec.samples = audioBufferSize;
	audioSpec.userdata = this;

	audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &audioSpec, nullptr, 0);


	swr_alloc_set_opts2(
		&swrContext,
		&outChannelLayout,
		AV_SAMPLE_FMT_S16,
		48000,
		&audioCodecContext->ch_layout,
		(AVSampleFormat)audioCodecContext->sample_fmt,
		audioCodecContext->sample_rate,
		0,
		nullptr);

	SDL_PauseAudioDevice(audioDeviceID, 0);


	return true;
}

bool Codec::CreateThread()
{
	PushPacketThrdad =  SDL_CreateThread(PushPacket, "PushPacket", this);
	if (!PushPacketThrdad)
	{
		SDL_Log("%s\n", SDL_GetError());
		return false;
	}

	PushVideoFrameThrdad = SDL_CreateThread(PushVideoFrame, "PushVideoFrame", this);
	if (!PushVideoFrameThrdad)
	{
		SDL_Log("%s\n", SDL_GetError());
		return false;
	}

	PushFrameThread = SDL_CreateThread(PushAudioFrame, "PushAudioFrame", this);
	if (!PushFrameThread)
	{
		SDL_Log("%s\n", SDL_GetError());
		return false;
	}

	return true;
}

int Codec::PushPacket(void* argc)
{
	Codec& codec = *(Codec*)argc;

	int ret = -1;

	do
	{
		bool isFull = false;
		{
			std::lock_guard<std::mutex> lock(codec.packetQueueMutex);
			isFull = (codec.audioPacketQueue.size() > 100 || codec.videoPacketQueue.size() > 100) ? true : false;
		}
		if (isFull)
		{
			SDL_Delay(10);
			continue;
		}

		AVPacket* pkt = av_packet_alloc();

		ret = av_read_frame(codec.formatContext, pkt);
		if (ret < 0)
		{
			break;
		}

		{
			std::lock_guard<std::mutex> lock(codec.packetQueueMutex);
			if (pkt->stream_index == codec.audioSreamIndex)
			{
				codec.audioPacketQueue.push(pkt);
				continue;
			}
			else if (pkt->stream_index == codec.videoSreamIndex)
			{
				codec.videoPacketQueue.push(pkt);
				continue;
			}
			else
			{
				av_packet_free(&pkt);
				continue;
			}
		}

	} while (!codec.isQuit.load());
	
	return 0;
}

int Codec::PushVideoFrame(void* argc)
{
	Codec& codec = *(Codec*)argc;

	int ret = -1;

	AVFrame* dstFrame = av_frame_alloc();
	do
	{
		{
			std::lock_guard<std::mutex> lock(codec.packetQueueMutex);
			if (codec.videoPacketQueue.empty())
			{
				continue;
			}

		}

		if (codec.videoFrameQueue.Size() > 100)
		{
			SDL_Delay(1);
			continue;
		}

		//read packet
		AVPacket* videoPacket = nullptr;
		{
			std::lock_guard<std::mutex> lock(codec.packetQueueMutex);

			videoPacket = codec.videoPacketQueue.front();
			codec.videoPacketQueue.pop();
		}


		ret = avcodec_send_packet(codec.videoCodecContext, videoPacket);
		if (ret < 0)
		{
			break;
		}
		av_packet_free(&videoPacket);

		do
		{
			AVFrame* frame = av_frame_alloc();
			int ret = avcodec_receive_frame(codec.videoCodecContext, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				break;
			}
			else if (ret < 0)
				break;

			codec.videoFrameQueue.push(frame);

		} while (!codec.isQuit.load() && ret >= 0);

	} while (!codec.isQuit.load());
	
	return 0;
}

int Codec::PushAudioFrame(void* argc)
{
	Codec& codec = *(Codec*)argc;
	int ret = -1;

	AVPacket* packet = nullptr;
	int audioBufferOffset = 0;
	do
	{
		//is empty
		{
			std::lock_guard<std::mutex> lock(codec.packetQueueMutex);
			if (codec.audioPacketQueue.empty())
				continue;
		}

		if (codec.audioFrameQueue.Size() > 100 || codec.audioBufferSizeQueue.size() > 500)
		{
			SDL_Delay(1);
			continue;
		}

		{
			std::lock_guard<std::mutex> lock(codec.packetQueueMutex);
			packet = codec.audioPacketQueue.front();
			codec.audioPacketQueue.pop();
		}
	
		ret = avcodec_send_packet(codec.audioCodecContext, packet);
		if (ret < 0)
			break;
		av_packet_free(&packet);

		while (ret >= 0 && !codec.isQuit.load())
		{
			AVFrame* frame = av_frame_alloc();
			ret = avcodec_receive_frame(codec.audioCodecContext, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				ret = 0;
				break;
			}
			else if (ret < 0)
				break;

			//swr
			ret = swr_init(codec.swrContext);
			if(ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "swr_init error\n");
				continue;
			}

			int outputNumberSamples = av_rescale_rnd(
				swr_get_delay(codec.swrContext, frame->sample_rate) + frame->nb_samples, codec.dstSampleRate, frame->sample_rate, AV_ROUND_UP);

			uint8_t** outputBufferData = nullptr;
			int outputLinesize = 0;
			av_samples_alloc_array_and_samples(
				&outputBufferData, &outputLinesize, codec.dstChannelNumber, outputNumberSamples, AV_SAMPLE_FMT_S16, 0);

			int realOutSamples = swr_convert(codec.swrContext, outputBufferData, outputNumberSamples, (uint8_t**)frame->data, frame->nb_samples);

			int outBufferSize = av_samples_get_buffer_size(NULL, codec.dstChannelNumber, realOutSamples, AV_SAMPLE_FMT_S16, 0);

			{
				std::lock_guard<std::mutex> lock(codec.audioMutex);

				if ((audioBufferOffset + outBufferSize) > codec.audioCallbackBufferSize)
				{
					int remainSize = codec.audioCallbackBufferSize - audioBufferOffset;
					SDL_memcpy(codec.audioBuffer + audioBufferOffset, outputBufferData[0], remainSize);
					SDL_memcpy(codec.audioBuffer, outputBufferData[0] + remainSize, outBufferSize - remainSize);

				}
				else
					SDL_memcpy(codec.audioBuffer + audioBufferOffset, outputBufferData[0], outBufferSize);
				audioBufferOffset = (audioBufferOffset + outBufferSize) % codec.audioCallbackBufferSize;

			}

			av_frame_free(&frame);
		}

	} while (!codec.isQuit.load());

	return 0;
}

void Codec::AudioCallback(void* userdata, Uint8* stream, int len)
{
	Codec& codec = *(Codec*)userdata;

	{
		std::lock_guard<std::mutex> lock(codec.audioMutex);
		if (!codec.audioBuffer)
			return;

		if ((codec.audioCallbackBufferOffset + len) > codec.audioCallbackBufferSize)
		{
			int remainSize = codec.audioCallbackBufferSize - codec.audioCallbackBufferOffset;
			SDL_memcpy(stream, codec.audioBuffer + codec.audioCallbackBufferOffset, remainSize);
			SDL_memcpy(stream + remainSize, codec.audioBuffer, len - remainSize);
		}
		else
		{

			SDL_memcpy(stream, codec.audioBuffer + codec.audioCallbackBufferOffset, len);
		}
		codec.audioCallbackBufferOffset = (codec.audioCallbackBufferOffset + len) % codec.audioCallbackBufferSize;
	}

	codec.audioTime += len / ((double)codec.dstBitRate);

	if (codec.isQuit.load())
	{
		int* quit = (int*)SDL_malloc(len);
		SDL_memset(quit, 0, len);
		SDL_memcpy(stream, quit, len);
		SDL_free(quit);
	}
}

bool Codec::sws(const AVFrame* frame, AVFrame* dstFrame, int dstWidth, int dstHeight)
{
	//sws
	enum AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;
	swsContext = sws_getCachedContext(
		swsContext,
		frame->width, frame->height, (enum AVPixelFormat)frame->format,
		dstWidth, dstHeight,
		dstFormat, SWS_BILINEAR, nullptr, nullptr, nullptr);

	if (!swsContext)
		return false;

	dstFrame->width = dstWidth;
	dstFrame->height = dstHeight;
	dstFrame->format = dstFormat;

	int imageBufferSize = av_image_alloc(
		dstFrame->data, dstFrame->linesize, dstFrame->width, dstFrame->height, (AVPixelFormat)dstFrame->format, 1);

	if (imageBufferSize <= 0)
		return false;

	int frameHeight = sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, dstFrame->data, dstFrame->linesize);
	if (frameHeight <= 0)
		return false;

	

	return true;
}

AVStream* Codec::GetVideoStream() const
{
	return formatContext->streams[videoSreamIndex];
}

