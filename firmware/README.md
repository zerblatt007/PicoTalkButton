# Firmware for PicoTalkButton

This firmware runs on the Raspberry Pi Pico (RP2040) using PlatformIO.

## Features

- Serial device acting as a push-to-talk button.
- Status LED support with standby breath-blink using PWM.

## Building and Uploading

```bash
cd firmware
platformio run --target upload

