/* SPDX-License-Identifier: Apache-2.0 */
#include "announcement/embedded_ogg_prompt.h"

#include "announcement/ogg_opus_decoder.h"
#include "audio/pcm_playback_buffer.h"

#include "esp_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAG "mybot_prompt"
#define PROMPT_FRAMES_PER_WRITE 320
#define PROMPT_MAX_CONSECUTIVE_TIMEOUTS 100
#define PROMPT_DRAIN_TIMEOUT_MS 1000
#define PROMPT_RATE_HZ 16000
#define PROMPT_CHANNELS 1
#define PROMPT_BITS 16

extern const uint8_t wificonfig_start[] asm("_binary_wificonfig_ogg_start");
extern const uint8_t wificonfig_end[] asm("_binary_wificonfig_ogg_end");

static bool playback_ops_valid(const mybot_audio_playback_ops_t *ops) {
    return ops && ops->init && ops->start && ops->write && ops->stop && ops->destroy;
}

static bool volume_ops_valid(const mybot_audio_volume_ops_t *ops) {
    return ops && ops->init && ops->destroy;
}

int mybot_embedded_ogg_play_wifi_provisioning(const mybot_audio_playback_ops_t *playback_ops,
                                              const mybot_audio_volume_ops_t *volume_ops,
                                              const char *trigger) {
    if (!playback_ops_valid(playback_ops)) {
        return -1;
    }
    if (!trigger) {
        trigger = "unknown";
    }

    ESP_LOGI(TAG,
             "event=prompt type=wifi_provisioning trigger=%s action=play phase=begin language=%s",
             trigger, CONFIG_MYBOT_LANGUAGE_TAG);

    mybot_ogg_pcm_t decoded = {0};
    size_t asset_size = (size_t)(wificonfig_end - wificonfig_start);
    if (mybot_ogg_opus_decode(wificonfig_start, asset_size, &decoded) < 0) {
        return -1;
    }

    void *playback_ctx = NULL;
    void *volume_ctx = NULL;
    bool playback_started = false;
    bool volume_active = false;
    int result = -1;

    if (playback_ops->init(&playback_ctx, PROMPT_RATE_HZ, PROMPT_CHANNELS, PROMPT_BITS) < 0) {
        ESP_LOGE(TAG, "event=prompt type=wifi_provisioning action=play result=error "
                      "reason=playback_initialize");
        goto cleanup;
    }
    if (volume_ops_valid(volume_ops)) {
        if (volume_ops->init(&volume_ctx) < 0) {
            ESP_LOGE(TAG, "event=prompt type=wifi_provisioning action=play result=error "
                          "reason=volume_initialize");
            goto cleanup;
        }
        volume_active = true;
    }
    if (playback_ops->start(playback_ctx) < 0) {
        ESP_LOGE(TAG, "event=prompt type=wifi_provisioning action=play result=error "
                      "reason=playback_start");
        goto cleanup;
    }
    playback_started = true;

    int offset = 0;
    int consecutive_timeouts = 0;
    while (offset < decoded.frames) {
        int frames = decoded.frames - offset;
        if (frames > PROMPT_FRAMES_PER_WRITE) {
            frames = PROMPT_FRAMES_PER_WRITE;
        }
        int written = playback_ops->write(playback_ctx, decoded.pcm + offset, frames);
        if (written < 0 || written > frames) {
            ESP_LOGE(TAG,
                     "event=prompt type=wifi_provisioning action=play result=error reason=write "
                     "offset=%d frames=%d",
                     offset, decoded.frames);
            goto cleanup;
        }
        if (written == 0) {
            if (++consecutive_timeouts >= PROMPT_MAX_CONSECUTIVE_TIMEOUTS) {
                ESP_LOGE(TAG,
                         "event=prompt type=wifi_provisioning action=play result=error "
                         "reason=write_timeout offset=%d frames=%d",
                         offset, decoded.frames);
                goto cleanup;
            }
            continue;
        }
        consecutive_timeouts = 0;
        offset += written;
    }

    if (mybot_audio_playback_drain(playback_ctx, PROMPT_DRAIN_TIMEOUT_MS) < 0) {
        ESP_LOGE(TAG, "event=prompt type=wifi_provisioning action=play result=error reason=drain");
        goto cleanup;
    }
    result = 0;

cleanup:
    if (playback_started && playback_ops->stop(playback_ctx) < 0) {
        result = -1;
        ESP_LOGE(TAG, "event=prompt type=wifi_provisioning action=play result=error reason=stop");
    }
    if (playback_ctx) {
        playback_ops->destroy(playback_ctx);
    }
    if (volume_active) {
        volume_ops->destroy(volume_ctx);
    }
    if (result == 0) {
        ESP_LOGI(TAG,
                 "event=prompt type=wifi_provisioning trigger=%s action=play frames=%d result=ok",
                 trigger, decoded.frames);
    }
    mybot_ogg_pcm_free(&decoded);
    return result;
}
