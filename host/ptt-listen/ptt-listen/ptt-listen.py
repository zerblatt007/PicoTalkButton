#!/usr/bin/env python3
import os
import time
import subprocess
import serial
import serial.tools.list_ports
from datetime import datetime
import signal
import sys

LOGTAG = "[PTT]"
VENDOR_NAME = "Storvik IT"
PRODUCT_NAME = "PicoTalkButton"

def log(message):
    print(f"{datetime.now():%F %T} {LOGTAG} {message}", flush=True)

def cleanup(signum, frame):
    """Cleanup function to unmute all sources when the script exits."""
    log("Received termination signal. Unmuting all sources for safety.")
    set_mute(False)  # Unmute all sources
    sys.exit(0)  # Exit gracefully

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
    """Check if all sources are muted and return the overall state and changed sources."""
    global last_mute_states
    result = subprocess.run(
        ["pactl", "list", "short", "sources"],
        capture_output=True,
        text=True,
    )
    sources = result.stdout.strip().splitlines()

    all_muted = True
    changed_sources = []  # Track sources that caused the state to change

    for source in sources:
        source_name = source.split()[1]  # Second column is the source name
        mute_status = subprocess.run(
            ["pactl", "get-source-mute", source_name],
            capture_output=True,
            text=True,
        )
        is_muted = mute_status.stdout.strip().endswith("yes")

        # Check if this source's mute state has changed
        if source_name not in last_mute_states or last_mute_states[source_name] != is_muted:
            changed_sources.append((source_name, is_muted))  # Track the change
            last_mute_states[source_name] = is_muted  # Update the state

        # If any source is unmuted, the system is not fully muted
        if not is_muted:
            all_muted = False

    return all_muted, changed_sources

def find_device():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.manufacturer == VENDOR_NAME and port.product == PRODUCT_NAME:
            return port.device
    return None

def main():
    # Track whether the device is initialized
    device_initialized = False
    last_overall_mute_status = None

    # Register signal handlers for graceful shutdown
    signal.signal(signal.SIGTERM, cleanup)  # Handle systemd stop
    signal.signal(signal.SIGINT, cleanup)   # Handle Ctrl+C or manual interruption

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
                startup_ok = False
                start_time = time.time()

                # Initialize mute state and LED state
                current_mute, _ = get_mute_status()
                try:
                    ser.write(b"MUTE\n" if current_mute else b"UNMUTE\n")
                    log(f"Sent initial mute state: {'MUTE' if current_mute else 'UNMUTE'}")
                    last_overall_mute_status = current_mute
                except Exception as e:
                    log(f"Failed to send initial mute state: {e}")
                    continue

                while time.time() - start_time < 5:
                    line = ser.readline().decode("utf-8", errors="ignore").strip()
                    if line:
                        log(f"Received: {line}")
                        if "Device enabled" in line and not device_initialized:
                            startup_ok = True
                            device_initialized = True  # Mark the device as initialized
                            break

                if not startup_ok:
                    log("No startup line received. Retrying...")
                    continue

                last_status_check = time.time()

                while True:
                    now = time.time()

                    # Poll for mute status every second
                    if now - last_status_check > 1.0:
                        last_status_check = now
                        current_mute, changed_sources = get_mute_status()

                        # Check for overall mute state change
                        if current_mute != last_overall_mute_status:
                            if changed_sources:
                                source_changes = ", ".join(
                                    [f"{'Muted' if muted else 'Unmuted'}: {source}" for source, muted in changed_sources]
                                )
                                log(f"System mute changed -> {'mute' if current_mute else 'unmute'} ({source_changes})")
                            else:
                                log(f"System mute changed -> {'mute' if current_mute else 'unmute'}")
                            last_overall_mute_status = current_mute

                        # Send mute/unmute command only on change
                        try:
                            ser.write(b"MUTE\n" if current_mute else b"UNMUTE\n")
                        except Exception as e:
                            log(f"Failed to send mute status: {e}")

                    # Read incoming line from serial
                    line = ser.readline().decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue

                    if "PTT pressed" in line:
                        log("PTT pressed -> unmute")
                        set_mute(False)  # Unmute all sources
                        ser.write(b"UNMUTE\n")
                        last_overall_mute_status = False
                    elif "PTT released" in line:
                        log("PTT released -> mute")
                        set_mute(True)  # Mute all sources
                        ser.write(b"MUTE\n")
                        last_overall_mute_status = True
                    elif "Device disabled" in line:
                        log("Device disabled -> unmute for safety")
                        set_mute(False)
                        ser.write(b"UNMUTE\n")
                        device_initialized = False  # Mark the device as uninitialized
                        last_overall_mute_status = False
                    elif "Device enabled" in line:
                        if not device_initialized:  # Ignore repeated "Device enabled" messages
                            log("Device enabled -> mute until pressed")
                            set_mute(True)  # Mute all sources
                            ser.write(b"MUTE\n")
                            device_initialized = True
                            last_overall_mute_status = True
                    elif line in ("Received MUTE from host", "Received UNMUTE from host"):
                        pass  # Ignore echoed commands
                    else:
                        log(f"Unknown message: {line}")

        except serial.SerialException as e:
            log(f"Serial exception: {e}")
            log("Device disconnected -> unmute for safety")
            set_mute(False)
            device_initialized = False  # Mark the device as uninitialized
            time.sleep(1)
        except Exception as e:
            log(f"Unexpected error: {e}")
            set_mute(False)
            device_initialized = False  # Mark the device as uninitialized
            time.sleep(1)

if __name__ == "__main__":
    main()
