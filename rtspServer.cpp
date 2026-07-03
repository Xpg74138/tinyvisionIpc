#include "rtspServer.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RtspServer"

static const int RTSP_H264_QUEUE_MAX = 3;

static void ReleaseH264Data(H264Data *h264Data)
{
    if (h264Data == nullptr) {
        return;
    }

    if (h264Data->buf) {
        free(h264Data->buf);
        h264Data->buf = nullptr;
    }

    delete h264Data;
}

RtspServer::RtspServer()
{

}

RtspServer::~RtspServer()
{

}

int RtspServer::Init(int nPort, const char *pLiveName, const char *pUsername, const char *pPassword)
{
    LOGI("start");
    if (initFlag == 1) {
        LOGI("reinit err");
        return -1;
    }
    szHandle = create_rtsp_demo(nPort);
    if (szHandle == NULL) {
        LOGI("create_rtsp_demo fail nPort %d", nPort);
        return  -2;
    }

    szSessionHandle = create_rtsp_session(szHandle, pLiveName, (char *)pUsername, (char *)pPassword);
    if (szSessionHandle == NULL) {
        LOGI("create_rtsp_session fail");
        rtsp_del_demo(szHandle);
        szHandle = NULL;
        return  -3;
    }

    callBackInfo.rtspPort = nPort;
    callBackInfo.rtspUrl = pLiveName;
    callBackInfo.rtspUsername = pUsername;
    callBackInfo.rtspPassword = pPassword;
    callBackInfo.cnt = 0;
    callBackInfo.getIDR = nullptr;
    callBackInfo.encObj = nullptr;

    initFlag = 1;
    LOGI("succ");

    return 0;
}

void RtspServer::Uninit()
{
    LOGI("start");
    if (initFlag == 0) {
        LOGI("no init");
        return;
    }
    initFlag = 0;
    if (szSessionHandle) {
        rtsp_del_session(szSessionHandle);
        szSessionHandle = NULL;
    }
    if (szHandle) {
        rtsp_del_demo(szHandle);
        szHandle = NULL;
    }
    LOGI("end");
}

int RtspServer::PktQueueSize()
{
    int size = h264Queue.size();
    return size;
}

void RtspServer::PktQueueEnqueue(H264Data *h264Data)
{
    if (h264Data == nullptr) {
        return;
    }

    if ((initFlag == 0) || ((stopFlag == 1))) {
        ReleaseH264Data(h264Data);
        return;
    }

    int dropCnt = 0;
    while (h264Queue.size() >= RTSP_H264_QUEUE_MAX) {
        H264Data *dropData = nullptr;
        if (h264Queue.dequeue(&dropData) < 0) {
            break;
        }
        ReleaseH264Data(dropData);
        dropCnt++;
    }

    if (dropCnt > 0) {
        LOGW("drop old h264 packets %d, queue limit %d", dropCnt, RTSP_H264_QUEUE_MAX);
    }

    h264Queue.enqueue(h264Data);
}

void RtspServer::run()
{
    int ret;
    H264Data *h264Data;

	int64_t rtspPts = 0;
    LOGI("loop start");
    while(stopFlag == 0) {
        IThreadMSleep(10);

        if (PktQueueSize() > 0) {
            h264Queue.dequeue(&h264Data);

            if(h264Data->buf) {
				SendPkt(h264Data->buf, h264Data->len, h264Data->pts);
			}
			
			ReleaseH264Data(h264Data);
			h264Data = nullptr;
			
        }
        
    }
    while(h264Queue.size() > 0) {
        h264Queue.dequeue(&h264Data);
		ReleaseH264Data(h264Data);
		h264Data = nullptr;
    }

    LOGI("loop end");
}

int RtspServer::SendPkt(uint8_t *pData, int nLen, uint64_t nPts)
{
    SendData(pData, nLen, nPts, 1);
	//LOGI("send");

    return 0;
}

int RtspServer::SendData(uint8_t *pData, int nLen, uint64_t nPts, bool bIsVideo)
{
    int ret;
    if (szHandle) {
        if (bIsVideo) {
            ret = rtsp_sever_tx_video(szHandle, szSessionHandle, (const uint8_t *)pData, nLen, nPts);
            return ret;
        } else {
            ret = rtsp_tx_audio( szSessionHandle, (const uint8_t*)pData, nLen, nPts);
            return ret;
        }
    } else {
        LOGI("szHandle is NULL");
        return -1;
    }
}

RtspServerCallBackInfo &RtspServer::GetCallBackInfo()
{
    return callBackInfo;
}

void RtspServer::SetListener( RtspCallbackStatus pListener, void *pObj)
{
    RtspSetCallback(szSessionHandle, pListener, pObj);
}
