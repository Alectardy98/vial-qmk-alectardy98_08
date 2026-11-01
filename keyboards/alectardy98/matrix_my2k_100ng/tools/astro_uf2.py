#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Strict offline ASTRO candidate packager. Never opens a device or sends an update.

The generated files conform to a deliberately narrower contract than the stock
loader. Ordered, unique blocks cannot prevent a host from replaying/reordering
them. See the release compatibility report before considering installation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile

MAGIC = (0x0a324655, 0x9e5d5157, 0x0ab16f30)
POLICIES = {
    'main': dict(base=0x08020000, end=0x08060000,
                 family=0x57755a57, ram_end=0x20020000),
    'aux': dict(base=0x08000000, end=0x08080000,
                family=0x53b80f00, ram_end=0x20040000),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def vectors(data, policy, vector_bytes=64, code_ranges=None, manufacturer=False):
    require(64 <= vector_bytes <= len(data) and vector_bytes % 4 == 0, 'invalid vector extent')
    words = struct.unpack_from('<' + 'I' * (vector_bytes // 4), data)
    require(0x20000000 < words[0] <= policy['ram_end'] and words[0] % 8 == 0,
            'stack outside permitted SRAM or not eight-byte aligned')
    for i, address in enumerate(words[1:], 1):
        # Stock main uses reserved slot13 as a data pointer (0803BA0C).
        # Only the hash-pinned reference path permits this; no handler claim.
        if manufacturer and i in (7, 8, 9, 10, 13):
            continue
        if address == 0:
            require(i not in (1, 2, 3), 'missing required reset/fault vector')
            continue
        pc = address & ~1
        require(address & 1 and policy['base'] + vector_bytes <= pc < policy['base'] + len(data),
                f'vector {i} is not a Thumb handler within the actual application')
        if code_ranges is not None:
            require(any(a <= pc < b for a, b in code_ranges), f'vector {i} is outside executable code')
    return {'initial_sp': hex(words[0]), 'reset_vector': hex(words[1]), 'vector_bytes': vector_bytes}


def validate_uf2(data, policy, *, full=True, expected=None):
    """Reject duplicates (even identical), noncanonical ordering and all gaps.

    `full=False` is only for immutable manufacturer fixtures; candidate mode
    requires the whole explicitly owned erase footprint. It never repairs input.
    """
    require(data and len(data) % 512 == 0, 'truncated/empty UF2')
    count = len(data) // 512
    limit = (policy['end'] - policy['base']) // 256
    require(count == limit if full else 0 < count <= limit, 'incorrect flash coverage')
    output = bytearray()
    seen = {}
    for i in range(count):
        block = data[i*512:(i+1)*512]
        m0, m1, flags, target, size, number, total, family = struct.unpack_from('<8I', block)
        require((m0, m1, struct.unpack_from('<I', block, 508)[0]) == MAGIC, f'magic at block {i}')
        require(flags == 0x2000 and family == policy['family'], f'flags/family at block {i}')
        require(size == 256 and target % 256 == 0, f'payload/alignment at block {i}')
        require(policy['base'] <= target and target + size <= policy['end'], 'reserved-region overlap')
        if number in seen:
            kind = 'identical' if seen[number] == block else 'conflicting'
            raise ValueError(f'{kind} duplicate block {number}; ASTRO replay is unsafe')
        seen[number] = block
        require(number == i and total == count, 'noncanonical block number/order/count')
        require(target == policy['base'] + i * 256, 'gap, overlap or wrong target')
        require(block[288:508] == bytes(220), 'unsupported extension/padding bytes')
        output.extend(block[32:288])
    result = bytes(output)
    vectors(result, policy, manufacturer=not full)
    if expected is not None:
        require(result == expected, 'UF2 does not exactly reconstruct intended binary')
    return result


def encode(application, policy):
    require(64 <= len(application) <= policy['end'] - policy['base'], 'application does not fit')
    require(len(application) % 4 == 0, 'application is not word aligned')
    vectors(application, policy)
    image = application.ljust(policy['end'] - policy['base'], b'\xff')
    count = len(image) // 256
    blocks = []
    for number in range(count):
        header = struct.pack('<8I', MAGIC[0], MAGIC[1], 0x2000,
                             policy['base'] + number * 256, 256, number, count, policy['family'])
        blocks.append(header + image[number*256:(number+1)*256] + bytes(220) + struct.pack('<I', MAGIC[2]))
    uf2 = b''.join(blocks)
    validate_uf2(uf2, policy, expected=image)
    return image, uf2

