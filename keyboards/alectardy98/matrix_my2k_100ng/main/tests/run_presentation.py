#!/usr/bin/env python3
"""Compile the actual Main/AUX presentation paths against bounded host fakes."""
from pathlib import Path
import os
import subprocess
import tempfile

BOARD = Path(__file__).resolve().parents[2]
MAIN, AUX, COMMON = BOARD / "main", BOARD / "aux", BOARD / "common"
FLAGS = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-ffunction-sections", "-fdata-sections"]
if os.environ.get("SANITIZE") == "1":
    FLAGS += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie"]

with tempfile.TemporaryDirectory(prefix="my2k-presentation-") as tmp:
    out = Path(tmp)
    headers = {
        "quantum.h": """#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "timer.h"
typedef struct { struct { struct { uint8_t row,col; } key; bool pressed; } event; } keyrecord_t;
void clear_keyboard(void);
#define MATRIX_ROWS 6
#define MATRIX_COLS 16
#define IS_QK_TO(x) false
#define IS_QK_MOMENTARY(x) false
#define IS_QK_DEF_LAYER(x) false
#define IS_QK_TOGGLE_LAYER(x) false
#define IS_QK_ONE_SHOT_LAYER(x) false
#define IS_QK_LAYER_TAP_TOGGLE(x) false
""",
        "keycodes.h": """#pragma once
#define QK_KB_0 0x7e00
#define QK_BOOTLOADER 0x7c00
enum { KC_A=4,KC_D=7,KC_S=22,KC_W=26,KC_X=27,KC_Z=29,KC_ENTER=40,KC_RIGHT=79,KC_LEFT=80,KC_DOWN=81,KC_UP=82,KC_RSFT=229 };
""",
        "eeprom_driver.h": """#pragma once
#include <stddef.h>
void eeprom_read_block(void *,const void *,size_t);
void eeprom_update_block(const void *,void *,size_t);
""",
        "timer.h": "#pragma once\n#include <stdint.h>\nuint32_t timer_read32(void);\n",
        "vial.h": "#pragma once\nextern int vial_unlocked;\n",
        "report.h": """#pragma once
#include <stdint.h>
typedef struct { uint8_t mods,reserved,keys[6]; } report_keyboard_t;
typedef struct { uint8_t mods,bits[32]; } report_nkro_t;
typedef struct { uint8_t buttons; int16_t x,y,v,h; } report_mouse_t;
typedef struct { uint16_t usage; } report_extra_t;
""",
        "host_driver.h": """#pragma once
#include "report.h"
typedef struct { void (*send_keyboard)(report_keyboard_t *); void (*send_nkro)(report_nkro_t *); void (*send_mouse)(report_mouse_t *); void (*send_extra)(report_extra_t *); } host_driver_t;
host_driver_t *host_get_driver(void);
void host_set_driver(host_driver_t *);
""",
        "host.h": "#pragma once\n#include \"host_driver.h\"\n",
    }
    for name, text in headers.items():
        (out / name).write_text(text)
    binary = out / "main"
    defs = ["MY2K_GAMES_PROFILE", "MY2K_ACTIVE_DEV_PROFILE", "MY2K_DEV15_PROFILE", "MY2K_EEPROM32_PROFILE", "MY2K_RELEASE1_PROFILE"]
    subprocess.run(["cc", *FLAGS, *["-D" + x + "=1" for x in defs],
                    *[arg for p in [out, MAIN / "src", MAIN / "drivers", MAIN / "storage", COMMON] for arg in ("-I", str(p))],
                    str(MAIN / "tests/test_presentation.c"), str(MAIN / "src/main_animation.c"), str(MAIN / "src/main_games.c"),
                    str(COMMON / "my2k_oled_config.c"), "-Wl,--gc-sections", "-o", str(binary)], check=True)
    for case in ("saved", "default", "older-aux", "highspeed", "atomic", "legacy", "probe", "probe-wrap"):
        subprocess.run([str(binary), case], check=True)
    binary = out / "aux-assets"
    defs = ["MY2K_STOCK_PARITY_PROFILE", "MY2K_RELEASE1_PROFILE", "MY2K_ANIMATION_PROFILE", "AUX_FRAME_RATE_CONTROL", "AUX_ORIGINAL_TIMING_DEFAULT"]
    subprocess.run(["cc", *FLAGS, *["-D" + x + "=1" for x in defs],
                    *[arg for p in [AUX / "include", AUX / "vendor/fatfs", COMMON] for arg in ("-I", str(p))],
                    str(AUX / "tests/test_presentation.c"), str(AUX / "src/assets.c"), str(AUX / "src/animation.c"),
                    str(COMMON / "my2k_asset.c"), "-Wl,--gc-sections", "-o", str(binary)], check=True)
    for case in ("saved", "default", "legacy"):
        subprocess.run([str(binary), case], check=True)
    binary = out / "aux-games"
    defs = ["MY2K_GBC_PROFILE", "MY2K_DOOM_PROFILE", "MY2K_RELEASE1_PROFILE", "MY2K_STOCK_PARITY_PROFILE"]
    subprocess.run(["cc", *FLAGS, *["-D" + x + "=1" for x in defs],
                    *[arg for p in [AUX / "include", AUX / "vendor/fatfs", COMMON] for arg in ("-I", str(p))],
                    str(AUX / "tests/test_game_handoff.c"), str(AUX / "src/nes.c"),
                    str(COMMON / "my2k_oled_config.c"), "-Wl,--gc-sections", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
