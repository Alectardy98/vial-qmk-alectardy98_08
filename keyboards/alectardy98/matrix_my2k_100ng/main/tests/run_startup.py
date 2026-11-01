#!/usr/bin/env python3
"""Test Main reset and AUX boot code with deterministic clocks and fake buses."""
from pathlib import Path
import os
import subprocess
import tempfile

BOARD = Path(__file__).resolve().parents[2]
FLAGS = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
if os.getenv("SANITIZE") == "1":
    FLAGS += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
              "-fno-omit-frame-pointer", "-no-pie"]

HAL = """#pragma once
#include <stdint.h>
typedef int32_t msg_t;
typedef struct {uint32_t speed,cr1,cr2,cr3;} SerialConfig;
typedef struct {int unused;} SerialDriver;
extern SerialDriver SD1;
#define GPIOA 1
#define GPIOB 2
#define PAL_MODE_ALTERNATE(x) (10+(x))
#define PAL_MODE_OUTPUT_PUSHPULL 4
#define PAL_STM32_OSPEED_MID2 0
#define PAL_MODE_INPUT 0
#define TIME_IMMEDIATE 0
#define MSG_OK 0
#define TIME_I2MS(x) (x)
uint32_t chVTGetSystemTimeX(void);
void chThdSleepMilliseconds(unsigned);
void sdStart(SerialDriver *,const SerialConfig *);
void palSetPadMode(unsigned,unsigned,unsigned);
void palWritePad(unsigned,unsigned,unsigned);
msg_t sdGetTimeout(SerialDriver *,unsigned);
msg_t sdPutTimeout(SerialDriver *,uint8_t,unsigned);
"""

with tempfile.TemporaryDirectory(prefix="my2k-startup-") as directory:
    output = Path(directory)
    (output / "hal.h").write_text(HAL)

    def build(name, definitions, includes, sources):
        subprocess.run([
            "cc", *FLAGS, *["-D" + value for value in definitions],
            *[arg for path in includes for arg in ("-I", str(path))],
            *[str(BOARD / path) for path in sources], "-o", str(output / name)
        ], check=True)

    build("main", ["MY2K_AUX_LINK_MANAGED=1", "MY2K_AUX_STARTUP_CONTROL_ENABLED=1"],
          [output, BOARD / "main/src", BOARD / "common"],
          ["main/tests/test_aux_startup.c", "main/src/aux_link.c", "common/my2k_wire.c"])
    for case in ("normal", "wrap"):
        subprocess.run([str(output / "main"), case], check=True)

    build("aux", ["MY2K_DEV21_PROFILE=1", "AUX_COLOR_STREAM=1", "MY2K_RELEASE1_PROFILE=1"],
          [BOARD / "aux/include", BOARD / "common"],
          ["aux/tests/test_startup.c", "aux/src/panels.c", "aux/src/perf21.c"])
    for case in ("normal", "wrap", "transfer-fail", "configure-fail"):
        subprocess.run([str(output / "aux"), case], check=True)
