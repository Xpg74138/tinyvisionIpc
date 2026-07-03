#pragma once
#include <thread>

#define IThreadMSleep(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))

class IThread
{
public:
	//纯虚方法 不使用本类的方法 使用派生类重写的run方法
	virtual void run() = 0;
	int Start();
	int Stop();

public:
	std::thread * t = nullptr;

    volatile int stopFlag = 0;
};
