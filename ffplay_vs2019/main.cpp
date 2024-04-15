#pragma once
#include "statx.h"
#include "DemuxThread.h"
#include "audioDecodeThread.h"
#include "videoDecodeThread.h"
#include "AudioOutput.h"
#include "VideoOutput.h"
#include "AVSync.h"
#include<iostream>

//退出的资源如何释放
//播放seek等
//诸如h264等硬件解码方式

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

	//纯音频s  
	//char fileName[] = "E:\\LZ的学习资料\\音视频\\0音频文件\\最伟大的作品.aac";
	// 
	//纯视频
	//char fileName[] = "E:\\LZ的学习资料\\音视频\\0视频文件\\bigbuckbunny_480x272.h264";
	// 
	//音视频
	//char fileName[] = "E:\\LZ的学习资料\\音视频\\0视频文件\\Titanic.ts";
	//char fileName[] = "E:\\LZ的学习资料\\音视频\\0视频文件\\time.mp4";
	//char fileName[] = "E:\\LZ的学习资料\\音视频\\0视频文件\\44cout.mp4 ";
	//char fileName[] = "rtsp://127.0.0.1:8557/test";
	char* fileName = argv[1];
	std::cout << fileName << std::endl;
	AVFrameQueue audioFrameQueue;
	AVFrameQueue videoFrameQueue;

	//设定同步时钟
	AVSync  avsync;
	avsync.init_clock();

	//解复用初始化与执行
	DemuxThread* pDemuxThread = new DemuxThread();
	ret = pDemuxThread->init(fileName);
	if (ret < 0) return -1;
	ret = pDemuxThread->start();
	if (ret < 0) return -1;

	audioDecodeThread* pAudioDecodeThread = nullptr;	//创建音视频解码线程
	videoDecodeThread * pVideoDecodeThread = nullptr;
	
	pAudioDecodeThread = new audioDecodeThread(&audioFrameQueue,pDemuxThread);
	pVideoDecodeThread = new videoDecodeThread(&videoFrameQueue,pDemuxThread);

	//解码线程初始化, 从解复用线程中找到解码参数.
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
	//设置音频相关参数

		AudioPrms audioPrms = { 0 };
		memset(&audioPrms, 0, sizeof(AudioPrms));
		auto audioCodecPrms = pDemuxThread->audio_codec_prms();

		//如果没有音频,则应以视频时间戳进行播放.
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
		//设置时间基准
		AVRational audioTimeBase = pDemuxThread->audio_time_base();

		//音频输出
		AudioOutput* audioOutput = new AudioOutput(&avsync, audioTimeBase, audioPrms, &audioFrameQueue);
		std::thread audioOutputThread(&AudioOutput::init, audioOutput);
		pAudioOutputThread = &audioOutputThread;

		
		//视频输出,  设置宽高,关联帧队列.
		//设置视频时间基准
		VideoOutput* pVideoOutput;
		AVRational videoTimeBase = pDemuxThread->video_time_base();

		//如果没有视频流,则仅只打开一个白色窗口
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
	

	//主线程执行时间
	std::this_thread::sleep_for(std::chrono::seconds(140));

	//线程暂停与资源释放
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