#ifndef _TINY_VISION_IPC_V1
#define _TINY_VISION_IPC_V1

#include "IThread.h"
#include "ThreadSafeQueue.h"
#include "yuvToH264Encoder.h"
#include "rtspServer.h"

class TinyvisonIpcV1 : public IThread
{
public:
	TinyvisonIpcV1();
	~TinyvisonIpcV1();
	
	int Init(int w, int h, int fps);
	void Uninit();
	
	virtual void run();
	
private:
	YuvToH264Encoder yuvToH264Encoder;
	RtspServer rtspServer;
};

#endif