# Embedded announcement assets

The `zh-CN` and `en-US` directories contain the Wi-Fi provisioning prompt, the pairing prompt, and
spoken digits `0` through `9`.

Only the locale selected by `CONFIG_MYBOT_LANGUAGE_*` is embedded in a firmware image. The audio is
decoded to 16 kHz mono signed-16 PCM in PSRAM when the SDK opens an announcement sound.

The copied assets use 20 ms Opus packets. Replacements must retain that packet duration, a mono
OpusHead stream, and a valid final granule position so playback can trim pre-skip and end padding.

The assets are distributed under the accompanying `LICENSE.xiaozhi-esp32` MIT license. The upstream
attribution is retained there and in the repository third-party notice.
