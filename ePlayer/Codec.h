#pragma once

#include <SDL2\SDL.h>
#include <queue>
#include <atomic>
#include <mutex>
#include "FrameQueue.h"

extern "C"
{

#include <libavformat\avformat.h>
#include <libavutil\log.h>
#include <libavcodec\avcodec.h>
#include <libswscale\swscale.h>
#include <libavutil\imgutils.h>
#include <libswresample\swresample.h>

}

class MainWindow;

class Codec
{
public:
	std::atomic<int>& isQuit;

	//mutex
	std::mutex packetQueueMutex;
	std::queue<AVPacket*> audioPacketQueue;
	std::queue<AVPacket*> videoPacketQueue;

	//thtrad
	SDL_Thread* PushPacketThrdad = nullptr;
	SDL_Thread* PushVideoFrameThrdad = nullptr;
	SDL_Thread* PushFrameThread = nullptr;

	int videoWidth  = 0;
	int videoHeight = 0;

	std::atomic<double> audioTime = 0.0;

public:
	Codec(FrameQueue& frameQueue,std::atomic<int>& quit);
	~Codec();

	bool OpenFile(const char* fileName);
	
	//Get video and audio streams, only get best, If there are more than 2, ignore it
	bool GetStreams() const;
	bool GetCodec();
	bool InitAudio();
	bool CreateThread();

	//sws
	bool sws(const AVFrame* frame, AVFrame* dstFrame, int dstWidth, int dstHeight);

	AVStream* GetVideoStream() const;


private:
	AVFormatContext* formatContext = nullptr;
	AVCodecContext* audioCodecContext = nullptr;
	AVCodecContext* videoCodecContext = nullptr;
	const AVCodec* audioCodec = nullptr;
	const AVCodec* videoCodec = nullptr;
	
	const int audioBufferSize = 1024;
	const int dstSampleRate = 48000;
	const int dstChannelNumber = 2;
	const int dstSampleSize = 2;
	const int dstBitRate = dstSampleRate * dstChannelNumber * dstSampleSize;

	int audioSreamIndex = -1;
	int videoSreamIndex = -1;

	int audioDeviceID = 1;

	FrameQueue& videoFrameQueue;
	FrameQueue audioFrameQueue = {};

	//sws
	SwsContext* swsContext = nullptr;
	SwrContext* swrContext = nullptr;
	const AVChannelLayout outChannelLayout = AV_CHANNEL_LAYOUT_STEREO;

	//audio
	uint8_t* audioBuffer = nullptr;
	const uint32_t audioCallbackBufferSize = 1024 * 1024 * 5;
	std::mutex audioMutex;
	std::queue<int> audioBufferSizeQueue;

	int remainAudioBuffer = 0;
	int audioCallbackBufferOffset = 0;


private:
	static int PushPacket(void* argc);
	static int PushVideoFrame(void* argc);
	static int PushAudioFrame(void* argc);
	static void AudioCallback(void* userdata, Uint8* stream, int len);


};

