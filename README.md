# PicoTalkButton

A DIY USB push-to-talk mute button using a Raspberry Pi Pico (RP2040) and a Python-based listener daemon on the host system.

## Features

- Appears as a USB Serial device (mute button).
- Color-coded LED status: green (unmuted), red (muted), blue (standby).
- Breath-blinking for standby mode.
- Host-side Python script detects button press and toggles mute via `pactl`.

## Repository Structure

- `firmware/` — PlatformIO-based firmware for RP2040 (USB CEC + LED).
- `host/ptt-listen/` — Host-side listener with systemd service and installer.

## License

MIT — see [LICENSE](LICENSE).

