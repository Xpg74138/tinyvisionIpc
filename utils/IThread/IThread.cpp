#include "IThread.h"
#include <thread>

int IThread::Start()
{
	if (t == nullptr)
	{
		stopFlag = 0;
		t = new std::thread(&IThread::run, this);
	}
	//实际上会传递派生类的run方法进来
	//std::thread t(&MMThread::run, this);
	//t.detach();

	return 0;
	
}

int IThread::Stop()
{
	if (t != nullptr)
	{
		stopFlag = 1;
		t->join();
		delete t;
		t = nullptr;
	}
		

	return 0;

}
