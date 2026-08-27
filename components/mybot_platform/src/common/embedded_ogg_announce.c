/* SPDX-License-Identifier: Apache-2.0 */
#include <mybot/platform/mybot_announce.h>

#include "ogg_opus_decoder.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TAG "mybot_announce"

#define DECLARE_ASSET(variable, file_name)                                                         \
    extern const uint8_t variable##_start[] asm("_binary_" #file_name "_ogg_start");               \
    extern const uint8_t variable##_end[] asm("_binary_" #file_name "_ogg_end")

DECLARE_ASSET(prompt, prompt);
DECLARE_ASSET(digit_0, 0);
DECLARE_ASSET(digit_1, 1);
DECLARE_ASSET(digit_2, 2);
DECLARE_ASSET(digit_3, 3);
DECLARE_ASSET(digit_4, 4);
DECLARE_ASSET(digit_5, 5);
DECLARE_ASSET(digit_6, 6);
DECLARE_ASSET(digit_7, 7);
DECLARE_ASSET(digit_8, 8);
DECLARE_ASSET(digit_9, 9);

typedef struct {
    const uint8_t *data;
    const uint8_t *end;
} embedded_asset_t;

typedef struct {
    uint8_t reserved;
} announce_context_t;

typedef struct {
    mybot_ogg_pcm_t decoded;
    int offset;
} announce_sound_t;

#define ASSET(name)                                                                                \
    { .data = name##_start, .end = name##_end }

static const embedded_asset_t s_assets[MYBOT_ANNOUNCE_SOUND_COUNT] = {
    [MYBOT_ANNOUNCE_SOUND_PROMPT] = ASSET(prompt),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_0] = ASSET(digit_0),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_1] = ASSET(digit_1),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_2] = ASSET(digit_2),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_3] = ASSET(digit_3),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_4] = ASSET(digit_4),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_5] = ASSET(digit_5),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_6] = ASSET(digit_6),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_7] = ASSET(digit_7),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_8] = ASSET(digit_8),
    [MYBOT_ANNOUNCE_SOUND_DIGIT_9] = ASSET(digit_9),
};

static int announce_init(void **out_ctx) {
    if (!out_ctx) {
        return -1;
    }
    *out_ctx = heap_caps_calloc(1, sizeof(announce_context_t), MALLOC_CAP_8BIT);
    if (!*out_ctx) {
        return -1;
    }
    ESP_LOGI(TAG, "event=announce action=initialize language=%s result=ok",
             CONFIG_MYBOT_LANGUAGE_TAG);
    return 0;
}

static void *announce_open(void *opaque, mybot_announce_sound_t sound) {
    if (!opaque || sound < MYBOT_ANNOUNCE_SOUND_PROMPT || sound >= MYBOT_ANNOUNCE_SOUND_COUNT) {
        return NULL;
    }

    announce_sound_t *handle =
        heap_caps_calloc(1, sizeof(*handle), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!handle) {
        return NULL;
    }
    const embedded_asset_t *asset = &s_assets[sound];
    size_t asset_size = (size_t)(asset->end - asset->data);
    if (mybot_ogg_opus_decode(asset->data, asset_size, &handle->decoded) < 0) {
        heap_caps_free(handle);
        return NULL;
    }
    ESP_LOGI(TAG, "event=announce action=open sound=%d frames=%d result=ok", (int)sound,
             handle->decoded.frames);
    return handle;
}

static int announce_read(void *opaque, void *sound, int16_t *dst, int max_frames) {
    (void)opaque;
    announce_sound_t *handle = sound;
    if (!handle || !dst || max_frames <= 0) {
        return 0;
    }

    int remaining = handle->decoded.frames - handle->offset;
    int frames = remaining < max_frames ? remaining : max_frames;
    if (frames <= 0) {
        return 0;
    }
    memcpy(dst, handle->decoded.pcm + handle->offset, (size_t)frames * sizeof(int16_t));
    handle->offset += frames;
    return frames;
}

static void announce_close(void *opaque, void *sound) {
    (void)opaque;
    announce_sound_t *handle = sound;
    if (!handle) {
        return;
    }
    mybot_ogg_pcm_free(&handle->decoded);
    heap_caps_free(handle);
}

static void announce_destroy(void *opaque) {
    heap_caps_free(opaque);
    ESP_LOGI(TAG, "event=announce action=destroy");
}

static const mybot_announce_ops_t s_announce_ops = {
    .init = announce_init,
    .open = announce_open,
    .read = announce_read,
    .close = announce_close,
    .destroy = announce_destroy,
};

const mybot_announce_ops_t *mybot_esp32s3_announce_ops(void) {
    return &s_announce_ops;
}
