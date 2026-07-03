# 项目整理说明

## 项目定位

这是一个面向 TinyVision / Allwinner V851/V853 类平台的 IPC 摄像头推流 demo。它不是通用 PC 程序，依赖目标板上的：

- V4L2 摄像头节点 `/dev/video0`
- Allwinner ISP 头文件和库
- Allwinner `vencoder` / `MemAdapter` / `cdc_base` 等硬件编码库
- Linux socket 与 RTSP/RTP 网络能力

## 模块关系

### 入口和主控

- `main.cpp`
  - 默认分辨率 `640x480`
  - 默认帧率 `30`
  - 接收 `SIGINT` 后停止

- `tinyvisonIpcV1.cpp`
  - 初始化编码器和 RTSP 服务
  - 启动 `YuvToH264Encoder` 与 `RtspServer`
  - 将编码队列中的 `H264Data` 转交给 RTSP 队列

### 摄像头采集

- `yuvReader.cpp`
  - 打开 `/dev/video0`
  - 设置 `V4L2_PIX_FMT_NV21`
  - 使用 `V4L2_MEMORY_MMAP`
  - 请求 2 个采集 buffer
  - 使用 `VIDIOC_DQBUF` / `VIDIOC_QBUF` 循环取帧

### H.264 编码

- `H264Encoder.cpp`
  - 设置 H.264 High Profile
  - 输入格式为 `VENC_PIXEL_YVU420SP`，对应 NV21
  - 根据分辨率选择默认码率
  - 从编码器取出 bitstream，并复制成 `malloc` 出来的连续缓冲区

### RTSP 推流

- `rtspServer.cpp`
  - 创建 RTSP 服务，默认端口 `554`
  - 创建 `/live` session
  - 给每个 H.264 包前追加 AUD NAL
  - 调用第三方 `rtsp_sever_tx_video`

- `3rd/rtsp_lib`
  - 负责 RTSP 交互、SDP、RTP 分包、TCP/UDP 发送
  - `rtsp_sever_tx_video` 内部会调用 `rtsp_do_event`

## 当前目录中的非源码产物

这些文件通常不建议纳入源码管理：

- `tinyvisionIpcV1`
- `*.o`
- `*.a`
- `Makefile`
- `cmake_install.cmake`
- `CMakeFiles/`
- `*.log`
- `*.vcxproj.user`

本次整理已新增 `.gitignore`，后续如果使用 Git，可以避免重复混入这些产物。

## 已发现的关键问题

### 1. 轮询带来的固定延迟

多个线程中存在 `IThreadMSleep(10)`：

- `tinyvisonIpcV1.cpp`
- `yuvToH264Encoder.cpp`
- `rtspServer.cpp`

在 30 FPS 下，一帧间隔约 33ms。多个 10ms 轮询叠加后，很容易形成可感知延迟和抖动。

### 2. 队列可能积压

`ThreadSafeQueue` 没有容量上限。只要 RTSP 发送慢于编码产出，队列就会持续增长，最终表现为画面越来越滞后。

### 3. H264Data 所有权不清晰

`H264Data` 使用裸指针：

```cpp
uint8_t *buf;
size_t len;
```

实际释放发生在 RTSP 线程中。如果异常退出、队列析构或中途丢帧处理不完整，容易内存泄漏。

### 4. SPS/PPS 处理不利于快速起播

RTSP session 初始化时没有传入 SPS/PPS，编码线程只在启动时入队一次 SPS/PPS。新客户端如果错过这段数据，可能需要等待 IDR 或解析后续帧，起播变慢。

### 5. FPS 统计变量

`yuvToH264Encoder.cpp` 中 FPS 统计变量已经在本次整理中初始化：

```cpp
int t0 = time_ms_get();
int t1 = 0;
int camera_cnt = 0;
int h264_cnt = 0;
```

## 推荐整理路线

第一阶段，低风险：

- 添加 RTSP 队列长度限制，满队列时丢旧帧
- 将关键配置整理成宏或配置结构体
- 继续清理明确的内存所有权边界

第二阶段，降低延迟：

- 将 `ThreadSafeQueue` 改成带条件变量的阻塞队列
- 移除 `TinyvisonIpcV1` 中的中转轮询
- IDR 帧前补 SPS/PPS
- 客户端连接时触发 IDR

第三阶段，性能优化：

- 研究 V4L2 DMABUF/ION 到 vencoder 的零拷贝
- 减少 H.264 输出后的重复 `malloc` / `memcpy`
- 把 CMake 中的 SDK 路径改成可配置 toolchain file
