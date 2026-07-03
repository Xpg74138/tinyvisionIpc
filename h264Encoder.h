#ifndef _H264_ENCODER_H_
#define _H264_ENCODER_H_

#include <stdio.h>
//#include <cdx_log.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>

extern "C" {
#include <memoryAdapter.h>
#include "vencoder.h"
#include "EncAdapter.h"
}

#include "ILOG.h"
#include "time_elaps.h"

#define ENCODER_MAX_NUM (5)
#define DEMO_FILE_NAME_LEN 256
#define USE_H265_ENC
#define ROI_NUM 4
#define NO_READ_WRITE 0
#define SAVE_AWSP     0
#define SETUP_VIDEO_TIME_INFO (1) //* time info about fps

#define SET_ROI_PARAM			0
#define ENABLE_GET_WRITE_BACK_YUV (0)
#define SHOW_PTS_INFO (0)
#define ENABLE_2D_FLITER (0)
#define ENABLE_3D_FLITER (0)
#define ENABLE_DROP_FRAME_NUM (0)

#define ALIGN_XXB(y, x) (((x) + ((y)-1)) & ~((y)-1))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define DEMO_INPUT_BUFFER_NUM (2)

typedef struct ptsDebugInfo
{
    long long curPts;
    long long prePts;
    long long maxPts;
    long long minPts;
}ptsDebugInfo;

typedef struct InputBufferInfo InputBufferInfo;

struct InputBufferInfo
{
    VencInputBuffer     inputbuffer;
    InputBufferInfo*    next;
};

typedef struct InputBufferMgr
{
    InputBufferInfo   buffer_node[DEMO_INPUT_BUFFER_NUM];
    InputBufferInfo*  valid_quene;
    InputBufferInfo*  empty_quene;
}InputBufferMgr;

typedef struct {
    int     argc;
    char**  argv;
    int nChannel;
    int nTotalChannelNum;
    int bOnlineMode;
    int bOnlineChannel;
    InputBufferMgr mInputBufMgr;
    VencBaseConfig baseConfig;
    VideoEncoder* pVideoEnc;
    VencMBModeCtrl  mMBModeCtrl;
    VencMBInfo      mMBInfo;
}encoder_Context;


typedef struct {
    VencHeaderData          sps_pps_data;
    VencH264Param           h264Param;
    VencMBModeCtrl          h264MBMode;
    VencMBInfo              MBInfo;
    VencFixQP           fixQP;
    VencSuperFrameConfig    sSuperFrameCfg;
    VencH264SVCSkip         SVCSkip; // set SVC and skip_frame
    VencH264AspectRatio     sAspectRatio;
    VencH264VideoSignal     sVideoSignal;
    VencCyclicIntraRefresh  sIntraRefresh;
    VencROIConfig           sRoiConfig[ROI_NUM];
    VeProcSet               sVeProcInfo;
    VencOverlayInfoS        sOverlayInfo;
    VencSmartFun            sH264Smart;
}h264_func_t;

typedef struct {
    char             intput_file[256];
    char             output_file[256];
    char             reference_file[256];
    char             overlay_file[256];
    char             log_file[256];
    int              compare_flag;
    int              log_flag;
    int              compare_result;

    unsigned int  encode_frame_num;
    unsigned int  encode_format;

    unsigned int src_size;
    unsigned int dst_size;

    unsigned int src_width;
    unsigned int src_height;
    unsigned int dst_width;
    unsigned int dst_height;

    unsigned int vbv_size;

    unsigned int is_ipc;
    unsigned int is_night;
    unsigned int sensor_type;

    int frequency;
    int bit_rate;
    int frame_rate;
    int maxKeyFrame;
    unsigned int test_cycle;
    unsigned int test_overlay_flag;
    unsigned int test_lbc;
    unsigned int test_afbc;
    unsigned int limit_encode_speed; //* limit encoder speed by framerate
    unsigned int i_qp_min;
    unsigned int i_qp_max;
    unsigned int p_qp_min;
    unsigned int p_qp_max;
    unsigned int qp_init;

    h264_func_t h264_func;
    unsigned int nChannel;
    VencCopyROIConfig pRoiConfig;
    unsigned int pixel_format;
    unsigned int extend_flag;
    unsigned int nShare_buf_num;

	//VencGdcParam mGdcParam;
	unsigned int bEnableGdc;
    unsigned int bEnableSharp;
	unsigned int rotate;

    unsigned int rec_lbc_mode; //*0: disable, 1:1.5x , 2: 2.0x, 3: 2.5x, 4: no_lossy
}encode_param_t;

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int width_aligh16;
    unsigned int height_aligh16;
    unsigned char* argb_addr;
    unsigned int size;
}BitMapInfoS;

class H264Encoder
{
public:
	H264Encoder();
	~H264Encoder();
	
	int Init(int width, int height, int fps);
	void Uninit();
	
	int YuvIn(uint8_t *y, uint8_t *c);
	int Out();
	int Out(uint8_t **data, size_t *len, int *flag, int64_t *pts);
	int GetSpsPps(uint8_t **data, size_t *len);
	
private:
	encoder_Context* pEncContext = NULL;
	InputBufferInfo *pInputBufInfo = NULL;
	VencInputBuffer *pInputBuf = NULL;
	VideoEncoder* pVideoEnc = NULL;
	
	VencBaseConfig baseConfig;
	encode_param_t encode_param;
	VencHeaderData sps_pps_data;
	VencOutputBuffer outputBuffer;
	int ptsStartFlag = 0;
	int64_t ptsStart = 0;
	
	long long pts = 0;
};

#endif