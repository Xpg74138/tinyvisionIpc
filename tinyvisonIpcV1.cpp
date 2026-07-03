#include "tinyvisonIpcV1.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "TinyvisonIpcV1"

static void RtspH264DataSink(H264Data *h264Data, void *obj)
{
	if(obj == nullptr)
	{
		return;
	}

	RtspServer *rtspServer = (RtspServer *)obj;
	rtspServer->PktQueueEnqueue(h264Data);
}

TinyvisonIpcV1::TinyvisonIpcV1()
{
	
}

TinyvisonIpcV1::~TinyvisonIpcV1()
{
	
}

int TinyvisonIpcV1::Init(int w, int h, int fps, int rtspPort, const char *rtspPath)
{
	int ret;
	
	ret = yuvToH264Encoder.Init(w, h, fps);
	if (ret < 0) {
		LOGE("yuvToH264Encoder init failed: %d", ret);
		return ret;
	}

	ret = rtspServer.Init(rtspPort, rtspPath, "", "");
	if (ret < 0) {
		LOGE("rtspServer init failed: %d", ret);
		yuvToH264Encoder.Uninit();
		return ret;
	}

	yuvToH264Encoder.SetH264DataSink(RtspH264DataSink, &rtspServer);
	return 0;
}

void TinyvisonIpcV1::Uninit()
{
	yuvToH264Encoder.Uninit();
	rtspServer.Uninit();
}

void TinyvisonIpcV1::run()
{
	rtspServer.Start();
	yuvToH264Encoder.Start();
	
	LOGI("loop start");
	while(stopFlag == 0)
	{
		IThreadMSleep(10);
	}
	yuvToH264Encoder.Stop();
	rtspServer.Stop();
	LOGI("loop end");
}
