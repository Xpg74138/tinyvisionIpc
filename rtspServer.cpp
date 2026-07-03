#include "rtspServer.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RtspServer"

RtspServer::RtspServer()
{

}

RtspServer::~RtspServer()
{

}

int RtspServer::Init(int nPort, char *pLiveName, char *pUsername, char *pPassword)
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

    szSessionHandle = create_rtsp_session(szHandle, pLiveName, pUsername, pPassword);
    if (szSessionHandle == NULL) {
        LOGI("create_rtsp_session fail");
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
    if ((initFlag == 0) || ((stopFlag == 1))) {
        return;
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
				free(h264Data->buf);
				h264Data->buf = nullptr;
			}
			
			delete h264Data;
			h264Data = nullptr;
			
        }
        
    }
    while(h264Queue.size() > 0) {
        h264Queue.dequeue(&h264Data);
		if(h264Data->buf) {
			free(h264Data->buf);
			h264Data->buf = nullptr;
		}
		
		delete h264Data;
		h264Data = nullptr;
    }

    LOGI("loop end");
}

int RtspServer::SendPkt(uint8_t *pData, int nLen, uint64_t nPts)
{
    uint8_t *rtspBuf = nullptr;
    int rtspLen;
    char nalHlsHead[6] = {0, 0, 0, 1, 9, 0x30};

    rtspLen = nLen + 6;
    rtspBuf = (uint8_t *)malloc(rtspLen);
    memcpy(rtspBuf, nalHlsHead, 6);
    memcpy((void *)(rtspBuf + 6), pData, nLen);
    SendData(rtspBuf, rtspLen, nPts, 1);
    free(rtspBuf);  
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
