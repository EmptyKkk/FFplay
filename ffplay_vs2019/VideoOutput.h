#pragma once
#include "statx.h"
#include "AVFrameQueue.h"
#include <thread>
#include "AVSync.h"

extern int _seek_req;
extern int _seek_req_flag;
extern int _seek_frame_release_flag;
static int _scale_flag = 0;

class VideoOutput
{
public:
	VideoOutput(AVSync *avsync, AVRational time_base, AVFrameQueue* frame_queue,  DemuxThread* demutx_ctx, int video_width = 0, int video_height=0)
		: avsync_(avsync), time_base_(time_base), frame_queue_(frame_queue), demutx_ctx_(demutx_ctx){ video_width_ = video_width, video_height_ = video_height; };
	~VideoOutput() {};
	int init();
	int main_loop(); 
	int stream_seek(double pos);
private:
	void video_refresh(double* remaining_time);
	AVFrameQueue* frame_queue_ = nullptr;
	SDL_Event event_;
	SDL_Rect rect_;
	SDL_Window *win_= nullptr;
	SDL_Renderer* renderer_ = nullptr;
	SDL_Texture* texture_ = nullptr;
	
	int video_width_ = 0;
	int video_height_ = 0;

	uint8_t* yuv_buf_ = nullptr;
	float yuv_buf_fer_size_ = 0;

	int is_full_screen_ = 0;

	//SDL_mutex* mutex_;
	AVRational time_base_;
	AVSync* avsync_;

	//һ��֡������pts��
	int frame_pts_size_=0;
	//ÿ֡��pts
	int frame_pts_=0;

	DemuxThread* demutx_ctx_;

	SwsContext* sws_ctx_ = nullptr;
	

	void refresh_loop_wait_event(SDL_Event* event);



};

#define REFRESH_RATE 0.01  //�趨ˢ��Ƶ��
void VideoOutput::refresh_loop_wait_event(SDL_Event* event)
{
	double remaining_time = 0.0;
	SDL_PumpEvents();
	
	while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT))
	{

		if (remaining_time > 0.0)
			std::this_thread::sleep_for(std::chrono::milliseconds (int64_t(remaining_time * 1000.0)));

		if (_seek_frame_release_flag)
		{
			frame_queue_->release();
			_seek_frame_release_flag = 0;
		}

		//����remain_time
		remaining_time = REFRESH_RATE;

		//����ˢ�»���
		video_refresh(&remaining_time);
		SDL_PumpEvents();
	}
}

void VideoOutput::video_refresh(double* remaining_time)
{

	//���������ͣ״̬,��˯��һ��ʱ�䲢����.
	if (avsync_->pause_)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		if (!avsync_->step_)  //�����������,������һ�η���,����������Ϊ0
		{
			return;
		}
		avsync_->step_ = 0;
	}


	AVFrame* frame = nullptr;
	start:
	double pts = 0;
	frame = frame_queue_->front(1);
	if (frame) //�����֡�ͽ�����Ⱦ
	{
		
		if (frame->pts >= 0)
		{
			pts = double(frame->pts) * av_q2d(time_base_);
			//	frame_pts_size_ = frame->pts - frame_pts_size_;
		}
		else  //�����Ƶ��ptsΪ��,��û�б��趨,���Լ��趨
		{
			//���ڿ���֡��
			int frame_rate = 24;

			frame_pts_size_ = time_base_.den / frame_rate;
			
			frame_pts_ += frame_pts_size_;

			 pts = frame_pts_ * 1.0 / double(time_base_.den);
		}

		//��ϸ�趨remaining_time
		double diff = pts - avsync_->get_clock();
		//�����Ƶ����Ƶ������������Ӧ��֡.
		if (abs(diff) > 1)
		{
			frame = frame_queue_->pop(0);
			av_frame_free(&frame);
			goto start;
		}

		if (diff > 0) //�����֡��Ӧ����ʾʱ�� > ��ǰʱ�� ���remaining_time �����趨, ѡȡdiff �� remaining_time��Сֵ.
		{
			*remaining_time = FFMIN(*remaining_time, diff);
			return;
		}
		else //�����Ƶʱ��û���趨,����pts,���ptsΪ��,�����30֡�Ĳ���,���ptsΪ��,��԰���ÿ֡��pts����ʱ���׼����֡���
		{
			*remaining_time = FFMAX(frame_pts_size_ * av_q2d(time_base_), REFRESH_RATE);
		}

		rect_.x = 0;
		rect_.y = 0;
		rect_.w = video_width_;
		rect_.h = video_height_;
		
		int winWidth, winHeight, rectXShow, rectYShow;
		SDL_GetWindowSize(win_, &winWidth, &winHeight);
		if (winWidth != video_width_ || winHeight != video_height_)
			_scale_flag = 1;

		//�ж��Ƿ����ȫ��ģʽ
		SDL_SetWindowFullscreen(win_, is_full_screen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP: 0);

		//�ж��Ƿ���Ҫͼ������
		if (is_full_screen_|| _scale_flag)
		{
			SDL_Rect dst_rect_;
			

			if (winHeight >= winWidth)  //���������
			{
			a:
				rectXShow = winWidth;
				rectYShow = winWidth * (double(video_height_) / double(video_width_));
				dst_rect_.x = 0;
			
				dst_rect_.y = winHeight / 2 - rectYShow / 2;
				if (dst_rect_.y < 0)
					goto b;
			}
			else		//���ں����
			{
			b:
				rectYShow = winHeight;
				rectXShow = winHeight * (double(video_width_) / double(video_height_));
		

				dst_rect_.y = 0;
				dst_rect_.x = winWidth / 2 - rectXShow / 2;
				if (dst_rect_.x < 0)
					goto a;

			}
			dst_rect_.w = rectXShow;
			dst_rect_.h = rectYShow;

			/*	av_opt_set_int(sws_ctx_, "dstw", rectXShow, 0);
				av_opt_set_int(sws_ctx_, "dsth", rectYShow, 0);
				av_opt_set_int(sws_ctx_, "dst_format", AV_PIX_FMT_YUV420P, 0);
				av_opt_set_int(sws_ctx_, "dst_range", 1, 0);*/

			sws_ctx_ = sws_getContext(video_width_, video_height_, AV_PIX_FMT_YUV420P,
										rectXShow, rectYShow, AV_PIX_FMT_YUV420P,
										SWS_BICUBIC, NULL, NULL, NULL);

			uint8_t* pDstData[4];
			int dstLineSize[4];
			av_image_alloc(pDstData, dstLineSize, rectXShow, rectYShow, AV_PIX_FMT_YUV420P, 1);

			
			sws_scale(sws_ctx_, frame->data, frame->linesize, 0, video_height_, pDstData, dstLineSize);
			

			SDL_DestroyTexture(texture_);
			texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, rectXShow, rectYShow);

			SDL_UpdateYUVTexture(texture_, NULL, pDstData[0],dstLineSize[0],
												 pDstData[1],dstLineSize[1],
												 pDstData[2],dstLineSize[2]);
			SDL_RenderClear(renderer_);
			SDL_RenderCopy(renderer_, texture_,NULL, &dst_rect_);
			SDL_RenderPresent(renderer_);
			av_freep(&pDstData[0]);
		}
		else
		{	
				//����ֻ������YUV��ʽ,�����Ҫ֧��������ʽ,��Ҫ���и�ʽת��.
			SDL_UpdateYUVTexture(texture_,&rect_,frame->data[0],frame->linesize[0],
												 frame->data[1],frame->linesize[1],
												 frame->data[2],frame->linesize[2]);
			SDL_RenderClear(renderer_);
			SDL_RenderCopy(renderer_, texture_, NULL, &rect_);
			SDL_RenderPresent(renderer_);
		}


		frame_queue_->pop(1);

		if (sws_ctx_)
		{
			sws_freeContext(sws_ctx_);
			sws_ctx_ = nullptr;
		}
		av_frame_free(&frame);
		
	}
}

//��ʼ������
int VideoOutput::init()
{
	//��ʼ��SDL_VIDEO
	if (SDL_Init(SDL_INIT_VIDEO))
	{
		std::cerr << "SDL_init VIDEO failed" << std::endl;
		return -1;
	}

	//��������
	win_ = SDL_CreateWindow("Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
							video_width_, video_height_,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if(!win_)
	{
		std::cerr << "SDL_CreateWindow failed" << std::endl;
		return -1;
	}

	//������Ⱦ��
	renderer_ = SDL_CreateRenderer(win_, -1, 0);
	if(!renderer_)
	{
		std::cerr << "SDL_CreateRenderer failed" << std::endl;
		return -1;
	}

	//��������
	texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, video_width_, video_height_);
	if (!texture_)
	{
		std::cerr << "SDL_CreateTexture failed" << std::endl;
		return -1;
	}

	/*sws_ctx_ = sws_alloc_context();
	av_opt_set_int(sws_ctx_, "sws_flags", SWS_BICUBIC | SWS_PRINT_INFO, 0);
	av_opt_set_int(sws_ctx_, "srcw", video_width_, 0);
	av_opt_set_int(sws_ctx_, "srch", video_height_, 0);
	av_opt_set_int(sws_ctx_, "src_format", AV_PIX_FMT_YUV420P, 0);
	av_opt_set_int(sws_ctx_, "src_range", 1, 0);
	sws_init_context(sws_ctx_, NULL, NULL);*/

	yuv_buf_fer_size_ = video_width_ * video_height_ * 1.5;
	yuv_buf_ = (uint8_t *)malloc(yuv_buf_fer_size_);



	return 0;
	
}

//�¼�ѭ��
int VideoOutput::main_loop()
{
	int x = 0;
	SDL_Event event;
	while (true)
	{
		refresh_loop_wait_event(&event);


		switch (event.type)
		{
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
				case SDLK_ESCAPE:
					std::cout << "esc key down" << std::endl;
					return 0;
					break;
				case SDLK_f:
					is_full_screen_ = !is_full_screen_;
					break;
				case SDLK_p:
				case SDLK_SPACE:
					avsync_->pause_ = !avsync_->pause_;
					break;
				case SDLK_s:
					avsync_->pause_ = 1;
					avsync_->step_ = 1;
					break;
				case SDLK_m:
					avsync_->muted_ = !avsync_->muted_;
					std::cout << "SDLK_m" << std::endl;
					break;
			}
			break;
		case SDL_MOUSEBUTTONDOWN:  //������˫���л�ȫ��
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					static int64_t last_mouse_left_click;
					if (avsync_->get_micro_seconds() - last_mouse_left_click <= 500000)  //0.5s
					{
						is_full_screen_ = !is_full_screen_;
						last_mouse_left_click = 0;
					}
					else
					{
						last_mouse_left_click = avsync_->get_micro_seconds();
					}
					break;
				}
				

				//�Ҽ�����,��¼��ǰ���λ��.
				if (event.button.button == SDL_BUTTON_RIGHT)
				{
					x = event.button.x;
					//std::cout << "x = event.button.x" << std::endl;
				}

			/*case SDL_MOUSEMOTION:
				if (event.type == SDL_MOUSEBUTTONDOWN)
				{
					
				}
				else
				{
					if (!(event.motion.state & SDL_BUTTON_RMASK))
						break;
					x = event.motion.x;
					std::cout << "x = event.motion.x" << std::endl;
				}*/

				if (x)
				{
					int winWidth;
					SDL_GetWindowSize(win_, &winWidth, NULL);
					stream_seek(double(x) / winWidth);
					x = 0;
					break;
				}
			break;
		case SDL_QUIT:
			std::cout << "SDL_QUIT" << std::endl;
			return 0;
			break;

		default:
			break;
		}
	}
}


int VideoOutput::stream_seek(double pos)
{
	_seek_req = 1;
	_seek_pos = pos;
	return 0;
}