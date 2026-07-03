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
| `main.cpp` | 程序入口，解析并校验分辨率/FPS/RTSP 端口/路径参数，启动主 IPC 线程 |
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

指定 RTSP 端口和路径：

```bash
./tinyvisionIpcV1 1280 720 30 8554 /live
```

参数范围：宽度 16-3840 且为偶数，高度 16-2160 且为偶数，FPS 1-60，端口 1-65535。RTSP 路径必须以 `/` 开头，例如 `/live` 或 `/live/main`。

## 修复与性能优化 TODO

约束：后续修复和优化只修改本项目业务代码，不修改 `3rd/` 和 `utils/` 目录下的第三方库与工具库。

- [x] 整理项目说明、构建说明、参考致谢和优化路线。
- [x] 清理源码目录中的历史编译产物。
- [x] 初始化 `yuvToH264Encoder.cpp` 中的 FPS 统计变量。
- [x] 修正 `H264Encoder::GetSpsPps` 的空指针判断。
- [x] 释放 `H264Encoder` 中 `pEncContext` 的生命周期资源。
- [x] 给 `RtspServer` 的 H.264 队列增加长度上限，发送慢时丢旧帧，避免延迟越积越大。
- [x] 在 IDR 帧前补发 SPS/PPS，提升新客户端起播成功率和起播速度。
- [x] 减少主控层中转开销，让编码输出尽快进入 RTSP 发送链路。
- [x] 减少 H.264 输出与 RTSP 发送之间的重复 `malloc` / `memcpy`。
- [x] 调整编码参数：合理降低 GOP、确认无 B 帧/重排序、收窄低延迟场景下的 VBV。
- [x] 增加运行参数校验，例如分辨率、FPS、端口、RTSP 路径。
- [ ] 将 Tina SDK 路径从 `CMakeLists.txt` 拆到可配置 toolchain 文件或 CMake 变量。
- [ ] 评估 V4L2 DMABUF/ION 到 vencoder 的零拷贝路径。

## 参考与致谢

本项目是在学习和参考全志在线论坛帖子《使用 TinyVision 制作简单的网络摄像机 IPC》的基础上整理和修改而来：

<https://bbs.aw-ol.com/topic/5484/%E4%BD%BF%E7%94%A8tinyvision%E5%88%B6%E4%BD%9C%E7%AE%80%E5%8D%95%E7%9A%84%E7%BD%91%E7%BB%9C%E6%91%84%E5%83%8F%E6%9C%BAipc?lang=en-x-pirate>

感谢原帖作者和全志在线社区贡献者公开分享 TinyVision IPC 示例思路、V4L2 摄像头采集、Allwinner 硬件 H.264 编码以及 RTSP 推流相关经验。
