#pragma once
#include "statx.h"
#include <ctime>
#include <chrono>
#include<cmath>


using namespace std::chrono;

class AVSync
{
public:
	double pts_ = 0;
	double pts_drift_ = 0;
	//暂停
	int pause_ = 0;
	//逐帧
	int step_ = 0;
	//静音
	int muted_ = 0;

public:
	AVSync() {};
	~AVSync() {};
	void init_clock();
	void set_clock_at(double pts, double time);
	void set_clock(double pts);
	double get_clock();
	time_t get_micro_seconds();
};
void AVSync::init_clock()
{
	set_clock(NAN);  //数学对比是个无效值
}
//获取当下时间,以微秒为单位
time_t AVSync::get_micro_seconds()
{
	system_clock::time_point timePointNew = system_clock::now();
	system_clock::duration duration_ = timePointNew.time_since_epoch();
	time_t us = duration_cast<microseconds>(duration_).count();
	return us;
}

double AVSync::get_clock()
{
	auto time = get_micro_seconds() / 1000000.0;
	return time + pts_drift_;

}

void AVSync::set_clock_at(double pts, double time)
{
	pts_ = pts ;
	pts_drift_ = pts - time;
}

void AVSync::set_clock(double pts)
{
	//获得当前时间,并调用set_clock_at
	auto time = get_micro_seconds()/1000000.0;
	set_clock_at(pts, time);
	return;
}

