# Embedded announcement assets

The `zh-CN` and `en-US` directories contain the Wi-Fi provisioning prompt, the pairing prompt, and
spoken digits `0` through `9`.
They are Opus-in-Ogg assets copied from the mybot BK7258 port at commit
`2577b5977a9f137855a7acf1fcdcd4040c5db2ea` (the `bk_solution_ai` submodule revision is
`8122deed89f283b5e6405aa772765d9a1acca3be`).

Only the locale selected by `CONFIG_MYBOT_LANGUAGE_*` is embedded in a firmware image. The audio is
decoded to 16 kHz mono signed-16 PCM in PSRAM when the SDK opens an announcement sound.

The copied assets use 20 ms Opus packets. Replacements must retain that packet duration, a mono
OpusHead stream, and a valid final granule position so playback can trim pre-skip and end padding.

The BK7258 source identifies these files as xiaozhi-esp32-derived assets and distributes them under
the accompanying `LICENSE.xiaozhi-esp32` MIT license. That upstream attribution is retained here.
