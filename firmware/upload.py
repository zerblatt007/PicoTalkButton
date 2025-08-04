#!/usr/bin/env python3
import os, time, shutil

mount_root = f"/media/{os.getenv('USER')}"
timeout = 10

print("Waiting for Pico (RPI-RP2) to appear...")
for _ in range(timeout):
    for entry in os.listdir(mount_root):
        if entry.startswith("RPI-RP2"):
            pico_path = os.path.join(mount_root, entry)
            uf2 = ".pio/build/picotalk_button/firmware.uf2"
            print(f"Found Pico at {pico_path}, copying {uf2}...")
            shutil.copy(uf2, pico_path)
            print("Done.")
            exit(0)
    time.sleep(1)

print("Pico not found, please put it in BOOTSEL mode.")
exit(1)
