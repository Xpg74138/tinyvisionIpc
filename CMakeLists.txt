cmake_minimum_required(VERSION 3.5)

project("tinyvisionIpcV1")

SET(CROSS_COMPILE 1)
SET(CMAKE_SYSTEM_NAME Linux)

SET(CMAKE_C_COMPILER "/root/tina-v853-open/prebuilt/rootfsbuilt/arm/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-gcc")
SET(CMAKE_CXX_COMPILER "/root/tina-v853-open/prebuilt/rootfsbuilt/arm/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-g++")
SET(CMAKE_SYSROOT "/root/tina-v853-open/prebuilt/rootfsbuilt/arm/toolchain-sunxi-musl-gcc-830")

add_definitions(-D__LINUX__)

include_directories("/root/tina-v853-open/out/v851se/tinyvision/openwrt/staging_dir/target/usr/include")
include_directories("/root/tina-v853-open/out/v851se/tinyvision/openwrt/staging_dir/target/usr/include/allwinner/include")
include_directories("/root/tina-v853-open/out/v851se/tinyvision/openwrt/staging_dir/usr/include/allwinner/include")
include_directories("/root/tina-v853-open/out/v851se/tinyvision/openwrt/staging_dir/usr/include/allwinner")

include_directories("./utils")
include_directories("./utils/IThread")
include_directories("./utils/time_elaps")
include_directories("./3rd/rtsp_lib")

link_directories(".")
link_directories("/root/tina-v853-open/out/v851se/tinyvision/openwrt/staging_dir/target/usr/lib")
#link_directories("/root/tina-v853-open/out/v851se/tinyvision/openwrt/staging_dir/target/root-v851se-tinyvision/usr/lib")

#link_directories("/root/tina-v853-open/out/v851se/tinyvision/openwrt/staging_dir/target/usr/lib")
add_subdirectory("utils/IThread")
add_subdirectory("utils/time_elaps")
add_subdirectory("3rd/rtsp_lib")

add_executable(tinyvisionIpcV1
	main.cpp
	tinyvisonIpcV1.h
	tinyvisonIpcV1.cpp
	yuvToH264Encoder.h
	yuvToH264Encoder.cpp
	yuvReader.h
	yuvReader.cpp
	H264Encoder.h
	H264Encoder.cpp
	rtspServer.h
	rtspServer.cpp
)

target_link_libraries(tinyvisionIpcV1 AWIspApi)
target_link_libraries(tinyvisionIpcV1 vencoder)
target_link_libraries(tinyvisionIpcV1 MemAdapter)
target_link_libraries(tinyvisionIpcV1 venc_base)
target_link_libraries(tinyvisionIpcV1 cdc_base)

target_link_libraries(tinyvisionIpcV1 IThread)
target_link_libraries(tinyvisionIpcV1 time_elaps)
target_link_libraries(tinyvisionIpcV1 rtsp_lib)
