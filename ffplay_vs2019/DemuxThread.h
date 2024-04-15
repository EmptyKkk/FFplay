#pragma once

#include "ffThread.h"
#include "statx.h"
#include "AVPacketQueue.h"

extern int _seek_req;
extern double _seek_pos ;
extern int _seek_frame_release_flag;
extern int _seek_audio_flush_decodec;
extern int _seek_video_flush_decodec ;
extern int _url_network;
extern int _audio_permit ;
extern int _video_permit ;


class DemuxThread : public ffThread
{
public:
	DemuxThread()  {};
	~DemuxThread() { stop(); };
	int init(const char* url); 
	int start();
	int stop();
	void run()override;


	AVPacketQueue* audio_queue_;
	AVPacketQueue* video_queue_;

	AVCodecParameters* audio_codec_prms();
	AVCodecParameters* video_codec_prms();
	AVRational audio_time_base();
	AVRational video_time_base();

	AVFormatContext* ifmt_ctx_ = nullptr;


private:
	std::string url_;

	


	int audio_index_ = -1;
	int video_index_ = -1;

	
};

//�����Ǻ���ʵ��

//�⸴���̳߳�ʼ������,��������ļ�,�ҵ�����Ϣ����������
int DemuxThread::init(const char* url){
	std::cout << "Demux thread: " << url << std::endl;
	int ret = 0;
	url_ = url;

	//����һ��ifmtctx
	ifmt_ctx_ = avformat_alloc_context();

	//�������ļ�
	ret = avformat_open_input(&ifmt_ctx_, url_.c_str(), NULL, NULL);
	//�����ʧ��,���� log
	if (ret < 0){
		std::cerr << "avformat_open_input failed" << std::endl;
		return -1;
	}

	//��������Ϣ
	ret = avformat_find_stream_info(ifmt_ctx_, NULL);
	if (ret < 0){
		std::cerr << "avformat_find_stream_info failed" << std::endl;
		return -1;
	}

	//��ӡ�⸴�ú� �������Ϣ
	av_dump_format(ifmt_ctx_, 0, url_.c_str(), 0);

	//���ｫ��Ƶ����������Ϊ  �Զ�����,һ������ ��Ƶ����Ϊ0 ��ƵΪ1
	audio_index_ = av_find_best_stream(ifmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	video_index_ = av_find_best_stream(ifmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

	/*if (audio_index_ < 0 || video_index_ < 0)
	{
		std::cerr << "�޷��ҵ���Ƶ����Ƶ��" << std::endl;
		return -1;
	}*/
	audio_queue_ = new AVPacketQueue;
	video_queue_ = new AVPacketQueue;

	return 0;
}



void release(AVPacketQueue* pktQueue)
{
	pktQueue->release();
}
//ѭ����ȡ֡��pkt��
void DemuxThread::run()
{
	int ret = 0;
	AVPacket pkt;
	int maxPktQueueSize = 100;

	while (abort_ != 1)
	{

		//seek���� 
		if (_seek_req ){
			av_read_pause(ifmt_ctx_);
			auto audioPktQueueRelease = audio_queue_;
			auto videoPktQueueRelease = video_queue_;

			std::thread audioRelease(&release,audioPktQueueRelease);
			audioRelease.detach();
			std::thread videoRelease(&release,videoPktQueueRelease);
			videoRelease.detach();

			/*audio_queue_->release();
			video_queue_->release();*/

			audio_queue_ = new AVPacketQueue;
			video_queue_ = new AVPacketQueue;

			int64_t audioPts, videoPts;

			double seekTime = 0.0;
			//����Ƶͼ��Ϊ��׼����seek,��Ҫ��������Ƶ��I֡��seek��Ӱ��ܴ�
			if(_audio_permit)
			seekTime = _seek_pos * (double)ifmt_ctx_->streams[audio_index_]->duration/ (double)ifmt_ctx_->streams[audio_index_]->time_base.den;  //�ҵ����λ�ö�Ӧ��ʱ��ֵ
			else 
			seekTime = _seek_pos * (double)ifmt_ctx_->streams[video_index_]->duration / (double)ifmt_ctx_->streams[video_index_]->time_base.den;


			if(_audio_permit)
			audioPts = seekTime * ifmt_ctx_->streams[audio_index_]->time_base.den;		//��ʱ��ת��Ϊpts
			if(_video_permit)
			videoPts = seekTime * ifmt_ctx_->streams[video_index_]->time_base.den;  //ͨ��ʵ��ʱ��ֵ�ҵ���Ƶ֡��pts

			//int64_t audioPts = seekTime * ifmt_ctx_->streams[audio_index_]->time_base.den;
			//�ƶ�����Ӧ��pts��
			//av_seek_frame(ifmt_ctx_, audio_index_, audioPts, AVSEEK_FLAG_ANY);
			////ѭ���ҵ�pts�����Ĺؼ�֡
			//while (1)
			//{
			//	ret = av_read_frame(ifmt_ctx_, &pkt);
			//	if (ret < 0)
			//	{
			//		std::cerr << "av_read_frame failed" << "\t";
			//		return;
			//	}
			//	if (pkt.stream_index == audio_index_ && pkt.flags == AV_PKT_FLAG_KEY)
			//		break;
			//	else
			//	{
			//		av_packet_unref(&pkt);
			//		continue;
			//	}
			//}
			//videoPts = pkt.pts;
			//seekTime = double(audioPts) / ifmt_ctx_->streams[audio_index_]->time_base.den;  //����ؼ�֡��ʵ��ʱ��ֵ
			
			_seek_frame_release_flag = 1;
			_seek_audio_flush_decodec = 1;
			_seek_video_flush_decodec = 1;

			//if (_video_permit)
			//av_seek_frame(ifmt_ctx_, video_index_, videoPts, AVSEEK_FLAG_BACKWARD);		//����Ƶpkt������ת
			//av_seek_frame(ifmt_ctx_, audio_index_, audioPts, AVSEEK_FLAG_BACKWARD);		//����Ƶpkt������ת

		
			if (_video_permit)
			avformat_seek_file(ifmt_ctx_, video_index_, INT64_MIN, videoPts, INT64_MAX, AVSEEK_FLAG_BACKWARD);

			if(_audio_permit)
			avformat_seek_file(ifmt_ctx_, audio_index_, INT64_MIN, audioPts, INT64_MAX, AVSEEK_FLAG_BACKWARD);
		

		
			//video_queue_->push(&pkt);
			//av_packet_unref(&pkt);
			_seek_req  = 0 ;
			av_read_play(ifmt_ctx_);

		}

		//��ifmtctx�ж�ȡ����pkt��
		ret = av_read_frame(ifmt_ctx_, &pkt);
		if (ret < 0){
			std::cerr << "av_read_frame failed" << "\t";
			return;
		}

		//����Ƶ����������Ƶ������
		if (pkt.stream_index == audio_index_){
			if (audio_queue_->size() >= maxPktQueueSize/2 || video_queue_->size()>= maxPktQueueSize / 2)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			audio_queue_->push(&pkt);
			av_packet_unref(&pkt);
			//std::cout << "audio pkt size:" << audio_queue_->size() << std::endl;
		}
		//����Ƶ����������Ƶ������
 		else if (pkt.stream_index == video_index_){
			if (video_queue_->size() >= maxPktQueueSize / 2 ||  audio_queue_->size() >= maxPktQueueSize / 2)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			video_queue_->push(&pkt);
			av_packet_unref(&pkt);
			//std::cout << "video pkt size:" << video_queue_->size() << std::endl;
		}

		//�������������в���,ֱ�ӽ�����
		else 
			av_packet_unref(&pkt);
	}
}

//�����̲߳���ʼִ��run����
int DemuxThread::start()
{
	thread_ = new std::thread(&DemuxThread::run, this);
	if (!thread_)
	{
		std::cerr << "������ʼ�߳�ʧ��" << std::endl;
		return -1;
	}
	return 0;
}

//ֹͣ����
int DemuxThread::stop()
{
	ffThread::stop();
	std::cout << "�⸴���߳��˳�" << std::endl;
	avformat_close_input(&ifmt_ctx_);
	return 0;

}

//�����Ƶ����������
AVCodecParameters* DemuxThread::audio_codec_prms()
{
	if (audio_index_ >= 0)
	{
		return ifmt_ctx_->streams[audio_index_]->codecpar;
	}
	else return nullptr;
}

//�����Ƶ����������
AVCodecParameters* DemuxThread::video_codec_prms()
{
	if (video_index_ >= 0)
	{
		return ifmt_ctx_->streams[video_index_]->codecpar;
	}
	else return nullptr;
}

//�����Ƶʱ���׼
AVRational DemuxThread::audio_time_base()
{
	if (audio_index_ >= 0)
	{
		return ifmt_ctx_->streams[audio_index_]->time_base;
	}
	else return AVRational{ 0,0 };
}

//�����Ƶʱ���׼
AVRational DemuxThread::video_time_base()
{
	if (video_index_ >= 0)
	{
		return ifmt_ctx_->streams[video_index_]->time_base;
	}
	else return AVRational{ 0,0 };
}