# Superpedestrian_App_Fix

A standalone ESP32-S3 touchscreen handlebar controller for a Superpedestrian Copenhagen Wheel. It replaces the phone's ride-control interface with ECO / STANDARD / TURBO arrows, wheel-derived speed, and resettable trip distance.

**Release v1.0.0 — working firmware v7, October 1, 2026.** Built and uploaded to an ESP32-S3 on Windows COM17. The bike owner confirmed the final speed, distance, reset, and mode interface worked.

## Why this project exists

The owner bought a bicycle fitted with a Copenhagen Wheel. The original phone app could no longer log in, while a purchased third-party RedWheel app worked on the owner's iPhone. The owner wanted family members to ride without borrowing that phone, using an independently powered touchscreen mounted on the handlebars.

This project supplies Arduino firmware for that controller. It is not an iPhone app and does not require the original online account login. Authentication still requires the wheel's own identity and Bluetooth passcode.

## Final interface

- Centered TERRY heading and ECO < STANDARD < TURBO guide.
- Left/right arrows select the adjacent assistance mode. The confirmed mode is centered between them.
- MPH shown to one decimal place; TRIP MILES shown to three decimal places.
- RESET clears only trip distance and immediately saves zero.
- One authenticated BLE connection stays open while the wheel is on. The controller retries automatically when disconnected.
- Disconnection replaces the dashboard with large white **Turn On Bike** text.
- Only changed fields are erased and redrawn. Idle mode checks do not flash the screen.
- The temporary 20-foot CAL / STORE interface has been removed from the final firmware.

## Hardware used

- Custom Tommie Berry Rev6 PCB with ESP32-S3.
- ILI9488 480 × 320 TFT, landscape, and XPT2046 resistive touchscreen.
- Standard USB power/data cable; independent USB battery power for handlebar use.
- Copenhagen Wheel paired and tested under the name Terry.

The alternative S3 SuperMini board was considered but not used in this release. A brighter display would need compatible wiring, driver, and backlight hardware. Software already drives this PCB's backlight fully on.

### Rev6 pin map

| Device/function | ESP32-S3 GPIO |
| --- | --- |
| TFT SCK / MOSI | 10 / 11 |
| TFT CS / DC / RESET | 14 / 12 / 13 |
| TFT backlight, active high through JP4 | 35 |
| Touch SCK / MISO / MOSI | 21 / 42 / 9 |
| Touch CS / IRQ | 18 / 7 |
| Unused SD CS, held high | 41 |
| NeoPixel chain, 12 LEDs (cleared at startup) | 8 |

The TFT has no separate MISO connection in this setup. Keep **PSRAM Disabled**: GPIO35 is used for the Rev6 backlight. The sketch includes a compile-time check for this.

## Files and wheel configuration

Keep the `.ino` and headers together in an Arduino sketch folder named `Superpedestrian_App_Fix`:

```text
Superpedestrian_App_Fix/
  Superpedestrian_App_Fix.ino
  BoardDisplay.h
  WheelPresets.h
  WheelTelemetry.h
  WheelConfig.example.h
  WheelConfig.h                 # YOUR local copy; never committed
```

1. Copy `WheelConfig.example.h` to `WheelConfig.h`.
2. Enter your wheel's public BLE address, 14-character electronics-board serial, and exact 16-byte wheel passcode.
3. Remove the template's `#error` after configuring it.
4. Keep `WheelConfig.h` private. The included `.gitignore` excludes it.

The public source preserves the working v7 logic and captured mode presets, with the original wheel's credentials moved out of the published code. A configured build is required; no firmware binary containing the owner's passcode is distributed. Do not use the zero-filled template as a real credential.

The passcode is distinct from an account email/password or an iPhone's Bluetooth address. This release does not perform an online login or include account credentials. Mode packets were captured from the owner's wheel; other wheel configurations may require their own verified presets.

## Arduino setup and build

Tested board package: **esp32 by Espressif Systems 3.3.12**.

| Arduino setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| Flash size | 16 MB |
| PSRAM | Disabled |
| USB CDC On Boot | Disabled |
| Port used during development | COM17 (select your actual port) |
| Serial monitor | 115200 baud |

Libraries used from the already-working Kingpan charger project:

| Library | Installed version used |
| --- | --- |
| ILI9488 | 1.0.2 |
| XPT2046_Touchscreen | 1.4 |
| Adafruit NeoPixel | 1.15.5 |
| Adafruit GFX Library | 1.12.6 |

BLE, Preferences, SPI, and Arduino support come from the ESP32 board package. This repository does not bundle third-party libraries; use the matching library implementations that provide these headers.

Open the sketch, configure your wheel, select the settings above, and upload. Close any serial monitor if it prevents uploading.

Equivalent Arduino CLI commands, after installing the dependencies and configuring the wheel:

```powershell
arduino-cli compile --jobs 4 --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=disabled --output-dir build .
arduino-cli upload --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=disabled --port COM17 --input-dir build .
```

## Using the controller

Wake the wheel and disconnect RedWheel or other BLE clients. Power the controller and wait for the dashboard. The initial connection reads the current assistance mode without changing it. Tap an arrow to move one step between ECO, STANDARD, and TURBO.

The controller's persistent connection may prevent the phone app or Windows BLE tools from connecting until the controller is powered off. Only one client was used at a time during development.

Tap RESET to start a new trip. Distance is checkpointed to ESP32 flash every 30 seconds when changed, and saved when telemetry is lost or the wheel disconnects. Abrupt loss of controller power can lose the most recent 30 seconds. Reset itself is saved immediately. The device resumes the last saved trip after reboot.

Touch coordinates use a saved three-point affine calibration. If it is missing, tap the screen and follow the crosses. Holding the TERRY heading for two seconds while idle still recalibrates touch; this is separate from the removed distance-calibration page.

### Serial commands (115200 baud)

| Character | Action |
| --- | --- |
| `r` | Read the wheel's assistance mode |
| `d` | Print telemetry validity, speed in tenths of mph, and trip millimetres |
| `z` | Reset trip distance |
| `c` | Run touch calibration while idle |

## Bluetooth investigation and control

The initial question was whether traffic from the working iPhone app could reveal the protocol. iPhone BLE scanner apps were available, but the investigation used direct authenticated GATT discovery from Windows and existing public Copenhagen Wheel protocol work, rather than claiming the phone scanners captured another app's traffic. No Mac was required.

GATT UUIDs use the base `52756265-6e43-6167-6e69-65435048` plus these suffixes:

| Suffix | Role used here |
| --- | --- |
| `5000` | Controller service |
| `5100` | Authentication service |
| `5105` | Electronics-board serial, read and checked |
| `5101` | Authentication write, 16-byte wheel passcode |
| `5104` | Access state; accepted exact byte `01` |
| `500a` | Assistance-mode configuration, 20-byte read/write |
| `5002` | Motion telemetry, 20-byte read |

The app did not ask the owner for a passcode. Public protocol data was investigated and an exact serial/passcode match was verified locally. The credential is deliberately absent from this publication.

ECO, STANDARD, and TURBO packets were captured after the owner selected each mode in RedWheel. Their CRCs were checked using the public protocol implementation. The firmware checks the first 18 configuration bytes when identifying mode because read responses may omit the CRC. It reads the current mode before writing the adjacent preset, then reads back to verify. Unrecognized configurations are not overwritten. Queued arrow taps are discarded after losing the connection.

This firmware does not change wheel firmware, lock settings, or speed limits.

## Speed and distance calibration

Read-only captures showed the first little-endian 16-bit word of `5002` was zero at rest, rose during rolling, and returned to zero when stopped. A possible counter in bytes 6–9 also advanced, but its short-distance resolution was too coarse for a precise conversion.

To establish a scale without adding a sensor, a temporary firmware page provided RESET and STORE. The owner reset while stationary, walked the bike exactly **20 feet (6.096 m)**, stopped, stored the record in flash, and returned the controller to COM17. Retrieval produced **30 samples over 14.782 seconds, with no detected read/connection gaps**.

Trapezoidal integration of the raw speed yielded 12192.115 raw-unit seconds. Applying:

```text
metres/second = raw_speed × 0.0005
mph          = raw_speed × 0.0011184681460272
```

produced **6.0960575 m (20.00019 ft)** for the measured 20-foot rollout. This is a close walking-test match, not a claim of that accuracy under all riding conditions. Riding-speed accuracy remains to be independently checked.

The final firmware reads motion telemetry approximately every 500 ms and integrates speed using measured elapsed time. It rejects integration across failed reads, disconnects, and sample intervals over two seconds. Invalid or stale speed displays `--`; there is no distance accumulation across a disconnected interval. It uses actual elapsed time rather than a fixed distance increment per notification.

## Session development summary

1. Inspected the Rev6 hardware and reused the display/touch libraries from the completed Kingpan CAN charger work.
2. Discovered the wheel from Windows, verified identity and local BLE authentication, and captured the three known-good assistance presets.
3. Implemented working touchscreen arrows, then changed transaction-by-transaction connections to one persistent connection.
4. Centered the mode text, TERRY heading, and mode guide; removed unused assistance/status lines.
5. Eliminated periodic full-screen refreshing by caching each visible field.
6. Investigated wheel-derived motion telemetry instead of adding a GPS or wheel sensor.
7. Added the temporary portable 20-foot recorder and retrieved the stored calibration.
8. Removed that temporary interface and added mph, persistent trip miles, and trip reset.
9. Compiled, uploaded, checked live stationary telemetry/reset, and received the owner's confirmation: **“It works perfect!”**

Internal discovery scripts, raw captures, backups, account screenshots, and credentials remain local. This release contains the final source and its documentation.

## Validation and limits

Working private v7 build: **663305 bytes flash; 30224 bytes static RAM**, with ESP32 core 3.3.12. Upload to COM17 completed with flash hash verification. Live checks showed valid telemetry, zero speed at rest, and trip reset returning zero. The owner confirmed the final touchscreen interface works on the tested bicycle.

Hardware validation applies to that Rev6 PCB and wheel. Other displays, GPIO maps, firmware versions, or presets need their own verification. The stored trip is an integrated travel estimate, not a lifetime wheel odometer. Reverse-motion interpretation has not been established. No automatic sensor-based or GPS correction is included.

## References and acknowledgements

- [subparpedestrian/cphw-pc](https://github.com/subparpedestrian/cphw-pc), inspected at commit `6daaf44d7f6f3dd5e4fef777dae4269646db22dd`: protocol/authentication research and CRC comparison.
- [Copenhagen Wheel web implementation R1.03](https://tb47.me/chw/R1.03/): motion-characteristic research; its empirical speed adjustment and fixed-per-event distance algorithm were not adopted.
- Espressif ESP32 Arduino core; ILI9488, XPT2046_Touchscreen, Adafruit GFX, and Adafruit NeoPixel libraries retain their own licenses.

The released firmware was developed collaboratively with Tommie Berry and Codex. It is an independent project and is not affiliated with Superpedestrian or RedWheel. No third-party source trees or library binaries are included, and no new license is assigned to this publication.
