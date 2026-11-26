#!/usr/bin/env python3
"""
setup_boards.py -- flash and configure ESP32-C6 boards, one plug-in each.

For each board you plug in (COM port, one board at a time), this:
  1. flashes the TX or RX firmware,
  2. types the settings into the board's serial console (saved in its flash),
  3. reboots it and checks it joins your Wi-Fi (and, for receivers, the hub).

Your Wi-Fi password is asked once with a hidden prompt, sent only to the
boards over USB, and never written to disk or printed.

  python3 setup_boards.py --role tx
  python3 setup_boards.py --role rx --first 1        # names them rx-1, rx-2, ...
"""

import argparse
import getpass
import glob
import os
import re
import subprocess
import sys
import time

import serial

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT_GLOBS = ["/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/cu.wchusbserial*"]


def board_ports():
    return sorted(p for g in PORT_GLOBS for p in glob.glob(g))


def laptop_ip():
    out = subprocess.run(["ipconfig", "getifaddr", "en0"], capture_output=True, text=True)
    return out.stdout.strip()


def read_for(ser, seconds, until=None):
    """Read serial output for up to `seconds`, stopping early once `until` appears."""
    end, out = time.time() + seconds, ""
    while time.time() < end:
        out += ser.read(4096).decode("utf-8", "replace")
        if until and re.search(until, out):
            break
    return out


def flash(role, port):
    print(f"  flashing {role} firmware ...", end="", flush=True)
    r = subprocess.run(["pio", "run", "-t", "upload", "--upload-port", port],
                       cwd=os.path.join(ROOT, "firmware", role), capture_output=True, text=True)
    if r.returncode != 0:
        print(" FAILED")
        print("\n".join(r.stdout.splitlines()[-15:]))
        print("  If it couldn't connect: hold BOOT, tap RESET, release BOOT, then try again.")
        return False
    print(" done")
    return True


def configure(port, settings):
    """Type KEY: value lines into the board's console, then reboot and check it comes up."""
    with serial.Serial(port, 921600, timeout=0.2) as ser:
        ser.dtr, ser.rts = False, True          # hard reset so we catch the boot prompt
        time.sleep(0.1)
        ser.rts = False
        read_for(ser, 6, until=r"Type KEY")
        for key, value in settings:
            ser.write(f"{key}: {value}\n".encode())
            reply = read_for(ser, 2, until=rf"(saved|failed to save) {key}")
            if f"saved {key}" not in reply:
                print(f"  ! board did not confirm {key}; reply was: {reply.strip()[-120:]!r}")
                return None
        ser.write(b"REBOOT\n")
        return read_for(ser, 15, until=r"MQTT connected|joined \"")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--role", choices=["tx", "rx"], required=True)
    ap.add_argument("--first", type=int, default=1, help="number of the first receiver (rx-N)")
    ap.add_argument("--hub-ip", default=None, help="laptop IP for receivers (default: this Mac's en0 address)")
    ap.add_argument("--no-flash", action="store_true", help="only configure, don't flash")
    args = ap.parse_args()

    ssid = input("Wi-Fi network name (2.4 GHz): ").strip()
    password = getpass.getpass("Wi-Fi password (hidden, not saved): ")
    hub_ip = args.hub_ip or laptop_ip()
    if args.role == "rx":
        print(f"Receivers will send to the hub at {hub_ip}. (Override with --hub-ip.)")

    n = args.first
    done = []
    while True:
        name = "tx" if args.role == "tx" else f"rx-{n}"
        if input(f"\nPlug in the board for {name} by its COM port, then press Enter (q to finish): ").strip().lower() == "q":
            break
        ports = board_ports()
        if len(ports) != 1:
            print(f"  Found {len(ports)} boards ({', '.join(ports) or 'none'}). Plug in exactly one and try again.")
            continue
        port = ports[0]
        if not args.no_flash and not flash(args.role, port):
            continue
        settings = [("STASSID", ssid), ("STAPASS", password)]
        if args.role == "rx":
            settings += [("NODEID", name), ("MQTTHOST", hub_ip)]
        print("  configuring ...", end="", flush=True)
        boot = configure(port, settings)
        if boot is None:
            continue
        joined = re.search(r'joined "[^"]*": ip ([\d.]+), channel (\d+)', boot)
        if joined:
            print(f" joined Wi-Fi as {joined.group(1)} on channel {joined.group(2)}")
        else:
            print(" saved, but it hasn't joined Wi-Fi yet (check the network name and password, "
                  "and that the network is 2.4 GHz)")
        if args.role == "rx":
            print("  MQTT: " + ("connected to the hub" if "MQTT connected" in boot
                                else "not connected yet (is mosquitto running on this Mac?)"))
        print(f"  Label this board '{name}' and unplug it.")
        done.append(name)
        if args.role == "tx":
            break
        n += 1

    print(f"\nSet up: {', '.join(done) or 'nothing'}")


if __name__ == "__main__":
    sys.exit(main())
