cmake_minimum_required (VERSION 3.5)

add_library(
	rtsp_lib

	our_md5.c
	rtp_enc.c
	rtsp_demo.c
	rtsp_msg.c
	stream_queue.c
	utils.c
	test.c
	authenticate.c
)

if(WIN32)
    target_link_libraries(rtsp_lib ws2_32.lib)
    target_link_libraries(rtsp_lib winmm.lib)
endif()
