# Firmware for PicoTalkButton

This firmware runs on the Raspberry Pi Pico (RP2040) using PlatformIO.  

This development version is running on a DFRobot DFR0959 board like this: https://no.mouser.com/ProductDetail/DFRobot/DFR0959?qs=Jm2GQyTW%2FbjZmmIfIrcQfg%3D%3D

A specific board for this application is in the works.

## Features

- Serial device acting as a push-to-talk button.
- Status LED support with standby breath-blink using PWM.

## Building and Uploading

```bash
cd firmware
platformio run --target upload

