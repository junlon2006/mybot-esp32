# Embedded assets

## Announcement audio

The `locales/zh-CN` and `locales/en-US` directories contain the Wi-Fi provisioning prompt, the
pairing prompt, and spoken digits `0` through `9`.

Only the locale selected by `CONFIG_MYBOT_LANGUAGE_*` is embedded in a firmware image. The audio is
decoded to 16 kHz mono signed-16 PCM in PSRAM when the SDK opens an announcement sound.

The copied assets use 20 ms Opus packets. Replacements must retain that packet duration, a mono
OpusHead stream, and a valid final granule position so playback can trim pre-skip and end padding.

The announcement audio is distributed under the accompanying
[MIT license](LICENSE.xiaozhi-esp32). Its attribution is retained there and in the repository
third-party notice.

## CoreS3 UI images

`ui/noto_emoji` contains four local state images: neutral, happy, relaxed, and thinking. The
original 64x61 PNG files are retained as reproducible inputs. The
[asset generator](../../../scripts/generate-lvgl-ui-assets.py) pads them to 64x64 and produces
constant ARGB8888 arrays in `src/drivers/display/renderers/lvgl/lvgl_assets.c`. Firmware builds use those
arrays only when an LVGL UI option is enabled; no runtime PNG decoder is needed.

These rasterized Noto Color Emoji font glyphs retain the font's
[SIL Open Font License 1.1](ui/noto_emoji/LICENSE.txt), separately from the audio's MIT terms.
[SOURCES.json](ui/noto_emoji/SOURCES.json) records the pinned source package, original font
provenance, per-file SHA-256 values, and conversion details. Run the generator with `--check`
to verify that the source hashes and generated arrays match.
