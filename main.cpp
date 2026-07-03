#include "tinyvisonIpcV1.h"
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int stopFlag = 0;

void signProc(int sign) {
    stopFlag = 1;
}

static void PrintUsage(const char *program)
{
	printf("Usage:\n");
	printf("  %s\n", program);
	printf("  %s <width> <height> <fps>\n", program);
	printf("  %s <width> <height> <fps> <rtsp_port>\n", program);
	printf("  %s <width> <height> <fps> <rtsp_port> <rtsp_path>\n", program);
	printf("\n");
	printf("Default: 640x480, 30 fps, RTSP port 554, path /live\n");
	printf("Range: width 16-3840 even, height 16-2160 even, fps 1-60, port 1-65535\n");
	printf("RTSP path must start with '/', for example /live or /live/main\n");
}

static int ParseIntArg(const char *text, const char *name, int minValue, int maxValue, int *outValue)
{
	char *end = nullptr;
	long value;

	if ((text == nullptr) || (text[0] == '\0')) {
		LOGE("%s is empty", name);
		return -1;
	}

	errno = 0;
	value = strtol(text, &end, 10);
	if ((errno != 0) || (end == text) || (*end != '\0')) {
		LOGE("invalid %s: %s", name, text);
		return -1;
	}

	if ((value < minValue) || (value > maxValue)) {
		LOGE("%s out of range: %ld, expect %d-%d", name, value, minValue, maxValue);
		return -1;
	}

	*outValue = (int)value;
	return 0;
}

static int ValidateRtspPath(const char *path)
{
	size_t len;
	size_t i;

	if ((path == nullptr) || (path[0] == '\0')) {
		LOGE("rtsp path is empty");
		return -1;
	}

	if (path[0] != '/') {
		LOGE("rtsp path must start with '/': %s", path);
		return -1;
	}

	len = strlen(path);
	if ((len < 2) || (len > 63)) {
		LOGE("rtsp path length out of range: %u, expect 2-63", (unsigned int)len);
		return -1;
	}

	for (i = 0; i < len; i++) {
		unsigned char c = (unsigned char)path[i];
		if (isalnum(c) || (c == '/') || (c == '_') || (c == '-') || (c == '.')) {
			continue;
		}

		LOGE("rtsp path contains invalid char: %c", path[i]);
		return -1;
	}

	return 0;
}

static int ParseArgs(int argc, char **argv, int *width, int *height, int *fps, int *rtspPort, const char **rtspPath)
{
	*width = 640;
	*height = 480;
	*fps = 30;
	*rtspPort = 554;
	*rtspPath = "/live";

	if ((argc != 1) && (argc != 4) && (argc != 5) && (argc != 6)) {
		LOGE("invalid argument count: %d", argc - 1);
		return -1;
	}

	if (argc >= 4) {
		if (ParseIntArg(argv[1], "width", 16, 3840, width) < 0) {
			return -1;
		}
		if (ParseIntArg(argv[2], "height", 16, 2160, height) < 0) {
			return -1;
		}
		if (ParseIntArg(argv[3], "fps", 1, 60, fps) < 0) {
			return -1;
		}
	}

	if (argc >= 5) {
		if (ParseIntArg(argv[4], "rtsp_port", 1, 65535, rtspPort) < 0) {
			return -1;
		}
	}

	if (argc >= 6) {
		*rtspPath = argv[5];
	}

	if (((*width) % 2 != 0) || ((*height) % 2 != 0)) {
		LOGE("width and height must be even: %dx%d", *width, *height);
		return -1;
	}

	if (ValidateRtspPath(*rtspPath) < 0) {
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	int w;
	int h;
	int fps;
	int rtspPort;
	const char *rtspPath;
	TinyvisonIpcV1 tinyvisonIpcV1;

	if (ParseArgs(argc, argv, &w, &h, &fps, &rtspPort, &rtspPath) < 0) {
		PrintUsage(argv[0]);
		return 1;
	}

	LOGI("start width %d height %d fps %d rtsp_port %d rtsp_path %s", w, h, fps, rtspPort, rtspPath);
	signal(SIGINT, signProc);

	ret = tinyvisonIpcV1.Init(w, h, fps, rtspPort, rtspPath);
	if (ret < 0) {
		LOGE("tinyvisonIpcV1 init failed: %d", ret);
		return 1;
	}

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
