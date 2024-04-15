#pragma once
#include "statx.h"
#ifndef FFTHREAD_H
#define FFTHREAD_H
class ffThread
{
public:
	ffThread() {};
	~ffThread() { if (thread_) stop(); };
	int start() {};
	int stop()
	{
		abort_ = 1; 
		if (thread_)
		{
			thread_->join();
			delete thread_;
			thread_ = nullptr;
		}
		
		return 0;
	}
	virtual void run() {};
	
protected:
	int abort_=0;
	std::thread* thread_ = nullptr;
};
#endif
