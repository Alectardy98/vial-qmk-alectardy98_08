#!/usr/bin/env python3
"""Replay the verified final AUX build with repository-relative inputs.

The AUX is a standalone Cortex-M7 application, not a QMK keymap. Its explicit
ELF target integrates with make keyboard:aux while retaining its tested ABI,
game RAM overlays, ITCM placement and pinned TinyUSB adaptations.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

BOARD = Path(__file__).resolve().parents[1] / "aux"
REPO = next(p for p in BOARD.parents if (p / "quantum").is_dir())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    args = parser.parse_args()
    elf = args.elf.resolve()
    out = elf.parent / (elf.stem + "-aux")
    out.mkdir(parents=True, exist_ok=True)
    manifest = BOARD / "build-commands.json"
    digest = hashlib.sha256()
    for directory in [BOARD / "src", BOARD / "include", BOARD / "vendor", BOARD.parent / "common"]:
        for path in sorted(directory.rglob("*")):
            if path.is_file() and "__pycache__" not in path.parts:
                digest.update(str(path.relative_to(BOARD.parent)).encode())
                digest.update(path.read_bytes())
    for name in ["prepare_tinyusb.py", "prepare_tinyusb_dcd.py", "prepare_tinyusb_msc_file.py"]:
        digest.update((BOARD.parent / "tools" / name).read_bytes())
    digest.update(manifest.read_bytes())
    digest.update((BOARD / "linker.ld").read_bytes())
    digest.update(Path(__file__).read_bytes())
    digest.update(subprocess.check_output(["arm-none-eabi-gcc", "--version"]))
    fingerprint = digest.hexdigest()
    receipt = out / "build-receipt.json"
    if receipt.exists() and elf.exists() and elf.with_suffix(".bin").exists():
        previous = json.loads(receipt.read_text())
        if (previous.get("inputs_sha256") == fingerprint and
                previous.get("ELF_sha256") == hashlib.sha256(elf.read_bytes()).hexdigest() and
                previous.get("BIN_sha256") == hashlib.sha256(elf.with_suffix(".bin").read_bytes()).hexdigest()):
            print("AUX unchanged; verified cached ELF/BIN")
            return
    shutil.copy2(BOARD / "linker.ld", out / "games.ld")
    replacements = {"@BOARD@": str(BOARD), "@TOOLS@": str(BOARD.parent / "tools"), "@REPO@": str(REPO), "@OUT@": str(out)}

    def expand(value):
        for old, new in replacements.items():
            value = value.replace(old, new)
        return value

    commands = json.loads(manifest.read_text())
    with (out / "build.log").open("w") as log:
        for index, entry in enumerate(commands):
            argv = [expand(value) for value in entry["argv"]]
            log.write("$ " + repr(argv) + "\n")
            log.flush()
            result = subprocess.run(argv, cwd=expand(entry["cwd"]), stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                print((out / "build.log").read_text()[-12000:])
                raise SystemExit(result.returncode)
            if index % 25 == 0:
                print(f"AUX build {index + 1}/{len(commands)}", flush=True)
    shutil.copy2(out / "my2k-aux.elf", elf)
    shutil.copy2(out / "my2k-aux.bin", elf.with_suffix(".bin"))
    shutil.copy2(out / "my2k-aux.map", elf.with_suffix(".map"))
    receipt.write_text(json.dumps(dict(inputs_sha256=fingerprint,
        ELF_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
        BIN_sha256=hashlib.sha256(elf.with_suffix(".bin").read_bytes()).hexdigest(),
        commands=len(commands)), indent=2) + "\n")
    print("Built standalone AUX:", elf)


if __name__ == "__main__":
    main()
