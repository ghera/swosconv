# swosconv

`swosconv` is a small C99-based converter for **Sensible World of Soccer** pitch graphics.

It converts between these formats:

- `MAP` - SWOS tile map format used by the game executable
- `RAW` - planar/interleaved Amiga pitch bitmap data
- `BMP` - constrained indexed Windows BMP used as an external editing/export format
- `ILBM` - Amiga bitmap format using the `.IFF` extension in this project

The project is intentionally small and portable so it can be built with older toolchains, including Amiga-oriented cross-compilers.

## Features

Supported conversions:

- `.RAW -> .MAP, .BMP, .IFF (ILBM)`
- `.MAP -> .RAW, .BMP, .IFF (ILBM)`
- `.BMP -> .RAW, .MAP`
- `.IFF (ILBM) -> .RAW, .MAP`

Notes:

- `.IFF` support means **ILBM**, not arbitrary IFF forms.
- `BMP -> MAP` and `ILBM -> MAP` are limited by the SWOS MAP format and may be rejected if the image contains too many distinct tiles.

## Build

Primary build path:

```sh
make
```

Clean:

```sh
make clean
```

Format:

```sh
make format
```

Lint:

```sh
make lint
```

Build check:

```sh
make check
```

Automated test suite:

```sh
make test
```

Strict host build example:

```sh
gcc -std=c99 -pedantic -Wall -Wextra -Werror swosconv.c -o swosconv.exe
```

Cross-compiler example:

```sh
make CC=m68k-amigaos-gcc EXE=swosconv
```

## Usage

```sh
swosconv -i <input> -o <output>
```

Examples:

```sh
swosconv -i test/map/SWCPICH1.MAP -o TEST.RAW
swosconv -i test/raw/SWCPICH1.RAW -o TEST.MAP
swosconv -i test/bmp/SWCPICH7.BMP -o TEST.RAW
swosconv -i test/ilbm/SWCPICH7.IFF -o TEST.RAW
```

Mode detection is automatic from file extensions.

## Repository Layout

- [`swosconv.c`](./swosconv.c) - single-file implementation
- [`Makefile`](./Makefile) - build entrypoint
- [`test/`](./test) - reference files, validation assets, and the Bash test runner

## Format Notes

### SWOS MAP

SWOS pitch MAP files contain:

1. a header of `55 x 42` cells
2. a tile data section

Each header cell is 4 bytes:

- `a = 0`
- `b = 0`
- `tile_index = 2 * c + (d == 0 ? 0 : 1)`

This means the maximum representable tile index is `511`, so a MAP file can contain at most:

- `512` distinct tiles

If an input image exceeds that limit, `RAW -> MAP`, `BMP -> MAP`, or `ILBM -> MAP` must fail explicitly.

### RAW

The RAW format handled by this tool is the SWOS pitch bitmap in Amiga planar/interleaved layout.

Geometry:

- width: `672`
- height: `880`
- tile grid: `42 x 55`
- tile size: `16 x 16`
- bit depth: `4` bitplanes

Each scanline is written as:

- plane 0 bytes for all tiles in the row
- plane 1 bytes for all tiles in the row
- plane 2 bytes for all tiles in the row
- plane 3 bytes for all tiles in the row

Within each plane byte, pixels are packed MSB-first.

### BMP

Supported BMP input is intentionally narrow:

- file type: Windows BMP (`BM`)
- DIB header: `BITMAPINFOHEADER`-compatible (`biSize >= 40`)
- size: `672x880`
- planes: `1`
- bit depth: `4bpp`
- compression: `BI_RGB` / uncompressed
- palette: `16` colors, or `biClrUsed = 0` with implied 16-color palette

BMP interpretation rules:

- row padding follows normal BMP rules
- bottom-up BMPs are supported
- top-down BMPs are supported if `biHeight` is negative
- for `4bpp`, the left pixel is the high nibble and the right pixel is the low nibble
- during `BMP -> RAW` / `BMP -> MAP`, only palette indices `0..15` matter; RGB palette values are ignored

BMP output produced by this tool is deterministic:

- `672x880`
- `4bpp`
- uncompressed
- bottom-up
- uses the SWOS palette found in the supplied reference BMP files

### ILBM

`.IFF` support in this project means **ILBM**.

Supported ILBM characteristics:

- `FORM ILBM`
- `672x880`
- `4` bitplanes
- masking: `0`
- compression:
  - `0` (uncompressed)
  - `1` (`ByteRun1`)

The implementation reads and writes the essential ILBM chunks:

- `BMHD`
- `CMAP`
- `CAMG`
- `DPI `
- `BODY`

The writer emits valid ILBM files using `ByteRun1` BODY compression.

## Validation

Reference MAP/RAW pairs:

- `test/map/SWCPICH1.MAP` through `SWCPICH6.MAP`
- `test/raw/SWCPICH1.RAW` through `SWCPICH6.RAW`

Reference BMP/RAW pairs:

- `test/bmp/SWCPICH7.BMP` <-> `test/raw/SWCPICH7.RAW`
- `test/bmp/SWCPICH8.BMP` <-> `test/raw/SWCPICH8.RAW`

Reference ILBM/RAW pairs:

- `test/ilbm/SWCPICH7.IFF` <-> `test/raw/SWCPICH7.RAW`
- `test/ilbm/SWCPICH8.IFF` <-> `test/raw/SWCPICH8.RAW`

Verified behaviors:

- `MAP -> RAW` matches the supplied RAW references
- `RAW -> MAP` matches the supplied MAP references for representable inputs
- `BMP -> RAW` matches the supplied RAW references
- `RAW -> BMP` matches the supplied BMP references byte-for-byte
- `ILBM -> RAW` matches the supplied RAW references
- `RAW -> ILBM -> RAW` is lossless
- `MAP -> BMP -> MAP` is lossless on representable inputs
- `MAP -> ILBM -> RAW` is lossless on representable inputs

Known non-representable examples for MAP output:

- `test/raw/SWCPICH7.RAW`
- `test/bmp/SWCPICH7.BMP`
- `test/ilbm/SWCPICH7.IFF`
- `test/raw/SWCPICH8.RAW`
- `test/bmp/SWCPICH8.BMP`
- `test/ilbm/SWCPICH8.IFF`

These use more than `512` distinct tiles and must be rejected for MAP generation.

## Design Goals

- small codebase
- portable C99 implementation with a conservative, retro-friendly style
- deterministic output where practical
- explicit rejection of unsupported or lossy cases

## Tooling

The repository includes lightweight formatting and linting configuration:

- `.clang-format` for consistent source formatting
- `.clang-tidy` with a deliberately narrow check set:
  - `clang-analyzer-*`
  - a few non-invasive `bugprone-*` checks

The lint setup is intentionally conservative to stay practical for low-level C code and retro-oriented toolchains.

## Limitations

- `ILBM` support is limited to `.IFF` files carrying `FORM ILBM`
- BMP support is restricted to the exact indexed 4bpp pitch format described above
- MAP output cannot represent more than `512` distinct tiles
- not every visually valid image can be converted to MAP without tile reduction

## License

This project is licensed under the **0BSD** license.

See [LICENSE](./LICENSE).
