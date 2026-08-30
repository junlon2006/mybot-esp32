/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_EMBEDDED_OGG_PROMPT_H_
#define MYBOT_EMBEDDED_OGG_PROMPT_H_

#include <mybot/platform/mybot_audio.h>

int mybot_embedded_ogg_play_wifi_provisioning(const mybot_audio_playback_ops_t *playback_ops,
                                              const mybot_audio_volume_ops_t *volume_ops,
                                              const char *trigger);

#endif /* MYBOT_EMBEDDED_OGG_PROMPT_H_ */
