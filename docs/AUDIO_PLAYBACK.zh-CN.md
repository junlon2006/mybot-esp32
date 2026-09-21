# 音频播放缓冲

[English](AUDIO_PLAYBACK.md) | [简体中文](AUDIO_PLAYBACK.zh-CN.md)

当前 9 个 Board profile 通过 7 个板级音频驱动共用同一套 PCM 播放缓冲。SDK 边界保持
16 kHz、单声道 signed-16 PCM，独立任务持续向 I2S 供数。公共实现位于
[`pcm_playback_buffer.c`](../components/mybot_platform/src/services/audio/pcm_playback_buffer.c)，
各板级驱动提供原生 I2S sink。

## 为什么增加缓冲

当前配套 SDK/RTSA 使用 60 ms、960 帧的 PCM 数据包。这既是默认值，也是随附 RTSA 库
唯一支持的帧长；没有更换匹配的库时，选择 20 或 40 ms 会在配置阶段被拒绝。
此前各驱动直接将这些数据包写入 I2S，突发补数和 DMA
连续消费之间缺少独立缓冲，输出连续性依赖缓冲水位及填充时机。CoreS3 对照测试中，
定时供数稳定时仍有杂音，连续供数正常；具体 DMA 插零及预取边界未直接测量。各板型
都采用这一供数方式，因此存在相同风险，但不能据此认定所有板型均有杂音。

公共模块的 FIFO 容量为 3,840 个单声道帧（7,680 字节）。累计 1,920 帧后启动播放；
首个入队帧到达后经过 100 ms 也会启动。这是播放任务执行时检查的预缓冲期限，
不是任务调度延迟的保证；显式排空也会解除预缓冲，让短尾音完成。
每次最多向 I2S 写入 240 帧（15 ms），
通过 I2S 驱动等待 DMA 空间控制后续写入节奏。各驱动均使用 6 个 240 帧的 DMA 缓冲，
在 16 kHz 下总容量为 90 ms。

FIFO 优先使用 PSRAM，失败时回退到内部 RAM。每个播放流还需要 4 KiB 任务栈、
480 字节 PCM 暂存区、格式转换状态及同步对象。播放任务优先级为 5，不固定 CPU 核心。
缓冲会增加延迟，除了试听音质，还需验证交互响应和 Cloud AEC。

## 各板型的输出格式

SDK 和缓冲接口中的帧数始终表示**单声道 PCM 帧数**，不随 I2S 槽数量、位宽改变。
独立播放任务调用各板型的 sink 完成原生格式转换。

| Board profile | `src/drivers/audio` 下的驱动 | 原生播放表示 |
| --- | --- | --- |
| `zhengchen-1.54tft-ml307` | `raw_i2s_audio.c` | 32 位单声道，保留软件音量缩放 |
| `zhengchen-1.54tft-wifi` | `raw_i2s_audio.c` | 32 位单声道，保留软件音量缩放 |
| `m5stack-core-s3` | `cores3_codec_audio.c` | 16 位单声道，保留原 TDM 槽布局 |
| `m5stack-stick-s3` | `sticks3_es8311_audio.c` | 16 位双声道，将单声道复制到两个槽 |
| `sensecap-watcher` | `sensecap_codec_audio.c` | 16 位双声道，将单声道复制到两个槽 |
| `esp-vocat` | `vocat_codec_audio.c` | 16 位双声道，将单声道复制到两个槽 |
| `esp32-s3-touch-amoled-1.75` | `amoled175_codec_audio.c` | 16 位双声道，将单声道复制到两个槽 |
| `esp32-s3-touch-amoled-1.75c` | `amoled175_codec_audio.c` | 16 位双声道，将单声道复制到两个槽 |
| `respeaker-flex-xvf3800-circular4-xiao` | `xvf3800_audio.c` | 32 位双声道，保留音量缩放及槽内对齐 |

短写或超时后继续重试剩余帧，保持 PCM 顺序。返回非法字节数、非超时驱动错误，或连续
多次写入没有进展时，播放流报错停止。FIFO 接受与 I2S 接受是不同的计数，两者都不能
单独证明扬声器已经实际播放对应帧。

## 停止、排空与复用

停止时取消 FIFO 中尚未播放的数据，等待播放任务结束正在执行的写入，再处理硬件。
等待失败时保留资源，允许重试清理。重新启动会重置 FIFO 和预缓冲状态，避免上一轮
对话音频残留在软件队列中。

使用 codec 的板型先静音或关闭输出，再等待旧 DMA 数据清除；如果采集仍需要 TX 时钟，
则保留该时钟。征辰和 XVF3800 停用播放通道并向 DMA 预装静音后再复用。电源、时钟、
音量行为仍由板级驱动维护。

配网提示音及离线测试停止前使用单独的排空操作，等待 FIFO 和硬件尾音完成。
配网提示音以 320 帧为一块写入同一 FIFO，不经过 SDK 的 960 帧供数节奏。最后一次
I2S 接受数据后额外等待 120 ms，覆盖当前 90 ms 的 DMA 容量。这是保守时间界限，
不是硬件播放完成事件。

## 日志与验证

所有板型均有正式的 `event=playback_buffer` 启动配置、写入错误和停止汇总日志。
`CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR` 默认关闭，开启后打印每核 CPU 估算、内部/PSRAM
堆统计，以及 `event=playback_stats` 和 `event=playback_gaps`。默认周期为 5,000 ms，
由 `CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR_INTERVAL_MS` 调整。播放计数按报告周期汇总，
描述独立任务的 I2S 写入，而非 SDK 数据包供数。旧渲染器的 `event=ui_stats` 已不再输出。
`rebuffer_events` 估算重新缓冲的次数，
可能包含正常语音停顿，不是 DMA 断供计数。

CoreS3 增加缓冲后，三个离线阶段均已通过真机听音测试，普通对话在视频开启、关闭两种
情况下也已验证正常。这是已记录的硬件验证基线；后续公共缓冲抽取及平台/UI 变化仍需
逐板回归，主机模拟及固件编译不能证明实际听音结果。
[离线对照测试](CORES3_AUDIO_TEST.zh-CN.md)仍仅支持 CoreS3。

其他板型应逐一验证：持续采集时的连续对话、首次启动及按键触发的配网提示音、配对码尾音、
反复挂断/重启对话、音量持久化及启动/清理失败场景。确认停止播放不影响采集、重新开始
不会重放上一轮音频、多轮会话后内存不持续增长。由于接收 PCM 到扬声器输出的延迟变化，
还需复测 Cloud AEC 和响应延迟。
