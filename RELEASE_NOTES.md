# Firmware v8 source update — October 9, 2026

Adds a large white remaining-charge percentage above RESET, without an icon. Reads wheel BLE characteristic 500b byte 7 every five seconds; valid readings range from 0% to 100%. Automatically fits the text and redraws only when its value changes. Unavailable or stale readings display --%.

Includes WheelBattery.h. WheelConfig.h remains a private local file. Owner compilation, upload, and hardware confirmation of v8 are pending. The existing v1.0.0 release remains the tested v7 version.

# Superpedestrian_App_Fix v1.0.0

First release of the owner-tested v7 ESP32-S3 touchscreen controller: ECO / STANDARD / TURBO arrows, persistent BLE connection, large Turn On Bike disconnect screen, change-only field updates, calibrated MPH, persistent trip miles, and trip RESET.

Includes final Arduino source and a README documenting the complete investigation and measured 20-foot calibration. Configure WheelConfig.h locally before building. Credentials and credential-bearing firmware binaries are excluded. Tested on the custom Rev6 480x320 ILI9488/XPT2046 hardware; riding-speed accuracy remains to be checked.
