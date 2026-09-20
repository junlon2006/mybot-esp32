/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_PCM_PLAYBACK_BUFFER_H_
#define MYBOT_PCM_PLAYBACK_BUFFER_H_

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

typedef struct mybot_pcm_playback_buffer mybot_pcm_playback_buffer_t;

/* The worker submits up to 240 mono signed-16 frames at 16 kHz. The board sink
 * converts to its native I2S slots and reports frames, including partial writes
 * on timeout. It must honor timeout_ms and must not acquire the lifecycle lock. */
typedef esp_err_t (*mybot_pcm_write_fn)(void *context, const int16_t *pcm, size_t frames,
                                        size_t *written_frames, uint32_t timeout_ms);

/* Lifecycle calls are serialized by the audio driver's mutex. The SDK owns one
 * producer; write() may run concurrently with stop(), but not destroy(). The
 * caller owns the sink context and keeps its I2S channel enabled until stop()
 * succeeds. All current board sinks use 6 x 240 DMA frames (90 ms at 16 kHz). */
mybot_pcm_playback_buffer_t *mybot_pcm_playback_buffer_create(mybot_pcm_write_fn write,
                                                              void *context);
int mybot_pcm_playback_buffer_start(mybot_pcm_playback_buffer_t *buffer);
int mybot_pcm_playback_buffer_write(mybot_pcm_playback_buffer_t *buffer, const void *pcm,
                                    int frames);
int mybot_pcm_playback_buffer_stop(mybot_pcm_playback_buffer_t *buffer);

/* Call after the producer finishes. Includes hardware DMA tail playback. */
int mybot_pcm_playback_buffer_drain(mybot_pcm_playback_buffer_t *buffer, uint32_t timeout_ms);

/* On error, retains all resources: the caller must keep the channel and context. */
int mybot_pcm_playback_buffer_destroy(mybot_pcm_playback_buffer_t *buffer);

/* Implemented by the selected board's audio driver, for finite local prompts. */
int mybot_audio_playback_drain(void *context, uint32_t timeout_ms);

#endif /* MYBOT_PCM_PLAYBACK_BUFFER_H_ */
