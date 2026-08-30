/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_OGG_OPUS_DECODER_H_
#define MYBOT_OGG_OPUS_DECODER_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t *pcm;
    int frames;
} mybot_ogg_pcm_t;

int mybot_ogg_opus_decode(const uint8_t *data, size_t size, mybot_ogg_pcm_t *output);
void mybot_ogg_pcm_free(mybot_ogg_pcm_t *output);

#endif /* MYBOT_OGG_OPUS_DECODER_H_ */
