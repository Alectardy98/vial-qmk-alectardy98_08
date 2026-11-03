#!/usr/bin/env python3
"""Capture supported Release 1 Main settings through GET-only HID commands.

Close VialAstro first. No flash, reset, setters, storage files or input reports.
The two independently encoded EEPROM images must agree before publication.
"""
import argparse
import datetime
import fcntl
import importlib.util
import json
from pathlib import Path
import struct
import time

BOARD = Path(__file__).resolve().parents[1] / "main"
SPEC = importlib.util.spec_from_file_location("defaults", Path(__file__).with_name("generate-defaults.py"))
DEFAULTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DEFAULTS)


def allowed(q):
    if q[0] in (1, 12, 13, 14, 18):
        return True
    if q[:2] == b"\x02\x02":
        return True
    if q[0] == 254:
        return q[1] in (0, 1, 2, 9, 10) or (q[1] == 13 and q[2] in (0, 1, 3, 5, 7))
    if q[0] != 253:
        return False
    if q[1] in (9, 11, 13, 28, 29, 33, 58, 61, 66, 69, 75):
        return True
    if q[1] in (234, 240):
        return q[2] == 0 or (q[1] == 240 and q[2] == 1)
    if q[1] in (231, 233, 235):
        return q[2] in (0, 1)
    if q[1] == 230:
        return q[2] == 5
    # ANI selection action 0 only queries the current slot; nonzero changes it.
    return q[1] == 237 and (q[2] in (0, 2) or
                            (q[2] == 1 and q[5:8] == b"\x09\0\0"))


def capture(get, schema, layout):
    x = {}
    x["device"] = get([253, 240, 0, 0])[4:].split(b"\0")[0].decode("ascii")
    x["diag"] = get([253, 240, 1, 0]).hex()
    x["nv"] = get([253, 234, 0, 1]).hex()
    x["performance"] = get([253, 75]).hex()
    x["layout"] = get([2, 2]).hex()
    x["qmk"] = {key: get(bytes([254, 10]) + struct.pack("<H", int(key))).hex()
                for key in schema["qmk"]}
    counts = get([254, 13, 0])
    x["dynamic_counts"] = counts.hex()
    x["dynamic"] = {str(op): [get([254, 13, op, i]).hex() for i in range(counts[j])]
                    for j, op in enumerate((1, 3, 5, 7))}
    x["keymap"] = [get(bytes([18]) + struct.pack(">HB", at, min(28, 1024-at))).hex()
                   for at in range(0, 1024, 28)]
    n = layout["DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE"]
    assert int.from_bytes(get([13])[1:3], "big") == n, "Macro layout changed"
    x["macros"] = [get(bytes([14]) + struct.pack(">HB", at, min(28, n-at))).hex()
                   for at in range(0, n, 28)]
    x["rgb_scalars"] = {str(op): get([253, op]).hex() for op in (9, 11, 13, 58)}
    x["rgb_groups"] = []
    for item in schema["rgb_groups"]:
        kind, index = item["type"], item["index"]
        x["rgb_groups"].append(dict(type=kind, index=index,
            info=get([253, 66 if kind == 3 else 28, index]).hex(),
            params={p: get([253, 61, kind, int(p), index]).hex() for p in item["params"]}))
    x["rgb_leds"] = [get([253, 69 if i < 64 else 29, i]).hex() for i in range(125)]
    x["rgb_indicators"] = [get([253, 33, i]).hex() for i in range(6)]
    rules = bytearray()
    for at in range(0, 128, 24):
        n = min(24, 128-at)
        reply = get([253, 233, 1, 2, 0, at, n])
        assert reply[:8] == bytes([253, 233, 170, 2, 1, 0, at, n])
        rules.extend(reply[8:8+n])
    x["lock"] = rules.hex()
    x["touch"] = get([253, 235, 1, 2, 0]).hex()
    x["sleep"] = get([253, 231, 1, 1, 0]).hex()
    x["games"] = {str(i): get([253, 230, 5, 1, i]).hex() for i in (1, 2, 3)}
    x["animation"] = get([253, 237, 0, 1]).hex()
    ack = get([253, 237, 1, 1, 211, 9, 0, 0])
    assert ack[:6] == bytes([253, 237, 170, 1, 1, 211])
    for _ in range(80):
        reply = get([253, 237, 2, 1, 211])
        assert reply[:6] == bytes([253, 237, 170, 1, 2, 211])
        if reply[6] != 1:
            assert reply[6] == 2 and reply[8] == 9
            x["slot"] = reply[12]
            assert 0 <= x["slot"] <= 10
            break
        time.sleep(.025)
    else:
        raise RuntimeError("Current animation selection GET timed out")
    return x


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit("Refusing to overwrite a capture")
    import hid
    for p in Path("/proc").iterdir():
        try:
            argv = (p / "cmdline").read_bytes().split(b"\0")
            if Path(argv[0].decode(errors="replace")).name == "VialAstro":
                raise SystemExit("Close VialAstro before capturing")
        except OSError:
            pass
    lock = open("/tmp/my2k-100ng-flash.lock", "a")
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    ds = [d for d in hid.enumerate(0x4d58, 0xf103) if d["usage_page"] == 0xff60 and d["usage"] == 0x61]
    assert len(ds) == 1, ds
    schema = json.loads((BOARD / "defaults/capture.json").read_text())["first"]
    layout = json.loads((BOARD / "defaults/layout.json").read_text())
    transactions = []
    with hid.Device(path=ds[0]["path"]) as dev:
        # A just-closed GUI may leave a completed GET response in the Raw HID
        # queue. Drain only existing replies before issuing the first request.
        for _ in range(64):
            if not dev.read(32, timeout=10):
                break
        else:
            raise RuntimeError("Raw HID replies remain active; close other keyboard clients")
        def get(value):
            q = bytes(value).ljust(32, b"\0")
            assert len(q) == 32 and allowed(q), "GET allowlist rejected " + q.hex()
            assert dev.write(b"\0" + q) == 33
            response = bytes(dev.read(32, timeout=3000))
            assert len(response) == 32
            transactions.append(dict(request=q.hex(), reply=response.hex()))
            if q[:2] == bytes([253, 240]):
                assert response[:4] == q[:3] + b"\x01", response.hex()
            elif q[0] == 253:
                assert response[:3] == q[:2] + b"\xaa", response.hex()
            return response
        first = capture(get, schema, layout)
        second = capture(get, schema, layout)
    result = dict(captured_UTC=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  two_readbacks_equal=True, first=first, second=second, transactions=transactions)
    # Compare persistent images, excluding volatile diagnostic/revision counters.
    a = DEFAULTS.build_image(result, layout)[0]
    b = DEFAULTS.build_image(dict(result, first=second), layout)[0]
    assert a == b, "Saved settings changed between readbacks; nothing published"
    result["comparison"] = "All encoded persistent fields equal; volatile diagnostics may differ"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print("Captured and verified twice:", args.output)


if __name__ == "__main__":
    main()
