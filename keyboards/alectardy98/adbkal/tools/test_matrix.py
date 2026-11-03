#!/usr/bin/env python3
"""Exercise the real scanner and QMK debounce with simulated switch closures.

Run from any directory. An optional source path permits testing the old scanner.
The outer scan/debounce sequence matches quantum/matrix_common.c (CUSTOM_MATRIX=lite).
"""
import pathlib
import subprocess
import sys
import tempfile

keyboard = pathlib.Path(__file__).resolve().parents[1]
repo = keyboard.parents[2]
scanner = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else keyboard / "matrix.c"

with tempfile.TemporaryDirectory(prefix="adbkal-matrix-test-") as temporary:
    work = pathlib.Path(temporary)
    (work / "quantum.h").write_text("""
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define MATRIX_ROWS 12
#define MATRIX_COLS 8
typedef uint8_t matrix_row_t;
typedef uint8_t pin_t;
void wait_us(unsigned us);
""" + "\n".join(f"#define GP{i} {i}" for i in range(30)))
    (work / "matrix.h").write_text('#include "quantum.h"\n')
    (work / "gpio.h").write_text("""
#include "quantum.h"
void setPinInputHigh(pin_t pin);
void setPinOutput(pin_t pin);
void writePinLow(pin_t pin);
bool readPin(pin_t pin);
""")
    (work / "timer.h").write_text("""
#include <stdint.h>
typedef uint16_t fast_timer_t;
uint16_t timer_read_fast(void);
uint16_t timer_elapsed_fast(uint16_t then);
""")
    (work / "main.c").write_text("""
#include <stdio.h>
#include <string.h>
#include "quantum.h"
#include "debounce.h"
void matrix_init_custom(void);
bool matrix_scan_custom(matrix_row_t current[]);
static const pin_t rows[] = {15,8,0,17,11,12,13,14,16,18};
static const pin_t cols[] = {23,22,21,29,28,27,26,25};
static const pin_t direct[] = {20,7,9,10,6,19,24};
static bool switches[12][8], output_low[30];
static uint16_t now;
static matrix_row_t raw[12], cooked[12];
void setPinInputHigh(pin_t pin) { output_low[pin] = false; }
void setPinOutput(pin_t pin) { (void)pin; }
void writePinLow(pin_t pin) { output_low[pin] = true; }
void wait_us(unsigned us) { (void)us; }
uint16_t timer_read_fast(void) { return now; }
uint16_t timer_elapsed_fast(uint16_t then) { return now - then; }
bool readPin(pin_t pin) {
    for (int r=0; r<10; ++r) if (pin==rows[r])
        for (int c=0; c<8; ++c)
            if (output_low[cols[c]] && switches[r][c]) return false;
    for (int i=0; i<7; ++i) if (pin==direct[i])
        return !switches[i==6 ? 11 : 10][i==6 ? 0 : i];
    return true;
}
static void scan_for(int ms) {
    for (int t=0; t<ms; ++t, ++now) {
        bool changed = matrix_scan_custom(raw);
        debounce(raw, cooked, MATRIX_ROWS, changed);
    }
}
static int check(int row, int col, bool pressed) {
    bool actual = (cooked[row] & (1u << col)) != 0;
    if (actual != pressed) {
        fprintf(stderr,"FAIL: row %d col %d expected %s after 20 ms, got %s\\n",
                row,col,pressed?"pressed":"released",actual?"pressed":"released");
        return 1;
    }
    return 0;
}
int main(void) {
    matrix_init_custom();
    debounce_init(MATRIX_ROWS);
    scan_for(20);
    int tested=0;
    for (int r=0; r<12; ++r) for (int c=0; c<8; ++c) {
        if ((r==10 && c>=6) || (r==11 && c>=1)) continue;
        switches[r][c]=true; scan_for(20);
        if (check(r,c,true)) return 1;
        switches[r][c]=false; scan_for(20);
        if (check(r,c,false)) return 1;
        ++tested;
    }
    // Simultaneous matrix keys and direct modifiers must share one debounce pass.
    switches[2][1]=switches[4][0]=switches[10][1]=true; scan_for(20);
    if (check(2,1,true)||check(4,0,true)||check(10,1,true)) return 1;
    memset(switches,0,sizeof(switches)); scan_for(20);
    for (int r=0;r<12;++r) if (cooked[r]) return 1;
    // A short bounce must not emit a key; a subsequent stable press must arrive.
    switches[0][1]=true; scan_for(2);
    if (check(0,1,false)) return 1;
    switches[0][1]=false; scan_for(10);
    if (check(0,1,false)) return 1;
    switches[0][1]=true; scan_for(20);
    if (check(0,1,true)) return 1;
    printf("PASS: %d switch positions press/release, simultaneous modifiers, and bounce filtering\\n",tested);
    return 0;
}
""")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Wno-unused-parameter",
                    "-I", str(work), "-I", str(repo / "quantum"),
                    str(scanner), str(repo / "quantum/debounce/sym_defer_g.c"),
                    str(work / "main.c"), "-o", str(work / "test")], check=True)
    subprocess.run([str(work / "test")], check=True)
