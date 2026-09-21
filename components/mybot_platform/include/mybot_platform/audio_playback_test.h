/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_PLATFORM_AUDIO_PLAYBACK_TEST_H_
#define MYBOT_PLATFORM_AUDIO_PLAYBACK_TEST_H_

/* Diagnostic firmware only: call after board registration, before starting Wi-Fi/mybot. */
int mybot_audio_playback_test_run(void);

#endif
