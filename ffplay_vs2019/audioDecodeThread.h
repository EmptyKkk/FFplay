#pragma once
#include "ffThread.h"
#include "statx.h"
#include "AVFrameQueue.h"
#include "AVPacketQueue.h"
#include "DemuxThread.h"

extern int _seek_audio_flush_decodec ;
extern int _seek_req;
extern int _audio_permit ;

class audioDecodeThread : public ffThread
{
public:
	audioDecodeThread(AVFrameQueue* framequeue,DemuxThread* demuxThread) :frame_queue_(framequeue),demux_thread_(demuxThread) 
	{
		pkt_queue_ = demuxThread->audio_queue_;
	};
	~audioDecodeThread();
	int init(AVCodecParameters* prm);
	int start();
	int stop();
	void run();

private:
	AVCodecContext* codec_ctx_ = nullptr;
	AVPacketQueue* pkt_queue_ = nullptr;
	AVFrameQueue* frame_queue_ = nullptr;
	DemuxThread* demux_thread_ = nullptr;
};

//解码线程初始化
int audioDecodeThread::init(AVCodecParameters* prm)
{
	if (prm == nullptr)
	{
		std::cerr << "avcodec_parameters failed" << std::endl;
		return -1;
	}
	else
	{
		//分配解码器上下文
		codec_ctx_ = avcodec_alloc_context3(NULL);

		//从prm中赋值给 解码器上下文参数
		int ret = avcodec_parameters_to_context(codec_ctx_, prm);
		if (ret < 0)
		{
			std::cerr << "avcodec_parameters_to_context failed" << std::endl;
			return -1;
		}
		//找到解码器

		AVCodec* codec;
	/*	if (AV_CODEC_ID_H264 == codec_ctx_->codec_id)
			codec = avcodec_find_decoder_by_name("h264_qsv");
		else */  //这段代码适合于硬件解码 ,但会出现问题.
		codec = avcodec_find_decoder(codec_ctx_->codec_id);
		if (codec == nullptr)
		{
			std::cerr << "avcodec_find_decoder failed" << std::endl;
			return -1;
		}

		//关联解码器和解码器上下文
		ret = avcodec_open2(codec_ctx_, codec, NULL);
		if (ret < 0)
		{
			std::cerr << "avcodec_open2 filed " << std::endl;
			return -1;
		}
		return 0;
	}
}

//解码线程入口
int audioDecodeThread::start()
{
	//创建线程
	thread_ = new std::thread(&audioDecodeThread::run,this);

	if (!thread_)
	{
		std::cerr << "解码线程创建失败" << std::endl;
		return -1;
	}

	return 0;
}

//停止函数
int audioDecodeThread::stop()
{
	ffThread::stop();
	std::cout << "音或视频解码线程退出" << std::endl;
	return 0;
}

//解码线程函数
void audioDecodeThread::run()
{
	AVFrame* frame = av_frame_alloc();
	while (abort_ != 1)
	{
		

		if (frame_queue_->size() >4)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(40));
			continue;
		}

		//用于清除缓冲区中的帧
		if (_seek_audio_flush_decodec&& _audio_permit)
		{
			pkt_queue_ = demux_thread_->audio_queue_;
			avcodec_flush_buffers(codec_ctx_);
			_seek_audio_flush_decodec = 0;

		}
		
		
		AVPacket* pkt = pkt_queue_->pop(1);
		if (pkt)
		{
			//将pkt送往解码器, 解码器进行解码 ,注意这里解码器并不会释放pkt资源,需要进行手动释放.
			int ret = 0;
			
			ret = avcodec_send_packet(codec_ctx_,pkt);
			av_packet_free(&pkt);
			if (ret<0)
			{
				std::cerr << "avcodec_send_packet failed" << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				continue;
			}
			//从解码器中获得解码后的帧, 这里使用循环是为了考虑B帧无法得到数据,需要等待P帧和I帧
			while (true)
			{
				ret = avcodec_receive_frame(codec_ctx_, frame);
				if (ret == 0)
				{
					frame_queue_->push(frame);
					//std::cout  << " frame_queue_->push(frame), size:" << frame_queue_->size() << std::endl;
					continue;
				}
				else if (ret == AVERROR(EAGAIN))
				{
					break;
				}
				else
				{
					abort_ = 1;
					std::cerr << "avcodec_receive_frame failed" << std::endl;
					break;
				}
			}
		}
		else 
		{
			//std::cout << "no pakcet" << std::endl;
		}
	}
	return;
}

audioDecodeThread::~audioDecodeThread()
{
	if (thread_ != nullptr)
	{
		stop();
		thread_ = nullptr;
	}
	if (codec_ctx_ != nullptr)
	{
		avcodec_close(codec_ctx_);
		codec_ctx_ = nullptr;
	}
}
