/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2025 Shenzhen Xinzhi Future Technology Co., Ltd. */
/* Ogg packet parsing follows the xiaozhi-esp32 OggDemuxer state model. */
#include "ogg_opus_decoder.h"

#include "decoder/esp_audio_dec.h"
#include "decoder/impl/esp_opus_dec.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TAG "mybot_ogg"
#define OGG_CAPTURE_PATTERN "OggS"
#define OGG_HEADER_SIZE 27U
#define OGG_FLAG_CONTINUED 0x01U
#define OGG_FLAG_BOS 0x02U
#define OGG_FLAG_EOS 0x04U
#define OGG_VALID_FLAGS (OGG_FLAG_CONTINUED | OGG_FLAG_BOS | OGG_FLAG_EOS)
#define OGG_MAX_FILE_SIZE (256U * 1024U)
#define OGG_MAX_PACKET_SIZE 8192U
#define OGG_OPUS_RATE 48000U
#define OUTPUT_MAX_FRAMES (16U * 16000U)
#define OUTPUT_RATE 16000U
#define OPUS_PACKET_FRAMES (OUTPUT_RATE * 20U / 1000U)

typedef int (*packet_visitor_t)(const uint8_t *packet, size_t size, void *user_data);

typedef struct {
    uint8_t packet[OGG_MAX_PACKET_SIZE];
    size_t packet_size;
} ogg_packet_reader_t;

typedef struct {
    bool head_seen;
    bool tags_seen;
    uint8_t channels;
    uint16_t pre_skip;
} opus_stream_info_t;

typedef struct {
    void *decoder;
    int16_t *decode_buffer;
    int16_t *pcm;
    size_t capacity;
    size_t written;
    size_t skip;
} decode_context_t;

static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint64_t read_le64(const uint8_t *data) {
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | data[i];
    }
    return value;
}

static bool is_opus_head(const uint8_t *packet, size_t size) {
    return size >= 8 && memcmp(packet, "OpusHead", 8) == 0;
}

static bool is_opus_tags(const uint8_t *packet, size_t size) {
    return size >= 8 && memcmp(packet, "OpusTags", 8) == 0;
}

static int visit_ogg_packets(const uint8_t *data, size_t size, packet_visitor_t visitor,
                             void *user_data, uint64_t *last_granule) {
    ogg_packet_reader_t *reader =
        heap_caps_calloc(1, sizeof(*reader), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!reader) {
        return -1;
    }

    size_t offset = 0;
    int result = -1;
    uint32_t stream_serial = 0;
    uint32_t expected_sequence = 0;
    bool stream_started = false;
    bool stream_ended = false;
    while (offset < size) {
        if (size - offset < OGG_HEADER_SIZE || memcmp(data + offset, OGG_CAPTURE_PATTERN, 4) != 0 ||
            data[offset + 4] != 0) {
            goto cleanup;
        }

        const uint8_t *header = data + offset;
        uint8_t flags = header[5];
        uint32_t serial = read_le32(header + 14);
        uint32_t sequence = read_le32(header + 18);
        bool continued = (flags & OGG_FLAG_CONTINUED) != 0;
        if ((flags & ~OGG_VALID_FLAGS) != 0 || stream_ended ||
            continued != (reader->packet_size != 0)) {
            goto cleanup;
        }
        if (!stream_started) {
            if ((flags & OGG_FLAG_BOS) == 0 || sequence != 0) {
                goto cleanup;
            }
            stream_serial = serial;
            stream_started = true;
        } else if ((flags & OGG_FLAG_BOS) != 0 || serial != stream_serial ||
                   sequence != expected_sequence) {
            goto cleanup;
        }
        expected_sequence = sequence + 1;

        size_t segment_count = header[26];
        size_t header_size = OGG_HEADER_SIZE + segment_count;
        if (header_size > size - offset) {
            goto cleanup;
        }

        uint64_t granule = read_le64(header + 6);
        if ((flags & OGG_FLAG_EOS) != 0 && granule == UINT64_MAX) {
            goto cleanup;
        }
        if (last_granule && granule != UINT64_MAX) {
            *last_granule = granule;
        }

        const uint8_t *segments = header + OGG_HEADER_SIZE;
        size_t body_size = 0;
        for (size_t i = 0; i < segment_count; ++i) {
            body_size += segments[i];
        }
        if (body_size > size - offset - header_size) {
            goto cleanup;
        }

        const uint8_t *body = header + header_size;
        size_t body_offset = 0;
        for (size_t i = 0; i < segment_count; ++i) {
            size_t segment_size = segments[i];
            if (reader->packet_size + segment_size > sizeof(reader->packet)) {
                goto cleanup;
            }
            memcpy(reader->packet + reader->packet_size, body + body_offset, segment_size);
            reader->packet_size += segment_size;
            body_offset += segment_size;

            if (segment_size < 255) {
                if (reader->packet_size > 0 &&
                    visitor(reader->packet, reader->packet_size, user_data) < 0) {
                    goto cleanup;
                }
                reader->packet_size = 0;
            }
        }
        offset += header_size + body_size;
        stream_ended = (flags & OGG_FLAG_EOS) != 0;
    }

    result = stream_started && stream_ended && reader->packet_size == 0 ? 0 : -1;

cleanup:
    heap_caps_free(reader);
    return result;
}

static int scan_packet(const uint8_t *packet, size_t size, void *user_data) {
    opus_stream_info_t *info = user_data;
    if (!info->head_seen) {
        if (!is_opus_head(packet, size) || size < 19 || packet[8] != 1 || packet[9] != 1) {
            return -1;
        }
        info->head_seen = true;
        info->channels = packet[9];
        info->pre_skip = read_le16(packet + 10);
        return 0;
    }
    if (!info->tags_seen) {
        if (!is_opus_tags(packet, size)) {
            return -1;
        }
        info->tags_seen = true;
    }
    return 0;
}

static int decode_packet(const uint8_t *packet, size_t size, void *user_data) {
    decode_context_t *ctx = user_data;
    if (is_opus_head(packet, size) || is_opus_tags(packet, size) || ctx->written >= ctx->capacity) {
        return 0;
    }

    esp_audio_dec_in_raw_t raw = {
        .buffer = (uint8_t *)packet,
        .len = (uint32_t)size,
        .frame_recover = ESP_AUDIO_DEC_RECOVERY_NONE,
    };
    esp_audio_dec_out_frame_t frame = {
        .buffer = (uint8_t *)ctx->decode_buffer,
        .len = OPUS_PACKET_FRAMES * sizeof(int16_t),
    };
    esp_audio_dec_info_t info = {0};
    if (esp_opus_dec_decode(ctx->decoder, &raw, &frame, &info) != ESP_AUDIO_ERR_OK ||
        raw.consumed != size || frame.decoded_size != OPUS_PACKET_FRAMES * sizeof(int16_t) ||
        info.sample_rate != OUTPUT_RATE || info.channel != 1 || info.bits_per_sample != 16) {
        return -1;
    }

    size_t frames = frame.decoded_size / sizeof(int16_t);
    size_t source_offset = ctx->skip < frames ? ctx->skip : frames;
    ctx->skip -= source_offset;
    size_t available = frames - source_offset;
    size_t remaining = ctx->capacity - ctx->written;
    size_t take = available < remaining ? available : remaining;
    memcpy(ctx->pcm + ctx->written, ctx->decode_buffer + source_offset, take * sizeof(int16_t));
    ctx->written += take;
    return 0;
}

int mybot_ogg_opus_decode(const uint8_t *data, size_t size, mybot_ogg_pcm_t *output) {
    if (!data || size == 0 || size > OGG_MAX_FILE_SIZE || !output) {
        return -1;
    }
    memset(output, 0, sizeof(*output));

    opus_stream_info_t stream_info = {0};
    uint64_t last_granule = 0;
    if (visit_ogg_packets(data, size, scan_packet, &stream_info, &last_granule) < 0 ||
        !stream_info.head_seen || !stream_info.tags_seen || last_granule <= stream_info.pre_skip) {
        ESP_LOGE(TAG, "event=ogg_decode result=error reason=container");
        return -1;
    }

    uint64_t target_frames = (last_granule - stream_info.pre_skip) * OUTPUT_RATE / OGG_OPUS_RATE;
    if (target_frames == 0 || target_frames > OUTPUT_MAX_FRAMES) {
        ESP_LOGE(TAG, "event=ogg_decode result=error reason=duration");
        return -1;
    }

    esp_opus_dec_cfg_t decoder_config = {
        .sample_rate = OUTPUT_RATE,
        .channel = ESP_AUDIO_MONO,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS,
        .self_delimited = false,
    };
    decode_context_t ctx = {
        .capacity = (size_t)target_frames,
        .skip = (size_t)stream_info.pre_skip * OUTPUT_RATE / OGG_OPUS_RATE,
    };
    if (esp_opus_dec_open(&decoder_config, sizeof(decoder_config), &ctx.decoder) !=
        ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "event=ogg_decode result=error reason=decoder_open");
        return -1;
    }

    ctx.pcm = heap_caps_malloc(ctx.capacity * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ctx.decode_buffer =
        heap_caps_malloc(OPUS_PACKET_FRAMES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int result = -1;
    if (!ctx.pcm || !ctx.decode_buffer) {
        ESP_LOGE(TAG, "event=ogg_decode result=error reason=memory");
        goto cleanup;
    }
    if (visit_ogg_packets(data, size, decode_packet, &ctx, NULL) < 0 || ctx.skip != 0 ||
        ctx.written != ctx.capacity) {
        ESP_LOGE(TAG, "event=ogg_decode result=error reason=packet");
        goto cleanup;
    }

    output->pcm = ctx.pcm;
    output->frames = (int)ctx.written;
    ctx.pcm = NULL;
    result = 0;

cleanup:
    heap_caps_free(ctx.decode_buffer);
    heap_caps_free(ctx.pcm);
    esp_opus_dec_close(ctx.decoder);
    return result;
}

void mybot_ogg_pcm_free(mybot_ogg_pcm_t *output) {
    if (!output) {
        return;
    }
    heap_caps_free(output->pcm);
    output->pcm = NULL;
    output->frames = 0;
}
