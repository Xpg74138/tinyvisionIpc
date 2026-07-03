#include "yuvToH264Encoder.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "YuvToH264Encoder"


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

void YuvToH264Encoder::run()
{
	int i;
	int ret;
	uint8_t *y;
	uint8_t *c;
	uint8_t *data;
	uint8_t *spsPps;
	size_t len;
	size_t spsPpsLen;
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
		memcpy(h264Data->buf, spsPps, spsPpsLen);
		h264Queue.enqueue(h264Data);
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
			
			//if(keyFrameFlag == 0)
			{
				h264Data->buf = data;
				h264Data->len = len;
			}
			/*else 
			{// keyframe spspps
				h264Data->buf = (uint8_t *)malloc(spsPpsLen + len);
				h264Data->len = spsPpsLen + len;
				memcpy(h264Data->buf, spsPps, spsPpsLen);
				memcpy(h264Data->buf + spsPpsLen, data, len);
				free(data);
				data = nullptr;
			}*/
			h264Data->flag = keyFrameFlag;
			h264Data->pts = pts;
			
			h264Queue.enqueue(h264Data);
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
