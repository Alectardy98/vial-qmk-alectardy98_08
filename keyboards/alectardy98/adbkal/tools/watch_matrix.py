#!/usr/bin/env python3
"""Read this keyboard's live VIA matrix and keymap without changing firmware/settings.

Requires Python's hid package (hidapi). Close Vial while capturing to avoid
competing readers on the same HID endpoint. Only this keyboard's raw HID
interface is opened; laptop input is never recorded.
"""
import argparse
import json
import time
from pathlib import Path

import hid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--seconds", type=float, default=60)
parser.add_argument("--serial", help="Select a particular board if several are connected")
parser.add_argument("--output", type=Path, help="Save identity, keymap and matrix transitions as JSON")
args = parser.parse_args()
if args.seconds <= 0:
    parser.error("--seconds must be positive")

devices = [d for d in hid.enumerate(0xFEED, 0x2040)
           if d["usage_page"] == 0xFF60 and d["usage"] == 0x61
           and (args.serial is None or d["serial_number"] == args.serial)]
if len(devices) != 1:
    parser.error(f"Expected one adbk raw HID interface; found {len(devices)}")

result = {"serial": devices[0]["serial_number"], "started": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
          "events": []}

with hid.Device(path=devices[0]["path"]) as device:
    def query(command):
        device.write(bytes([0] + command + [0] * (32 - len(command))))
        reply = device.read(32, timeout=1000)
        if len(reply) != 32 or list(reply[:len(command)]) != command:
            raise RuntimeError(f"Unexpected HID response: {reply.hex()}; close Vial and retry")
        return reply

    result["uptime_ms"] = int.from_bytes(query([2, 1])[2:6], "big")
    result["keymap"] = [[[int.from_bytes(query([4, layer, row, col])[4:6], "big")
                           for col in range(8)] for row in range(12)] for layer in range(3)]
    print(f"Connected {result['serial']}; uptime {result['uptime_ms']} ms", flush=True)
    print("Press and release keys. Positions are electrical matrix coordinates.", flush=True)
    start, previous = time.monotonic(), None
    try:
        while time.monotonic() - start < args.seconds:
            rows = list(query([2, 3])[2:14])
            if rows != previous:
                elapsed = round(time.monotonic() - start, 3)
                held = [[r, c] for r, bits in enumerate(rows) for c in range(8) if bits & (1 << c)]
                event = {"seconds": elapsed, "rows": rows, "held": held}
                result["events"].append(event)
                print(f"{elapsed:8.3f}s held={held}", flush=True)
                previous = rows
            time.sleep(0.005)
    except KeyboardInterrupt:
        pass
    finally:
        if args.output:
            args.output.write_text(json.dumps(result, indent=2) + "\n")
            print(f"Saved {args.output}", flush=True)
