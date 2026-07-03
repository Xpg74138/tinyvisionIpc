#ifndef _YUV_TO_H264_ENCODER_H_
#define _YUV_TO_H264_ENCODER_H_

#include <unistd.h>
#include "IThread.h"
#include "ThreadSafeQueue.h"
#include "h264Data.h"
#include "yuvReader.h"
#include "h264Encoder.h"
#include "time_elaps.h"

typedef void (*H264DataSink)(H264Data *h264Data, void *obj);

class YuvToH264Encoder : public IThread
{
public:
	YuvToH264Encoder();
	~YuvToH264Encoder();
	
	int Init(int w, int h, int _fps);
	void Uninit();
	
	int H264Dequeue(H264Data **h264Data);
	void SetH264DataSink(H264DataSink sink, void *obj);
	virtual void run();
	
private:
	void EmitH264Data(H264Data *h264Data);
	void EmitSpsPpsBeforeKeyFrame(uint8_t *spsPps, size_t spsPpsLen, int64_t pts);

	int width;
	int height;
	int fps;
	YuvReader yuvReader;
	H264Encoder h264Encoder;
	ThreadSafeQueue<H264Data> h264Queue;
	H264DataSink h264DataSink = nullptr;
	void *h264DataSinkObj = nullptr;
};


#endif
