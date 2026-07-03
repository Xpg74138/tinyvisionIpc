#ifndef _YUV_READER_H_
#define _YUV_READER_H_

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <semaphore.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#define YUV_BUF_NUM_MAX	2
#define __USE_VIN_ISP__

#define ALIGN_16B(x) (((x) + (15)) & ~(15))

extern "C" {
#ifdef __USE_VIN_ISP__
#include "AWIspApi.h"
#include "sunxi_camera_v2.h"
#endif

#include "ion_alloc.h"
}
#include "ILog.h"


struct buffer {
	void *start[YUV_BUF_NUM_MAX];
	unsigned int length[YUV_BUF_NUM_MAX];
	int fd[YUV_BUF_NUM_MAX];
};

struct v4l2_frmsize {
	unsigned int width;
	unsigned int height;
};

typedef struct camera_hal {
	int camera_index;
	int videofd;
	unsigned char isDefault;
	int driver_type;
	int sensor_type;
	int ispId;
	int ispGain;
        int ispExp;
        int sensorGain;
        int sensorExp;
#ifdef __USE_VIN_ISP__
	AWIspApi *ispPort;
#endif
	int photo_type;
	int photo_num;
	char save_path[64];
	struct buffer *buffers;
	int buf_count;
	int nplanes;
	unsigned int win_width;
	unsigned int win_height;
	unsigned int fps;
	unsigned int pixelformat;
	enum v4l2_memory memory;
} camera_handle;

class YuvReader 
{
public:
	YuvReader();
	~YuvReader();
	
	int Init(int w, int h, int fps);
	void Uninit();
	
	int OutBuf(uint8_t **y, uint8_t **c);
	int InBuf();
	int GetAddr(uint8_t **y, uint8_t **c);
	
private:
	camera_handle camera;
	struct v4l2_requestbuffers req; 
	struct v4l2_buffer buf;
	struct v4l2_buffer read_buf;
	
	int width16;
	int height16;
	
};

#endif
