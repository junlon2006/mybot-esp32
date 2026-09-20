# CoreS3 audio playback comparison

[English](CORES3_AUDIO_TEST.md) | [简体中文](CORES3_AUDIO_TEST.zh-CN.md)

This offline listening test helps investigate intermittent playback crackle on `m5stack-core-s3`.
It compares timed and continuous producers through the same buffered playback driver, then repeats
continuous playback with microphone capture disabled. All three stages exercise the shared
PCM FIFO and independent I2S writer. The expected result is clean playback in every stage; this
still requires listening on the device.

## Playback path under test

CoreS3 accepts PCM into a bounded 3,840-sample FIFO (7,680 bytes). An independent writer starts
after buffering 1,920 samples (120 ms of audio), or after at most 100 ms from the first sample so
that short clips can play. It feeds I2S continuously in 240-sample blocks (15 ms of audio), with
driver waits pacing output. This separates producer scheduling from hardware feeding.

Normal CoreS3 firmware also uses this path. `CONFIG_MYBOT_AUDIO_PLAYBACK_TEST` only enables the
boot test; it is not a switch for buffered playback. All board profiles use the same PCM buffer,
with board-specific I2S formatting; see [buffered audio playback](AUDIO_PLAYBACK.md). The SDK,
16 kHz sample rate, and codec settings remain unchanged. Buffering adds playback latency: after the offline
test, verify normal conversations, end-of-speech tails, interruption/hangup, and Cloud AEC.

The shared buffer is extracted from the CoreS3 approach that passed real-device testing with
video enabled and disabled. The extracted implementation and other boards still need hardware
regression testing.

## Build and run

Use ESP-IDF v5.5.2 and a separate build directory:

```sh
idf.py -B build/cores3-audio-test \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/cores3-audio-test/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;ci/audio-test.defaults" build
idf.py -B build/cores3-audio-test -p <PORT> flash monitor
```

`CONFIG_MYBOT_AUDIO_PLAYBACK_TEST` defaults to `n`; `ci/audio-test.defaults` enables it and disables
video. The test and video options are mutually exclusive. Enabling the test on another board is
rejected at configuration time. The test runs after
board preparation on every boot, before Wi-Fi or `mybot_start`. Normal provisioning, pairing, and
conversations do not run in this firmware.

Repeat the same build and flash commands in the same build directory after source updates to
retest. The test restores the saved speaker volume from NVS and does not change it. Use the same device,
power supply, volume, and listening position throughout the comparison. No network connection or
server audio is required.

## Listen to all three stages

The built-in pairing prompt and spoken digits 0–9 are decoded once to PCM before testing. Five-ms
fades and 250-ms silence separate clips to avoid abrupt joins. Each stage repeats that same audio
from the beginning, using 16 kHz, mono signed-16 PCM and producer writes of 960 samples (60 ms of
audio; partial writes retry the remaining samples). These are writes into the FIFO, not individual
I2S transactions. Language follows the firmware's prompt language setting.

| Stage | Audio duration | Playback | Microphone |
| --- | --- | --- | --- |
| A | 60 seconds | AOSL MPQ timer every 60 ms, as in the SDK | Captured and discarded |
| B | 60 seconds | Continuous producer; waits for FIFO space | Captured and discarded |
| C | 60 seconds | Continuous producer; waits for FIFO space | Disabled |

Playback workers use the same priority in all stages. A and B capture and discard microphone
audio on a separate 60-ms AOSL timer. Each stage must accept 960,000 samples (60 seconds of audio);
it fails and stops the test if it cannot finish within 75 seconds. Before stopping each stage, the
test drains the FIFO and hardware tail with a 1,000-ms limit; a drain failure also fails the test.
There are three seconds of silence between stages. Brief silence or transients when starting,
stopping, or looping the test audio should be recorded separately from crackle within spoken
audio. Stage changes and statistics every five seconds appear in the serial log. Record the
stage and log timestamp whenever crackle is heard; save the complete boot-to-completion log.
The `phase` labels are `A_timer_capture`, `B_stream_capture`, and `C_stream_only`:

```text
event=progress phase=A_timer_capture written_frames=... write_calls=... short_writes=... zero_writes=... errors=... write_max_us=... gap_max_us=... late_max_us=... capture_frames=... capture_errors=...
event=phase phase=A_timer_capture action=complete elapsed_ms=...
event=test action=complete
```

`action=failed` reports a failed stage or test; do not interpret an incomplete stage as a clean
listening result.

After the last stage, audio resources are cleaned up and the device stays in test mode. It does
not start mybot automatically. Reboot to repeat the test. To restore normal operation, disable
`CONFIG_MYBOT_AUDIO_PLAYBACK_TEST`, rebuild and flash, or flash your regular firmware build.

## Interpret the comparison

- Crackle only in A supports investigating timed feeding and its interaction with DMA playback.
- Crackle in A and B, but not C, supports investigating capture/playback interaction or capture load.
- Crackle in all stages points toward the common playback path, including I2S, clocks, and amplifier.
- No crackle means this test did not reproduce the issue; it does not prove normal playback is safe
  from the issue. This offline test omits SDK, network, and normal application load.

The test's `event=progress written_frames` counts samples accepted into the FIFO. Its write timing
and gap statistics describe the producer, not the I2S writer. Continuous producers can spend
longer waiting for FIFO space than timed producers. Producer writes wait at most 50 ms, so B/C
may report short writes under normal backpressure; the test retries the remaining samples.
Short writes alone do not mean data was lost.

When resource/debug statistics are enabled, `event=playback_stats` measures the I2S writer's
240-sample transactions rather than the producer's 960-sample writes. `event=playback_buffer`
reports the following totals when playback stops:

```text
event=playback_buffer action=stop result=ok admitted_frames=... dma_written_frames=... rebuffer_events=... queue_high_water=... driver_errors=... driver_timeouts=... producer_timeouts=...
```

After a successful test-stage drain, both `admitted_frames` and `dma_written_frames` should be
960,000. They count FIFO acceptance and I2S-driver acceptance respectively. Queued samples,
samples accepted by I2S, and samples physically played are different states. `queue_high_water`
is the maximum queued sample count; `rebuffer_events` can include normal speech boundaries and
is not a DMA-underrun counter. None of these statistics alone proves uninterrupted speaker output.
