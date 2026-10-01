#pragma once
// Copy to WheelConfig.h, then fill in YOUR wheel identity and 16-byte passcode.
// WheelConfig.h is excluded from Git and release archives.
#error "Configure your wheel, then remove this #error line."
const char* wheelAddress="00:00:00:00:00:00";
const char* wheelBoardSerial="00000000000000"; // exactly 14 ASCII characters
const uint8_t wheelPasscode[16]={0}; // replace all 16 bytes
