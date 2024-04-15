#pragma once
#pragma once
#include "ffThread.h"
#include "statx.h"
#include "AVFrameQueue.h"
#include "AVPacketQueue.h"
#include "DemuxThread.h"

extern int _seek_video_flush_decodec;
extern int _seek_req;
extern int _video_permit ;

class videoDecodeThread : public ffThread
{
public:
	videoDecodeThread(AVFrameQueue* framequeue, DemuxThread* demuxThread) :frame_queue_(framequeue), demux_thread_(demuxThread)
	{
		pkt_queue_ = demuxThread->video_queue_;
	};
	~videoDecodeThread();
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

//�����̳߳�ʼ��
int videoDecodeThread::init(AVCodecParameters* prm)
{
	if (prm == nullptr)
	{
		std::cerr << "avcodec_parameters failed" << std::endl;
		return -1;
	}
	else
	{
		//���������������
		codec_ctx_ = avcodec_alloc_context3(NULL);

		//��prm�и�ֵ�� �����������Ĳ���
		int ret = avcodec_parameters_to_context(codec_ctx_, prm);
		if (ret < 0)
		{
			std::cerr << "avcodec_parameters_to_context failed" << std::endl;
			return -1;
		}
		//�ҵ�������

		AVCodec* codec;
		/*	if (AV_CODEC_ID_H264 == codec_ctx_->codec_id)
				codec = avcodec_find_decoder_by_name("h264_qsv");
			else */  //��δ����ʺ���Ӳ������ ,�����������.
		codec = avcodec_find_decoder(codec_ctx_->codec_id);
		if (codec == nullptr)
		{
			std::cerr << "avcodec_find_decoder failed" << std::endl;
			return -1;
		}

		//�����������ͽ�����������
		ret = avcodec_open2(codec_ctx_, codec, NULL);
		if (ret < 0)
		{
			std::cerr << "avcodec_open2 filed " << std::endl;
			return -1;
		}
		return 0;
	}
}

//�����߳����
int videoDecodeThread::start()
{
	//�����߳�
	thread_ = new std::thread(&videoDecodeThread::run, this);

	if (!thread_)
	{
		std::cerr << "�����̴߳���ʧ��" << std::endl;
		return -1;
	}

	return 0;
}

//ֹͣ����
int videoDecodeThread::stop()
{
	ffThread::stop();
	std::cout << "������Ƶ�����߳��˳�" << std::endl;
	return 0;
}

//�����̺߳���
void videoDecodeThread::run()
{
	AVFrame* frame = av_frame_alloc();
	while (abort_ != 1)
	{


		if (frame_queue_->size() > 4)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(40));
			continue;
		}

		if (_seek_video_flush_decodec&& _video_permit)
		{
			avcodec_flush_buffers(codec_ctx_);
			pkt_queue_ = demux_thread_->video_queue_;
			_seek_video_flush_decodec = 0;

		}
			
	

		AVPacket* pkt = pkt_queue_->pop(1);
		if (pkt)
		{
			//��pkt����������, ���������н��� ,ע������������������ͷ�pkt��Դ,��Ҫ�����ֶ��ͷ�.
			int ret = 0;
			//�����seek����,�ͳ�ϴ�������еĲ���֡
		

			ret = avcodec_send_packet(codec_ctx_, pkt);
			av_packet_free(&pkt);
			if (ret < 0)
			{
				std::cerr << "avcodec_send_packet failed" << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				continue;
			}
			//�ӽ������л�ý�����֡, ����ʹ��ѭ����Ϊ�˿���B֡�޷��õ�����,��Ҫ�ȴ�P֡��I֡
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

videoDecodeThread::~videoDecodeThread()
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
