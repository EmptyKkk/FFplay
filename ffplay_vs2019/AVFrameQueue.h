#pragma once
#include "Queue.h"


class AVFrameQueue
{
public: 
	AVFrameQueue() = default;
	~AVFrameQueue()= default;
	void abort();
	void release();
	int size();
	int push(AVFrame* val);
	AVFrame* pop(const int timeout);
	AVFrame* front(const int timeout);

private:
	Queue<AVFrame*> queue_;
};

int AVFrameQueue::push(AVFrame* val)
{
	AVFrame* tem_frm = av_frame_alloc();
	av_frame_move_ref(tem_frm, val);
	return queue_.push(tem_frm);
}

AVFrame* AVFrameQueue::pop(const int timeout)
{
	AVFrame* tem_frm = nullptr;
	int ret = queue_.pop(tem_frm, timeout);
	//std::cout << "avframe pop one" << std::endl;
	if (ret < 0)
	{
		//std::cerr << "avframe pop failer" << std::endl;
		return nullptr;
	}
	return tem_frm;
}

AVFrame* AVFrameQueue::front(const int timeout)
{
	AVFrame* tem_frm = nullptr;
	int ret = queue_.front(tem_frm);
	if (ret < 0)
	{
		//std::cerr << "avframe front failer" << std::endl;
		return nullptr;
	}
	return tem_frm;
}

int AVFrameQueue::size()
{
	return queue_.size();
}

void AVFrameQueue::abort()
{
	release();
	queue_.abort();
}

void AVFrameQueue::release()
{
	while (1)
	{
		AVFrame* frame = nullptr;
		int ret = queue_.pop(frame, 0);
		if (ret < 0)
			break;
		else
		{
			av_frame_free(&frame);
			continue;
		}
	}
}