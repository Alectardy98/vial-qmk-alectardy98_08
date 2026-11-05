#!/usr/bin/env python3
"""Build in a disposable pinned TinyUF2 worktree; never flash or edit its owner.

Dependencies must already be fetched at the revisions in upstream.json.
Only these verified clean dependencies are shared with the temporary checkout.
The existing TinyUF2 build contract performs the actual CMake/Ninja build.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def run(args, **kwargs):
    return subprocess.run([str(x) for x in args], check=True, **kwargs)


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tinyuf2', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pre-commit', type=Path, help='Optional pre-commit executable for the isolated source audit')
    args = parser.parse_args()
    owner, output = args.tinyuf2.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    meta = json.loads((HERE / 'upstream.json').read_text())
    patch = HERE / 'upstream-hardening.patch'
    if hashlib.sha256(patch.read_bytes()).hexdigest() != meta['patch_sha256']:
        raise SystemExit('Hardening patch checksum mismatch')
    for name, pin in meta['dependencies'].items():
        if git(owner / name, 'rev-parse', 'HEAD') != pin:
            raise SystemExit(f'Dependency revision mismatch: {name}; expected {pin}')
        if git(owner / name, 'status', '--porcelain', '--untracked-files=no'):
            raise SystemExit(f'Refusing modified dependency: {name}')
    with tempfile.TemporaryDirectory(prefix='virgo-tinyuf2-') as tmp:
        stage = Path(tmp) / 'source'
        run(['git', '-C', owner, 'worktree', 'add', '--detach', stage, meta['revision']])
        try:
            run(['git', '-C', stage, 'apply', patch])
            board = stage / 'ports/stm32f4/boards/ec_virgo'
            shutil.copytree(HERE / 'board', board)
            modules = ('lib/tinyusb', 'lib/uf2', 'lib/agentrc')
            command = ['git', '-C', stage, '-c', 'protocol.file.allow=always']
            for name in modules:
                command += ['-c', f'submodule.{name}.url={owner / name}']
            run(command + ['submodule', 'update', '--init', *modules])
            for name in meta['dependencies']:
                if name in modules:
                    continue
                target = stage / name
                if target.exists():
                    target.rmdir()  # Only empty uninitialized submodule dirs.
                target.parent.mkdir(parents=True, exist_ok=True)
                target.symlink_to(owner / name, target_is_directory=True)
            if args.pre_commit:
                # Audit only in this disposable worktree; keep the build's exact
                # input patch after formatter hooks have reported their results.
                run(['git', '-C', stage, 'add', 'ports/stm32f4/boards/ec_virgo'])
                with (output / 'pre-commit.log').open('w') as log:
                    audit = subprocess.run([str(args.pre_commit.resolve()), 'run', '--all-files'],
                                           cwd=stage, stdout=log, stderr=subprocess.STDOUT)
                (output / 'pre-commit-status.json').write_text(json.dumps({'exit_code': audit.returncode})+'\n')
                run(['git', '-C', stage, 'restore', '--worktree', '.'])
                run(['git', '-C', stage, 'apply', patch])
                shutil.copytree(HERE / 'board', board, dirs_exist_ok=True)
            source = output / 'built-source'
            for name in ('ports/stm32f4/board_flash.c', 'ports/stm32f4/boards.c', 'src/ghostfat.c', 'src/uf2.h'):
                target = source / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(stage / name, target)
            contract = stage / '.claude/skills/build/scripts/check_build.py'
            with (output / 'build.log').open('w') as log:
                result = subprocess.run(['python3', str(contract), '--board', 'ec_virgo', '--shared'],
                                        cwd=stage, stdout=subprocess.PIPE, stderr=log, text=True)
            (output / 'receipt.json').write_text(result.stdout)
            receipt = json.loads(result.stdout.splitlines()[-1])
            build = stage / 'cmake-build/cmake-build-ec_virgo'
            if (build / 'check_build.log').is_file():
                shutil.copy2(build / 'check_build.log', output / 'compiler.log')
            if result.returncode or not receipt.get('pass'):
                raise SystemExit('TinyUF2 build contract did not pass; see receipt.json')
            build = stage / 'cmake-build/cmake-build-ec_virgo'
            for name in ('tinyuf2.bin', 'tinyuf2.hex', 'tinyuf2.elf'):
                src = build / name
                if not src.is_file():
                    src = build / '_bin' / name
                shutil.copy2(src, output / ('ec_virgo_' + name))
            shutil.copy2(build / 'check_build.log', output / 'compiler.log')
            provenance = dict(meta, board='ec_virgo', hardware_tested=False,
                              source_isolation='temporary detached worktree; owner sources unchanged')
            (output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
        finally:
            run(['git', '-C', owner, 'worktree', 'remove', '--force', stage])
    print(output)


if __name__ == '__main__':
    main()
