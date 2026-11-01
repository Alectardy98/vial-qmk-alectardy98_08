#!/usr/bin/env python3
"""Generate a private MY-2K ChibiOS overlay without changing either submodule.

Only the affected files and header include boundaries are copied. Unchanged
subtrees are linked to the checked-out libraries. Pinned patches are applied
only inside .build, then checked against the exact tested file hashes.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

REPO = next(p for p in Path(__file__).resolve().parents if (p / "quantum").is_dir())
PATCHES = REPO / "keyboards/alectardy98/matrix_my2k_100ng/main/chibios"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def checked_input(base, name, entry):
    data = (base / name).read_bytes()
    digest = sha(data)
    if digest == entry["upstream_sha256"]:
        return data
    raise ValueError("Unrecognized upstream file; refusing overlay: " + str(base / name))


def mirror(base, destination, copies):
    """Copy only private paths; link untouched branches, excluding .git."""
    def walk(source, target, prefix=""):
        target.mkdir(parents=True, exist_ok=True)
        for item in source.iterdir():
            if item.name == ".git":
                continue
            name = prefix + item.name
            out = target / item.name
            if name in copies:
                out.write_bytes(copies[name])
            elif any(path.startswith(name + "/") for path in copies):
                walk(item, out, name + "/")
            else:
                # This is a disposable generated path, not a checked-in home
                # directory. Absolute targets survive the atomic stage rename;
                # the input fingerprint regenerates them if the checkout moves.
                out.symlink_to(item, target_is_directory=item.is_dir())
    walk(base, destination)
    # Add board-only files absent from a pristine submodule.
    for name, data in copies.items():
        path = destination / name
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)


def valid_output(output, expected):
    for name, digest in expected.items():
        path = output / name
        if not path.is_file() or path.is_symlink() or sha(path.read_bytes()) != digest:
            return False
    # Only check build entrypoints: upstream also contains optional dangling
    # example links which are unrelated to this board.
    for module in ["chibios", "chibios-contrib"]:
        if not (output / module / "os/hal/hal.mk").is_file():
            return False
    return True


def prepare(output):
    manifest_bytes = (PATCHES / "manifest.json").read_bytes()
    manifest = json.loads(manifest_bytes)
    digest = hashlib.sha256(manifest_bytes + Path(__file__).read_bytes() + str(REPO).encode())
    inputs, expected = {}, {}
    for module, settings in manifest["modules"].items():
        base = REPO / "lib" / module
        if (base / ".git").exists():
            commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=base, text=True).strip()
            if commit != settings["commit"]:
                raise ValueError("Submodule revision differs from tested version: " + module)
        patch = (PATCHES / (module + ".patch")).read_bytes()
        if sha(patch) != settings["patch_sha256"]:
            raise ValueError("Board patch digest mismatch: " + module)
        digest.update(patch)
        copies = {}
        for name, entry in settings["files"].items():
            copies[name] = checked_input(base, name, entry)
            expected[module + "/" + name] = entry["patched_sha256"]
            digest.update((base / name).read_bytes())
        for name, expected_hash in settings.get("header_inputs", {}).items():
            data = (base / name).read_bytes()
            if sha(data) != expected_hash:
                raise ValueError("Header boundary differs from tested version: " + name)
            copies[name] = data
            expected[module + "/" + name] = expected_hash
            digest.update(data)
        for name, entry in settings.get("extra", {}).items():
            data = (PATCHES / entry["asset"]).read_bytes()
            if sha(data) != entry["sha256"]:
                raise ValueError("Board-only header digest mismatch: " + name)
            copies[name] = data
            expected[module + "/" + name] = entry["sha256"]
            digest.update(data)
        inputs[module] = copies
    fingerprint = digest.hexdigest()
    receipt = output / "manifest.json"
    if receipt.exists():
        cached = json.loads(receipt.read_text())
        if cached.get("inputs_sha256") == fingerprint and valid_output(output, expected):
            return
    with tempfile.TemporaryDirectory(prefix=".my2k-chibios-", dir=output.parent) as temporary:
        stage = Path(temporary) / "overlay"
        for module, copies in inputs.items():
            destination = stage / module
            mirror(REPO / "lib" / module, destination, copies)
            # Run outside the parent Git worktree: git otherwise silently
            # filters patches whose paths do not match a nested cwd prefix.
            subprocess.run(["git", "apply", "--no-index", "--unsafe-paths",
                            "--whitespace=nowarn", "--directory=" + str(destination),
                            str(PATCHES / (module + ".patch"))],
                           cwd=tempfile.gettempdir(), check=True)
        if not valid_output(stage, expected):
            raise ValueError("Private overlay does not reproduce the tested files")
        (stage / "manifest.json").write_text(json.dumps(dict(inputs_sha256=fingerprint,
            patched_files=expected, shared_sources_modified=False), indent=2) + "\n")
        previous = output.with_name(output.name + ".old")
        if previous.exists():
            shutil.rmtree(previous)
        if output.exists():
            output.rename(previous)
        stage.rename(output)
        if previous.exists():
            shutil.rmtree(previous)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    # Build artifacts must stay under this checkout's disposable .build tree.
    relative = output.relative_to((REPO / ".build").resolve())
    if not relative.parts:
        parser.error("Use a private subdirectory inside .build, not .build itself")
    output.parent.mkdir(parents=True, exist_ok=True)
    with (output.parent / ("." + output.name + ".lock")).open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        prepare(output)


if __name__ == "__main__":
    main()
