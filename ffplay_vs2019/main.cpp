#pragma once
#include "statx.h"
#include "DemuxThread.h"
#include "audioDecodeThread.h"
#include "videoDecodeThread.h"
#include "AudioOutput.h"
#include "VideoOutput.h"
#include "AVSync.h"
#include<iostream>

//�˳�����Դ����ͷ�
//����seek��
//����h264��Ӳ�����뷽ʽ

extern int _seek_req = 0;
extern double _seek_pos = 0;
extern int _seek_frame_release_flag = 0;
extern int _seek_video_flush_decodec = 0;
extern int _seek_audio_flush_decodec = 0;
extern int _url_network = 0;
extern int _audio_permit = 0;  
extern int _video_permit = 0;

int main(int argc, char* argv[])
{
	int ret;

	//����Ƶs  
	//char fileName[] = "E:\\LZ��ѧϰ����\\����Ƶ\\0��Ƶ�ļ�\\��ΰ�����Ʒ.aac";
	// 
	//����Ƶ
	//char fileName[] = "E:\\LZ��ѧϰ����\\����Ƶ\\0��Ƶ�ļ�\\bigbuckbunny_480x272.h264";
	// 
	//����Ƶ
	//char fileName[] = "E:\\LZ��ѧϰ����\\����Ƶ\\0��Ƶ�ļ�\\Titanic.ts";
	//char fileName[] = "E:\\LZ��ѧϰ����\\����Ƶ\\0��Ƶ�ļ�\\time.mp4";
	//char fileName[] = "E:\\LZ��ѧϰ����\\����Ƶ\\0��Ƶ�ļ�\\44cout.mp4 ";
	//char fileName[] = "rtsp://127.0.0.1:8557/test";
	char* fileName = argv[1];
	std::cout << fileName << std::endl;
	AVFrameQueue audioFrameQueue;
	AVFrameQueue videoFrameQueue;

	//�趨ͬ��ʱ��
	AVSync  avsync;
	avsync.init_clock();

	//�⸴�ó�ʼ����ִ��
	DemuxThread* pDemuxThread = new DemuxThread();
	ret = pDemuxThread->init(fileName);
	if (ret < 0) return -1;
	ret = pDemuxThread->start();
	if (ret < 0) return -1;

	audioDecodeThread* pAudioDecodeThread = nullptr;	//��������Ƶ�����߳�
	videoDecodeThread * pVideoDecodeThread = nullptr;
	
	pAudioDecodeThread = new audioDecodeThread(&audioFrameQueue,pDemuxThread);
	pVideoDecodeThread = new videoDecodeThread(&videoFrameQueue,pDemuxThread);

	//�����̳߳�ʼ��, �ӽ⸴���߳����ҵ��������.
	ret = pAudioDecodeThread->init(pDemuxThread->audio_codec_prms());
	if (ret < 0)
	{
		std::cerr << "pAudioDecodeThread failed " << std::endl;
	}
	ret = pVideoDecodeThread->init(pDemuxThread->video_codec_prms());
	if (ret < 0)
	{
		std::cerr << "pVideoDecodeThread failed " << std::endl;
	}
	ret = pAudioDecodeThread->start();
	if (ret < 0) return -1;
	ret = pVideoDecodeThread->start();
	if (ret < 0) return -1; 

	std::thread* pAudioOutputThread;
	//������Ƶ��ز���

		AudioPrms audioPrms = { 0 };
		memset(&audioPrms, 0, sizeof(AudioPrms));
		auto audioCodecPrms = pDemuxThread->audio_codec_prms();

		//���û����Ƶ,��Ӧ����Ƶʱ������в���.
		if (!audioCodecPrms){
			goto jump_audio_set;
		}
		audioPrms.channels = audioCodecPrms->channels;
		audioPrms.channelLayout = audioCodecPrms->channel_layout;
		audioPrms.fmt = (enum AVSampleFormat)audioCodecPrms->format;
		audioPrms.sampleRate = audioCodecPrms->sample_rate;
		audioPrms.frameSize = audioCodecPrms->frame_size;
		_audio_permit = 1;
		jump_audio_set:
		//����ʱ���׼
		AVRational audioTimeBase = pDemuxThread->audio_time_base();

		//��Ƶ���
		AudioOutput* audioOutput = new AudioOutput(&avsync, audioTimeBase, audioPrms, &audioFrameQueue);
		std::thread audioOutputThread(&AudioOutput::init, audioOutput);
		pAudioOutputThread = &audioOutputThread;

		
		//��Ƶ���,  ���ÿ��,����֡����.
		//������Ƶʱ���׼
		VideoOutput* pVideoOutput;
		AVRational videoTimeBase = pDemuxThread->video_time_base();

		//���û����Ƶ��,���ֻ��һ����ɫ����
		if (!pDemuxThread->video_codec_prms()){
			pVideoOutput = new VideoOutput(&avsync, videoTimeBase, &videoFrameQueue, pDemuxThread, 600, 400);
		}
		else
		{
			pVideoOutput = new VideoOutput(&avsync, videoTimeBase, &videoFrameQueue, pDemuxThread,
											pDemuxThread->video_codec_prms()->width, pDemuxThread->video_codec_prms()->height);
			_video_permit = 1;
		}
		ret = pVideoOutput->init();
		if(ret<0)
		{
			std::cerr << "pVideoOutput init failed" << std::endl;
			return -1;
		}
		pVideoOutput->main_loop();
	

	//���߳�ִ��ʱ��
	std::this_thread::sleep_for(std::chrono::seconds(140));

	//�߳���ͣ����Դ�ͷ�
	pDemuxThread->stop();
	pAudioDecodeThread->stop();
	pVideoDecodeThread->stop();
	

	delete pAudioDecodeThread;
	delete pVideoDecodeThread;
	delete pDemuxThread;

	if(pAudioOutputThread)
		pAudioOutputThread->join();

	return 0;
}