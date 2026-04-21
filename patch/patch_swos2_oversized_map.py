#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys


PITCH_HEADER_BASE = 0x000C0000
PITCH_TILE_BASE = PITCH_HEADER_BASE + 0x2418
PITCH_SECOND_PAGE = PITCH_HEADER_BASE + 0x54
MAX_MAP_SIZE = 0x0001A000


PATCHES = [
    {
        "name": "pitch-specific MAP size check",
        "offset": 0x2A20,
        "expected": bytes.fromhex("0C 81 00 00 B2 20 63 0C"),
        "patched": bytes.fromhex("0C 81 00 01 A0 00 63 0C"),
    },
    {
        "name": "generic asset loader size check",
        "offset": 0x599C,
        "expected": bytes.fromhex("0C 81 00 00 BB B0 62 00 00 92"),
        "patched": bytes.fromhex("0C 81 00 01 A0 00 62 00 00 92"),
    },
    {
        "name": "pitch loader destination",
        "offset": 0x2A16,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
    {
        "name": "MAP fixup header base",
        "offset": 0x2A38,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
    {
        "name": "MAP fixup tile base",
        "offset": 0x2A46,
        "expected": (0x00002818).to_bytes(4, "big"),
        "patched": PITCH_TILE_BASE.to_bytes(4, "big"),
    },
    {
        "name": "pitch page A base",
        "offset": 0x2A6A,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
    {
        "name": "pitch page B base",
        "offset": 0x2A70,
        "expected": (0x00000454).to_bytes(4, "big"),
        "patched": PITCH_SECOND_PAGE.to_bytes(4, "big"),
    },
    {
        "name": "pitch DMA source base",
        "offset": 0x2D44,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
    {
        "name": "pitch scroll source base",
        "offset": 0x2FC2,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
    {
        "name": "tile pointer table base",
        "offset": 0x2FA2A,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
    {
        "name": "tile pointer lower bound",
        "offset": 0x2FA34,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
    {
        "name": "tile pointer upper bound",
        "offset": 0x2FA3C,
        "expected": (0x00002818).to_bytes(4, "big"),
        "patched": PITCH_TILE_BASE.to_bytes(4, "big"),
    },
    {
        "name": "tile pointer normalization subtract",
        "offset": 0x2FA54,
        "expected": (0x00002818).to_bytes(4, "big"),
        "patched": PITCH_TILE_BASE.to_bytes(4, "big"),
    },
    {
        "name": "tile pointer rebuild add",
        "offset": 0x2FA86,
        "expected": (0x00002818).to_bytes(4, "big"),
        "patched": PITCH_TILE_BASE.to_bytes(4, "big"),
    },
    {
        "name": "tile pointer update source base",
        "offset": 0x2FBEA,
        "expected": (0x00000400).to_bytes(4, "big"),
        "patched": PITCH_HEADER_BASE.to_bytes(4, "big"),
    },
]


def patch_file(src: Path, dst: Path) -> None:
    data = bytearray(src.read_bytes())

    for patch in PATCHES:
        off = patch["offset"]
        expected = patch["expected"]
        patched = patch["patched"]
        current = bytes(data[off : off + len(expected)])

        if current != expected:
            raise ValueError(
                f"{patch['name']} mismatch at 0x{off:X}: "
                f"expected {expected.hex(' ')}, found {current.hex(' ')}"
            )

        data[off : off + len(patched)] = patched

    dst.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Create an experimental patched copy of an unpacked SWOS2 executable for oversized "
            "legacy MAP loading under WHDLoad. The patch raises the two size checks "
            "and relocates the pitch MAP buffer from low memory to 0x0C0000 so "
            "large fields no longer trample the loader stack and other low-memory "
            "state."
        )
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Source executable, e.g. patch/SWOS2_UNP_SWOSDE or patch/SWOS2_UNP_WHDL",
    )
    parser.add_argument("output", type=Path, help="Patched copy to write")
    args = parser.parse_args()

    if not args.input.is_file():
        print(f"Input file not found: {args.input}", file=sys.stderr)
        return 1

    if args.input.resolve() == args.output.resolve():
        print("Input and output must be different files.", file=sys.stderr)
        return 1

    try:
        patch_file(args.input, args.output)
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        return 1

    print(f"Patched copy written to: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
