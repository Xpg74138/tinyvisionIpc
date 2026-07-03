#include "tinyvisonIpcV1.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "TinyvisonIpcV1"


TinyvisonIpcV1::TinyvisonIpcV1()
{
	
}

TinyvisonIpcV1::~TinyvisonIpcV1()
{
	
}

int TinyvisonIpcV1::Init(int w, int h, int fps)
{
	int i;
	int ret;
	
	yuvToH264Encoder.Init(w, h, fps);
	rtspServer.Init(554, "/live", "", "");
	return 0;
}

void TinyvisonIpcV1::Uninit()
{
	yuvToH264Encoder.Uninit();
	rtspServer.Uninit();
}

void TinyvisonIpcV1::run()
{
	int i;
	int ret;
	H264Data *h264Data;
	
	yuvToH264Encoder.Start();
	rtspServer.Start();
	
	LOGI("loop start");
	while(stopFlag == 0)
	{
		IThreadMSleep(10);
		
		ret = yuvToH264Encoder.H264Dequeue(&h264Data);
		if(ret < 0)
		{
			continue;
		}
		
		rtspServer.PktQueueEnqueue(h264Data);
		/*if(h264Data->buf)
		{
			free(h264Data->buf);
			h264Data->buf = nullptr;
		}
		delete h264Data;
		h264Data = nullptr;*/
	}
	yuvToH264Encoder.Stop();
	rtspServer.Stop();
	LOGI("loop end");
}