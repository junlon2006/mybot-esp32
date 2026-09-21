# Buffered audio playback

[English](AUDIO_PLAYBACK.md) | [简体中文](AUDIO_PLAYBACK.zh-CN.md)

All nine board profiles use one PCM playback buffer through seven board audio drivers. This
keeps the SDK boundary at 16 kHz, mono signed-16 PCM and moves continuous I2S feeding into an
independent task. The shared implementation is
[`pcm_playback_buffer.c`](../components/mybot_platform/src/services/audio/pcm_playback_buffer.c);
each board driver supplies its native I2S sink.

## Why buffer playback

The bundled SDK/RTSA combination uses 960-frame PCM packets at 60 ms. This is the default and
the only packet duration supported by the bundled RTSA library; selecting 20 or 40 ms is rejected
at configuration time until a matching library is supplied. Previously, each driver wrote these packets
directly to I2S; continuity between burst writes and continuous DMA consumption depended on
buffer levels and refill timing. The CoreS3 comparison reproduced crackle with stable timed
feeding but clean continuous feeding; DMA zero insertion and prefetch boundaries were not
measured directly. All boards shared this supply pattern, but audible noise was not established
on every board.

The shared writer accepts up to 3,840 mono frames (7,680 bytes) into a FIFO. Playback starts
at 1,920 buffered frames, or when 100 ms has elapsed from the first queued frame. The latter
is a prebuffer deadline checked when the worker runs, not a guaranteed scheduling latency.
An explicit drain also releases the prebuffer so a short tail can finish. It writes at most
240 frames (15 ms) at a time; the I2S driver's wait for available DMA space paces subsequent
writes. All current sinks use six DMA buffers of 240 frames, totaling 90 ms at 16 kHz.

The FIFO prefers PSRAM and falls back to internal RAM. Each playback stream also owns a
4-KiB task stack, a 480-byte PCM scratch buffer, conversion state, and synchronization objects.
The writer runs at priority 5 without fixed core affinity. Buffering adds latency; verify
conversation responsiveness and Cloud AEC as well as clean audio.

## Board output formats

Frame counts at the SDK and buffer boundary always mean **mono PCM frames**, regardless of
the number or width of I2S slots. Native conversion occurs only in the worker's board sink.

| Board profile | Driver in `src/drivers/audio` | Native playback representation |
| --- | --- | --- |
| `zhengchen-1.54tft-ml307` | `raw_i2s_audio.c` | 32-bit mono; existing software volume scaling |
| `zhengchen-1.54tft-wifi` | `raw_i2s_audio.c` | 32-bit mono; existing software volume scaling |
| `m5stack-core-s3` | `cores3_codec_audio.c` | 16-bit mono in the existing TDM slot layout |
| `m5stack-stick-s3` | `sticks3_es8311_audio.c` | 16-bit stereo; duplicate mono into both slots |
| `sensecap-watcher` | `sensecap_codec_audio.c` | 16-bit stereo; duplicate mono into both slots |
| `esp-vocat` | `vocat_codec_audio.c` | 16-bit stereo; duplicate mono into both slots |
| `esp32-s3-touch-amoled-1.75` | `amoled175_codec_audio.c` | 16-bit stereo; duplicate mono into both slots |
| `esp32-s3-touch-amoled-1.75c` | `amoled175_codec_audio.c` | 16-bit stereo; duplicate mono into both slots |
| `respeaker-flex-xvf3800-circular4-xiao` | `xvf3800_audio.c` | 32-bit stereo; existing volume scaling and slot alignment |

Writes retry unwritten frames after a short write or timeout, preserving order. Invalid byte
counts, non-timeout driver errors, or repeated zero-progress writes stop the stream with an
error. FIFO acceptance and I2S acceptance are different counters; neither proves the speaker
has physically played those frames.

## Stop, drain, and reuse

Stopping cancels queued PCM and waits for the writer's in-flight call before touching hardware.
If that wait fails, resources remain allocated for cleanup retry. Restart resets the FIFO and
prebuffer state, so audio from a previous conversation cannot remain in the software queue.

Codec-based boards mute or close the output before allowing old DMA data to clear. TX clocks
remain active when capture still needs them. Zhengchen and XVF3800 disable the playback channel
and preload silence into DMA before reusing it. The board-specific power, clock, and volume
behavior remains in each driver.

Wi-Fi provisioning prompts and the offline test use a separate drain operation before stopping,
allowing the FIFO and hardware tail to finish. Provisioning prompts feed this same FIFO in
320-frame chunks, independently of the SDK's 960-frame packets. Drain includes 120 ms after the last accepted I2S write, covering the
current 90-ms DMA capacity; this is a conservative timing bound, not a hardware completion event.

## Diagnostics and validation

Normal `event=playback_buffer` logs report start configuration, write failures, and stop totals
on every board. `CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR` defaults to off. Enabling it adds per-core
CPU estimates, internal/PSRAM heap statistics, and `event=playback_stats`/`event=playback_gaps`.
The default interval is 5,000 ms, controlled by `CONFIG_MYBOT_DEBUG_RESOURCE_MONITOR_INTERVAL_MS`.
Playback counters cover each report interval and describe the worker's I2S writes rather than
SDK packet delivery. The removed renderer's `event=ui_stats` is no longer emitted.
`rebuffer_events` estimates when playback needs buffering again and may include speech pauses.
It is not a DMA-underrun counter.

CoreS3 real-device testing passed all three offline stages after buffering was added; normal
conversations also passed with video enabled and disabled. That is the recorded hardware baseline.
The subsequent shared-buffer extraction and platform/UI changes still need per-board regression;
host simulations and firmware builds cannot establish their audible result. The
[offline comparison](CORES3_AUDIO_TEST.md) remains CoreS3-only.

For each additional board, test continuous conversation with capture active, first-boot and
button-triggered Wi-Fi prompts, pairing-code tails, repeated hangup/restart, volume persistence,
and startup/cleanup errors. Check that stopping playback preserves capture, previous audio is
not replayed on restart, and repeated sessions do not grow memory usage. Retest Cloud AEC and
response latency because buffering changes the delay between received PCM and speaker output.
