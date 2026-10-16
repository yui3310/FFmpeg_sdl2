#include "FrameQueue.h"

void FrameQueue::push(AVFrame* frame)
{
	std::lock_guard<std::mutex> lock(mutex);
	frameQueue.push(frame);

}

AVFrame* FrameQueue::pop()
{
	std::lock_guard<std::mutex> lock(mutex);
	AVFrame* frame = frameQueue.front();
	frameQueue.pop();

	return frame;
}

bool FrameQueue::isEmpty()
{
	return frameQueue.empty() ? true : false;
}

int FrameQueue::Size()
{
	return frameQueue.size();
}
