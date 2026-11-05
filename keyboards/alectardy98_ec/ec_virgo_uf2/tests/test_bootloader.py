#!/usr/bin/env python3
"""Compile exact built UF2 parser/write functions with mocked flash and HAL."""
import argparse
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
BOARD = HERE.parent


def function(path, signature):
    text = path.read_text()
    start = text.index(signature)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    args = parser.parse_args()
    root = args.build / 'built-source'
    uf2 = (root / 'src/uf2.h').read_text()
    types = uf2[uf2.index('// All entries are little endian.'):uf2.index('\nvoid uf2_init')]
    write = function(root / 'ports/stm32f4/board_flash.c', 'bool board_flash_write(')
    write = write.replace('bool board_flash_write(', 'bool virgo_port_flash_write(')
    # ADC, flash and USB hardware are mocked; these parser/write bodies are real.
    validation = (BOARD / 'bootloader/board/validation.c').read_text().replace('#include "board_api.h"', '')
    parts = [(root / 'src/ghostfat.c', 'static inline bool is_uf2_block ('),
             (root / 'src/ghostfat.c', 'int uf2_write_block ('),
             (root / 'ports/stm32f4/boards.c', 'void board_dfu_complete(')]
    source = (HERE / 'boot_host_prefix.h').read_text() + types + write + validation
    source += '\n'.join(function(path, signature) for path, signature in parts)
    source += (HERE / 'boot_host_main.c').read_text()
    cfile, exe = args.build / 'boot-host.c', args.build / 'boot-host'
    cfile.write_text(source)
    subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I', str(BOARD / 'bootloader/board'), str(cfile), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
