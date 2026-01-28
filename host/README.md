# Host Listener: ptt-listen

This is the host-side component of the PicoTalkButton project — a Python-based listener daemon that communicates with the USB mute button (RP2040) and toggles the system microphone mute status using `pactl`.

## Features

- Listens to serial input from the RP2040 device
- Uses `pactl` to toggle microphone mute
- Systemd user service integration
- Supports Python virtual environments
- Simple user-level install and uninstall scripts

## Requirements

- Linux system with `pactl` (typically part of `pulseaudio-utils`)
- Python 3.7 or newer
- `pyserial` Python package (installed automatically)
- `systemd` user services enabled

## Making .deb package

### 1. Build the `.deb` package if you want to change something. 
Or else just use the prebuilt ptt-listen-x.xx.deb files in the next step.

```bash
dpkg-deb --build ptt-listen
```

This will create ptt-listen.deb in the current directory

#### Build .deb with version number:

```bash
PTTNAME="ptt-listen"; PTTVERSION=$(awk '/^Version:/ { print $2 }' $PTTNAME/DEBIAN/control); dpkg-deb --build ptt-listen "ptt-listen-$PTTVERSION.deb"
```

### 2. Extract and install locally

```bash
dpkg -x ptt-listen.deb ~/.local/share/ptt-listen-install
bash ~/.local/share/ptt-listen-install/ptt-listen/install.sh
```

The installer will prompt you to:

    - Choose a Python environment (system, virtualenv, or custom path)
    - Install pyserial if needed
    - Set up the systemd user service

### 3. Start the service


```bash
systemctl --user start ptt-listen.service
```

To enable it on boot:

```bash
systemctl --user enable ptt-listen.service
```

### 4. Plug in PTT USB button 
  If you have not done it already..

  Start Muting! And check logs to know what is happening..

## Uninstallation


```bash
bash ~/.local/share/ptt-listen-install/ptt-listen/uninstall.sh
```

This will stop/disable the service and remove the files.

## Development tips


To test manually without systemd:

```bash
~/.local/share/ptt-listen/ptt-listen/ptt-listen.py
```

To monitor logs:

```bash
journalctl --user -fu ptt-listen.service
```

## LICENSE
MIT
