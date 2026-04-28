#!/usr/bin/env python3
import os
import subprocess
import sys
import time

DRIVER_PATH = "driver/rumble.ko"
DEV_PATH = "/dev/rumble0"


def load_driver():
    result = subprocess.run(["lsmod"], capture_output=True, text=True)
    if "rumble" in result.stdout:
        print("Driver already loaded")
        if os.path.exists(DEV_PATH):
            subprocess.run(["sudo", "chmod", "666", DEV_PATH], check=False)
        return

    if not os.path.exists(DRIVER_PATH):
        print("Building driver...")
        subprocess.run(["make", "-C", "driver"], check=True)

    print(f"Loading {DRIVER_PATH}...")
    subprocess.run(["sudo", "insmod", DRIVER_PATH], check=True)
    time.sleep(0.5)

    if not os.path.exists(DEV_PATH):
        print(f"Error: {DEV_PATH} not created")
        sys.exit(1)

    subprocess.run(["sudo", "chmod", "666", DEV_PATH], check=False)
    print(f"Driver loaded, {DEV_PATH} ready")


def main():
    load_driver()

    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "rumble_dash"))
    from rumble_dash.ui.app import run
    run(DEV_PATH)


if __name__ == "__main__":
    main()
