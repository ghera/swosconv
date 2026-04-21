#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys

from patch_swos2_oversized_map import PATCHES


def build_ips() -> bytes:
    chunks = [b"PATCH"]

    for patch in PATCHES:
        offset = patch["offset"]
        expected = patch["expected"]
        patched = patch["patched"]

        if len(expected) != len(patched):
            raise ValueError(
                f"IPS record size mismatch for {patch['name']}: "
                f"{len(expected)} != {len(patched)}"
            )

        chunks.append(offset.to_bytes(3, "big"))
        chunks.append(len(patched).to_bytes(2, "big"))
        chunks.append(patched)

    chunks.append(b"EOF")
    return b"".join(chunks)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Write an IPS patch for the stable SWOS2 oversized legacy MAP runtime "
            "patch. This raises the file-size checks and relocates the pitch MAP "
            "buffer to 0x0C0000."
        )
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path(__file__).resolve().parent / "SWOS2_OVERSIZED_MAP.ips",
        help="Output IPS path. Defaults to SWOS2_OVERSIZED_MAP.ips in this script's directory.",
    )
    args = parser.parse_args()

    try:
        data = build_ips()
        args.output.write_bytes(data)
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        return 1

    print(f"IPS patch written to: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
