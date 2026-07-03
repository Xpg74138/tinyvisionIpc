#ifndef _H264_DATA_H_
#define _H264_DATA_H_

#include <stddef.h>
#include <stdint.h>

class H264Data
{
public:
	H264Data(){};
	~H264Data(){};
	uint8_t *buf = nullptr;
	size_t len = 0;
	int flag = 0;//keyframe
	int64_t pts = 0;
};

#endif
