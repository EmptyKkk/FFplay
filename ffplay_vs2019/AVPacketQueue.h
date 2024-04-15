#pragma once
#include "Queue.h"


class AVPacketQueue
{
public:
	AVPacketQueue() = default;
	~AVPacketQueue() = default;
	void abort();
	void release();
	int size();
	int push(AVPacket* val);
	AVPacket* pop(const int timeout);
	AVPacket* front(const int timeout);


private:
	Queue<AVPacket*> queue_;
};


//下面是函数代码实现

void AVPacketQueue::abort()
{
	release();
	queue_.abort();
};

int AVPacketQueue::push(AVPacket* val)
{
	AVPacket* tem_pkt = av_packet_alloc();
	av_packet_move_ref(tem_pkt, val);
	//std::cout << this << " : push pkt" << std::endl;
	return queue_.push(tem_pkt);
}

AVPacket* AVPacketQueue::pop(const int timeout)
{
	AVPacket* tem_pkt = nullptr;
	int ret = queue_.pop(tem_pkt, timeout);
	if (ret < 0)
	{
		//std::cerr << "avpacket pop failer" << std::endl;
		return nullptr;
	}
	return tem_pkt;
}
 
AVPacket* AVPacketQueue::front(const int timeout)
{
	AVPacket* tem_pkt = nullptr;
	int ret = queue_.front(tem_pkt);
	if (ret < 0)
	{
		//std::cerr << "avpacket front failer" << std::endl;
		return nullptr;
	}
	return tem_pkt;
}


int AVPacketQueue::size()
{
	return queue_.size();
}

void AVPacketQueue::release()
{
	while (true)
	{
		AVPacket* pkt = nullptr;
		int ret = queue_.pop(pkt, 0);
		if (ret >= 0)
		{
			//std::cout << this<<"pkt free" << std::endl;
			av_packet_free(&pkt);
			continue;
		}
		else break;
	}
}


