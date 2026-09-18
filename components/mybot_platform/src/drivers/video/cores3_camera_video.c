/* SPDX-License-Identifier: Apache-2.0 */
#include "board_config.h"
#include "cores3_hardware.h"

#include <mybot/platform/mybot_video.h>
#include <mybot/mybot_build_config.h>

#include "esp_heap_caps.h"
#include "esp_jpeg_enc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <fcntl.h>
#include <inttypes.h>
#include <linux/videodev2.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define TAG "cores3_video"
#define VIDEO_WIDTH 320
#define VIDEO_HEIGHT 240
#define VIDEO_RAW_BYTES (VIDEO_WIDTH * VIDEO_HEIGHT * 2)
#define VIDEO_BUFFER_COUNT 2
#define VIDEO_JPEG_CAPACITY (128 * 1024)
#define VIDEO_INTERVAL_US INT64_C(1000000)
#define VIDEO_MIN_BPS 64000U
#define VIDEO_MAX_BPS 512000U
#define VIDEO_INITIAL_BPS ((VIDEO_MIN_BPS + VIDEO_MAX_BPS) / 2)
#define VIDEO_STOP_TIMEOUT_MS 3000

_Static_assert(VIDEO_JPEG_CAPACITY <= MYBOT_VIDEO_MAX_FRAME_BYTES, "JPEG exceeds SDK limit");

typedef struct {
    int fd;
    bool device_ready;
    bool initialized;
    bool streaming;
    bool task_created;
    bool buffers_clean;
    bool stop_requested;
    uint32_t target_bps;
    SemaphoreHandle_t stopped;
    jpeg_enc_handle_t encoder;
    uint8_t *jpeg;
    uint8_t *buffers[VIDEO_BUFFER_COUNT];
    size_t lengths[VIDEO_BUFFER_COUNT];
    mybot_video_frame_handler_t handler;
    void *user_data;
} video_context_t;

/* SDK lifecycle calls are serialized; only stop/bitrate state is shared with the worker. */
static video_context_t s_video = {.fd = -1};
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool should_stop(void) {
    portENTER_CRITICAL(&s_lock);
    const bool stop = s_video.stop_requested;
    portEXIT_CRITICAL(&s_lock);
    return stop;
}

static uint32_t target_bitrate(void) {
    portENTER_CRITICAL(&s_lock);
    const uint32_t bps = s_video.target_bps;
    portEXIT_CRITICAL(&s_lock);
    return bps;
}

static int queue_buffer(unsigned int index) {
    struct v4l2_buffer buffer = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
        .index = index,
    };
    return ioctl(s_video.fd, VIDIOC_QBUF, &buffer);
}

static int set_capture_timeout(unsigned int ms) {
    const struct timeval timeout = {.tv_sec = ms / 1000, .tv_usec = (ms % 1000) * 1000};
    return ioctl(s_video.fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout);
}

static int stop_stream(void) {
    if (!s_video.streaming) {
        return 0;
    }
    const enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_video.fd, VIDIOC_STREAMOFF, &type) < 0) {
        ESP_LOGE(TAG, "event=video action=stream_off result=error");
        return -1;
    }
    s_video.streaming = false;
    return 0;
}

static int release_video(void) {
    if (stop_stream() < 0) {
        return -1;
    }
    if (s_video.fd >= 0) {
        if (close(s_video.fd) < 0) {
            ESP_LOGE(TAG, "event=video action=close result=error");
            return -1;
        }
        s_video.fd = -1;
        /* The V4L2 owner frees MMAP capture buffers on close. */
        memset(s_video.buffers, 0, sizeof(s_video.buffers));
        memset(s_video.lengths, 0, sizeof(s_video.lengths));
    }
    if (s_video.device_ready) {
        if (esp_video_deinit() != ESP_OK) {
            ESP_LOGE(TAG, "event=video action=deinitialize result=error");
            return -1;
        }
        s_video.device_ready = false;
    }
    if (s_video.encoder) {
        if (jpeg_enc_close(s_video.encoder) != JPEG_ERR_OK) {
            return -1;
        }
        s_video.encoder = NULL;
    }
    heap_caps_free(s_video.jpeg);
    s_video.jpeg = NULL;
    if (s_video.stopped) {
        vSemaphoreDelete(s_video.stopped);
        s_video.stopped = NULL;
    }
    return 0;
}

static int prepare_buffers(void) {
    struct v4l2_requestbuffers request = {
        .count = VIDEO_BUFFER_COUNT,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    /* Also discards any partially queued buffers from a failed stream start. */
    if (ioctl(s_video.fd, VIDIOC_REQBUFS, &request) < 0 || request.count != VIDEO_BUFFER_COUNT) {
        return -1;
    }
    for (unsigned int index = 0; index < VIDEO_BUFFER_COUNT; ++index) {
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index = index,
        };
        if (ioctl(s_video.fd, VIDIOC_QUERYBUF, &buffer) < 0 || buffer.length < VIDEO_RAW_BYTES) {
            return -1;
        }
        s_video.buffers[index] = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                      s_video.fd, buffer.m.offset);
        if (!s_video.buffers[index] || s_video.buffers[index] == MAP_FAILED ||
            ((uintptr_t)s_video.buffers[index] & 15U) != 0) {
            return -1;
        }
        s_video.lengths[index] = buffer.length;
    }
    s_video.buffers_clean = true;
    return 0;
}

static int prepare_capture(void) {
    const esp_video_init_dvp_config_t dvp = {
        .sccb_config =
            {
                .init_sccb = false,
                .i2c_handle = mybot_cores3_i2c_bus_handle(),
                .freq = 100000,
            },
        .reset_pin = GPIO_NUM_NC,
        .pwdn_pin = GPIO_NUM_NC,
        .dvp_pin =
            {
                .data_width = CAM_CTLR_DATA_WIDTH_8,
                .data_io = {MYBOT_CAMERA_D0, MYBOT_CAMERA_D1, MYBOT_CAMERA_D2, MYBOT_CAMERA_D3,
                            MYBOT_CAMERA_D4, MYBOT_CAMERA_D5, MYBOT_CAMERA_D6, MYBOT_CAMERA_D7},
                .vsync_io = MYBOT_CAMERA_VSYNC,
                .de_io = MYBOT_CAMERA_HREF,
                .pclk_io = MYBOT_CAMERA_PCLK,
                .xclk_io = MYBOT_CAMERA_XCLK,
            },
        .xclk_freq = MYBOT_CAMERA_XCLK_HZ,
    };
    const esp_video_init_config_t config = {.dvp = &dvp};
    if (!dvp.sccb_config.i2c_handle || mybot_cores3_reset_camera() < 0) {
        return -1;
    }
    if (esp_video_init(&config) != ESP_OK) {
        return -1;
    }
    s_video.device_ready = true;
    s_video.fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDWR);
    if (s_video.fd < 0) {
        return -1;
    }
    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    };
    /* The pinned sensor default selects QVGA YUYV before the V4L2 device opens. */
    if (ioctl(s_video.fd, VIDIOC_G_FMT, &format) < 0 || format.fmt.pix.width != VIDEO_WIDTH ||
        format.fmt.pix.height != VIDEO_HEIGHT || format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV ||
        (format.fmt.pix.bytesperline && format.fmt.pix.bytesperline != VIDEO_WIDTH * 2)) {
        ESP_LOGE(TAG, "event=video action=format result=error expected=320x240_yuyv");
        return -1;
    }
    if (prepare_buffers() < 0) {
        return -1;
    }
    return set_capture_timeout(100);
}

static void video_worker(void *argument) {
    (void)argument;
    unsigned int encoded = 0, sent = 0, rejected = 0, dropped = 0, capture_errors = 0;
    unsigned int quality = 60;
    unsigned int stale_frames = 0;
    uint32_t previous_bps = target_bitrate();
    int64_t next_frame_us = esp_timer_get_time() + VIDEO_INTERVAL_US;
    int64_t next_send_us = 0;
    int64_t report_us = esp_timer_get_time();
    int64_t max_encode_us = 0;
    int last_bytes = 0;

    s_video.buffers_clean = false;
    for (unsigned int index = 0; index < VIDEO_BUFFER_COUNT; ++index) {
        if (queue_buffer(index) < 0) {
            ESP_LOGE(TAG, "event=video action=queue result=error");
            goto done;
        }
    }
    const enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (should_stop() || ioctl(s_video.fd, VIDIOC_STREAMON, &type) < 0) {
        ESP_LOGW(TAG, "event=video action=stream_on result=not_started");
        goto done;
    }
    s_video.streaming = true;
    ESP_LOGI(TAG, "event=video action=stream_on width=320 height=240 format=yuyv uplink_fps=1");

    while (!should_stop()) {
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        if (ioctl(s_video.fd, VIDIOC_DQBUF, &buffer) < 0) {
            ++capture_errors;
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            if (buffer.index >= VIDEO_BUFFER_COUNT) {
                ESP_LOGE(TAG, "event=video action=capture result=error reason=buffer_index");
                break;
            }
            int bytes = 0;
            bool jpeg_ready = false;
            const int64_t now = esp_timer_get_time();
            if (stale_frames > 0) {
                --stale_frames;
            } else if (!should_stop() && now >= next_frame_us) {
                next_frame_us = now + VIDEO_INTERVAL_US;
                /* At most one queued older frame can remain with two capture buffers. */
                if (set_capture_timeout(0) < 0) {
                    break;
                }
                struct v4l2_buffer latest = {
                    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                    .memory = V4L2_MEMORY_MMAP,
                };
                if (ioctl(s_video.fd, VIDIOC_DQBUF, &latest) == 0) {
                    if (queue_buffer(buffer.index) < 0 || latest.index >= VIDEO_BUFFER_COUNT) {
                        break;
                    }
                    buffer = latest;
                }
                if (set_capture_timeout(100) < 0) {
                    break;
                }
                const uint32_t bps = target_bitrate();
                if (bps != previous_bps) {
                    quality = 30 + (bps - VIDEO_MIN_BPS) * 30 / (VIDEO_MAX_BPS - VIDEO_MIN_BPS);
                    previous_bps = bps;
                }
                if (!should_stop() && !(buffer.flags & V4L2_BUF_FLAG_ERROR) &&
                    buffer.bytesused == VIDEO_RAW_BYTES &&
                    buffer.bytesused <= s_video.lengths[buffer.index]) {
                    const int64_t begin = esp_timer_get_time();
                    if (jpeg_enc_set_quality(s_video.encoder, quality) == JPEG_ERR_OK &&
                        jpeg_enc_process(s_video.encoder, s_video.buffers[buffer.index],
                                         VIDEO_RAW_BYTES, s_video.jpeg, VIDEO_JPEG_CAPACITY,
                                         &bytes) == JPEG_ERR_OK &&
                        bytes > 0 && bytes <= VIDEO_JPEG_CAPACITY) {
                        ++encoded;
                        last_bytes = bytes;
                        jpeg_ready = true;
                    } else {
                        ++dropped;
                    }
                    const int64_t elapsed = esp_timer_get_time() - begin;
                    if (elapsed > max_encode_us) {
                        max_encode_us = elapsed;
                    }
                } else {
                    ++dropped;
                }
            }
            if (queue_buffer(buffer.index) < 0) {
                ESP_LOGE(TAG, "event=video action=requeue result=error");
                break;
            }
            if (jpeg_ready && !should_stop()) {
                const uint32_t budget = target_bitrate() / 8;
                const int64_t send_us = esp_timer_get_time();
                if ((uint32_t)bytes > budget || send_us < next_send_us) {
                    ++dropped;
                    if ((uint32_t)bytes > budget && quality > 30) {
                        quality -= 10;
                        if (quality < 30) {
                            quality = 30;
                        }
                    }
                } else {
                    const mybot_video_frame_t frame = {
                        .data = s_video.jpeg,
                        .len = (size_t)bytes,
                        .codec = MYBOT_VIDEO_CODEC_JPEG,
                    };
                    /* JPEG storage is held until the synchronous SDK callback returns. */
                    next_send_us = send_us + VIDEO_INTERVAL_US;
                    next_frame_us = next_send_us;
                    if (s_video.handler(&frame, s_video.user_data) == 0) {
                        ++sent;
                    } else {
                        ++rejected;
                    }
                    /* A slow RTC callback may leave both raw buffers old. Drain them before
                     * selecting the next image, rather than encoding a pre-stall frame. */
                    stale_frames = VIDEO_BUFFER_COUNT;
                }
            }
        }
        const int64_t now = esp_timer_get_time();
        if (now - report_us >= INT64_C(10000000)) {
            ESP_LOGI(TAG,
                     "event=video_stats encoded=%u sent=%u rejected=%u dropped=%u "
                     "capture_errors=%u last_bytes=%d quality=%u target_bps=%" PRIu32
                     " encode_max_us=%" PRId64,
                     encoded, sent, rejected, dropped, capture_errors, last_bytes, quality,
                     target_bitrate(), max_encode_us);
            encoded = sent = rejected = dropped = capture_errors = 0;
            max_encode_us = 0;
            report_us = now;
        }
    }
done:
    (void)stop_stream();
    if (should_stop()) {
        ESP_LOGI(TAG, "event=video action=worker_exit result=stopped sent=%u rejected=%u", sent,
                 rejected);
    } else {
        ESP_LOGE(TAG, "event=video action=worker_exit result=error sent=%u rejected=%u", sent,
                 rejected);
    }
    xSemaphoreGive(s_video.stopped);
    vTaskDelete(NULL);
}

static int video_init(void **out_context, mybot_video_frame_handler_t handler, void *user_data) {
    if (!out_context || !handler || s_video.initialized || s_video.task_created) {
        return -1;
    }
    *out_context = NULL;
    if (release_video() < 0) {
        return -1;
    }
    s_video = (video_context_t){.fd = -1, .target_bps = VIDEO_INITIAL_BPS};
    s_video.stopped = xSemaphoreCreateBinary();
    s_video.jpeg =
        heap_caps_aligned_alloc(16, VIDEO_JPEG_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jpeg_enc_config_t jpeg_config = DEFAULT_JPEG_ENC_CONFIG();
    jpeg_config.width = VIDEO_WIDTH;
    jpeg_config.height = VIDEO_HEIGHT;
    jpeg_config.src_type = JPEG_PIXEL_FORMAT_YCbYCr;
    jpeg_config.quality = 60;
    jpeg_config.task_enable = false;
    if (!s_video.stopped || !s_video.jpeg ||
        jpeg_enc_open(&jpeg_config, &s_video.encoder) != JPEG_ERR_OK || prepare_capture() < 0) {
        (void)release_video();
        ESP_LOGE(TAG, "event=video action=initialize result=error");
        return -1;
    }
    s_video.handler = handler;
    s_video.user_data = user_data;
    s_video.initialized = true;
    *out_context = &s_video;
    ESP_LOGI(TAG, "event=video action=initialize result=ok codec=jpeg uplink_fps=1 buffers=2");
    return 0;
}

static int video_start(void *opaque) {
    if (opaque != &s_video || !s_video.initialized || s_video.task_created) {
        return -1;
    }
    if (!s_video.buffers_clean && prepare_buffers() < 0) {
        return -1;
    }
    (void)xSemaphoreTake(s_video.stopped, 0);
    portENTER_CRITICAL(&s_lock);
    s_video.stop_requested = false;
    portEXIT_CRITICAL(&s_lock);
    if (xTaskCreate(video_worker, "cores3_video", 6144, NULL, 3, NULL) != pdPASS) {
        return -1;
    }
    s_video.task_created = true;
    return 0;
}

static int video_stop(void *opaque) {
    if (opaque != &s_video || !s_video.initialized) {
        return -1;
    }
    portENTER_CRITICAL(&s_lock);
    s_video.stop_requested = true;
    portEXIT_CRITICAL(&s_lock);
    if (s_video.task_created) {
        if (xSemaphoreTake(s_video.stopped, pdMS_TO_TICKS(VIDEO_STOP_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGW(TAG, "event=video action=stop result=pending");
            return -1;
        }
        s_video.task_created = false;
    }
    if (stop_stream() < 0) {
        return -1;
    }
    ESP_LOGI(TAG, "event=video action=stop result=ok");
    return 0;
}

static void video_bitrate(void *opaque, uint32_t bps) {
    if (opaque != &s_video) {
        return;
    }
    if (bps < VIDEO_MIN_BPS) {
        bps = VIDEO_MIN_BPS;
    } else if (bps > VIDEO_MAX_BPS) {
        bps = VIDEO_MAX_BPS;
    }
    portENTER_CRITICAL(&s_lock);
    s_video.target_bps = bps;
    portEXIT_CRITICAL(&s_lock);
}

static void video_destroy(void *opaque) {
    if (opaque != &s_video || video_stop(opaque) < 0) {
        return;
    }
    s_video.initialized = false;
    s_video.handler = NULL;
    s_video.user_data = NULL;
    if (release_video() < 0) {
        ESP_LOGE(TAG, "event=video action=destroy result=retained");
        return;
    }
    s_video = (video_context_t){.fd = -1};
    ESP_LOGI(TAG, "event=video action=destroy result=ok");
}

static const mybot_video_ops_t s_ops = {
    .min_bps = VIDEO_MIN_BPS,
    .max_bps = VIDEO_MAX_BPS,
    .init = video_init,
    .start = video_start,
    .stop = video_stop,
    .on_target_bitrate_changed = video_bitrate,
    .destroy = video_destroy,
};

const mybot_video_ops_t *mybot_cores3_video_ops(void) {
    return &s_ops;
}
