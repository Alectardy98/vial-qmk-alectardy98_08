#!/usr/bin/env python3
"""Exercise the real vial_alec keymap offline; no HID/device access."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

MAIN = Path(__file__).resolve().parents[1]
REPO = next(path for path in MAIN.parents if (path / "quantum").is_dir())
KEYMAP = MAIN / "keymaps/vial_alec"


def tap(code):
    return bytes([1, 1, code])


def mod(code, contents):
    return bytes([1, 2, code]) + contents + bytes([1, 3, code])


EXPECTED = {
    "TEST": b"TEST",
    "VDRT": mod(0xe3, mod(0xe0, tap(0x4f))),
    "VDLT": mod(0xe3, mod(0xe0, tap(0x50))),
    "VDUP": mod(0xe3, tap(0x2b) + tap(0x17)),
    "VDDN": mod(0xe3, b"d"),
    "EXPL": mod(0xe3, b"e"),
    "SNIP": mod(0xe3, mod(0xe1, b"s")),
    "COPY": tap(0x3b) + mod(0xe0, tap(0x04) + tap(0x06)),
    "CUTT": tap(0x3b) + mod(0xe0, tap(0x04) + tap(0x1b)),
    "PAST": tap(0x3b) + mod(0xe0, tap(0x19)),
    "TASK": mod(0xe0, mod(0xe5, tap(0x29))) + mod(0xe0, mod(0xe2, tap(0x29))),
    "GOGL": mod(0xe0, mod(0xe2, tap(0x0a))),
    "CMD": mod(0xe0, mod(0xe2, tap(0x17))),
    "QUIT": mod(0xe2, tap(0x3d)) + mod(0xe0, tap(0x1a)),
    "DZRO": b"00",
    "ATAB": mod(0xe2, tap(0x2b)),
    "LPRC": b"(",
    "RPRC": b")",
    "NUM": tap(0x53),
}
EXPECTED.update({name: text.encode() + tap(0x28) for name, text in [
    ("THIS", ":this:"), ("OOF", ":oof:"), ("PIKA", ":surprised:"),
    ("SCAT", ":smiley_cat2:"), ("FCAT", ":smiley_cat1:"), ("HART", ":heart:"),
    ("DROL", ":drooling_face:"), ("MONY", ":smiley_take:"), ("FHAT", ":smiley_fedora:"),
    ("SPIT", ":smiley_spit:"), ("KING", ":pepe_king:"), ("FLEX", ":muscle:"),
    ("HAHA", ":smiley_kekw:"), ("LCRY", ":joy:"), ("NICE", ":nice~2:"),
    ("PPOG", ":pepe_pog:"), ("PRAY", ":pray_tone2:"), ("SWET", ":sweat_smile:"),
    ("COLD", ":cold_face:"), ("THUM", ":thumbsup:"),
]})

HEADER = r'''#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "keycodes.h"
#include "send_string_keycodes.h"
#define MATRIX_ROWS 8
#define MATRIX_COLS 16
#define PROGMEM
#define MO(layer) (QK_MOMENTARY | ((layer) & 0x1f))
typedef struct {
    struct {struct {uint8_t row,col;} key; bool pressed; uint16_t time;} event;
    struct {bool interrupted; uint8_t count;} tap;
} keyrecord_t;
void test_send(const char *);
int uprintf(const char *, ...);
#define SEND_STRING(text) test_send(text)
'''

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "qmk_test.h"
#include "keymap.c"
static unsigned length,calls;
static unsigned char output[256];
void test_send(const char *text) {
    size_t n=strlen(text);
    assert(length+n<=sizeof output);
    memcpy(output+length,text,n); length+=(unsigned)n; calls++;
}
int uprintf(const char *format, ...) { (void)format; return 0; }
int main(void) {
    assert(TEST==0x7e12&&THUM==0x7e38);
    keyrecord_t record={0};
    for (unsigned code=TEST;code<=THUM;code++) {
        length=calls=0;record.event.pressed=true;
        assert(!process_record_user(code,&record)&&calls);
        printf("%04x ",code);
        for (unsigned i=0;i<length;i++) printf("%02x",output[i]);
        putchar('\n');
        unsigned before=length,before_calls=calls;
        record.event.pressed=false;
        assert(!process_record_user(code,&record));
        assert(length==before&&calls==before_calls);
    }
    for (unsigned code=0;code<=65535;code++) {
        if (code>=TEST&&code<=THUM) continue;
        calls=length=0;
        record.event.pressed=true;assert(process_record_user(code,&record));
        record.event.pressed=false;assert(process_record_user(code,&record));
        assert(!length&&!calls);
    }
}
'''


def check_balanced(sequence):
    held = set()
    position = 0
    while position < len(sequence):
        if sequence[position] != 1:
            position += 1
            continue
        operation, code = sequence[position + 1:position + 3]
        if operation == 2:
            assert code not in held
            held.add(code)
        elif operation == 3:
            assert code in held
            held.remove(code)
        else:
            assert operation == 1
        position += 3
    assert not held, held


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gui", type=Path, help="Optional VialAstro checkout for real keycode-model checks")
    args = parser.parse_args()
    base = json.loads((MAIN / "keymaps/vial/vial.json").read_text())
    definition = json.loads((KEYMAP / "vial.json").read_text())
    entries = definition["customKeycodes"]
    assert entries[:18] == base["customKeycodes"] and len(entries) == 57
    assert [item["shortName"] for item in entries[18:]] == list(EXPECTED)
    assert {key: value for key, value in definition.items() if key != "customKeycodes"} == {
        key: value for key, value in base.items() if key != "customKeycodes"}
    flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    if os.getenv("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie"]
    with tempfile.TemporaryDirectory(prefix="my2k-vial-alec-") as directory:
        directory = Path(directory)
        (directory / "qmk_test.h").write_text(HEADER)
        (directory / "test.c").write_text(HARNESS)
        includes = [directory, KEYMAP, MAIN / "src", REPO / "quantum", REPO / "quantum/send_string"]
        for console in (False, True):
            executable = directory / "test"
            subprocess.run(["cc", *flags, '-DQMK_KEYBOARD_H="qmk_test.h"',
                            *(["-DCONSOLE_ENABLE"] if console else []),
                            *[argument for path in includes for argument in ("-I", str(path))],
                            str(directory / "test.c"), "-o", str(executable)], check=True)
            rows = subprocess.check_output([str(executable)], text=True).splitlines()
            assert len(rows) == 39
            for index, ((name, expected), row) in enumerate(zip(EXPECTED.items(), rows), 18):
                code, actual = row.split()
                actual = bytes.fromhex(actual)
                assert int(code, 16) == 0x7e00 + index and actual == expected, (name, actual, expected)
                check_balanced(actual)
    if args.gui:
        sys.path.insert(0, str(args.gui / "src/main/python"))
        from keycodes.keycodes import Keycode, KEYCODES_USER, create_custom_user_keycodes, recreate_keycodes
        create_custom_user_keycodes(entries)
        assert len(KEYCODES_USER) == 57
        Keycode.protocol = 6
        recreate_keycodes()
        for index, entry in enumerate(entries):
            code = Keycode.find_by_qmk_id(f"USER{index:02}")
            assert code and code.label == entry["shortName"]
            assert Keycode.resolve(code.qmk_id) == 0x7e00 + index
            assert Keycode.deserialize(code.qmk_id, reraise=True) == 0x7e00 + index
            assert Keycode.serialize(0x7e00 + index) == code.qmk_id
        print("PASS real VialAstro model: all 57 labels and numeric keycodes")
    print("PASS 39 press sequences, silent releases, balanced modifiers, all other keycodes pass through; console on/off")
    print("PASS original 18 controls and layout/features unchanged")


if __name__ == "__main__":
    main()
