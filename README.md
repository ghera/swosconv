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

> **Note:** `.MAP` files must be uncompressed. The game reads both compressed and uncompressed `.MAP` files, but original game assets use [RNC ProPack](https://aminet.net/package/util/pack/RNC_ProPack). Decompress before use with `swosconv`.

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

Build release binaries for Windows and Amiga:

```sh
make release
```

Cross-compile with [bebbo's amiga-gcc](https://franke.ms/git/bebbo/amiga-gcc):

```sh
make amiga
```

Compile on a real Amiga with [vbcc](http://sun.hasenbraten.de/vbcc/) ([Aminet link](https://aminet.net/package/dev/c/vbcc_bin_amigaos68k)):

```sh
vc -c99 -O2 -o swosconv swosconv.c
```

## Usage

```sh
swosconv [-n|--no-tile-limit] -i <input> -o <output>
```

Examples:

```sh
swosconv -i test/map/SWCPICH1.MAP -o TEST.RAW
swosconv -i test/raw/SWCPICH1.RAW -o TEST.MAP
swosconv -i test/bmp/SWCPICH7.BMP -o TEST.RAW
swosconv -i test/ilbm/SWCPICH7.IFF -o TEST.RAW
```

Mode detection is automatic from file extensions.

### `--no-tile-limit`

`--no-tile-limit` is an experimental flag for writing oversized `.MAP` files.

The resulting files are not compatible with the standard Amiga executable (see [Runtime Patch Assets](#runtime-patch-assets)).

> **Oversized** refers to the number of distinct tiles in the `.MAP` file exceeding the stock loader's tile limit, not to image resolution.

Oversized pitch files such as `SWCPICH7.MAP` and `SWCPICH8.MAP` are not extra game slots. To use one in SWOS, replace or rename it over one of the standard pitch files, `SWCPICH1.MAP` through `SWCPICH6.MAP`.

![Oversized field example](noTileLimit.png)

## Runtime Patch Assets

The [`patch/`](./patch) directory contains an experimental `SWOS2` runtime patch for loading oversized `.MAP` files generated with `--no-tile-limit`.
See [`SWOS2_OVERSIZED_MAP_PATCH.md`](./patch/SWOS2_OVERSIZED_MAP_PATCH.md) for details.

## White Label Assets

The [`whitelabel/`](./whitelabel) directory contains graphic assets for a neutral SWOS build, without references to specific events or seasons such as `95/96` or `96/97`.

Menu and loader graphics are provided in all supported bitmap formats:

- `CJCGRAFS`
- `MENUS2`
- `LOADER1`
- `MENUBG`

Each is available as `.IFF` (ILBM), `.BMP`, and `.RAW`.

The `SWCPICH1` through `SWCPICH6` pitch files in `whitelabel/` are not byte-identical copies of the original game assets. They include minor fixes and visual improvements while preserving the standard six pitch slots.

## Format Notes

### SWOS MAP

SWOS pitch MAP files contain:

1. a header of `55 x 42` cells
2. a tile data section

Each header cell is 4 bytes:

- big-endian tile offset within the tile data section

For the supplied SWOS pitch assets, tile offsets are aligned to `128` bytes, matching the on-disk tile payload size used by the game.

The stock Amiga executable also enforces a practical MAP file size limit of `0xB220` bytes. With the current pitch geometry, that means at most:

- `284` distinct tiles

without patching the runtime.

### RAW

For files whose basename starts with `SWCPICH`, RAW is the SWOS pitch bitmap in Amiga planar/interleaved layout.

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

For other 4-bitplane graphics such as menu and loader assets, `.IFF <-> .RAW` uses a simple planar/interleaved bitmap conversion instead of the tilemapped format. The converter reads the ILBM dimensions from `BMHD` for `.IFF -> .RAW`; for `.RAW -> .IFF`, the supported simple RAW sizes are:

- `40960` bytes: `320x256`, 4 bitplanes
- `47872` bytes: `345x272`, 4 bitplanes

### BMP

Supported BMP input is intentionally narrow.

For `SWCPICH*` files:

- size: `672x880`

For non-`SWCPICH` simple graphics:

- size: `320x256` or `345x272`

Common requirements:

- file type: Windows BMP (`BM`)
- DIB header: `BITMAPINFOHEADER`-compatible (`biSize >= 40`)
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

- `672x880` for `SWCPICH*`
- `320x256` or `345x272` for supported non-`SWCPICH` RAW sizes
- `4bpp`
- uncompressed
- bottom-up
- uses the pitch palette for `SWCPICH*`
- uses the default non-pitch graphics palette for other files

### ILBM

`.IFF` support in this project means **ILBM**.

Supported pitch ILBM characteristics for `SWCPICH*` files:

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

The writer emits valid ILBM files using `ByteRun1` BODY compression. Compression is applied per ILBM row and bitplane, which is required for compatibility with Amiga ILBM readers.

For non-`SWCPICH` ILBM files, `.IFF -> .RAW` accepts 4-bitplane ILBM images with masking `0` and ByteRun1 or uncompressed BODY data. These files are converted directly row by row and are not valid input for `.MAP` conversion.

Generated ILBM files intentionally omit non-essential metadata chunks such as `ANNO` and `DPPS`. They contain only `BMHD`, `CMAP`, `CAMG`, `DPI `, and `BODY`.

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

Extended MAP fixtures for experimental use:

- `test/map/SWCPICH7.MAP` <-> `test/raw/SWCPICH7.RAW`
- `test/map/SWCPICH8.MAP` <-> `test/raw/SWCPICH8.RAW`

Verified behaviors:

- `MAP -> RAW` matches the supplied RAW references
- `RAW -> MAP` matches the supplied MAP references for representable inputs
- `BMP -> RAW` matches the supplied RAW references
- `RAW -> BMP` matches the supplied BMP references byte-for-byte
- `ILBM -> RAW` matches the supplied RAW references
- `RAW -> ILBM -> RAW` is lossless
- `MAP -> BMP -> MAP` is lossless on representable inputs
- `MAP -> ILBM -> RAW` is lossless on representable inputs
- `RAW -> MAP` with `--no-tile-limit` is deterministic for `SWCPICH7` and `SWCPICH8`
- `BMP -> MAP` with `--no-tile-limit` matches the supplied extended MAP references
- `ILBM -> MAP` with `--no-tile-limit` matches the supplied extended MAP references

Known non-representable examples for MAP output:

- `test/raw/SWCPICH7.RAW`
- `test/bmp/SWCPICH7.BMP`
- `test/ilbm/SWCPICH7.IFF`
- `test/raw/SWCPICH8.RAW`
- `test/bmp/SWCPICH8.BMP`
- `test/ilbm/SWCPICH8.IFF`

These exceed the stock-compatible MAP size limit and must be rejected for MAP generation unless `--no-tile-limit` is used.

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
- stock-compatible MAP output is limited by the executable's practical file size cap
- not every visually valid image can be converted to MAP without tile reduction

## License

This project is licensed under the **0BSD** license.

See [LICENSE](./LICENSE).
