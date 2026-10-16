#pragma once

#include <queue>
#include <mutex>

extern "C"
{
#include <libavformat\avformat.h>
}

class FrameQueue
{
public:
	FrameQueue() {};

	void push(AVFrame* frame);
	AVFrame* pop();
	bool isEmpty();
	int Size();

private:
	std::mutex mutex;
	std::queue<AVFrame*> frameQueue;

};

