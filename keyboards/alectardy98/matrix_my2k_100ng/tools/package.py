#!/usr/bin/env python3
"""Package only the tested ASTRO application footprint; never flash."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from astro_uf2 import POLICIES, encode, validate_uf2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mcu", choices=["main", "aux"], required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--uf2-output", type=Path, help="Internal UF2 for QMK to copy to the final output")
    parser.add_argument("--generate-bin", choices=["yes", "no"], default="no")
    parser.add_argument("--generate-json", choices=["yes", "no"], default="no")
    parser.add_argument("--generate-sha256", choices=["yes", "no"], default="no")
    args = parser.parse_args()
    policy = dict(POLICIES[args.mcu])
    policy["end"] = 0x08060000 if args.mcu == "main" else 0x08080000
    # Confirm every loadable flash segment lies inside the application window.
    segments = subprocess.check_output(["arm-none-eabi-readelf", "-lW", str(args.elf)], text=True)
    for line in segments.splitlines():
        fields = line.split()
        if fields and fields[0] == "LOAD":
            address, size = int(fields[3], 16), int(fields[4], 16)
            if size:
                assert policy["base"] <= address < address + size <= policy["end"], line
    raw = args.input.read_bytes()
    application = raw + b"\xff" * ((-len(raw)) % 4)
    _, result = encode(application, policy)
    image = validate_uf2(result, policy)
    assert image[:len(raw)] == raw
    final_uf2 = args.output.with_suffix(".uf2")
    uf2 = args.uf2_output or final_uf2
    final_bin = args.output.with_suffix(".bin")
    if args.generate_bin == "yes" and final_bin.resolve() == args.input.resolve():
        parser.error("Final BIN must not overwrite the internal input BIN")
    uf2.write_bytes(result)
    paths = [uf2]
    hashes = {str(final_uf2): hashlib.sha256(result).hexdigest()}
    if args.generate_bin == "yes":
        final_bin.write_bytes(image)
        paths.append(final_bin)
        hashes[str(final_bin)] = hashlib.sha256(image).hexdigest()
    elif final_bin.resolve() != args.input.resolve():
        # Remove only this target's stale final output, never its internal BIN.
        final_bin.unlink(missing_ok=True)
    checksum = args.output.with_suffix(".sha256")
    if args.generate_sha256 == "yes":
        checksum.write_text("".join(f"{digest}  {name}\n" for name, digest in hashes.items()))
        paths.append(checksum)
    else:
        checksum.unlink(missing_ok=True)
    metadata = args.output.with_suffix(".json")
    if args.generate_json == "yes":
        metadata.write_text(json.dumps(dict(mcu=args.mcu,
            application_bytes=len(raw), flash_start=hex(policy["base"]), flash_end_exclusive=hex(policy["end"]),
            hashes=hashes, flash_performed=False, settings_included_in_UF2=False), indent=2) + "\n")
        paths.append(metadata)
    else:
        metadata.unlink(missing_ok=True)
    for path in paths:
        print(path)


if __name__ == "__main__":
    main()
