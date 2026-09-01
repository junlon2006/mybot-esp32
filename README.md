# mybot-esp32

[![CI](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/junlon2006/mybot-esp32/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/junlon2006/mybot-esp32)](LICENSE)

**[English](README.md) | [简体中文](README.zh-CN.md)**

`mybot-esp32` is an independent ESP-IDF firmware project that brings the
[mybot AI voice-chat SDK](https://github.com/junlon2006/mybot) to ESP32-S3 devices. It owns board
initialization, Wi-Fi provisioning, persistent storage, secure HTTPS transport, audio
capture/playback, input, display, and the firmware lifecycle around mybot.

The current development baseline is **ESP-IDF v5.5.2**. Supported board profiles are the Zhengchen
1.54 TFT ML307 and Wi-Fi variants, Espressif ESP-VoCat, Waveshare ESP32-S3 Touch AMOLED 1.75 and
1.75C, M5Stack CoreS3, M5Stack StickS3, ReSpeaker Flex XVF3800 Circular-4 with XIAO ESP32S3, and
SenseCAP Watcher.

> Project-maintained code is Apache-2.0 unless a file says otherwise. Bundled dependencies and media
> assets have separate terms. Read [License and dependencies](#license-and-dependencies) before
> redistributing source or firmware images.

## Features

- Native ESP-IDF component build of the pinned mybot SDK.
- Wi-Fi station reconnect and first-boot captive-portal provisioning.
- NVS persistence for device credentials and 0-100 speaker volume.
- HTTPS through `esp-tls` with the system CA bundle, SNI, and hostname verification.
- 16 kHz mono signed-16 PCM capture/playback with configurable 20/40/60 ms packet duration.
- Agora RTSA full-duplex audio, Cloud AEC, AI QoS, RTM channel subscription, and voice-print status.
- Chinese and English local pairing-code and Wi-Fi provisioning prompts.
- Compile-time board profiles with isolated Flash, PSRAM, partition, driver, and pin configuration.

## Supported Boards

| Board profile | Audio | Display and input | Storage |
| --- | --- | --- | --- |
| `zhengchen-1.54tft-ml307` | I2S microphone and speaker | ST7789, Boot and volume buttons | 16 MB QIO Flash, 8 MB Octal PSRAM |
| `zhengchen-1.54tft-wifi` | I2S microphone and speaker | ST7789, Boot and volume buttons | 16 MB QIO Flash, Octal PSRAM at 80 MHz |
| `esp-vocat` | ES7210 and ES8311 | ST77916, CST816S touch and Boot | 16 MB QIO Flash, Octal PSRAM at 80 MHz |
| `esp32-s3-touch-amoled-1.75` | ES7210 and ES8311 | CO5300 AMOLED, CST9217 touch and Boot | 16 MB QIO Flash, 8 MB Octal PSRAM |
| `esp32-s3-touch-amoled-1.75c` | ES7210 and ES8311 | CO5300 AMOLED, CST9217 touch and Boot | Safe 16 MB QIO Flash profile, 8 MB Octal PSRAM |
| `m5stack-core-s3` | ES7210 and AW88298 | ILI9342 and FT6336 touch | 16 MB QIO Flash, 8 MB Quad PSRAM |
| `m5stack-stick-s3` | ES8311 | ST7789P3 and main button | 8 MB QIO Flash, 8 MB Octal PSRAM |
| `respeaker-flex-xvf3800-circular4-xiao` | XVF3800 and AIC3104 | XIAO Boot and XVF onboard buttons; no display | 8 MB Flash, 8 MB Octal PSRAM |
| `sensecap-watcher` | ES8311 and ES7243E | SPD2010 and rotary encoder | 32 MB QIO Flash, Octal PSRAM |

All profiles currently use Wi-Fi networking. The ML307 profile reserves the Zhengchen modem UART,
but the modem AT socket is not an lwIP network interface and is not supported by the RTC transport.
The Wi-Fi profile does not configure or access the modem UART pins, GPIO11 and GPIO12.

## Build

Install ESP-IDF v5.5.2 by following the
[official setup guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/index.html),
then activate that installation in the current shell:

```sh
. /path/to/esp-idf/export.sh
test "$(idf.py --version)" = "ESP-IDF v5.5.2"
```

Use a separate build directory and generated sdkconfig for each board:

```sh
idf.py -B build/zhengchen-1.54tft-ml307 \
  -DMYBOT_BOARD=zhengchen-1.54tft-ml307 \
  -DSDKCONFIG=build/zhengchen-1.54tft-ml307/sdkconfig build

idf.py -B build/zhengchen-1.54tft-wifi \
  -DMYBOT_BOARD=zhengchen-1.54tft-wifi \
  -DSDKCONFIG=build/zhengchen-1.54tft-wifi/sdkconfig build

idf.py -B build/esp-vocat \
  -DMYBOT_BOARD=esp-vocat \
  -DSDKCONFIG=build/esp-vocat/sdkconfig build

idf.py -B build/esp32-s3-touch-amoled-1.75 \
  -DMYBOT_BOARD=esp32-s3-touch-amoled-1.75 \
  -DSDKCONFIG=build/esp32-s3-touch-amoled-1.75/sdkconfig build

idf.py -B build/esp32-s3-touch-amoled-1.75c \
  -DMYBOT_BOARD=esp32-s3-touch-amoled-1.75c \
  -DSDKCONFIG=build/esp32-s3-touch-amoled-1.75c/sdkconfig build

idf.py -B build/m5stack-core-s3 \
  -DMYBOT_BOARD=m5stack-core-s3 \
  -DSDKCONFIG=build/m5stack-core-s3/sdkconfig build

idf.py -B build/m5stack-stick-s3 \
  -DMYBOT_BOARD=m5stack-stick-s3 \
  -DSDKCONFIG=build/m5stack-stick-s3/sdkconfig build

idf.py -B build/respeaker-flex-xvf3800-circular4-xiao \
  -DMYBOT_BOARD=respeaker-flex-xvf3800-circular4-xiao \
  -DSDKCONFIG=build/respeaker-flex-xvf3800-circular4-xiao/sdkconfig build

idf.py -B build/sensecap-watcher \
  -DMYBOT_BOARD=sensecap-watcher \
  -DSDKCONFIG=build/sensecap-watcher/sdkconfig build
```

The default profile is `zhengchen-1.54tft-ml307`, but explicit board selection is recommended.
Board defaults supply the required Flash, PSRAM, and partition settings.

Flash and monitor the selected build:

```sh
idf.py -B build/zhengchen-1.54tft-ml307 -p /dev/tty.wchusbserial1430 flash monitor
```

Before first flashing a SenseCAP Watcher, back up its 200 KiB factory-data partition:

```sh
python -m esptool --chip esp32s3 --baud 2000000 --before default_reset \
  --after hard_reset --no-stub read_flash 0x9000 204800 nvsfactory.bin
```

Always build the `sensecap-watcher` profile and use `idf.py flash`; never run `erase-flash` on this
device. Erasing or replacing the `nvsfactory` region can permanently remove factory identifiers and
service-recovery data.

## Provisioning and Controls

When NVS contains no Wi-Fi credentials, the device creates a configuration AP whose SSID starts
with `mybot-`. Connect to it and open `http://192.168.4.1`. mybot starts only after the station has
a usable IP address; boards with a display show `WIFI SETUP` while provisioning.

- Zhengchen ML307 and Wi-Fi: short-press Boot to start/stop a conversation; hold Boot for 3 seconds
  to provision. The volume buttons adjust and persist speaker volume.
- ESP-VoCat: tap the display or short-press Boot to start/stop a conversation; hold either input for
  3 seconds to provision.
- Waveshare AMOLED 1.75 and 1.75C: tap the display or short-press Boot to start/stop a conversation;
  hold either input for 3 seconds to provision.
- CoreS3: short-touch the screen to start/stop a conversation; hold for 3 seconds to provision.
- StickS3: short-press the main button to start/stop a conversation; hold it for 3 seconds to
  provision.
- ReSpeaker Flex: short-press XIAO Boot or the XVF onboard button (GPI0/X1D09) to start/stop a
  conversation; hold either button for 3 seconds to provision.
- SenseCAP Watcher: rotate the encoder to adjust volume, short-press it to start/stop a conversation,
  and hold it for 3 seconds to provision.

A provisioning request stops mybot first. After Wi-Fi reconnects and obtains an IP address, the
firmware starts mybot again.

The default device-service endpoint is:

```text
https://mybot.sh2.agoralab.co/api
```

Use `idf.py -B <build-dir> menuconfig` and the `mybot` menu to select the language, endpoint, audio
packet duration, Cloud AEC, and AI QoS.

## Hardware Notes

### Zhengchen 1.54 TFT ML307 and Wi-Fi

| Capability | Pins/configuration |
| --- | --- |
| Microphone I2S1 RX | WS GPIO4, BCLK GPIO5, DIN GPIO6 |
| Speaker I2S0 TX | DOUT GPIO7, BCLK GPIO15, WS GPIO16 |
| Buttons | Boot GPIO0, volume up GPIO10, volume down GPIO39 |
| ST7789 | MOSI GPIO41, SCLK GPIO42, CS GPIO21, DC GPIO40, RESET GPIO45 |
| Backlight / power hold | GPIO20 / GPIO2 high |
| ML307 UART (`zhengchen-1.54tft-ml307` only) | ESP TX GPIO12, RX GPIO11 |

The two profiles share the display, audio, buttons, and GPIO2 power-hold implementation. The Wi-Fi
profile leaves GPIO11 and GPIO12 unconfigured. Its defaults select 16 MB QIO Flash and Octal PSRAM
at 80 MHz; confirm the physical PSRAM capacity from the startup log before release.

The mybot boundary is 16 kHz mono signed-16 PCM; the initial physical I2S link uses 16 kHz mono
32-bit left-slot words. The board's original speaker configuration uses 24 kHz output, so
real-device validation must confirm playback speed, pitch, stability, and bidirectional audio. If
the hardware requires 24 kHz output, keep the SDK boundary at 16 kHz and add stateful resampling
inside the Board audio driver.

GPIO9 charge status, GPIO8 battery ADC, temperature monitoring, automatic sleep, and low-power
behavior are not part of the initial Wi-Fi profile.

### Espressif ESP-VoCat

| Capability | Pins/configuration |
| --- | --- |
| Shared I2C0 | SDA GPIO2, SCL GPIO1; ES8311 `0x18`, ES7210 `0x40`, CST816S `0x15` |
| ES7210/ES8311 I2S0 | MCLK GPIO42, BCLK GPIO40, WS GPIO39, DOUT GPIO41 |
| ST77916 QSPI | SPI2, CLK GPIO18, CS GPIO14, D0-D3 GPIO46/GPIO13/GPIO11/GPIO12, backlight GPIO44 |
| Input / power | CST816S INT GPIO10, Boot GPIO0, peripheral power GPIO9 active low |

The profile detects its PCB revision before initializing the display or audio. V1.0 uses audio DIN
GPIO15, PA GPIO4, and an active-low LCD reset on GPIO3. V1.2 uses DIN GPIO3, PA GPIO15, and an
active-high LCD reset on GPIO47. If neither GPIO48 power state exposes the ES8311 at `0x18`, Board
preparation fails instead of selecting a potentially destructive fallback pin map.

The 360 x 360 round display uses the panel-specific ST77916 initialization sequence and a strip DMA
renderer; no framebuffer, LVGL, or external animation assets are required. USB Serial/JTAG owns the
console because the normal ESP32-S3 UART0 pins overlap the backlight and board LED.

The physical audio link runs directly at mybot's 16 kHz boundary. Capture and playback share a
two-slot standard-I2S clock domain; capture exposes only the primary microphone slot and playback
duplicates mono samples into both output slots. The second microphone, playback-reference channel,
local AEC, battery gauge, IMU, PCB capacitive slider, SD card, camera expansion, and low-power
behavior are outside the initial profile. Current production material identifies a 16 MB PSRAM
module, but release hardware must confirm detected PSRAM capacity and both PCB pin maps from startup
logs.

### Waveshare ESP32-S3 Touch AMOLED 1.75 and 1.75C

| Capability | Pins/configuration |
| --- | --- |
| Shared I2C0 | SDA GPIO15, SCL GPIO14; AXP2101 `0x34`, ES8311 `0x18`, ES7210 `0x40`, CST9217 `0x5a` |
| ES7210/ES8311 I2S0 | BCLK GPIO9, WS GPIO45, DIN GPIO10, DOUT GPIO8; PA GPIO46 |
| CO5300 QSPI | SPI2, CS GPIO12, CLK GPIO38, D0-D3 GPIO4/GPIO5/GPIO6/GPIO7 |
| Input | CST9217 INT GPIO11; Boot GPIO0 |

| Profile | Audio MCLK | CO5300 reset | CST9217 reset | TCA9554 |
| --- | --- | --- | --- | --- |
| `esp32-s3-touch-amoled-1.75` | GPIO42 | GPIO39 | GPIO40 | Optional probe at `0x20` |
| `esp32-s3-touch-amoled-1.75c` | GPIO16 | GPIO1 | GPIO2 | Not probed or required |

Both profiles use a 466 x 466 CO5300 panel with a (6, 0) display offset and share the power, audio,
display, and input implementations. They are separate firmware targets because the three
variant-specific pins are not interchangeable. Do not flash one profile onto the other revision.

The 1.75C defaults deliberately use 16 MB QIO Flash, 8 MB Octal PSRAM at 80 MHz, and
`partitions/v2/16m.csv`. Its current product page describes 32 MB Flash while the reference firmware
targets 16 MB and available hardware materials conflict. Confirm the detected Flash capacity in the
startup log before adding a larger partition layout; the conservative profile is safe on either
capacity.

The initial audio path configures the physical link directly at mybot's 16 kHz mono signed-16
boundary, captures the ES7210 primary microphone slot, and keeps Cloud AEC enabled. The playback
reference channel and local AEC are not exposed. RTC, IMU, TF card, battery reporting, automatic
sleep, and power-off gestures are outside the initial profiles. The 1.75C profile also has no
TCA9554 integration.

### M5Stack CoreS3

| Capability | Pins/configuration |
| --- | --- |
| Shared I2C1 | SDA GPIO12, SCL GPIO11 |
| ES7210/AW88298 I2S0 | MCLK GPIO0, BCLK GPIO34, WS GPIO33, DIN GPIO14, DOUT GPIO13 |
| ILI9342 SPI3 | MOSI GPIO37, SCLK GPIO36, CS GPIO3, DC GPIO35 |
| FT6336 / AXP2101 / AW9523 | I2C addresses `0x38` / `0x34` / `0x58` |

GPIO0 is audio MCLK on CoreS3 and is not used as a button. CoreS3 has no dedicated volume keys;
the firmware restores its persisted device volume at startup.

### M5Stack StickS3

| Capability | Pins/configuration |
| --- | --- |
| Shared I2C0 | SDA GPIO47, SCL GPIO48; M5PM1 at `0x6e`, ES8311 at `0x18` |
| ES8311 I2S0 | MCLK GPIO18, BCLK GPIO17, WS GPIO15, DIN GPIO16, DOUT GPIO14 |
| ST7789P3 SPI3 | MOSI GPIO39, SCLK GPIO40, CS GPIO41, DC GPIO45, RESET GPIO21, backlight GPIO38 |
| Input | Main button GPIO11, active low |

M5PM1 G2 powers both the display and codec and remains enabled while the firmware runs. G3 enables
the speaker amplifier only during playback. The display uses a 135 x 240 window at offset (52, 40).
The initial profile leaves GPIO12, the IMU, infrared functions, battery reporting, shutdown gestures,
and low-power behavior unused.

### ReSpeaker Flex XVF3800 Circular-4 with XIAO ESP32S3

This profile targets the
[Seeed ReSpeaker Flex Circular-4 assembly](https://wiki.seeedstudio.com/respeaker_flex_introduction/)
with a XIAO ESP32S3.

| Capability | Pins/configuration |
| --- | --- |
| XVF3800 I2S0 | BCLK GPIO8, WS GPIO7, DOUT GPIO44, DIN GPIO43 |
| Shared I2C0 | SDA GPIO5, SCL GPIO6, 400 kHz |
| XVF3800 / AIC3104 | I2C addresses `0x2c` / `0x18` |
| Buttons | XIAO Boot GPIO0; XVF onboard button GPI0/X1D09 through I2C resource 36 |

The XVF3800 must be flashed separately with the Circular-4 **16 kHz, two-channel I2S firmware**
before this profile can capture or play audio. The ESP32-S3 is the I2S slave. GPIO43 and GPIO44 are
reserved for I2S, so use USB Serial/JTAG for the console. XVF3800 performs the acoustic processing;
Cloud AEC is disabled for this profile to avoid applying AEC twice.

### SenseCAP Watcher

| Capability | Pins/configuration |
| --- | --- |
| ES8311/ES7243E I2S0 | MCLK GPIO10, BCLK GPIO11, WS GPIO12, DIN GPIO15, DOUT GPIO16 |
| Shared I2C0 | SDA GPIO47, SCL GPIO48; TCA9555 at `0x21` |
| SPD2010 QSPI | CLK GPIO7, D0 GPIO9, D1 GPIO1, D2 GPIO14, D3 GPIO13, CS GPIO45 |
| Input / backlight | Encoder GPIO41/GPIO42, encoder button on TCA9555 P0.3, backlight GPIO8 |

The profile drives the codec bus directly at mybot's 16 kHz mono signed-16 boundary. TCA9555 owns
the system, LCD, and codec-amplifier power sequence. The dedicated 32 MB partition table preserves
the factory-data region and provides two 4 MB OTA slots. Camera, touch, LED, battery reporting,
power-off gestures, and automatic sleep are not part of the initial port.

## Repository Layout

```text
components/agora_rtc/        ESP32-S3 Agora RTSA package
components/aosl/             AOSL with ESP32-S3 platform integration
components/mybot_sdk/        ESP-IDF build wrapper for the mybot SDK
components/mybot_platform/   Common services, reusable drivers, and board profiles
third_party/mybot/           Pinned mybot public headers and core sources
main/                        Firmware entry point and project Kconfig
```

## Validation and Limitations

CI builds all board profiles, both languages, and 20/40/60 ms audio packet durations where
applicable. M5Stack CoreS3 provisioning and bidirectional voice interaction have been validated on
real hardware. The Zhengchen Wi-Fi, ESP-VoCat, both Waveshare AMOLED 1.75 revisions, M5Stack
StickS3, ReSpeaker Flex, and SenseCAP Watcher profiles have not yet completed real-device
validation; a successful build is not a substitute for hardware validation on a release device.

Known limitations:

- ML307/4G networking and local wake words are not wired up.
- Zhengchen charge status, battery ADC, temperature monitoring, automatic sleep, and low-power
  operation are not wired up. The Wi-Fi profile's physical PSRAM capacity is not yet confirmed.
- ESP-VoCat battery reporting, IMU, PCB capacitive controls, SD card, LED, camera expansion, local
  AEC, reference audio, shutdown, and low-power operation are not wired up. PCB V1.0 also has a
  known hardware power-integrity issue that firmware cannot correct.
- Waveshare AMOLED 1.75 and 1.75C do not yet expose the playback reference channel, local AEC,
  battery reporting, or low-power operation. RTC, IMU, TF card, and TCA9554 are not supported on
  the 1.75C profile.
- CoreS3 camera, battery reporting, and automatic sleep are not wired up.
- StickS3 GPIO12, IMU, infrared functions, battery reporting, shutdown gestures, and low-power
  operation are not wired up.
- ReSpeaker Flex support is limited to Circular-4 and requires separately flashed XVF3800 16 kHz
  I2S firmware; Linear-4, XVF3800 firmware update, LED-ring status, and LCD output are not
  implemented.
- SenseCAP Watcher camera, touch, LED, battery reporting, shutdown, and low-power operation are not
  implemented. Its factory-data partition must be preserved when flashing.
- NVS encryption, Flash encryption, and Secure Boot belong to the product provisioning process.

## Documentation

- [Board porting](docs/BOARD_PORTING.md)
- [Contributing](CONTRIBUTING.md)
- [Support](SUPPORT.md)
- [Release checklist](docs/RELEASING.md)
- [Vendored dependency baselines](VENDORED_SOURCES.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## License and Dependencies

Project-maintained code is licensed under [Apache-2.0](LICENSE) unless a file carries another SPDX
identifier. The repository also contains dependencies and media assets under separate licenses or
distribution terms, including AOSL and Agora RTSA. The root license does not override those terms.
Review [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and every bundled license before use or
redistribution.
