# CoreS3 音频播放对照测试

[English](CORES3_AUDIO_TEST.md) | [简体中文](CORES3_AUDIO_TEST.zh-CN.md)

此离线听音测试用于排查 `m5stack-core-s3` 偶发的播放沙沙声。测试对比定时供数与连续供数，
再关闭麦克风采集重复连续播放。三个阶段均经过同一套公共 PCM FIFO 和独立 I2S
播放任务，预期三个阶段都无杂音，仍需真机试听确认。实现位于
[`cores3_playback_test.c`](../components/mybot_platform/src/drivers/audio/cores3_playback_test.c)。

## 当前测试的播放路径

CoreS3 将 PCM 接收到有界 FIFO，容量为 3,840 个采样点（7,680 字节）。独立播放任务
预缓冲 1,920 个采样点，即 120 ms 音频后开始输出；短音频达到 100 ms 预缓冲期限
也会启动，实际唤醒时间仍受任务调度影响。随后每次向 I2S 连续写入最多 240 个采样点，
即 15 ms 音频，由驱动等待
控制输出节奏，避免供数任务的调度节奏直接影响硬件供数。

普通 CoreS3 固件同样使用这条路径。`CONFIG_MYBOT_AUDIO_PLAYBACK_TEST` 只控制上电测试，
不是播放缓冲的开关。所有板型均使用这套 PCM 缓冲，各板型负责自己的 I2S 格式转换，
详见[音频播放缓冲](AUDIO_PLAYBACK.zh-CN.md)。SDK、16 kHz 采样率及 codec 配置保持不变。
预缓冲会增加播放延迟：离线测试后还需验证普通对话、尾音、打断/挂断及 Cloud AEC。

增加缓冲后的三个阶段已通过 CoreS3 真机听音测试，普通对话在视频开启、关闭两种情况下
也已验证正常。后续公共缓冲抽取及平台/UI 变化仍需回归，也不能替代其他板型的真机验证。

## 构建与运行

激活 ESP-IDF v5.5.2 环境后，在仓库根目录使用独立构建目录：

```sh
idf.py -B build/cores3-audio-test \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/cores3-audio-test/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/audio-test.defaults" build
idf.py -B build/cores3-audio-test -p <PORT> flash monitor
```

`CONFIG_MYBOT_AUDIO_PLAYBACK_TEST` 默认关闭，`ci/audio-test.defaults` 开启测试并关闭视频，
测试选项与视频选项互斥。其他板型开启此选项会在配置时被拒绝。每次上电完成板级准备后、
启动 Wi-Fi 和 `mybot_start` 之前执行测试；此固件不运行正常的配网、配对和语音交互流程。
全新构建默认使用中文提示音。测试英文时，在新的构建目录中给 `SDKCONFIG_DEFAULTS`
追加 `;ci/en-us.defaults`，或执行 `idf.py -B build/cores3-audio-test menuconfig`
修改 `mybot → Product language`。

源码更新后，可在同一构建目录重复上述构建和烧录命令复测。测试使用 NVS 保存的音量，
没有有效记录时使用驱动默认值 70，不调用音量设置接口；常规驱动可能清理无效 NVS 记录。
整个对照过程保持同一设备、电源、音量和听音位置，
无需联网或服务端音频。

## 依次试听三个阶段

测试前将内置配对提示语和数字 0～9 一次性解码为 PCM，并在各片段边界加入 5 ms 淡入淡出
和 250 ms 静音，避免直接拼接产生爆音。每阶段从头循环同一段音频，格式均为 16 kHz、
单声道 signed-16 PCM，供数端每次写入 960 个采样点，即 60 ms 音频；部分写入时重试
剩余采样。这里写入的是 FIFO，不是单次 I2S 事务。60 ms 节奏与当前随附 RTSA 包一致；
20/40 ms 固件配置需要另行提供匹配的库。提示音语言跟随固件的提示音语言配置。

| 阶段 | 音频时长 | 播放方式 | 麦克风 |
| --- | --- | --- | --- |
| A | 60 秒 | 与 SDK 相同的 AOSL MPQ 定时器，每 60 ms 写入一次 | 持续采集并丢弃 |
| B | 60 秒 | 连续供数，等待 FIFO 空间 | 持续采集并丢弃 |
| C | 60 秒 | 连续供数，等待 FIFO 空间 | 关闭 |

三个阶段的播放任务优先级相同。A、B 使用独立的 AOSL 60 ms 定时器读取并丢弃麦克风音频。
每阶段需完成 960,000 个采样点，即 60 秒音频；超过 75 秒未完成则判定失败并停止测试。
每阶段停止前还需等待 FIFO 和硬件尾音排空，上限为 1,000 ms；排空失败也会判定测试失败。
阶段之间静音 3 秒。开始、结束和音频循环边界的短暂停顿或瞬态声音，应与语音中途的沙沙声
分开记录。串口会打印阶段切换，以及每 5 秒一次的统计。听到杂音时记录所在阶段和对应日志
时间，保留从上电到测试结束的完整日志。日志中的阶段名分别为 `A_timer_capture`、
`B_stream_capture`、`C_stream_only`：

```text
event=progress phase=A_timer_capture written_frames=... write_calls=... short_writes=... zero_writes=... errors=... write_max_us=... gap_max_us=... late_max_us=... capture_frames=... capture_errors=...
event=phase phase=A_timer_capture action=complete elapsed_ms=...
event=test action=complete
```

`action=failed` 表示该阶段或整个测试失败，不能将未完成的阶段视为播放正常。

最后一个阶段结束后清理音频资源，设备停留在测试模式，不自动启动 mybot。重启可重复测试。
恢复正常功能时，关闭 `CONFIG_MYBOT_AUDIO_PLAYBACK_TEST` 后重新编译烧录，或烧录原有的
普通固件。

## 如何解读结果

- 仅 A 有杂音：支持优先检查定时供数与 DMA 播放衔接。
- A、B 有杂音而 C 没有：支持优先检查同时采集、播放时的相互影响或采集负载。
- 三个阶段都有杂音：重点检查共同播放链路，包括 I2S、时钟与功放。
- 都没有杂音：仅说明本次未复现，不能证明正常交互没有问题；离线测试不包含 SDK、网络和
  正常应用负载。

测试 `event=progress` 中的 `written_frames` 统计 FIFO 接受的采样数；写入耗时和间隔
描述的是供数端，不是 I2S 播放任务。连续供数等待 FIFO 空间的时间可以明显长于定时供数，
单次最多等待 50 ms，因此 B/C 在正常背压下可能出现短写，测试会重试剩余采样。
短写本身不代表音频丢失。

`CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR` 默认关闭，`ci/audio-test.defaults` 不会开启它；
测试自身的 `event=progress` 日志不依赖此开关。开启后，`event=playback_stats` 测量独立
播放任务每次最多 240 个采样点的 I2S 写入，不对应供数端的 960 个采样点。
停止播放时，`event=playback_buffer` 汇总如下：

```text
event=playback_buffer action=stop result=ok admitted_frames=... dma_written_frames=... rebuffer_events=... queue_high_water=... driver_errors=... driver_timeouts=... producer_timeouts=...
```

测试阶段成功排空后，`admitted_frames` 与 `dma_written_frames` 都应为 960,000，分别
表示 FIFO 接受与 I2S 驱动接受的采样数。入队、I2S 接受和扬声器实际播放是不同状态。
`queue_high_water` 是队列最高采样数；`rebuffer_events` 也可能包括正常语音边界的重新
缓冲，不是真正的 DMA 断供计数。这些统计均不能单独证明扬声器输出连续。
