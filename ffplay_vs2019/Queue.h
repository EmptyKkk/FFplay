#pragma once
#include "statx.h"
#include <mutex>
#include <condition_variable>
#include <queue>
#ifndef  QUEUE_H
#define QUEUE_H

template<typename T>
class Queue
{
public:
	Queue() = default;
	~Queue() = default ;

	//退出函数
	void abort()
	{
		abort_ = 1;
		cond_.notify_all();
	}

	//压入元素
	int push(T val)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (abort_ == 1) return -1;
		queue_.push(val);
		cond_.notify_one();
		return 0;
	}
	

	//获取队头元素并推出
	int pop(T& val, const int timeout = 0)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		if (queue_.empty())
		{
			//等待push或超时唤醒, 当Queue不为空或abor_为1的时候继续向下执行
			cond_.wait_for(lock, std::chrono::milliseconds(timeout), [this] {return !queue_.empty() || abort_; });
		}
		//判断额外条件
		if (abort_ == 1) return -1;
		if (queue_.empty()) return -2;
		//取出值并推出一个数据
		val = queue_.front();
		queue_.pop();

		return 0;
	}

	//获取队头元素
	int front(T& val)
	{
		std::lock_guard<std::mutex> lock(mutex_);
	
		//判断额外条件
		if (abort_ == 1) return -1;
		if (queue_.empty()) return -2;
		//取出值并推出一个数据
		val = queue_.front();
		return 0;
	}
	

	//返回队列中元素个数
	int size()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		int size =  queue_.size();
		return size;
	}


private: 
	int abort_ = 0;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::queue<T> queue_;
};
#endif


