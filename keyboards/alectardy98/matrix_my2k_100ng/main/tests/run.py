#!/usr/bin/env python3
"""Native regression tests of the final board sources. Never accesses USB."""
from pathlib import Path
import os
import subprocess
import tempfile

BOARD = Path(__file__).resolve().parents[1]
flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-I", str(BOARD), "-I", str(BOARD / "src")]
if os.environ.get("SANITIZE") == "1":
    flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie"]
with tempfile.TemporaryDirectory(prefix="my2k-final-tests-") as directory:
    out = Path(directory)
    for test, sources in [
        ("nv32", ["main_nv32.c"]),
        ("factory", ["main_nv32.c", "main_factory_defaults.c"]),
        ("performance", ["main_performance_mode.c", "main_usb_performance.c"]),
    ]:
        binary = out / test
        subprocess.run(["cc", *flags, "-DMY2K_PERFORMANCE_HOST=1", *[str(BOARD / "src" / name) for name in sources],
                        str(BOARD / f"tests/test_{test}.c"), "-o", str(binary)], check=True)
        scenarios = [*range(21), 30] if test == "performance" else [None]
        for scenario in scenarios:
            subprocess.run([str(binary), *([] if scenario is None else [str(scenario)])], check=True)
print("PASS final board NV32/defaults and 22 Standard/High-Speed policy scenarios")
