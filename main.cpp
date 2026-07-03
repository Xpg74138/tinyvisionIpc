#include "tinyvisonIpcV1.h"
#include <unistd.h>
#include <signal.h>

int stopFlag = 0;

void signProc(int sign) {
    stopFlag = 1;
}

int main(int argc, char **argv)
{
	int i;
	int ret;
	int w = 640;
	int h = 480;
	int fps = 30;
	TinyvisonIpcV1 tinyvisonIpcV1;
	
	LOGI("start");
	signal(SIGINT, signProc);
	
	if(argc == 4)
	{
		w = atoi(argv[1]);
		h = atoi(argv[2]);
		fps = atoi(argv[3]);
	}
	tinyvisonIpcV1.Init(w, h, fps);
	tinyvisonIpcV1.Start();
	while(stopFlag == 0)
	{
		IThreadMSleep(10);
		
	}
	tinyvisonIpcV1.Stop();
	tinyvisonIpcV1.Uninit();
	
	LOGI("done");
	
	return 0;
}
