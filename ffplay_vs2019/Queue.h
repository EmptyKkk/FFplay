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

	//�˳�����
	void abort()
	{
		abort_ = 1;
		cond_.notify_all();
	}

	//ѹ��Ԫ��
	int push(T val)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (abort_ == 1) return -1;
		queue_.push(val);
		cond_.notify_one();
		return 0;
	}
	

	//��ȡ��ͷԪ�ز��Ƴ�
	int pop(T& val, const int timeout = 0)
	{
		std::unique_lock<std::mutex> lock(mutex_);
		if (queue_.empty())
		{
			//�ȴ�push��ʱ����, ��Queue��Ϊ�ջ�abor_Ϊ1��ʱ���������ִ��
			cond_.wait_for(lock, std::chrono::milliseconds(timeout), [this] {return !queue_.empty() || abort_; });
		}
		//�ж϶�������
		if (abort_ == 1) return -1;
		if (queue_.empty()) return -2;
		//ȡ��ֵ���Ƴ�һ������
		val = queue_.front();
		queue_.pop();

		return 0;
	}

	//��ȡ��ͷԪ��
	int front(T& val)
	{
		std::lock_guard<std::mutex> lock(mutex_);
	
		//�ж϶�������
		if (abort_ == 1) return -1;
		if (queue_.empty()) return -2;
		//ȡ��ֵ���Ƴ�һ������
		val = queue_.front();
		return 0;
	}
	

	//���ض�����Ԫ�ظ���
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


