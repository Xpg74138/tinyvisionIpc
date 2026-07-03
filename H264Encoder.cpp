#include "h264Encoder.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "H264Encoder"

static const int LOW_LATENCY_GOP_DIVISOR = 2;
static const int LOW_LATENCY_VBV_MS = 500;
static const unsigned int LOW_LATENCY_VBV_MIN = 256 * 1024;
static const unsigned int LOW_LATENCY_VBV_MAX = 2 * 1024 * 1024;

BitMapInfoS bit_map_info[13];

static int setEncParam(encoder_Context* pEncContext, encode_param_t *encode_param);
static int initH264Func(encode_param_t *encode_param);
static int initGdcFunc(sGdcParam *pGdcParam);
static int initSharpFunc(    sEncppSharpParamDynamic *pSharpParamDynamic, sEncppSharpParamStatic  *pSharpParamStatic);
static void init_fix_qp(VencFixQP *fixQP);
static void init_super_frame_cfg(VencSuperFrameConfig *sSuperFrameCfg);
static void init_svc_skip(VencH264SVCSkip *SVCSkip);
static void init_aspect_ratio(VencH264AspectRatio *sAspectRatio);
static void init_video_signal(VencH264VideoSignal *sVideoSignal);
static void init_intra_refresh(VencCyclicIntraRefresh *sIntraRefresh);
static void init_roi(VencROIConfig *sRoiConfig);
static void init_enc_proc_info(VeProcSet *ve_proc_set);
static void init_overlay_info(VencOverlayInfoS *pOverlayInfo, encode_param_t *encode_param);
static int init_Roi_config(VencBaseConfig *baseConfig, VencCopyROIConfig*pRoiConfig);
static int EventHandler(VideoEncoder* pEncoder, void* pAppData, VencEventType eEvent,
                             unsigned int nData1, unsigned int nData2, void* pEventData);
static void enqueue(InputBufferInfo** pp_head, InputBufferInfo* p);
static InputBufferInfo* dequeue(InputBufferInfo** pp_head);
static int InputBufferDone(VideoEncoder* pEncoder,    void* pAppData,
                          VencCbInputBufferDoneInfo* pBufferDoneInfo);
void init_mb_mode(VencMBModeCtrl *pMBMode, unsigned int dst_width, unsigned int dst_height);
void init_mb_info(VencMBInfo *MBInfo, encode_param_t *encode_param);
void setMbMode(encoder_Context* pEncContext, encode_param_t *encode_param);
void getMbMinfo(encoder_Context* pEncContext);
void releaseMb(encode_param_t *encode_param);

static int lowLatencyGopFrames(int fps)
{
	int gop = fps / LOW_LATENCY_GOP_DIVISOR;
	if(gop < 1)
	{
		gop = 1;
	}
	return gop;
}

static unsigned int lowLatencyVbvSize(int bitRate)
{
	unsigned int vbvSize = (unsigned int)(((int64_t)bitRate * LOW_LATENCY_VBV_MS) / 8000);

	if(vbvSize < LOW_LATENCY_VBV_MIN)
	{
		vbvSize = LOW_LATENCY_VBV_MIN;
	}
	else if(vbvSize > LOW_LATENCY_VBV_MAX)
	{
		vbvSize = LOW_LATENCY_VBV_MAX;
	}

	return vbvSize;
}

H264Encoder::H264Encoder()
{
	
}

H264Encoder::~H264Encoder()
{
	
}

int H264Encoder::Init(int width, int height, int fps)
{
	int nRet = 0;
    int i = 0;
    int nEncoderNum = 1;
    int bOnline_mode = 0;
	
	LOGI("start");
	pEncContext = (encoder_Context *)calloc(sizeof(encoder_Context), 1);
    if(pEncContext == NULL)
    {
        LOGI("malloc for pEncContext failed");
		return -1;
    }
    //pEncContext[i]->argc = argc;
    //pEncContext[i]->argv = argv;
    pEncContext->nChannel = i;
    pEncContext->nTotalChannelNum = nEncoderNum;
	
	//encoder_Context* pEncContext = (encoder_Context*)param;

    
    VencAllocateBufferParam bufferParam;
    
    //VencInputBuffer inputBuffer;
    
    
    unsigned char *uv_tmp_buffer = NULL;
    unsigned int afbc_header_size;
    ptsDebugInfo mPtsInfo;
    memset(&mPtsInfo, 0, sizeof(ptsDebugInfo));
	
	int result = 0;
    long long pts = 0;
	
	char *reference_buffer = NULL;
    //char *log_buffer = logcat_buf;
    int log_len = 0;
    int value;

    unsigned int m = 0;
    unsigned int cycle_num = 1;
    long long timeMin = 0;
    long long timeMax = 0;
    long long time1=0;
    long long time2=0;
    long long time3=0;
    long long time_start=0;
    long long time_end=0;
    long long time_begin=0;

    int bHadStartTimeFlag = 0;
    long long nFirstEncodePicTime = 0;
    long long curSysTime  = 0;

    int64_t nCurPts = 0;
    int64_t nDuration = 0;

    VencCopyROIConfig pRoiConfig;
    memset(&pRoiConfig, 0, sizeof(VencCopyROIConfig));
	
	VencMBSumInfo sMbSumInfo;
    memset(&sMbSumInfo, 0, sizeof(VencMBSumInfo));
	
	/******** begin set the default encode param ********/
    
    memset(&encode_param, 0, sizeof(encode_param));
	//encode_param.log_flag = 1;
	//memset(encode_param.log_file, 0, sizeof(encode_param.log_file));
	//sprintf(encode_param.log_file, "%s", "/tmp/log0.txt");
	
	encode_param.dst_width = width;
    encode_param.dst_height = height;
    encode_param.src_width = encode_param.dst_width;
    encode_param.src_height = encode_param.dst_height;
    
    encode_param.bit_rate = 8*1024*1024;
	
    encode_param.frame_rate = fps;
    encode_param.maxKeyFrame = lowLatencyGopFrames(fps);
	encode_param.pixel_format = VENC_PIXEL_YVU420SP;//VENC_PIXEL_YUV420SP NV12 VENC_PIXEL_YVU420SP NV21
    encode_param.encode_format = VENC_CODEC_H264;
    //encode_param.encode_frame_num = 100;
    //encode_param.test_cycle = 1;
    encode_param.nChannel = pEncContext->nChannel;
	
	int mult = 1;
	if(encode_param.dst_width == 3840)
        encode_param.bit_rate = 20*1024*1024 * mult;
    else if(encode_param.dst_width == 1920)
        encode_param.bit_rate = 10*1024*1024 * mult;
    else if(encode_param.dst_width ==1280)
        encode_param.bit_rate = 6*1024*1024 * mult;//6*1024*1024;
    else if(encode_param.dst_width == 640)
        encode_param.bit_rate = 2*1024*1024 * mult;
    else if(encode_param.dst_width == 288)
        encode_param.bit_rate = 1*1024*1024 * mult;
    else
        encode_param.bit_rate = 4*1024*1024 * mult;
    encode_param.vbv_size = lowLatencyVbvSize(encode_param.bit_rate);
    LOGI("low latency encode fps %d gop %d bitrate %d vbv %u",
         encode_param.frame_rate, encode_param.maxKeyFrame,
         encode_param.bit_rate, encode_param.vbv_size);
		
	/******** begin set baseConfig param********/
    memset(&baseConfig, 0 ,sizeof(VencBaseConfig));
    memset(&bufferParam, 0 ,sizeof(VencAllocateBufferParam));
    baseConfig.memops = MemAdapterGetOpsS();
    if (baseConfig.memops == NULL)
    {
        LOGW("MemAdapterGetOpsS failed\n");
        //goto out;
		return -2;
    }
    CdcMemOpen(baseConfig.memops);
    baseConfig.nInputWidth= encode_param.src_width;
    baseConfig.nInputHeight = encode_param.src_height;
    baseConfig.nStride = encode_param.src_width;
    baseConfig.nDstWidth = encode_param.dst_width;
    baseConfig.nDstHeight = encode_param.dst_height;
    baseConfig.bEncH264Nalu = 0;
    /*
        * the format of yuv file is yuv420p, but the old ic only support the yuv420sp,
        * so use the func yu12_nv12() to config all the format.
        */
    //baseConfig.eInputFormat = VENC_PIXEL_YUV420SP;
    baseConfig.eInputFormat = (VENC_PIXEL_FMT)encode_param.pixel_format;
    baseConfig.extend_flag  = encode_param.extend_flag;
    baseConfig.nOnlineShareBufNum  = encode_param.nShare_buf_num;
	//baseConfig.mGdcParam    = encode_param.mGdcParam;
	baseConfig.rec_lbc_mode = (eVeLbcMode)encode_param.rec_lbc_mode;

    LOGI("****eInputFormat = %d, extend_flag = %d, nOnlineShareBufNum = %d ",
    		baseConfig.eInputFormat, baseConfig.extend_flag,
    		baseConfig.nOnlineShareBufNum );

    LOGI("size of sharp param: %d, %d", sizeof(sEncppSharpParamDynamic), sizeof(sEncppSharpParamStatic));

    if(baseConfig.eInputFormat == VENC_PIXEL_YUV420P)
    {
        #ifdef YU12_NV12
                baseConfig.eInputFormat = VENC_PIXEL_YUV420SP;
                yu12_nv12_flag = 1;
        #endif

        #ifdef YU12_NV21
            baseConfig.eInputFormat = VENC_PIXEL_YVU420SP;
            yu12_nv21_flag = 1;
        #endif
    }
    //* ve require 16-align
    int nAlignW = (baseConfig.nInputWidth + 15)& ~15;
    int nAlignH = (baseConfig.nInputHeight + 15)& ~15;

    bufferParam.nSizeY = nAlignW*nAlignH;
    bufferParam.nSizeC = nAlignW*nAlignH/2;

    if(encode_param.test_afbc == 1)
    {
		afbc_header_size = ((baseConfig.nInputWidth +127)>>7)*((baseConfig.nInputHeight+31)>>5)*96;
		LOGI("size_y:%x, size_c:%x, afbc_header:%x\n",
									bufferParam.nSizeY,
									bufferParam.nSizeC,
									afbc_header_size);
		bufferParam.nSizeY += afbc_header_size + bufferParam.nSizeC;
		bufferParam.nSizeC = 0;
		LOGI("afbc buffer size:%x\n", bufferParam.nSizeY);
	
		baseConfig.eInputFormat = VENC_PIXEL_AFBC_AW;
    }
	
	//LBC 对视频帧数据进行压缩前处理的逻辑
	#if 1
	//just test
    if(pEncContext->bOnlineMode == 0) {
		encode_param.test_lbc = 0;
	}
	#endif

    unsigned int lbc_ext_size = 0;
    if(encode_param.test_lbc != 0)
    {
		int y_stride = 0;
		int yc_stride = 0;
		int bit_depth = 8;
		int com_ratio_even = 0;
		int com_ratio_odd = 0;
		int pic_width_32align = (baseConfig.nInputWidth + 31) & ~31;
		int pic_height = baseConfig.nInputHeight;
	
		if(encode_param.test_lbc == 1)
			baseConfig.bLbcLossyComEnFlag1_5x = 1;
		else if(encode_param.test_lbc == 2)
			baseConfig.bLbcLossyComEnFlag2x = 1;
		else if(encode_param.test_lbc == 3)
			baseConfig.bLbcLossyComEnFlag2_5x = 1;
	
		if(baseConfig.bLbcLossyComEnFlag1_5x == 1)
		{
			com_ratio_even = 670;
			com_ratio_odd = 658;
			y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
			yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
		}
		else if(baseConfig.bLbcLossyComEnFlag2x == 1)
		{
			com_ratio_even = 600;
			com_ratio_odd = 450;
			y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
			yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
		}
		else if(baseConfig.bLbcLossyComEnFlag2_5x == 1)
		{
			com_ratio_even = 440;
			com_ratio_odd = 380;
			y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
			yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
		}
		else
		{
			y_stride = ((pic_width_32align * bit_depth + pic_width_32align / 16 * 2 + 511) & (~511)) >> 3;
			yc_stride = ((pic_width_32align * bit_depth * 2 + pic_width_32align / 16 * 4 + 511) & (~511)) >> 3;
		}
	
	
		int total_stream_len = (y_stride + yc_stride) * pic_height / 2;
	
		//* add more 1KB to fix ve-lbc-error
		lbc_ext_size = 1*1024;
		total_stream_len += lbc_ext_size;
	
		LOGI("LBC in buf:com_ratio: %d, %d, w32alin = %d, pic_height = %d, \
			y_s = %d, yc_s = %d, total_len = %d,\n",
			com_ratio_even, com_ratio_odd,
			pic_width_32align, pic_height,
			y_stride, yc_stride, total_stream_len);
	
		bufferParam.nSizeY = total_stream_len;
		bufferParam.nSizeC = 0;
		baseConfig.eInputFormat = VENC_PIXEL_LBC_AW;
	
    }
	//LBC end
	
	/******** end set baseConfig param********/

    //create encoder
    LOGI("encode_param.encode_format:%d\n", encode_param.encode_format);
    pVideoEnc = VencCreate((VENC_CODEC_TYPE)encode_param.encode_format);

    pEncContext->pVideoEnc = pVideoEnc;
    //set enc parameter
    result = setEncParam(pEncContext ,&encode_param);
    if(result)
    {
        LOGI("setEncParam error, return");
        return -3;
    }
    baseConfig.bOnlineMode    = pEncContext->bOnlineMode;
    baseConfig.bOnlineChannel = pEncContext->bOnlineChannel;
    VencInit(pVideoEnc, &baseConfig);
	
    LOGI("bEnableGdc = %d, bEnableSharp = %d",
        encode_param.bEnableGdc, encode_param.bEnableSharp);

    memcpy(&pEncContext->baseConfig, &baseConfig, sizeof(VencBaseConfig));
#if ENABLE_GET_WRITE_BACK_YUV
    unsigned int bEnableWbYuv = 1;
    unsigned int ThumbScaler = 0;
    VencSetParameter(pVideoEnc, VENC_IndexParamSetThumbScaler, &ThumbScaler);
    VencSetParameter(pVideoEnc, VENC_IndexParamEnableWbYuv, &bEnableWbYuv);
#endif
#if SET_ROI_PARAM
	for(i = 0; i < ROI_NUM; i++)
	
		if(encode_param.encode_format == VENC_CODEC_H264)
		{
			VencSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &encode_param.h264_func.sRoiConfig[i]);
		}
		else if(encode_param.encode_format == VENC_CODEC_H265)
		{
			VencSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &encode_param.h265_func.sRoiConfig[i]);
		}
	
#endif
    if((pRoiConfig.bEnable==1) && \
        (encode_param.encode_format == VENC_CODEC_H264 || \
        encode_param.encode_format == VENC_CODEC_JPEG || \
        encode_param.encode_format == VENC_CODEC_H265))
    {
        init_Roi_config(&baseConfig, &pRoiConfig);
        VencSetParameter(pVideoEnc, VENC_IndexParamRoi, &pRoiConfig);
    }

    if(encode_param.frequency > 0)
        VencSetFreq(pVideoEnc, encode_param.frequency);
	
    unsigned int head_num = 0;

    VencGetParameter(pVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
    //unsigned char value = 1;
    //VencGetParameter(pVideoEnc, VENC_IndexParamAllParams, &value);
    
    //fwrite(sps_pps_data.pBuffer, 1, sps_pps_data.nLength, out_file);
    LOGI("sps_pps_data.nLength: %d", sps_pps_data.nLength);
	
    //for(head_num=0; head_num<sps_pps_data.nLength; head_num++)
        //LOGI("the sps_pps :%02x\n", *(sps_pps_data.pBuffer+head_num));

	if(baseConfig.bOnlineChannel == 0)
    {
        for(i = 0; i < DEMO_INPUT_BUFFER_NUM; i++)
        {
            VencAllocateInputBuf(pVideoEnc, &bufferParam, &pEncContext->mInputBufMgr.buffer_node[i].inputbuffer);
            enqueue(&pEncContext->mInputBufMgr.valid_quene, &pEncContext->mInputBufMgr.buffer_node[i]);
        }
    }
	
	VencCbType vencCallBack;
    vencCallBack.EventHandler = EventHandler;
    if(baseConfig.bOnlineChannel == 0)
        vencCallBack.InputBufferDone = InputBufferDone;

    VencSetCallbacks(pVideoEnc, &vencCallBack, pEncContext);

    VencStart(pVideoEnc);
	LOGI("succ");

	return 0;
}

void H264Encoder::Uninit()
{
	if(pVideoEnc)
    {
        VencDestroy(pVideoEnc);
    }
    pVideoEnc = NULL;
	
	if(baseConfig.memops)
    {
        CdcMemClose(baseConfig.memops);
    }

    releaseMb(&encode_param);

    if(pEncContext)
    {
        free(pEncContext);
        pEncContext = NULL;
    }
    pInputBufInfo = NULL;
    pInputBuf = NULL;
}

int H264Encoder::YuvIn(uint8_t *y, uint8_t *c)
{
	int ret;
	pInputBufInfo = dequeue(&pEncContext->mInputBufMgr.valid_quene);
    LOGV("get input buf, pInputBufInfo = %p", pInputBufInfo);
    if(pInputBufInfo == NULL)
    {
		return -1;
	}
	pInputBuf = &pInputBufInfo->inputbuffer;
	memcpy(pInputBuf->pAddrVirY, y, ALIGN_XXB(16, encode_param.src_width) * ALIGN_XXB(16, encode_param.src_height));
	memcpy(pInputBuf->pAddrVirC, c, ALIGN_XXB(16, encode_param.src_width) * ALIGN_XXB(16, encode_param.src_height / 2));
	//pInputBuf->pAddrVirY = y;
	//pInputBuf->pAddrVirC = c;
	pInputBuf->bEnableCorp = 0;
    pInputBuf->sCropInfo.nLeft =  240;
    pInputBuf->sCropInfo.nTop  =  240;
    pInputBuf->sCropInfo.nWidth  =  240;
    pInputBuf->sCropInfo.nHeight =  240;


    pts += US_PER_S/encode_param.frame_rate;
    /*if(ptsStartFlag == 0)
    {
        ptsStartFlag = 1;
        ptsStart = time_us_get();
    }
    pts = time_us_get() - ptsStart;*/
	pInputBuf->nPts = pts;
    pInputBuf->bNeedFlushCache = 1;
    VencQueueInputBuf(pVideoEnc, pInputBuf);
	//LOGI("y %x %x %x %x", y[0], y[1], y[2], y[3]);
	//LOGI("c %x %x %x %x", c[0], c[1], c[2], c[3]);
    enqueue(&pEncContext->mInputBufMgr.empty_quene, pInputBufInfo);
	
	/*ret = VideoEncodeOneFrame(pVideoEnc);
    if(ret < 0)
    {
        LOGI("encoder error, goto out");
    }*/
	
	return 0;
}

int H264Encoder::Out()
{
	int ret;
	
	ret = VencDequeueOutputBuf(pVideoEnc, &outputBuffer);
	if (ret != 0)
	{
		return -1;
	}
	
	if(outputBuffer.pData0) 
	{
		uint8_t *d0 = outputBuffer.pData0;
		//outputBuffer.nSize0
		if(outputBuffer.nSize0 > 4)
		LOGI("data0 %x %x %x %x", d0[0], d0[1], d0[2], d0[3]);
	}
	
	if(outputBuffer.pData1) 
	{
		uint8_t *d1 = outputBuffer.pData1;
		//outputBuffer.nSize1
		if(outputBuffer.nSize1 > 4)
		LOGI("data1 %x %x %x %x", d1[0], d1[1], d1[2], d1[3]);
	}
	
	
	VencQueueOutputBuf(pVideoEnc, &outputBuffer);
	
	return 0;
}

int H264Encoder::Out(uint8_t **data, size_t *len, int *flag, int64_t *pts)
{
	int ret;
	uint8_t *d0 = nullptr;
	uint8_t *d1 = nullptr;
	size_t len0 = 0;
	size_t len1 = 0;
	
	ret = VencDequeueOutputBuf(pVideoEnc, &outputBuffer);
	if (ret != 0)
	{
		return -1;
	}
	
	if(outputBuffer.pData0) 
	{
		d0 = outputBuffer.pData0;
		len0 = outputBuffer.nSize0;
	}
	
	if(outputBuffer.pData1) 
	{
		d1 = outputBuffer.pData1;
		len1 = outputBuffer.nSize1;
	}
	
	*len = len0 + len1;
	*data = (uint8_t *)malloc(*len);
	if(d0 && d1)
	{
		memcpy(*data, d0, len0);
		memcpy(*data + len0, d1, len1);
		
	} 
	else if(d0)
	{
		memcpy(*data, d0, len0);
	}
	else if(d1)
	{
		memcpy(*data, d1, len1);
	}
	//LOGI("h264 out len %d", *len);
	*flag = outputBuffer.nFlag;
	*pts = outputBuffer.nPts;
	int gopi = outputBuffer.frame_info.nGopIndex;
	int fi = outputBuffer.frame_info.nFrameIndex;
	//LOGI("gopi %d fi %d flag %d pts %lld", gopi, fi, flag, pts);
	
	VencQueueOutputBuf(pVideoEnc, &outputBuffer);
	
	return 0;
}

int H264Encoder::GetSpsPps(uint8_t **data, size_t *len)
{
	if(data == nullptr || len == nullptr || sps_pps_data.pBuffer == nullptr || sps_pps_data.nLength == 0)
	{
		return -1;
	}

	*data = sps_pps_data.pBuffer;
	*len = sps_pps_data.nLength;
	
	return 0;
}

static int setEncParam(encoder_Context* pEncContext, encode_param_t *encode_param)
{
    VideoEncoder *pVideoEnc = pEncContext->pVideoEnc;
    int result = 0;
    VeProcSet mProcSet;
	unsigned int vbv_size = 8*1024*1024;
    mProcSet.bProcEnable = 1;
    mProcSet.nProcFreq = 30;
    mProcSet.nStatisBitRateTime = 1000;
    mProcSet.nStatisFrRateTime  = 1000;

    VencSetParameter(pVideoEnc, VENC_IndexParamProcSet, &mProcSet);

#if ENABLE_DROP_FRAME_NUM
    unsigned int nDropFrameNum = 10;
    VencSetParameter(pVideoEnc, VENC_IndexParamDropFrame, &nDropFrameNum);
#endif


#if ENABLE_3D_FLITER
    s3DfilterParam m3DfilterParam;
    m3DfilterParam.enable_3d_filter = 1;
    m3DfilterParam.adjust_pix_level_enable = 1;
    m3DfilterParam.smooth_filter_enable = 1;
    m3DfilterParam.max_pix_diff_th = 13;
    m3DfilterParam.max_mad_th = 18;
    m3DfilterParam.max_mv_th  = 20;
    m3DfilterParam.max_coef = 15;
    m3DfilterParam.min_coef = 2;
    VencSetParameter(pVideoEnc, VENC_IndexParam3DFilterNew, &m3DfilterParam);
#endif

#if ENABLE_2D_FLITER
    s2DfilterParam m2DfilterParam;
    m2DfilterParam.enable_2d_filter = 1;
    m2DfilterParam.filter_strength_uv = 126;
    m2DfilterParam.filter_strength_y  = 126;
    m2DfilterParam.filter_th_uv       = 6;
    m2DfilterParam.filter_th_y        = 6;
    VencSetParameter(pVideoEnc, VENC_IndexParam2DFilter, &m2DfilterParam);
#endif

    if(encode_param->bEnableGdc == 1)
    {
        sGdcParam mGdcParam;
        memset(&mGdcParam, 0, sizeof(sGdcParam));
        initGdcFunc(&mGdcParam);
        VencSetParameter(pVideoEnc, VENC_IndexParamGdcConfig, &mGdcParam);
    }
    if(encode_param->bEnableSharp == 1)
    {
        sEncppSharpParamDynamic mSharpParamDynamic;
        sEncppSharpParamStatic  mSharpParamStatic;
        memset(&mSharpParamDynamic, 0, sizeof(sEncppSharpParamDynamic));
        memset(&mSharpParamStatic, 0, sizeof(sEncppSharpParamStatic));
        initSharpFunc(&mSharpParamDynamic, &mSharpParamStatic);

        sEncppSharpParam mSharpParam;
        mSharpParam.mDynamicParam = mSharpParamDynamic;
        mSharpParam.mStaticParam  = mSharpParamStatic;
        unsigned int enableSharp = 1;

        VencSetParameter(pVideoEnc, VENC_IndexParamSharpConfig, &mSharpParam);
        VencSetParameter(pVideoEnc, VENC_IndexParamEnableEncppSharp, &enableSharp);
    }

#if SETUP_VIDEO_TIME_INFO
    unsigned int fps = encode_param->frame_rate;
    VencH264VideoTiming mTiming;
    mTiming.fixed_frame_rate_flag  = 0;
    mTiming.num_units_in_tick = 1000;
    mTiming.time_scale = mTiming.num_units_in_tick*fps*2;

    VencSetParameter(pVideoEnc, VENC_IndexParamH264VideoTiming, &mTiming);
#endif

	if(encode_param->vbv_size)
		vbv_size = encode_param->vbv_size;

    LOGI("encode_param->rotate = %d, vbv_size = %d", encode_param->rotate, vbv_size);

    VencSetParameter(pVideoEnc, VENC_IndexParamRotation, &encode_param->rotate);
	
#ifdef SET_MB_INFO
    //init VencMBModeCtrl
    init_mb_mode(&pEncContext->mMBModeCtrl, encode_param->dst_width, encode_param->dst_height);
#endif

#ifdef GET_MB_INFO
    //init VencMBInfo
    init_mb_info(&pEncContext->mMBInfo, encode_param);
#endif
    if(encode_param->encode_format == VENC_CODEC_H264)
    {
        result = initH264Func(encode_param);
        if(result)
        {
            LOGI("initH264Func error, return \n");
            return -1;
        }
        VencSetParameter(pVideoEnc, VENC_IndexParamProductCase, &encode_param->is_ipc);
        VencSetParameter(pVideoEnc, VENC_IndexParamIsNightCaseFlag, &encode_param->is_night);
        VencSetParameter(pVideoEnc, VENC_IndexParamSensorType, &encode_param->sensor_type);

        VencSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264Param, &encode_param->h264_func.h264Param);
        //VencSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &h264_func.fixQP);
        if(encode_param->test_overlay_flag == 1)
        {
            VencSetParameter(pVideoEnc, VENC_IndexParamSetOverlay, &encode_param->h264_func.sOverlayInfo);
        }

        VencSetParameter(pVideoEnc, VENC_IndexParamProcSet, &encode_param->h264_func.sVeProcInfo);
#ifdef USE_VIDEO_SIGNAL
        VencH264VideoSignal mVencH264VideoSignal;
        memset(&mVencH264VideoSignal, 0, sizeof(VencH264VideoSignal));
        mVencH264VideoSignal.video_format = DEFAULT;
        mVencH264VideoSignal.full_range_flag = 1;
        mVencH264VideoSignal.src_colour_primaries = VENC_BT709;
        mVencH264VideoSignal.dst_colour_primaries = VENC_BT709;
        VencSetParameter(pVideoEnc, VENC_IndexParamH264VideoSignal, &mVencH264VideoSignal);
#endif

#ifdef DETECT_MOTION
        MotionParam mMotionPara;
        mMotionPara.nMaxNumStaticFrame = 4;
        mMotionPara.nMotionDetectEnable = 1;
        mMotionPara.nMotionDetectRatio = 0;
        mMotionPara.nMV64x64Ratio = 0.01;
        mMotionPara.nMVXTh = 6;
        mMotionPara.nMVYTh = 6;
        mMotionPara.nStaticBitsRatio = 0.2;
        mMotionPara.nStaticDetectRatio = 2;
        VencSetParameter(pVideoEnc, VENC_IndexParamMotionDetectStatus,
                                                &mMotionPara);
#endif

#ifdef DETECT_MOTION
        MotionParam mMotionPara;
        mMotionPara.nMaxNumStaticFrame = 4;
        mMotionPara.nMotionDetectEnable = 1;
        mMotionPara.nMotionDetectRatio = 0;
        mMotionPara.nMV64x64Ratio = 0.01;
        mMotionPara.nMVXTh = 6;
        mMotionPara.nMVYTh = 6;
        mMotionPara.nStaticBitsRatio = 0.2;
        mMotionPara.nStaticDetectRatio = 2;

        VencSetParameter(pVideoEnc, VENC_IndexParamMotionDetectStatus,
                                                &mMotionPara);
#endif

        //int tmptmp = 60;
        //VencSetParameter(pVideoEnc, VENC_IndexParamVirtualIFrame, &tmptmp);

#ifdef GET_MB_INFO
        VencSetParameter(pVideoEnc, VENC_IndexParamMBInfoOutput, &pEncContext->mMBInfo);
#endif


#if 0
        unsigned char value = 1;
        //set the specify func
        VencSetParameter(pVideoEnc, VENC_IndexParamH264SVCSkip, &h264_func.SVCSkip);
        value = 0;
        VencSetParameter(pVideoEnc, VENC_IndexParamIfilter, &value);
        value = 0; //degree
        VencSetParameter(pVideoEnc, VENC_IndexParamRotation, &value);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &h264_func.fixQP);
        VencSetParameter(pVideoEnc,
            VENC_IndexParamH264CyclicIntraRefresh, &h264_func.sIntraRefresh);
        value = 720/4;
        VencSetParameter(pVideoEnc, VENC_IndexParamSliceHeight, &value);
        value = 0;
        VencSetParameter(pVideoEnc, VENC_IndexParamSetPSkip, &value);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264AspectRatio, &h264_func.sAspectRatio);
        value = 0;
        VencSetParameter(pVideoEnc, VENC_IndexParamFastEnc, &value);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264VideoSignal, &h264_func.sVideoSignal);
        VencSetParameter(pVideoEnc, VENC_IndexParamSuperFrameConfig, &h264_func.sSuperFrameCfg);
#endif
    }
    return 0;
}

static int initGdcFunc(sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eWarpMode = Gdc_Warp_LDC;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;
    pGdcParam->calib_widht  = 3264;
    pGdcParam->calib_height = 2448;

    pGdcParam->fx = 2417.19;
    pGdcParam->fy = 2408.43;
    pGdcParam->cx = 1631.50;
    pGdcParam->cy = 1223.50;
    pGdcParam->fx_scale = 2161.82;
    pGdcParam->fy_scale = 2153.99;
    pGdcParam->cx_scale = 1631.50;
    pGdcParam->cy_scale = 1223.50;

    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    pGdcParam->distCoef_wide_ra[0] = -0.3849;
    pGdcParam->distCoef_wide_ra[1] = 0.1567;
    pGdcParam->distCoef_wide_ra[2] = -0.0030;
    pGdcParam->distCoef_wide_ta[0] = -0.00005;
    pGdcParam->distCoef_wide_ta[1] = 0.0016;

    pGdcParam->distCoef_fish_k[0]  = -0.0024;
    pGdcParam->distCoef_fish_k[1]  = 0.141;
    pGdcParam->distCoef_fish_k[2]  = -0.3;
    pGdcParam->distCoef_fish_k[3]  = 0.2328;

    pGdcParam->centerOffsetX         =      0;
    pGdcParam->centerOffsetY         =      0;
    pGdcParam->rotateAngle           =      0;     //[0,360]
    pGdcParam->radialDistortCoef     =      0;     //[-255,255]
    pGdcParam->trapezoidDistortCoef  =      0;     //[-255,255]
    pGdcParam->fanDistortCoef        =      0;     //[-255,255]
    pGdcParam->pan                   =      0;     //pano360:[0,360]; others:[-90,90]
    pGdcParam->tilt                  =      0;     //[-90,90]
    pGdcParam->zoomH                 =      100;   //[0,100]
    pGdcParam->zoomV                 =      100;   //[0,100]
    pGdcParam->scale                 =      100;   //[0,100]
    pGdcParam->innerRadius           =      0;     //[0,width/2]
    pGdcParam->roll                  =      0;     //[-90,90]
    pGdcParam->pitch                 =      0;     //[-90,90]
    pGdcParam->yaw                   =      0;     //[-90,90]

    pGdcParam->perspFunc             =    Gdc_Persp_Only;
    pGdcParam->perspectiveProjMat[0] =    1.0;
    pGdcParam->perspectiveProjMat[1] =    0.0;
    pGdcParam->perspectiveProjMat[2] =    0.0;
    pGdcParam->perspectiveProjMat[3] =    0.0;
    pGdcParam->perspectiveProjMat[4] =    1.0;
    pGdcParam->perspectiveProjMat[5] =    0.0;
    pGdcParam->perspectiveProjMat[6] =    0.0;
    pGdcParam->perspectiveProjMat[7] =    0.0;
    pGdcParam->perspectiveProjMat[8] =    1.0;

    pGdcParam->mountHeight           =      0.85; //meters
    pGdcParam->roiDist_ahead         =      4.5;  //meters
    pGdcParam->roiDist_left          =     -1.5;  //meters
    pGdcParam->roiDist_right         =      1.5;  //meters
    pGdcParam->roiDist_bottom        =      0.65; //meters

    pGdcParam->peaking_en            =      1;    //0/1
    pGdcParam->peaking_clamp         =      1;    //0/1
    pGdcParam->peak_m                =     16;    //[0,63]
    pGdcParam->th_strong_edge        =      6;    //[0,15]
    pGdcParam->peak_weights_strength =      2;    //[0,15]

    if(pGdcParam->eWarpMode == Gdc_Warp_LDC)
    {
        pGdcParam->birdsImg_width    = 768;
        pGdcParam->birdsImg_height   = 1080;
    }

    return 0;
}

static int initSharpFunc(    sEncppSharpParamDynamic *pSharpParamDynamic, sEncppSharpParamStatic  *pSharpParamStatic)
{
    pSharpParamDynamic->ss_ns_lw  = 0;           //[0,255];
    pSharpParamDynamic->ss_ns_hi  = 0;           //[0,255];
    pSharpParamDynamic->ls_ns_lw  = 0;           //[0,255];
    pSharpParamDynamic->ls_ns_hi  = 0;           //[0,255];
    pSharpParamDynamic->ss_lw_cor = 0;           //[0,255];
    pSharpParamDynamic->ss_hi_cor = 0;           //[0,255];
    pSharpParamDynamic->ls_lw_cor = 0;           //[0,255];
    pSharpParamDynamic->ls_hi_cor = 0;           //[0,255];
    pSharpParamDynamic->ss_blk_stren = 256;      //[0,4095];
    pSharpParamDynamic->ss_wht_stren = 256;      //[0,4095];
    pSharpParamDynamic->ls_blk_stren = 256;      //[0,4095];
    pSharpParamDynamic->ls_wht_stren = 256;      //[0,4095];
    pSharpParamDynamic->wht_clp_para = 256;       //[0,1023];
    pSharpParamDynamic->blk_clp_para = 256;       //[0,1023];
    pSharpParamDynamic->ss_avg_smth  = 0;         //[0,255];
    pSharpParamDynamic->hfr_mf_blk_stren  = 0;    //[0,4095];
    pSharpParamDynamic->hfr_hf_blk_stren  = 0;    //[0,4095];
    pSharpParamDynamic->hfr_hf_wht_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_hf_cor_ratio  = 0;    //[0,255];
    pSharpParamDynamic->hfr_mf_mix_ratio  = 390;  //[0,1023];
    pSharpParamDynamic->ss_dir_smth  = 0;       //[0,16];
    pSharpParamDynamic->wht_clp_slp  = 16;        //[0,63];
    pSharpParamDynamic->blk_clp_slp  = 8;         //[0,63];
    pSharpParamDynamic->max_clp_ratio  = 64;        //[0,255];
    pSharpParamDynamic->hfr_hf_blk_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_smth_ratio  = 0;      //[0,32];
    pSharpParamDynamic->dir_smth[0] = 0;         //[0,16];
    pSharpParamDynamic->dir_smth[1] = 0;         //[0,16];
    pSharpParamDynamic->dir_smth[2] = 0;         //[0,16];
    pSharpParamDynamic->dir_smth[3] = 0;         //[0,16];
    pSharpParamStatic->ss_shp_ratio = 0;           //[0,255];
    pSharpParamStatic->ls_shp_ratio = 0;           //[0,255];
    pSharpParamStatic->ss_dir_ratio = 98;          //[0,1023];
    pSharpParamStatic->ls_dir_ratio = 90;          //[0,1023];
    pSharpParamStatic->ss_crc_stren = 128;        //[0,1023];
    pSharpParamStatic->ss_crc_min   = 16;         //[0,255];
    pSharpParamDynamic->hfr_mf_blk_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_mf_wht_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_hf_wht_stren = 0;    //[0,4095];
    pSharpParamDynamic->hfr_mf_wht_stren = 0;    //[0,4095];
    pSharpParamDynamic->hfr_mf_cor_ratio = 0;    //[0,255];
    pSharpParamDynamic->hfr_hf_mix_ratio = 390;  //[0,1023];
    pSharpParamDynamic->hfr_hf_mix_min_ratio = 0;   //[0,255];
    pSharpParamDynamic->hfr_mf_mix_min_ratio = 0;   //[0,255];

    pSharpParamStatic->sharp_ss_value[0]=384;
    pSharpParamStatic->sharp_ss_value[1]=416;
    pSharpParamStatic->sharp_ss_value[2]=471;
    pSharpParamStatic->sharp_ss_value[3]=477;
    pSharpParamStatic->sharp_ss_value[4]=443;
    pSharpParamStatic->sharp_ss_value[5]=409;
    pSharpParamStatic->sharp_ss_value[6]=374;
    pSharpParamStatic->sharp_ss_value[7]=340;
    pSharpParamStatic->sharp_ss_value[8]=306;
    pSharpParamStatic->sharp_ss_value[9]=272;
    pSharpParamStatic->sharp_ss_value[10]=237;
    pSharpParamStatic->sharp_ss_value[11]=203;
    pSharpParamStatic->sharp_ss_value[12]=169;
    pSharpParamStatic->sharp_ss_value[13]=134;
    pSharpParamStatic->sharp_ss_value[14]=100;
    pSharpParamStatic->sharp_ss_value[15]=66;
    pSharpParamStatic->sharp_ss_value[16]=41;
    pSharpParamStatic->sharp_ss_value[17]=32;
    pSharpParamStatic->sharp_ss_value[18]=32;
    pSharpParamStatic->sharp_ss_value[19]=32;
    pSharpParamStatic->sharp_ss_value[20]=32;
    pSharpParamStatic->sharp_ss_value[21]=32;
    pSharpParamStatic->sharp_ss_value[22]=32;
    pSharpParamStatic->sharp_ss_value[23]=32;
    pSharpParamStatic->sharp_ss_value[24]=32;
    pSharpParamStatic->sharp_ss_value[25]=32;
    pSharpParamStatic->sharp_ss_value[26]=32;
    pSharpParamStatic->sharp_ss_value[27]=32;
    pSharpParamStatic->sharp_ss_value[28]=32;
    pSharpParamStatic->sharp_ss_value[29]=32;
    pSharpParamStatic->sharp_ss_value[30]=32;
    pSharpParamStatic->sharp_ss_value[31]=32;
    pSharpParamStatic->sharp_ss_value[32]=32;

    pSharpParamStatic->sharp_ls_value[0]=384;
    pSharpParamStatic->sharp_ls_value[1]=395;
    pSharpParamStatic->sharp_ls_value[2]=427;
    pSharpParamStatic->sharp_ls_value[3]=470;
    pSharpParamStatic->sharp_ls_value[4]=478;
    pSharpParamStatic->sharp_ls_value[5]=416;
    pSharpParamStatic->sharp_ls_value[6]=320;
    pSharpParamStatic->sharp_ls_value[7]=224;
    pSharpParamStatic->sharp_ls_value[8]=152;
    pSharpParamStatic->sharp_ls_value[9]=128;
    pSharpParamStatic->sharp_ls_value[10]=128;
    pSharpParamStatic->sharp_ls_value[11]=128;
    pSharpParamStatic->sharp_ls_value[12]=128;
    pSharpParamStatic->sharp_ls_value[13]=128;
    pSharpParamStatic->sharp_ls_value[14]=128;
    pSharpParamStatic->sharp_ls_value[15]=128;
    pSharpParamStatic->sharp_ls_value[16]=128;
    pSharpParamStatic->sharp_ls_value[17]=128;
    pSharpParamStatic->sharp_ls_value[18]=128;
    pSharpParamStatic->sharp_ls_value[19]=128;
    pSharpParamStatic->sharp_ls_value[20]=128;
    pSharpParamStatic->sharp_ls_value[21]=128;
    pSharpParamStatic->sharp_ls_value[22]=128;
    pSharpParamStatic->sharp_ls_value[23]=128;
    pSharpParamStatic->sharp_ls_value[24]=128;
    pSharpParamStatic->sharp_ls_value[25]=128;
    pSharpParamStatic->sharp_ls_value[26]=128;
    pSharpParamStatic->sharp_ls_value[27]=128;
    pSharpParamStatic->sharp_ls_value[28]=128;
    pSharpParamStatic->sharp_ls_value[29]=128;
    pSharpParamStatic->sharp_ls_value[30]=128;
    pSharpParamStatic->sharp_ls_value[31]=128;
    pSharpParamStatic->sharp_ls_value[32]=128;

    pSharpParamStatic->sharp_hsv[0]=218;
    pSharpParamStatic->sharp_hsv[1]=206;
    pSharpParamStatic->sharp_hsv[2]=214;
    pSharpParamStatic->sharp_hsv[3]=247;
    pSharpParamStatic->sharp_hsv[4]=282;
    pSharpParamStatic->sharp_hsv[5]=299;
    pSharpParamStatic->sharp_hsv[6]=308;
    pSharpParamStatic->sharp_hsv[7]=316;
    pSharpParamStatic->sharp_hsv[8]=325;
    pSharpParamStatic->sharp_hsv[9]=333;
    pSharpParamStatic->sharp_hsv[10]=342;
    pSharpParamStatic->sharp_hsv[11]=350;
    pSharpParamStatic->sharp_hsv[12]=359;
    pSharpParamStatic->sharp_hsv[13]=367;
    pSharpParamStatic->sharp_hsv[14]=376;
    pSharpParamStatic->sharp_hsv[15]=380;
    pSharpParamStatic->sharp_hsv[16]=375;
    pSharpParamStatic->sharp_hsv[17]=366;
    pSharpParamStatic->sharp_hsv[18]=358;
    pSharpParamStatic->sharp_hsv[19]=349;
    pSharpParamStatic->sharp_hsv[20]=341;
    pSharpParamStatic->sharp_hsv[21]=332;
    pSharpParamStatic->sharp_hsv[22]=324;
    pSharpParamStatic->sharp_hsv[23]=315;
    pSharpParamStatic->sharp_hsv[24]=307;
    pSharpParamStatic->sharp_hsv[25]=298;
    pSharpParamStatic->sharp_hsv[26]=290;
    pSharpParamStatic->sharp_hsv[27]=281;
    pSharpParamStatic->sharp_hsv[28]=273;
    pSharpParamStatic->sharp_hsv[29]=264;
    pSharpParamStatic->sharp_hsv[30]=258;
    pSharpParamStatic->sharp_hsv[31]=256;
    pSharpParamStatic->sharp_hsv[32]=256;
    pSharpParamStatic->sharp_hsv[33]=256;
    pSharpParamStatic->sharp_hsv[34]=256;
    pSharpParamStatic->sharp_hsv[35]=256;
    pSharpParamStatic->sharp_hsv[36]=256;
    pSharpParamStatic->sharp_hsv[37]=256;
    pSharpParamStatic->sharp_hsv[38]=256;
    pSharpParamStatic->sharp_hsv[39]=256;
    pSharpParamStatic->sharp_hsv[40]=256;
    pSharpParamStatic->sharp_hsv[41]=256;
    pSharpParamStatic->sharp_hsv[42]=256;
    pSharpParamStatic->sharp_hsv[43]=256;
    pSharpParamStatic->sharp_hsv[44]=256;
    pSharpParamStatic->sharp_hsv[45]=256;

    pSharpParamDynamic->sharp_edge_lum[0]=128;
    pSharpParamDynamic->sharp_edge_lum[1]=144;
    pSharpParamDynamic->sharp_edge_lum[2]=160;
    pSharpParamDynamic->sharp_edge_lum[3]=176;
    pSharpParamDynamic->sharp_edge_lum[4]=192;
    pSharpParamDynamic->sharp_edge_lum[5]=208;
    pSharpParamDynamic->sharp_edge_lum[6]=224;
    pSharpParamDynamic->sharp_edge_lum[7]=240;
    pSharpParamDynamic->sharp_edge_lum[8]=256;
    pSharpParamDynamic->sharp_edge_lum[9]=256;
    pSharpParamDynamic->sharp_edge_lum[10]=256;
    pSharpParamDynamic->sharp_edge_lum[11]=256;
    pSharpParamDynamic->sharp_edge_lum[12]=256;
    pSharpParamDynamic->sharp_edge_lum[13]=256;
    pSharpParamDynamic->sharp_edge_lum[14]=256;
    pSharpParamDynamic->sharp_edge_lum[15]=256;
    pSharpParamDynamic->sharp_edge_lum[16]=256;
    pSharpParamDynamic->sharp_edge_lum[17]=256;
    pSharpParamDynamic->sharp_edge_lum[18]=256;
    pSharpParamDynamic->sharp_edge_lum[19]=256;
    pSharpParamDynamic->sharp_edge_lum[20]=256;
    pSharpParamDynamic->sharp_edge_lum[21]=256;
    pSharpParamDynamic->sharp_edge_lum[22]=256;
    pSharpParamDynamic->sharp_edge_lum[23]=256;
    pSharpParamDynamic->sharp_edge_lum[24]=256;
    pSharpParamDynamic->sharp_edge_lum[25]=256;
    pSharpParamDynamic->sharp_edge_lum[26]=256;
    pSharpParamDynamic->sharp_edge_lum[27]=256;
    pSharpParamDynamic->sharp_edge_lum[28]=256;
    pSharpParamDynamic->sharp_edge_lum[29]=256;
    pSharpParamDynamic->sharp_edge_lum[30]=256;
    pSharpParamDynamic->sharp_edge_lum[31]=256;
    pSharpParamDynamic->sharp_edge_lum[32]=256;

#if 0
    pSharpParamDynamic->roi_num = 0; //<=8
    pSharpParamDynamic->roi_item[0].x      = 40;
    pSharpParamDynamic->roi_item[0].y      = 59;
    pSharpParamDynamic->roi_item[0].width  = 32;
    pSharpParamDynamic->roi_item[0].height = 32;


    pSharpParam->roi_item[1].x      = 40;
    pSharpParam->roi_item[1].y      = 59;
    pSharpParam->roi_item[1].width  = 32;
    pSharpParam->roi_item[1].height = 32;

    pSharpParam->roi_item[2].x      = 40;
    pSharpParam->roi_item[2].y      = 59;
    pSharpParam->roi_item[2].width  = 32;
    pSharpParam->roi_item[2].height = 32;

    pSharpParam->roi_item[3].x      = 40;
    pSharpParam->roi_item[3].y      = 59;
    pSharpParam->roi_item[3].width  = 32;
    pSharpParam->roi_item[3].height = 32;

    pSharpParam->roi_item[4].x      = 40;
    pSharpParam->roi_item[4].y      = 59;
    pSharpParam->roi_item[4].width  = 32;
    pSharpParam->roi_item[4].height = 32;

    pSharpParam->roi_item[5].x      = 40;
    pSharpParam->roi_item[5].y      = 59;
    pSharpParam->roi_item[5].width  = 32;
    pSharpParam->roi_item[5].height = 32;

    pSharpParam->roi_item[6].x      = 40;
    pSharpParam->roi_item[6].y      = 59;
    pSharpParam->roi_item[6].width  = 32;
    pSharpParam->roi_item[6].height = 32;

    pSharpParam->roi_item[7].x      = 40;
    pSharpParam->roi_item[7].y      = 59;
    pSharpParam->roi_item[7].width  = 32;
    pSharpParam->roi_item[7].height = 32;
#endif

    return 0;
}


static int initH264Func(encode_param_t *encode_param)
{
    h264_func_t *h264_func = &encode_param->h264_func;

    //init h264Param
#ifdef SET_MB_INFO
    h264_func->h264Param.sRcParam.eRcMode = AW_QPMAP;
#endif
    h264_func->h264Param.bEntropyCodingCABAC = 1;
    h264_func->h264Param.nBitrate = encode_param->bit_rate;
    h264_func->h264Param.nFramerate = encode_param->frame_rate;
    h264_func->h264Param.nMaxKeyInterval = encode_param->maxKeyFrame;
    h264_func->h264Param.nCodingMode = VENC_FRAME_CODING;
    h264_func->h264Param.sProfileLevel.nProfile = VENC_H264ProfileHigh;
    h264_func->h264Param.sProfileLevel.nLevel = VENC_H264Level51;
    h264_func->h264Param.sGopParam.eGopMode = (VENC_VIDEO_GOP_MODE)1;
    if(h264_func->h264Param.nMaxKeyInterval == 0)
    {
        h264_func->h264Param.nMaxKeyInterval = h264_func->h264Param.nFramerate;
    }

    if(0 < encode_param->i_qp_min && encode_param->i_qp_max < 52
        && encode_param->i_qp_min < encode_param->i_qp_max)
    {
        h264_func->h264Param.sQPRange.nMinqp = encode_param->i_qp_min;
        h264_func->h264Param.sQPRange.nMaxqp = encode_param->i_qp_max;

    }
    else
    {
        h264_func->h264Param.sQPRange.nMinqp = 10;
        h264_func->h264Param.sQPRange.nMaxqp = 50;
    }

    if(0 < encode_param->p_qp_min && encode_param->p_qp_max < 52
        && encode_param->p_qp_min < encode_param->p_qp_max)
    {
        h264_func->h264Param.sQPRange.nMinPqp = encode_param->p_qp_min;
        h264_func->h264Param.sQPRange.nMaxPqp = encode_param->p_qp_max;

    }
    else
    {
        h264_func->h264Param.sQPRange.nMinPqp = 10;
        h264_func->h264Param.sQPRange.nMaxPqp = 50;
    }

    if(0 < encode_param->qp_init && encode_param->qp_init < 52)
    {
        h264_func->h264Param.sQPRange.nQpInit = encode_param->qp_init;
    }
    else
    {
        h264_func->h264Param.sQPRange.nQpInit = 30;
    }

    //h264_func->h264Param.bLongRefEnable = 1;
    //h264_func->h264Param.nLongRefPoc = 0;

#if 0
    h264_func->sH264Smart.img_bin_en = 1;
    h264_func->sH264Smart.img_bin_th = 27;
    h264_func->sH264Smart.shift_bits = 2;
    h264_func->sH264Smart.smart_fun_en = 1;
#endif

    //init VencH264FixQP
    init_fix_qp(&h264_func->fixQP);

    //init VencSuperFrameConfig
    init_super_frame_cfg(&h264_func->sSuperFrameCfg);

    //init VencH264SVCSkip
    init_svc_skip(&h264_func->SVCSkip);

    //init VencH264AspectRatio
    init_aspect_ratio(&h264_func->sAspectRatio);

    //init VencH264AspectRatio
    init_video_signal(&h264_func->sVideoSignal);

    //init CyclicIntraRefresh
    init_intra_refresh(&h264_func->sIntraRefresh);

    //init VencROIConfig
    init_roi(h264_func->sRoiConfig);

    //init proc info
    init_enc_proc_info(&h264_func->sVeProcInfo);

#if 0
    //init VencOverlayConfig
    init_overlay_info(&h264_func->sOverlayInfo, encode_param);
#endif

    return 0;
}

static void init_fix_qp(VencFixQP *fixQP)
{
    fixQP->bEnable = 1;
    fixQP->nIQp = 35;
    fixQP->nPQp = 35;
}

static void init_super_frame_cfg(VencSuperFrameConfig *sSuperFrameCfg)
{
    sSuperFrameCfg->eSuperFrameMode = VENC_SUPERFRAME_NONE;
    sSuperFrameCfg->nMaxIFrameBits = 30000*8;
    sSuperFrameCfg->nMaxPFrameBits = 15000*8;
}

static void init_svc_skip(VencH264SVCSkip *SVCSkip)
{
    SVCSkip->nTemporalSVC = T_LAYER_4;
    switch(SVCSkip->nTemporalSVC)
    {
        case T_LAYER_4:
            SVCSkip->nSkipFrame = SKIP_8;
            break;
        case T_LAYER_3:
            SVCSkip->nSkipFrame = SKIP_4;
            break;
        case T_LAYER_2:
            SVCSkip->nSkipFrame = SKIP_2;
            break;
        default:
            SVCSkip->nSkipFrame = NO_SKIP;
            break;
    }
}

static void init_aspect_ratio(VencH264AspectRatio *sAspectRatio)
{
    sAspectRatio->aspect_ratio_idc = 255;
    sAspectRatio->sar_width = 4;
    sAspectRatio->sar_height = 3;
}

static void init_video_signal(VencH264VideoSignal *sVideoSignal)
{
    sVideoSignal->video_format = (VENC_VIDEO_FORMAT)5;
    sVideoSignal->src_colour_primaries = (VENC_COLOR_SPACE)0;
    sVideoSignal->dst_colour_primaries = (VENC_COLOR_SPACE)1;
}

static void init_intra_refresh(VencCyclicIntraRefresh *sIntraRefresh)
{
    sIntraRefresh->bEnable = 1;
    sIntraRefresh->nBlockNumber = 10;
}
static void init_roi(VencROIConfig *sRoiConfig)
{
    sRoiConfig[0].bEnable = 1;
    sRoiConfig[0].index = 0;
    sRoiConfig[0].nQPoffset = 10;
    sRoiConfig[0].sRect.nLeft = 0;
    sRoiConfig[0].sRect.nTop = 0;
    sRoiConfig[0].sRect.nWidth = 1280;
    sRoiConfig[0].sRect.nHeight = 320;

    sRoiConfig[1].bEnable = 1;
    sRoiConfig[1].index = 1;
    sRoiConfig[1].nQPoffset = 10;
    sRoiConfig[1].sRect.nLeft = 320;
    sRoiConfig[1].sRect.nTop = 180;
    sRoiConfig[1].sRect.nWidth = 320;
    sRoiConfig[1].sRect.nHeight = 180;

    sRoiConfig[2].bEnable = 1;
    sRoiConfig[2].index = 2;
    sRoiConfig[2].nQPoffset = 10;
    sRoiConfig[2].sRect.nLeft = 320;
    sRoiConfig[2].sRect.nTop = 180;
    sRoiConfig[2].sRect.nWidth = 320;
    sRoiConfig[2].sRect.nHeight = 180;

    sRoiConfig[3].bEnable = 1;
    sRoiConfig[3].index = 3;
    sRoiConfig[3].nQPoffset = 10;
    sRoiConfig[3].sRect.nLeft = 320;
    sRoiConfig[3].sRect.nTop = 180;
    sRoiConfig[3].sRect.nWidth = 320;
    sRoiConfig[3].sRect.nHeight = 180;
}

static void init_enc_proc_info(VeProcSet *ve_proc_set)
{
    ve_proc_set->bProcEnable = 1;
    ve_proc_set->nProcFreq = 3;
}

static void init_overlay_info(VencOverlayInfoS *pOverlayInfo, encode_param_t *encode_param)
{
    int i;
    unsigned char num_bitMap = 2;
    BitMapInfoS* pBitMapInfo;
    unsigned int time_id_list[19];
    unsigned int start_mb_x;
    unsigned int start_mb_y;

    memset(pOverlayInfo, 0, sizeof(VencOverlayInfoS));

#if 0
    char filename[64];
    int ret;
    for(i = 0; i < num_bitMap; i++)
    {
        FILE* icon_hdle = NULL;
        int width  = 0;
        int height = 0;

        sLOGI(filename, "%s%d.bmp", "/mnt/libcedarc/bitmap/icon_720p_",i);

        icon_hdle   = fopen(filename, "r");
        if (icon_hdle == NULL) {
            LOGI("get wartermark %s error\n", filename);
            return;
        }

        //get watermark picture size
        fseek(icon_hdle, 18, SEEK_SET);
        fread(&width, 1, 4, icon_hdle);
        fread(&height, 1, 4, icon_hdle);

        fseek(icon_hdle, 54, SEEK_SET);

        bit_map_info[i].argb_addr = NULL;
        bit_map_info[i].width = 0;
        bit_map_info[i].height = 0;

        bit_map_info[i].width = width;
        bit_map_info[i].height = height*(-1);

        bit_map_info[i].width_aligh16 = ALIGN_XXB(16, bit_map_info[i].width);
        bit_map_info[i].height_aligh16 = ALIGN_XXB(16, bit_map_info[i].height);
        if(bit_map_info[i].argb_addr == NULL) {
            bit_map_info[i].argb_addr =
            (unsigned char*)malloc(bit_map_info[i].width_aligh16*bit_map_info[i].height_aligh16*4);

            if(bit_map_info[i].argb_addr == NULL)
            {
                LOGI("malloc bit_map_info[%d].argb_addr fail\n", i);
                return;
            }
        }
        LOGI("bitMap[%d] size[%d,%d], size_align16[%d, %d], argb_addr:%p\n", i,
                                    bit_map_info[i].width,
                                    bit_map_info[i].height,
                                    bit_map_info[i].width_aligh16,
                                    bit_map_info[i].height_aligh16,
                                    bit_map_info[i].argb_addr);

        ret = fread(bit_map_info[i].argb_addr, 1,
            bit_map_info[i].width*bit_map_info[i].height*4, icon_hdle);
        if(ret != bit_map_info[i].width*bit_map_info[i].height*4)
            LOGI("read bitMap[%d] error, ret value:%d\n", i, ret);

        bit_map_info[i].size = bit_map_info[i].width_aligh16 * bit_map_info[i].height_aligh16 * 4;

        if (icon_hdle) {
            fclose(icon_hdle);
            icon_hdle = NULL;
        }
    }

    //time 2017-04-27 18:28:26
    time_id_list[0] = 2;
    time_id_list[1] = 0;
    time_id_list[2] = 1;
    time_id_list[3] = 7;
    time_id_list[4] = 11;
    time_id_list[5] = 0;
    time_id_list[6] = 4;
    time_id_list[7] = 11;
    time_id_list[8] = 2;
    time_id_list[9] = 7;
    time_id_list[10] = 10;
    time_id_list[11] = 1;
    time_id_list[12] = 8;
    time_id_list[13] = 12;
    time_id_list[14] = 2;
    time_id_list[15] = 8;
    time_id_list[16] = 12;
    time_id_list[17] = 2;
    time_id_list[18] = 6;

    LOGI("pOverlayInfo:%p\n", pOverlayInfo);
    pOverlayInfo->blk_num = 19;
#else
        FILE* icon_hdle = NULL;
        int width  = 464;
        int height = 32;
        memset(time_id_list, 0 ,sizeof(time_id_list));

        icon_hdle = fopen(encode_param->overlay_file, "r");
        LOGI("icon_hdle = %p",icon_hdle);
        if (icon_hdle == NULL) {
            LOGI("get icon_hdle error\n");
            return;
        }

        for(i = 0; i < num_bitMap; i++)
        {
            bit_map_info[i].argb_addr = NULL;
            bit_map_info[i].width = width;
            bit_map_info[i].height = height;

            bit_map_info[i].width_aligh16 = ALIGN_XXB(16, bit_map_info[i].width);
            bit_map_info[i].height_aligh16 = ALIGN_XXB(16, bit_map_info[i].height);
            if(bit_map_info[i].argb_addr == NULL) {
                bit_map_info[i].argb_addr =
            (unsigned char*)malloc(bit_map_info[i].width_aligh16*bit_map_info[i].height_aligh16*4);

                if(bit_map_info[i].argb_addr == NULL)
                {
                    LOGI("malloc bit_map_info[%d].argb_addr fail\n", i);
                    if (icon_hdle) {
                        fclose(icon_hdle);
                        icon_hdle = NULL;
                    }

                    return;
                }
            }
            LOGI("bitMap[%d] size[%d,%d], size_align16[%d, %d], argb_addr:%p\n", i,
                                                        bit_map_info[i].width,
                                                        bit_map_info[i].height,
                                                        bit_map_info[i].width_aligh16,
                                                        bit_map_info[i].height_aligh16,
                                                        bit_map_info[i].argb_addr);

            int ret;
            ret = fread(bit_map_info[i].argb_addr, 1,
                        bit_map_info[i].width*bit_map_info[i].height*4, icon_hdle);
            if(ret != (int)(bit_map_info[i].width*bit_map_info[i].height*4))
            LOGI("read bitMap[%d] error, ret value:%d\n", i, ret);

            bit_map_info[i].size
                = bit_map_info[i].width_aligh16 * bit_map_info[i].height_aligh16 * 4;
            fseek(icon_hdle, 0, SEEK_SET);
        }
        if (icon_hdle) {
            fclose(icon_hdle);
            icon_hdle = NULL;
        }
#endif
        pOverlayInfo->argb_type = VENC_OVERLAY_ARGB8888;
        pOverlayInfo->blk_num = num_bitMap;
        LOGI("blk_num:%d, argb_type:%d\n", pOverlayInfo->blk_num, pOverlayInfo->argb_type);
        //pOverlayInfo->invert_threshold = 200;
        //pOverlayInfo->invert_mode = 3;

        start_mb_x = 0;
        start_mb_y = 0;
        for(i=0; i<pOverlayInfo->blk_num; i++)
        {
            //id = time_id_list[i];
            //pBitMapInfo = &bit_map_info[id];
            pBitMapInfo = &bit_map_info[i];

            pOverlayInfo->overlayHeaderList[i].start_mb_x = start_mb_x;
            pOverlayInfo->overlayHeaderList[i].start_mb_y = start_mb_y;
            pOverlayInfo->overlayHeaderList[i].end_mb_x = start_mb_x
                                        + (pBitMapInfo->width_aligh16 / 16 - 1);
            pOverlayInfo->overlayHeaderList[i].end_mb_y = start_mb_y
                                        + (pBitMapInfo->height_aligh16 / 16 -1);

            pOverlayInfo->overlayHeaderList[i].extra_alpha_flag = 0;
            pOverlayInfo->overlayHeaderList[i].extra_alpha = 8;
            if(i%3 == 0)
                pOverlayInfo->overlayHeaderList[i].overlay_type = LUMA_REVERSE_OVERLAY;
            else if(i%2 == 0 && i!=0)
                pOverlayInfo->overlayHeaderList[i].overlay_type = COVER_OVERLAY;
            else
                pOverlayInfo->overlayHeaderList[i].overlay_type = NORMAL_OVERLAY;


            if(pOverlayInfo->overlayHeaderList[i].overlay_type == COVER_OVERLAY)
            {
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_y = 0xff;
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_u = 0xff;
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_v = 0xff;
            }

            //pOverlayInfo->overlayHeaderList[i].bforce_reverse_flag = 1;

            pOverlayInfo->overlayHeaderList[i].overlay_blk_addr = pBitMapInfo->argb_addr;
            pOverlayInfo->overlayHeaderList[i].bitmap_size = pBitMapInfo->size;

            LOGV("blk_%d[%d], start_mb[%d,%d], end_mb[%d,%d],extra_alpha_flag:%d, extra_alpha:%d\n",
                                i,
                                time_id_list[i],
                                pOverlayInfo->overlayHeaderList[i].start_mb_x,
                                pOverlayInfo->overlayHeaderList[i].start_mb_y,
                                pOverlayInfo->overlayHeaderList[i].end_mb_x,
                                pOverlayInfo->overlayHeaderList[i].end_mb_y,
                                pOverlayInfo->overlayHeaderList[i].extra_alpha_flag,
                                pOverlayInfo->overlayHeaderList[i].extra_alpha);
            LOGV("overlay_type:%d, cover_yuv[%d,%d,%d], overlay_blk_addr:%p, bitmap_size:%d\n",
                                pOverlayInfo->overlayHeaderList[i].overlay_type,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_y,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_u,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_v,
                                pOverlayInfo->overlayHeaderList[i].overlay_blk_addr,
                                pOverlayInfo->overlayHeaderList[i].bitmap_size);
            //if(i != 5)
            {
                start_mb_x += pBitMapInfo->width_aligh16 / 16;
                start_mb_y += pBitMapInfo->height_aligh16 / 16;
            }
        }

    return;
}

static int init_Roi_config(VencBaseConfig *baseConfig, VencCopyROIConfig*pRoiConfig)
{
    unsigned int i, j, idx, num, nyLen;
    struct ScMemOpsS *_memops = baseConfig->memops;
    void *veOps = (void *)baseConfig->veOpsS;
    void *pVeopsSelf = baseConfig->pVeOpsSelf;
    //CEDARC_UNUSE(_memops);
    //CEDARC_UNUSE(veOps);
    //CEDARC_UNUSE(pVeopsSelf);

    nyLen = 0;
    num = 16;
    pRoiConfig->bEnable = 1;
    pRoiConfig->num = num;

    for(i=0; i<4; i++)
    {
         for(j=0; j<4; j++)
         {
              idx = i*4 + j;
              pRoiConfig->sRect[idx].nTop = 270*i;
              pRoiConfig->sRect[idx].nLeft = 430*j;
              pRoiConfig->sRect[idx].nWidth= 320;
              pRoiConfig->sRect[idx].nHeight = 240;
              nyLen += ((640+15)&(~15))*((480+15)&(~15));
          }
     }

     pRoiConfig->pRoiYAddrVir = (unsigned char*)EncAdapterMemPalloc(nyLen);
     if(pRoiConfig->pRoiYAddrVir == NULL)
     {
          LOGI("error:palloc roi Y buffer error,return -1");
          return -1;
     }
     memset(pRoiConfig->pRoiYAddrVir, 0x80, nyLen);
     EncAdapterMemFlushCache(pRoiConfig->pRoiYAddrVir, nyLen);
     pRoiConfig->pRoiYAddrPhy =
			(unsigned long)EncAdapterMemGetPhysicAddress(pRoiConfig->pRoiYAddrVir);

      pRoiConfig->pRoiCAddrVir = (unsigned char*)EncAdapterMemPalloc(nyLen/2);
      if(pRoiConfig->pRoiCAddrVir == NULL)
      {
           LOGI("error:palloc roi C buffer error,return -1");
           EncAdapterMemPfree(pRoiConfig->pRoiYAddrVir);
           pRoiConfig->pRoiYAddrVir= NULL;
           return -1;
      }
      memset(pRoiConfig->pRoiCAddrVir, 0x80, nyLen/2);
      EncAdapterMemFlushCache(pRoiConfig->pRoiCAddrVir, nyLen/2);
      pRoiConfig->pRoiCAddrPhy =
			(unsigned long)EncAdapterMemGetPhysicAddress(pRoiConfig->pRoiCAddrVir);

      LOGI("nRoiYLen=%d", nyLen);
      LOGI("pRoiYAddrVir=%p, pRoiCAddrVir=%p", pRoiConfig->pRoiYAddrVir, pRoiConfig->pRoiCAddrVir);
      LOGI("pRoiYAddrPhy=%lx, pRoiCAddrPhy=%lx", pRoiConfig->pRoiYAddrPhy, pRoiConfig->pRoiCAddrPhy);
      return 0;
}

static int EventHandler(VideoEncoder* pEncoder, void* pAppData, VencEventType eEvent,
                             unsigned int nData1, unsigned int nData2, void* pEventData)
{
    encoder_Context* pEncContext = (encoder_Context*)pAppData;
    LOGV("event = %d", eEvent);
    switch(eEvent)
    {
        case VencEvent_UpdateMbModeInfo:
        {
            VencMBModeCtrl *pMbModeCtl = (VencMBModeCtrl *)pEventData;
            //* update mb_mode here
            init_mb_mode(&pEncContext->mMBModeCtrl, pEncContext->baseConfig.nDstWidth, pEncContext->baseConfig.nDstHeight);

            memcpy(pMbModeCtl, &pEncContext->mMBModeCtrl, sizeof(VencMBModeCtrl));
            LOGV("**mode_ctrl_en = %d", pMbModeCtl->mode_ctrl_en);
            break;
        }
        case VencEvent_UpdateMbStatInfo:
        {
            getMbMinfo(pEncContext);
            break;
        }
        default:
            LOGV("not support the event: %d", eEvent);
            break;
    }

    return 0;
}

static void enqueue(InputBufferInfo** pp_head, InputBufferInfo* p)
{
    InputBufferInfo* cur;

    cur = *pp_head;

    if (cur == NULL)
    {
        *pp_head = p;
        p->next  = NULL;
        return;
    }
    else
    {
        while(cur->next != NULL)
            cur = cur->next;

        cur->next = p;
        p->next   = NULL;

        return;
    }
}

static InputBufferInfo* dequeue(InputBufferInfo** pp_head)
{
    InputBufferInfo* head;

    head = *pp_head;

    if (head == NULL)
    {
        return NULL;
    }
    else
    {
        *pp_head = head->next;
        head->next = NULL;
        return head;
    }
}

static int InputBufferDone(VideoEncoder* pEncoder,    void* pAppData,
                          VencCbInputBufferDoneInfo* pBufferDoneInfo)
{
    encoder_Context* pEncContext = (encoder_Context*)pAppData;
    InputBufferInfo *pInputBufInfo = NULL;
    pInputBufInfo = dequeue(&pEncContext->mInputBufMgr.empty_quene);
    if(pInputBufInfo == NULL)
    {
        LOGW("error: dequeue empty_queue failed");
        return -1;
    }
    memcpy(&pInputBufInfo->inputbuffer, pBufferDoneInfo->pInputBuffer, sizeof(VencInputBuffer));
    enqueue(&pEncContext->mInputBufMgr.valid_quene, pInputBufInfo);
    return 0;
}

void init_mb_mode(VencMBModeCtrl *pMBMode, unsigned int dst_width, unsigned int dst_height)
{
    unsigned int mb_num;
    unsigned int j;
    mb_num = (ALIGN_XXB(16, dst_width) >> 4)
                * (ALIGN_XXB(16, dst_height) >> 4);

    LOGI("set mb mode");
    if(pMBMode->p_map_info == NULL)
    {
        pMBMode->p_map_info = (unsigned char *)malloc(sizeof(VencMBModeCtrlInfo) * mb_num);
        pMBMode->mode_ctrl_en = 1;
    }

    #if 1
    int mb_en = 0;
    int mb_skip_flag = 0;
    int mb_qp = 0;
    for (j = 0; j < mb_num / 2; j++)
    {
        mb_en = 1;
        mb_skip_flag = 0;
        mb_qp = 22;
        pMBMode->p_map_info[j] = (unsigned char)((mb_en << 7) | (mb_skip_flag << 6) | mb_qp);
    }
    for (; j < mb_num; j++)
    {
        mb_en = 1;
        mb_skip_flag = 0;
        mb_qp = 32;
        pMBMode->p_map_info[j] = (unsigned char)((mb_en << 7) | (mb_skip_flag << 6) | mb_qp);
    }
    #endif
}

void init_mb_info(VencMBInfo *MBInfo, encode_param_t *encode_param)
{
    if(encode_param->encode_format == VENC_CODEC_H265)
    {
        MBInfo->num_mb = (ALIGN_XXB(32, encode_param->dst_width) *
                            ALIGN_XXB(32, encode_param->dst_height)) >> 10;
    }
    else
    {
        MBInfo->num_mb = (ALIGN_XXB(16, encode_param->dst_width) *
                            ALIGN_XXB(16, encode_param->dst_height)) >> 8;
    }
    MBInfo->p_para = (VencMBInfoPara *)malloc(sizeof(VencMBInfoPara) * MBInfo->num_mb);
    if(MBInfo->p_para == NULL)
    {
        LOGW("malloc MBInfo->p_para error\n");
        return;
    }
    LOGI("mb_num:%d, mb_info_queue_addr:%p\n", MBInfo->num_mb, MBInfo->p_para);
}

void setMbMode(encoder_Context* pEncContext, encode_param_t *encode_param)
{
    VideoEncoder *pVideoEnc = pEncContext->pVideoEnc;
    if(encode_param->encode_format == VENC_CODEC_H264 || encode_param->encode_format == VENC_CODEC_H265)
    {
        if(pEncContext->mMBModeCtrl.mode_ctrl_en == 1)
            VencSetParameter(pVideoEnc, VENC_IndexParamMBModeCtrl, &pEncContext->mMBModeCtrl);
    }
}

void getMbMinfo(encoder_Context* pEncContext)
{
    VencMBInfo *pMBInfo = &pEncContext->mMBInfo;

    LOGV("pMBInfo = %p, p_para = %p", pMBInfo, pMBInfo->p_para);

    if(pMBInfo == NULL || pMBInfo->p_para == NULL)
        return ;

    unsigned int i;
    for(i = 0; i < pMBInfo->num_mb; i++)
    {
        if(i == 4)
        LOGI("No.%d MB: mad=%d, qp=%d, sse=%d, psnr=%f\n",i,pMBInfo->p_para[i].mb_mad,
        pMBInfo->p_para[i].mb_qp, pMBInfo->p_para[i].mb_sse, pMBInfo->p_para[i].mb_psnr);
    }
}

void releaseMb(encode_param_t *encode_param)
{
    VencMBInfo *pMBInfo;
    VencMBModeCtrl *pMBMode;
    if(encode_param->encode_format == VENC_CODEC_H264 && encode_param->h264_func.h264MBMode.mode_ctrl_en)
    {
        pMBInfo = &encode_param->h264_func.MBInfo;
        pMBMode = &encode_param->h264_func.h264MBMode;
    }
    else
        return;

    if(pMBInfo->p_para)
        free(pMBInfo->p_para);
    if(pMBMode->p_map_info)
        free(pMBMode->p_map_info);
}
