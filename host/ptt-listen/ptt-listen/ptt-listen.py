#!/usr/bin/env python3
import os
import time
import subprocess
import serial
import serial.tools.list_ports
from datetime import datetime

LOGTAG = "[PTT]"
VENDOR_NAME = "Storvik IT"
PRODUCT_NAME = "PicoTalkButton"

def log(message):
    print(f"{datetime.now():%F %T} {LOGTAG} {message}", flush=True)

# A dictionary to keep track of the last known mute state of each source
last_mute_states = {}

def set_mute(mute: bool):
    """Mute or unmute all sources and log changes only when the state changes."""
    global last_mute_states
    result = subprocess.run(
        ["pactl", "list", "short", "sources"],
        capture_output=True,
        text=True,
    )
    sources = result.stdout.strip().splitlines()
    for source in sources:
        source_name = source.split()[1]  # Second column is the source name

        # Check the current mute status of the source
        mute_status = subprocess.run(
            ["pactl", "get-source-mute", source_name],
            capture_output=True,
            text=True,
        )
        is_muted = mute_status.stdout.strip().endswith("yes")

        # Only change and log if the state is different from the desired state
        if is_muted != mute:
            subprocess.run(["pactl", "set-source-mute", source_name, "1" if mute else "0"])
            log(f"{'Muted' if mute else 'Unmuted'} source: {source_name}")
            last_mute_states[source_name] = mute
        else:
            # Update the last known state without logging (no change)
            last_mute_states[source_name] = is_muted
    log(f"All sources {'muted' if mute else 'unmuted'} (only logged changes)")

def get_mute_status():
    """Check if all sources are muted and avoid unnecessary logs."""
    result = subprocess.run(
        ["pactl", "list", "short", "sources"],
        capture_output=True,
        text=True,
    )
    sources = result.stdout.strip().splitlines()
    all_muted = True
    for source in sources:
        source_name = source.split()[1]  # Second column is the source name
        mute_status = subprocess.run(
            ["pactl", "get-source-mute", source_name],
            capture_output=True,
            text=True,
        )
        is_muted = mute_status.stdout.strip().endswith("yes")
        if not is_muted:
            all_muted = False
    return all_muted


def find_device():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.manufacturer == VENDOR_NAME and port.product == PRODUCT_NAME:
            return port.device
    return None

def main():
    while True:
        set_mute(False)
        log("Device missing -> unmute for safety")
        log(f"Waiting for {VENDOR_NAME} {PRODUCT_NAME} to appear...")

        device_path = None
        while not device_path:
            device_path = find_device()
            if not device_path:
                time.sleep(1)

        log(f"Device found at {device_path}. Muting mic until pressed.")
        set_mute(True)

        try:
            with serial.Serial(device_path, baudrate=115200, timeout=1) as ser:
                # Wait for "Device enabled"
                startup_ok = False
                start_time = time.time()

                current_mute = get_mute_status()
                try:
                    ser.write(b"MUTE\n" if current_mute else b"UNMUTE\n")
                    log(f"Sent initial mute state: {'MUTE' if current_mute else 'UNMUTE'}")
                except Exception as e:
                    log(f"Failed to send initial mute state: {e}")
                    continue

                while time.time() - start_time < 5:
                    line = ser.readline().decode("utf-8", errors="ignore").strip()
                    if line:
                        log(f"Received: {line}")
                        if "Device enabled" in line:
                            startup_ok = True
                            break

                if not startup_ok:
                    log("No startup line received. Retrying...")
                    continue

                device_initialized = False
                last_mute_status = None
                last_status_check = time.time()

                while True:
                    now = time.time()

                    # Poll for mute status every second
                    if now - last_status_check > 1.0:
                        last_status_check = now
                        current_mute = get_mute_status()
                        try:
                            ser.write(b"MUTE\n" if current_mute else b"UNMUTE\n")
                        except Exception as e:
                            log(f"Failed to send mute status: {e}")
                        if current_mute != last_mute_status:
                            log(f"System mute changed -> {'mute' if current_mute else 'unmute'}")
                            last_mute_status = current_mute


                    # Read incoming line from serial
                    line = ser.readline().decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue

                    if "PTT pressed" in line:
                        log("PTT pressed -> unmute")
                        set_mute(False)
                        ser.write(b"UNMUTE\n")
                        last_mute_status = False
                    elif "PTT released" in line:
                        log("PTT released -> mute")
                        set_mute(True)
                        ser.write(b"MUTE\n")
                        last_mute_status = True
                    elif "Device disabled" in line:
                        log("Device disabled -> unmute for safety")
                        set_mute(False)
                        ser.write(b"UNMUTE\n")
                        device_initialized = False
                        last_mute_status = False
                    elif "Device enabled" in line:
                        if not device_initialized:
                            log("Device enabled -> mute until pressed")
                            set_mute(True)
                            ser.write(b"MUTE\n")
                            device_initialized = True
                            last_mute_status = True
                    #    else:
                    #        log("Ignoring repeated Device enabled")
                    elif line in ("Received MUTE from host", "Received UNMUTE from host"):
                        pass  # ignore echoed commands
                    else:
                        log(f"Unknown message: {line}")

        except serial.SerialException as e:
            log(f"Serial exception: {e}")
            log("Device disconnected -> unmute for safety")
            set_mute(False)
            time.sleep(1)
        except Exception as e:
            log(f"Unexpected error: {e}")
            set_mute(False)
            time.sleep(1)


if __name__ == "__main__":
    main()
