# HSEC Solar Lights Device

ESP32/Arduino firmware for a solar light camera node. The device connects to Wi-Fi, opens a secure WebSocket connection to the backend, sends camera images and telemetry, and accepts remote light-control actions.

## Hardware Target

The current PlatformIO environment targets an ESP32-WROVER-E / MGN4R8 module with 4 MB flash and 8 MB PSRAM:

```ini
[env:freenove_esp32_wrover]
platform = espressif32
board = freenove_esp32_wrover
framework = arduino
```

The firmware expects PSRAM for camera frame buffers and uses these core peripherals:

- ESP32 camera module
- DHT sensor on GPIO 13
- Light output on GPIO 14

Camera pin defaults, capture defaults, and day/night sensor profiles live in `include/config.h`. Override the `CAMERA_*` defines there if this firmware is moved to a different ESP32 camera board.

## Configure

Non-secret device behavior lives in `include/config.h`, including pins, send intervals, NTP servers, timezone, WSS path, default light schedule, and camera settings.

Create `include/secrets.h`. This file is intentionally not committed because it contains Wi-Fi and backend credentials.

```cpp
#pragma once

#define SECRET_WIFI_SSID ""
#define SECRET_WIFI_PASS ""
#define SECRET_DEVICE_NAME ""
#define SECRET_HOST_ADDRESS ""
#define SECRET_HOST_PORT 443

// Leave empty unless the backend enforces one.
#define SECRET_DEVICE_TOKEN ""
```

`SECRET_DEVICE_NAME` is sent as the `deviceId` query parameter when the WSS connection is opened.

## Build, Upload, Monitor

From the repo root:

```powershell
pio run
pio run -t upload
pio device monitor
```

If `pio` is not on your PATH, use the PlatformIO virtualenv directly:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

## TLS Certificates

WSS certificate validation is enabled. PlatformIO embeds this generated ESP certificate bundle into the firmware:

```text
data/cert/x509_crt_bundle.bin
```

The source PEM bundle is:

```text
data/cert/cacrt_all.pem
```

To download a fresh Mozilla CA bundle from curl and regenerate the ESP `.bin` file:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\update_certs.ps1
```

To regenerate the `.bin` from the checked-in PEM without downloading anything:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\update_certs.ps1 -SkipDownload
```

After updating certs, rebuild and flash the firmware. Devices do not refresh CA certificates at runtime; they receive updated certificates through a new firmware image.

More detail is in [CERTS.md](CERTS.md).

## Runtime Behavior

On boot, the device:

- Loads persisted light/camera settings from EEPROM-backed storage.
- Initializes the camera and DHT sensor.
- Connects to Wi-Fi.
- Restarts if Wi-Fi, camera setup, or clock sync fails during startup.
- Syncs time with NTP before opening WSS so TLS certificate date validation can succeed.
- Opens `wss://SECRET_HOST_ADDRESS:SECRET_HOST_PORT/WSS_PATH?deviceId=SECRET_DEVICE_NAME`.

When connected, it sends:

- `deviceReady` metadata with supported actions.
- Stored state such as light mode and auto on/off times.
- Stored camera settings such as frame size and JPEG quality.
- Camera images as binary WebSocket messages with JSON metadata.
- Telemetry fields such as humidity, temperature, current time, and light state.

Supported remote actions include:

- `lightOff`
- `lightOn`
- `lightAuto`
- `light`
- `setOnTime`
- `setOffTime`
- `frame_size` with `QQVGA`, `HQVGA`, `QVGA`, `VGA`, `SVGA`, `XGA`, `SXGA`, or `UXGA`
- `quality` with an integer from `10` to `26`; lower values produce higher JPEG quality.

Times use `HH:MM:SS` format. The configured timezone is currently Mountain time with DST rules.

## Notes

- The firmware restarts if it cannot remain connected to the backend for about five minutes.
- The WebSocket client uses heartbeat pings and a reconnect interval.
- Certificate validation depends on successful NTP sync.
