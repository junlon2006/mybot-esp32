# CoreS3 video uplink

[English](CORES3_VIDEO.md) | [简体中文](CORES3_VIDEO.zh-CN.md)

The optional `m5stack-core-s3` camera adapter captures GC0308 QVGA YUYV frames and encodes complete
JPEG images for the mybot public video callback. Uplink is capped at one frame per second, including
when RTC rejects a frame. Sensor capture runs at 20 fps; intermediate frames are discarded, and
only selected frames are encoded. Slow sending does not create an unbounded queue or catch-up burst.

## Build and enable

Use ESP-IDF v5.5.2 and an isolated configuration:

```sh
idf.py -B build/cores3-video \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/cores3-video/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/video.defaults" build
idf.py -B build/cores3-video -p <PORT> flash monitor
```

For an existing CoreS3 build, enable `mybot → Enable CoreS3 camera JPEG uplink (1 fps)` in
menuconfig for that build directory. `CONFIG_MYBOT_ENABLE_VIDEO` defaults to `n`. Other boards
reject the enabled configuration. CoreS3-SE has no GC0308 and is not a supported video target.

## Hardware and memory

| Signal | Wiring |
| --- | --- |
| SCCB | Shared native I2C1, SDA12/SCL11, GC0308 address `0x21`, 100 kHz |
| D0–D7 | GPIO39/40/41/42/15/16/48/47 |
| VSYNC/HREF/PCLK | GPIO46/38/45 |
| XCLK | External 20 MHz oscillator, no GPIO output |
| Reset | AW9523 P1_0; locked masked writes preserve LCD and audio reset lines |

Two 153,600-byte capture buffers and one 131,072-byte JPEG buffer use about 428 KiB of PSRAM.
The encoder also allocates workspace. The DVP driver reserves 32 KiB of internal DMA memory plus
descriptors and a capture task; the encoding worker has a 6 KiB stack. Check internal free memory
and the largest free block during a call, not only total PSRAM. Existing UI caches remain enabled.

The JPEG worker uses priority 3 and no additional encoder helper task. The DVP copy task retains
the upstream driver's higher priority for timely DMA service. This path does not use the audio
I2S peripheral. The LCD continues to show workflow/status pages; no camera preview is rendered.

## Lifecycle and bandwidth

Initialization allocates resources but does not start DVP capture or invoke the SDK frame callback.
After RTC connects, `start` schedules the worker. JPEG buffers remain unchanged until the send
callback returns. Capture dequeues have a 100 ms timeout. Stop waits up to 3 seconds for the worker
and in-flight callback; if still pending, it retains resources and reports failure for SDK retry.
Only a fully stopped source is destroyed. Camera teardown never deletes the Board-owned I2C bus.

The advertised bitrate range is 64–512 kbit/s, starting at 288 kbit/s. JPEG quality starts at 60;
bandwidth feedback changes quality within 30–60. Frames exceeding the current one-second byte
budget or the fixed output buffer are dropped. Therefore poor bandwidth or complex images may
result in fewer than one delivered frame per second. SDK acceptance does not prove server inference.

## Diagnostics and acceptance

`cores3_video` emits lifecycle logs and `event=video_stats` every 10 seconds during streaming:
`encoded`, `sent`, `rejected`, `dropped`, `capture_errors`, `last_bytes`, `quality`, `target_bps`,
and `encode_max_us`. Counters reset each interval. `sent` means RTSA accepted the SDK callback.

Validate GC0308 detection, correct colors/orientation, server reception/inference, send spacing,
JPEG quality, CPU/heap use, and audio continuity. Exercise repeated conversation start/stop,
Wi-Fi loss, long-touch provisioning, bitrate reduction, and missing camera. Confirm no frames after
stop and stable memory after repeated cycles. Both Chinese/English video builds are covered in CI;
hardware and server acceptance must be recorded separately.
