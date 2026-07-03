#include "yuvToH264Encoder.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "YuvToH264Encoder"

#include <stdlib.h>
#include <string.h>

YuvToH264Encoder::YuvToH264Encoder()
{
	
}

YuvToH264Encoder::~YuvToH264Encoder()
{
	
}

int YuvToH264Encoder::Init(int w, int h, int _fps)
{
	width = w;
	height = h;
	fps = _fps;
	
	yuvReader.Init(width, height, fps);
	h264Encoder.Init(width, height, fps);
	
	return 0;
}

void YuvToH264Encoder::Uninit()
{
	h264Encoder.Uninit();
	yuvReader.Uninit();
}

int YuvToH264Encoder::H264Dequeue(H264Data **h264Data)
{
	int ret;
	
	ret = h264Queue.dequeue(h264Data);
	if(ret < 0)
	{
		return -1;
	}
	return 0;
}

void YuvToH264Encoder::SetH264DataSink(H264DataSink sink, void *obj)
{
	h264DataSink = sink;
	h264DataSinkObj = obj;
}

void YuvToH264Encoder::EmitH264Data(H264Data *h264Data)
{
	if(h264DataSink)
	{
		h264DataSink(h264Data, h264DataSinkObj);
		return;
	}

	h264Queue.enqueue(h264Data);
}

void YuvToH264Encoder::EmitSpsPpsBeforeKeyFrame(uint8_t *spsPps, size_t spsPpsLen, int64_t pts)
{
	H264Data *spsPpsData = new H264Data();

	spsPpsData->len = spsPpsLen;
	spsPpsData->buf = (uint8_t *)malloc(spsPpsLen);
	if(spsPpsData->buf)
	{
		memcpy(spsPpsData->buf, spsPps, spsPpsLen);
		spsPpsData->pts = pts;
		EmitH264Data(spsPpsData);
	}
	else
	{
		LOGW("malloc keyframe sps pps err");
		delete spsPpsData;
	}
}

void YuvToH264Encoder::run()
{
	int i;
	int ret;
	uint8_t *y;
	uint8_t *c;
	uint8_t *data;
	uint8_t *spsPps = nullptr;
	size_t len;
	size_t spsPpsLen = 0;
	int keyFrameFlag;
	int64_t pts;
	int staFlag = 0;
	int t0 = time_ms_get();
	int t1 = 0;
	int camera_cnt = 0;
	int h264_cnt = 0;
	
	H264Data *h264Data;
	
	ret = h264Encoder.GetSpsPps(&spsPps ,&spsPpsLen);
	if(ret < 0)
	{
		LOGW("get sps pps err");
	}
	else 
	{
		h264Data = new H264Data();
	
		h264Data->len = spsPpsLen;
		h264Data->buf = (uint8_t *)malloc(spsPpsLen);
		if(h264Data->buf)
		{
			memcpy(h264Data->buf, spsPps, spsPpsLen);
			EmitH264Data(h264Data);
		}
		else
		{
			LOGW("malloc sps pps err");
			delete h264Data;
			h264Data = nullptr;
		}
	}
	
	LOGI("loop start");
	while(stopFlag == 0)
	{
		IThreadMSleep(10);
		
		t1 = time_ms_elaps(t0);
		if(t1 > 1000)
		{
			LOGI("camera fps %d h264 %d", camera_cnt, h264_cnt);
			t0 = time_ms_get();
			camera_cnt = 0;
			h264_cnt = 0;
		}
		
		if(staFlag == 0)
		{
			ret = yuvReader.OutBuf(&y, &c);
			//ret = -1;
			if(ret < 0) {
				continue;
			}
			camera_cnt++;
			staFlag = 1;
		}
		
		if(staFlag == 1)
		{
			//LOGI("sta4");
			ret = h264Encoder.YuvIn(y, c);
			//ret = -1;
			if(ret < 0) {
				continue;
			}
			yuvReader.InBuf();
			staFlag = 2;
		}
		
		if(staFlag == 2)
		{
			ret = h264Encoder.Out(&data, &len, &keyFrameFlag, &pts);
			if(ret < 0) {
				continue;
			}
			h264_cnt++;
			
			h264Data = new H264Data();
			
			if((keyFrameFlag != 0) && (spsPps != nullptr) && (spsPpsLen > 0))
			{
				EmitSpsPpsBeforeKeyFrame(spsPps, spsPpsLen, pts);
			}
			h264Data->buf = data;
			h264Data->len = len;
			h264Data->flag = keyFrameFlag;
			h264Data->pts = pts;
			
			EmitH264Data(h264Data);
			//LOGI("sta1");
			/*if(h264Data)
			{
				free(h264Data);
				h264Data = nullptr;
			}*/
			staFlag = 0;
		}
	}
	yuvReader.Uninit();
	LOGI("loop end");
}
