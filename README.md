# tinyvisionIpc

TinyVision / Allwinner 平台上的简易 IPC 网络摄像机示例程序。

程序从 V4L2 摄像头采集 NV21(YUV) 数据，送入 Allwinner `vencoder` 硬件编码为 H.264，再通过内置 RTSP 服务对外推流。

## 数据链路

```text
/dev/video0
  -> YuvReader
  -> H264Encoder
  -> YuvToH264Encoder queue
  -> TinyvisonIpcV1 relay
  -> RtspServer queue
  -> RTSP/RTP client
```

默认 RTSP 地址：

```text
rtsp://<device-ip>/live
```

## 主要文件

| 路径 | 作用 |
| --- | --- |
| `main.cpp` | 程序入口，解析分辨率/FPS 参数，启动主 IPC 线程 |
| `tinyvisonIpcV1.*` | 应用主控，连接编码线程和 RTSP 线程 |
| `yuvReader.*` | V4L2 摄像头采集，默认 `/dev/video0`、NV21、MMAP |
| `H264Encoder.*` | Allwinner `vencoder` H.264 硬件编码封装 |
| `yuvToH264Encoder.*` | 采集 YUV、编码 H.264、输出编码包队列 |
| `rtspServer.*` | RTSP 服务封装，将 H.264 包发送给客户端 |
| `3rd/rtsp_lib/` | 第三方 RTSP/RTP 实现 |
| `utils/IThread/` | 简单 C++ 线程封装 |
| `utils/time_elaps/` | 毫秒/微秒计时工具 |
| `ion_alloc.*` | ION 内存分配相关工具，当前主流程未完全启用 |

## 构建说明

当前 `CMakeLists.txt` 中硬编码了 Tina/OpenWrt/Allwinner SDK 路径，例如：

```cmake
/root/tina-v853-open/...
```

所以构建环境需要与原开发环境一致，或者先把这些路径整理成可配置变量/独立 toolchain file。

推荐后续构建目录：

```bash
cmake -S . -B build
cmake --build build
```

## 运行方式

默认运行：

```bash
./tinyvisionIpcV1
```

指定宽高和 FPS：

```bash
./tinyvisionIpcV1 640 480 30
```

## 低延迟优先优化

优先级从高到低：

1. 将队列改为有界队列，RTSP 发送慢时丢旧帧，避免延迟持续累积。
2. 用条件变量替代多个 `IThreadMSleep(10)` 轮询。
3. 减少中转线程和中间队列，编码输出后尽快进入 RTSP 发送。
4. 每个 IDR 前补 SPS/PPS，或者在 RTSP SDP 中设置 SPS/PPS，降低新客户端起播等待。
5. 研究 V4L2 DMABUF/ION 到 vencoder 的零拷贝，减少 YUV 帧 memcpy。
6. 调整编码器低延迟参数：无 B 帧、较小 VBV、合理 GOP、连接时强制 IDR。

## 参考与致谢

本项目是在学习和参考全志在线论坛帖子《使用 TinyVision 制作简单的网络摄像机 IPC》的基础上整理和修改而来：

<https://bbs.aw-ol.com/topic/5484/%E4%BD%BF%E7%94%A8tinyvision%E5%88%B6%E4%BD%9C%E7%AE%80%E5%8D%95%E7%9A%84%E7%BD%91%E7%BB%9C%E6%91%84%E5%83%8F%E6%9C%BAipc?lang=en-x-pirate>

感谢原帖作者和全志在线社区贡献者公开分享 TinyVision IPC 示例思路、V4L2 摄像头采集、Allwinner 硬件 H.264 编码以及 RTSP 推流相关经验。

