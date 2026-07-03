#ifndef RTSPSERVER_H
#define RTSPSERVER_H

#include "ILog.h"
#include "IThread.h"
#include "ThreadSafeQueue.h"
#include "rtsp_demo.h"
#include "time_elaps.h"
#include "h264Data.h"


extern "C" {
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
};

class RtspServerCallBackInfo
{
public:
    RtspServerCallBackInfo() {

    };
    ~RtspServerCallBackInfo(){

    };
    int rtspPort;
    char *rtspUrl = nullptr;
    char *rtspUsername = nullptr;
    char *rtspPassword = nullptr;
    size_t cnt = 0;
    int (*getIDR)() = nullptr;
    void *encObj = nullptr;
};

class RtspServer : public IThread
{
public:
    RtspServer();
    ~RtspServer();

    int Init(int nPort, char *pLiveName, char *pUsername, char *pPassword);
    void Uninit();

    int PktQueueSize();
    void PktQueueEnqueue(H264Data *h264Queue);

    virtual void run();

public:
    int SendPkt(uint8_t *pData, int nLen, uint64_t nPts);
    int SendData(uint8_t *pData, int nLen, uint64_t nPts, bool bIsVideo);
    RtspServerCallBackInfo &GetCallBackInfo();
    void SetListener( RtspCallbackStatus pListener, void *pObj);

private:
    volatile int initFlag = 0;
    rtsp_demo_handle szHandle = nullptr;
    rtsp_session_handle szSessionHandle = nullptr;
    RtspServerCallBackInfo callBackInfo;

    ThreadSafeQueue<H264Data> h264Queue;

};

#endif // RTSPSERVER_H
