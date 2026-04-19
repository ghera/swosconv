# MAPX Binary Specification

## Status

Draft specification for an extended SWOS pitch map format intended to exceed the legacy `512`-tile MAP limit.

This spec is designed for:

- `swosconv` implementation
- automated validation
- Amiga runtime patching

## Purpose

`MAPX` is a versioned binary format for storing:

- a pitch cell grid
- a distinct tile set
- tile references wider than the legacy MAP encoding

It is intended to coexist with legacy `.MAP`, not replace it.

## Byte Order

All multibyte integer fields are little-endian.

This must be enforced consistently by:

- file writers
- file readers
- runtime patch code

## File Extension

Recommended extension:

- `.MAPX`

The extension is part of the compatibility strategy and should not be treated as cosmetic.

## Fixed Geometry For Version 1

Version `1` of `MAPX` is restricted to the same pitch geometry used by the current SWOS tooling:

- columns: `42`
- rows: `55`
- tile width: `16`
- tile height: `16`
- bit depth: `4bpp`
- bytes per tile: `32`

Readers must reject files that deviate from these values unless a future version explicitly allows them.

## High-Level Layout

The file contains three main regions:

1. fixed-size header
2. cell table
3. tile data block

The header stores explicit offsets so the format remains versionable.

## Header Layout

Header size is fixed at `32` bytes in version `1`.

```text
offset  size  type   name
0x00    4     char4  magic
0x04    2     u16    version
0x06    2     u16    flags
0x08    2     u16    cols
0x0A    2     u16    rows
0x0C    2     u16    tile_width
0x0E    2     u16    tile_height
0x10    4     u32    distinct_tile_count
0x14    4     u32    cell_table_offset
0x18    4     u32    tile_data_offset
0x1C    4     u32    reserved
```

## Header Field Semantics

### `magic`

Must be ASCII:

```text
MAPX
```

Files with any other magic must be rejected.

### `version`

For this spec:

- value must be `1`

Unknown versions must be rejected explicitly.

### `flags`

For version `1`:

- must be `0`

Non-zero flags must be rejected unless support is intentionally added later.

### `cols`

For version `1`:

- must be `42`

### `rows`

For version `1`:

- must be `55`

### `tile_width`

For version `1`:

- must be `16`

### `tile_height`

For version `1`:

- must be `16`

### `distinct_tile_count`

Number of unique tiles stored in the tile data block.

Valid range for version `1`:

- minimum: `1`
- maximum: `65535`

Practical implementations may choose a lower maximum for safety, but they must reject above-limit inputs explicitly.

### `cell_table_offset`

Offset, in bytes from the start of the file, to the first cell index entry.

For version `1`, writers should set:

- `cell_table_offset = 0x20`

Readers may accept other values only if:

- the offset is at least `0x20`
- the offset is inside the file
- the cell table fits entirely inside the file

### `tile_data_offset`

Offset, in bytes from the start of the file, to the first tile byte.

For version `1`, writers should set:

- `tile_data_offset = cell_table_offset + cell_table_size`

Readers must verify:

- `tile_data_offset >= cell_table_offset + cell_table_size`
- tile data block fits entirely inside the file

### `reserved`

For version `1`:

- must be `0`

Non-zero values should be rejected.

## Derived Values

Readers and writers must derive these values using the header:

```text
cell_count      = cols * rows
cell_entry_size = 2
cell_table_size = cell_count * cell_entry_size
tile_size       = 32
tile_data_size  = distinct_tile_count * tile_size
```

For version `1`, this resolves to:

```text
cell_count      = 2310
cell_table_size = 4620
tile_size       = 32
```

## Cell Table Encoding

The cell table stores one tile index per map cell.

### Entry Type

Each entry is:

- `u16`

### Entry Order

Entries are row-major:

```text
index = row * cols + col
```

with:

- `row` in `0..54`
- `col` in `0..41`

### Entry Constraints

Each cell entry value must satisfy:

- `cell_index < distinct_tile_count`

Any out-of-range cell reference must cause the file to be rejected.

## Tile Data Block

The tile data block stores `distinct_tile_count` unique tiles.

### Tile Order

Tile `n` in the tile data block is the tile referenced by cell entries with value `n`.

### Tile Encoding

Each tile is exactly `32` bytes and uses the same tile byte layout already used by the current SWOS MAP tile payload.

That means:

- `16 x 16` pixels
- `4bpp`
- planar/interleaved tile byte representation compatible with existing SWOS tile handling in `swosconv`

This spec does not redefine the internal 32-byte tile layout. It inherits the current SWOS tile encoding already used by the legacy converter pipeline.

### Tile Block Constraints

Tile data size must be exactly:

```text
distinct_tile_count * 32
```

If the file truncates the tile block, it must be rejected.

Extra trailing data after the tile block:

- should be rejected by strict readers
- may be ignored by permissive debug tools

For production readers, strict rejection is recommended.

## Writer Rules

Version `1` writers must:

- emit the exact `MAPX` magic
- emit version `1`
- emit zeroed `flags`
- emit fixed geometry values
- emit `cell_table_offset = 0x20`
- emit `tile_data_offset = 0x20 + 4620`
- emit zeroed `reserved`
- emit row-major `u16` cell entries
- emit `32` bytes per distinct tile

Writers must not:

- emit out-of-range cell indices
- emit duplicate semantic versions with incompatible layouts
- silently clamp tile counts

## Reader Rules

Version `1` readers must reject files if any of the following hold:

- magic is not `MAPX`
- version is unsupported
- flags is non-zero
- geometry differs from version `1` fixed values
- `distinct_tile_count == 0`
- `cell_table_offset` is invalid
- `tile_data_offset` is invalid
- cell table is truncated
- tile data block is truncated
- any cell index is out of range
- arithmetic overflows occur during size computation

## Validation Algorithm

Recommended reader validation order:

1. read and validate the 32-byte header
2. compute derived sizes using checked arithmetic
3. verify `cell_table_offset`
4. verify `tile_data_offset`
5. verify full file coverage of cell table and tile block
6. read cell table
7. validate every cell index against `distinct_tile_count`
8. read tile data

This order reduces the risk of malformed-file memory issues.

## Canonical Version 1 Layout

For a valid canonical version `1` file:

```text
header_size       = 0x20
cell_table_offset = 0x20
cell_table_size   = 4620
tile_data_offset  = 0x20 + 4620 = 0x122C
file_size         = 0x122C + distinct_tile_count * 32
```

Example:

- if `distinct_tile_count = 513`
- tile block size = `16416`
- total file size = `4668 + 16416 = 21084` bytes

## Compatibility With Legacy MAP

`MAPX` is not binary-compatible with legacy `.MAP`.

A legacy `.MAP` reader must not be expected to parse it.

Compatibility strategy is:

- legacy assets remain `.MAP`
- extended assets use `.MAPX`
- patched runtime dispatches based on format identity

## Runtime Integration Notes

A patched runtime may either:

- consume the `MAPX` cell table and tile block directly
- or expand `MAPX` into the same internal representation used after legacy MAP load

The second option is usually safer because it limits invasive changes deeper in the renderer.

## Future Versioning

Potential future uses of:

- `flags`
- `reserved`
- `version`

Examples:

- alternate tile encoding
- compressed tile block
- larger geometry
- metadata chunks

Version `1` readers must not attempt to interpret future layouts speculatively.

## Test Vectors To Prepare

At minimum, implementations should be tested with:

- smallest valid tile count
- exactly `512` tiles
- exactly `513` tiles
- larger over-limit case such as `700`
- bad magic
- bad version
- bad geometry
- truncated cell table
- truncated tile block
- out-of-range cell index

## Summary

`MAPX` version `1` is:

- little-endian
- explicitly versioned
- fixed-geometry
- row-major
- based on `u16` cell indices
- based on 32-byte SWOS-compatible tile payloads

Its main purpose is to lift the legacy MAP index-width limit while keeping the implementation simple and auditable.
