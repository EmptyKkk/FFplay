#pragma once
#include "statx.h"
#include "AVFrameQueue.h"
#include "AVSync.h"

//ffmpeg自带的aac解码器输出的pcm格式为float p
//而SDL音频要求输入的pcm格式为s16,因此需要进行重采样.




struct AudioPrms
{
	int sampleRate;  //采样率
	int channels;	//声道数
	int channelLayout;	//声道布局
	enum AVSampleFormat fmt;	//采样格式
	int frameSize;	//一帧中的采样数
};

class AudioOutput
{
public:
	AudioOutput(AVSync* avsync, AVRational time_base, const AudioPrms& audio_prms, AVFrameQueue* frame_queue) : 
		src_prms_(audio_prms),frame_queue_(frame_queue), avsync_(avsync), time_base_(time_base){};
	~AudioOutput() { Deinit(); };
	int init();
	int Deinit();

public:
	AudioPrms src_prms_;  //解码后的音频帧参数
	AudioPrms dst_prms_;	//SDL实际输出的音频帧参数
	AVFrameQueue* frame_queue_ = nullptr; 

	SwrContext* swr_ctx_ = nullptr;
	uint8_t* audio_buf_ = nullptr;  //数据的首指针
	uint8_t* audio_buf1_ = nullptr;	//数据的遍历指针
	uint32_t audio_buf_size_ = 0;	//数据的总长度
	uint32_t audio_buf1_size_ = 0;	//数据的遍历长度
	uint32_t audio_buf_index_ = 0;	//数据的遍历索引 ?
	AVSync* avsync_ =nullptr;
	AVRational time_base_;
	int64_t pts_ = AV_NOPTS_VALUE;

	//静音
	int muted_ = 0;
};

FILE* dump_pcm = NULL ;

//SDL音频回调函数
void fill_audio_pcm(void* udata, Uint8* stream, int len)
{
	//从frameQueue读取解码后的PCM数据,填充到stream中
	AudioOutput* is = (AudioOutput*)udata;
	int len1=0;
	int audio_size;
	
	//if (!dump_pcm) 
	//{
	//	dump_pcm = fopen("dump.pcm", "wb");s
	//}

	//如果处于暂停状态就阻塞在这. 如果单步运行,就放开一次
	while (is->avsync_->pause_)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		if (is->avsync_->step_)
			break;
	}
	
	while (len > 0)
	{
		if (is->audio_buf_index_ == is->audio_buf_size_)
		{
			is->audio_buf_index_ = 0;
			AVFrame* frame = is->frame_queue_->pop(1);
			if (frame)  //如果读到解码后的数据
			{
				//设定is 的pts
				is->pts_ = frame->pts;
				//判断是否做重采样
				if ( ( (frame->format != is->dst_prms_.fmt)
					|| (frame->sample_rate != is->dst_prms_.sampleRate)
					|| (frame->channel_layout != is->dst_prms_.channelLayout) )
					&& (!is->swr_ctx_) )
				{
					//如果做重采样,则首先分配转换结构体,并设置一些选项
					is->swr_ctx_ = swr_alloc_set_opts(NULL,
													is->dst_prms_.channelLayout,  //目标
													(enum AVSampleFormat)is->dst_prms_.fmt,  //目标
													is->dst_prms_.sampleRate,  //目标
													frame->channel_layout, //源
													(enum AVSampleFormat)frame->format, //源
													frame->sample_rate, //源
													0, NULL);
					if (!is->swr_ctx_ || swr_init(is->swr_ctx_) < 0)
					{
						std::cerr << "cant create sample rate convert for conversion" << std::endl;
						swr_free((SwrContext**) & is->swr_ctx_);
						return;
					}
						

				}
				if (is->swr_ctx_)  //重采样
				{
					 uint8_t** in =  (uint8_t**)frame->extended_data;
					uint8_t** out = &is->audio_buf1_;

					int out_samples = frame->nb_samples * is->dst_prms_.sampleRate / frame->sample_rate +256;
					int out_bytes = av_samples_get_buffer_size(NULL, is->dst_prms_.channels,out_samples, is->dst_prms_.fmt, 0);
					if (out_bytes < 0)
					{
						std::cerr << "av_samples_get_buffer_size failed" << std::endl;
						return;
					}
					av_fast_malloc(&is->audio_buf1_, &is->audio_buf1_size_, out_bytes);

					int len2 = swr_convert(is->swr_ctx_, out, out_samples, (const uint8_t**)in, frame->nb_samples);  //返回样本数
					if (len2 < 0)
					{
						std::cerr << "swr_convert failed" << std::endl;
						return;
					}
					is->audio_buf_ = is->audio_buf1_;
					is->audio_buf_size_ = av_samples_get_buffer_size(NULL, is->dst_prms_.channels, len2, (enum AVSampleFormat)is->dst_prms_.fmt, 1);
				
				}
				else  //没有重采样 
				{
					audio_size = av_samples_get_buffer_size(NULL, frame->channels, frame->nb_samples, (enum AVSampleFormat)frame->format, 1);
					av_fast_malloc(&is->audio_buf1_,&is->audio_buf1_size_,audio_size);
					is->audio_buf_ = is->audio_buf1_;
					is->audio_buf1_size_ = audio_size;
					memcpy(is->audio_buf_, frame->data[0], audio_size);
				
				}

				
				av_frame_free(&frame);
			}
			else  //没有读到解码后的数据
			{
				is->audio_buf_ = nullptr;
				is->audio_buf_size_ = 512;
			}
		}
		len1 = is->audio_buf_size_ - is->audio_buf_index_;
		if (len1 > len)
			len1 = len;

		if (!is->audio_buf_)
		{
			if(!is->muted_)
			memset(stream, 0,len1);
		}
		else  //真正拷贝有效的数据
		{ 
			if(!is->avsync_->muted_)
			memcpy(stream, is->audio_buf_+is->audio_buf_index_,len1);  
			else memset(stream, 0, len1);
			//写pcm文件
			/*fwrite((uint8_t*)is->audio_buf_ + is->audio_buf_index_, 1, len1, dump_pcm);
			fflush(dump_pcm);*/
		}
		len -= len1;
		stream += len1;
		is->audio_buf_index_ += len1;
	}


	//设置时钟
	if (is->pts_ != AV_NOPTS_VALUE)
	{
		double pts = is->pts_ * av_q2d(is->time_base_);
		//std::cout << "audio pts : " << pts << std::endl;
		is->avsync_->set_clock(pts);
	}



}

//对解码后的音频帧做处理,使其适合SDL输出.
int AudioOutput::init()
{
	if (SDL_Init(SDL_INIT_AUDIO| SDL_INIT_TIMER) != 0)
	{
		std::cerr << "SDL_Init faild" << std::endl;
		return -1;
	}

	SDL_AudioSpec wantedSpc, spc;
	wantedSpc.channels = src_prms_.channels;
	wantedSpc.freq = src_prms_.sampleRate;
	wantedSpc.format = AUDIO_S16;
	wantedSpc.silence = 0;  //是否静音静音
	wantedSpc.callback = fill_audio_pcm;
	wantedSpc.samples = src_prms_.frameSize;  //采样数量
	wantedSpc.userdata = this;


	 int ret = SDL_OpenAudio(&wantedSpc,NULL);
	 if (ret < 0)
	 {
		std:: cerr << "SDL_OpenAudio failed" << std::endl;
		return -1;
	 }

	 dst_prms_.channels = wantedSpc.channels;
	 dst_prms_.sampleRate = wantedSpc.freq;
	 dst_prms_.frameSize = 1024;
	 dst_prms_.fmt = AV_SAMPLE_FMT_S16;
	 dst_prms_.channelLayout = av_get_default_channel_layout(2);

	 SDL_PauseAudio(0);
	// std::cout << "SDL_PauseAudio(0)" << std::endl;
	 return 0;
}

int AudioOutput::Deinit()
{
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	return 0;
}
