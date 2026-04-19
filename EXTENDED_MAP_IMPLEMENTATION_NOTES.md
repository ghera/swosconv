# Extended MAP Implementation Notes

## Goal

Define a practical path to support pitches with more than `512` distinct tiles, without breaking legacy `.MAP` handling.

This document is intentionally operational. It focuses on:

- proposed extended format shape
- compatibility rules
- reverse-engineering checklist for the Amiga executable
- phased implementation steps

## Working Assumption

The current stock SWOS runtime understands only the legacy MAP encoding.

Therefore:

- legacy `.MAP` must remain unchanged
- extended maps must use a distinguishable format
- the Amiga executable must be patched to load the extended variant

## Recommended Format Choice

Use a new extension instead of overloading legacy `.MAP`.

Recommended filename extension:

- `.MAPX`

Rationale:

- avoids ambiguity at tool level
- avoids accidental loading in unpatched environments
- makes compatibility rules obvious

## Proposed `.MAPX` File Layout

### Header

Use a small explicit file header before the cell grid:

- magic: `MAPX`
- version: `1`
- width in tiles: `42`
- height in tiles: `55`
- tile width: `16`
- tile height: `16`
- tile count: `u16` or `u32`
- flags: reserved

Suggested v1 encoding:

```text
offset  size  meaning
0x00    4     magic = "MAPX"
0x04    2     version = 1
0x06    2     flags = 0
0x08    2     cols = 42
0x0A    2     rows = 55
0x0C    2     tile_width = 16
0x0E    2     tile_height = 16
0x10    4     distinct_tile_count
0x14    4     cell_table_offset
0x18    4     tile_data_offset
0x1C    4     reserved
```

### Cell Table

Store one `u16` tile index per map cell:

- cell count = `42 * 55 = 2310`
- cell table size = `2310 * 2 = 4620` bytes

Cell order should match the existing row-major MAP order to simplify conversion and runtime adaptation.

### Tile Data

Store unique tiles in the same tile byte layout already used by the current converter for MAP tiles:

- `16 x 16`
- `4bpp`
- `32` bytes per tile

This keeps tile serialization close to the existing code and reduces migration risk.

## Why `u16` Is Enough

Even `4096` distinct tiles would already be far beyond current needs and likely above what is practical on the target runtime.

Using `u16`:

- simplifies implementation
- avoids premature format bloat
- is enough for any realistic SWOS pitch modding scenario

There is no immediate need for `u32` tile indices in the cell table.

## Compatibility Rules

### Tooling Rules

`swosconv` should behave like this:

- `.MAP` output:
  - preserve current behavior
  - reject inputs above `512` distinct tiles
- `.MAPX` output:
  - allow counts above `512`
  - emit explicit versioned header
- `.MAPX` input:
  - decode directly
- `.MAP` input:
  - decode using the legacy `c/d` rule

### Runtime Rules

Patched game behavior should be:

- if file is legacy `.MAP`, use old loader path
- if file is `.MAPX`, use new loader path

Do not try to make one decoder guess both formats without a reliable marker.

### Failure Rules

Unpatched game:

- should not be expected to load `.MAPX`

Patched game:

- should reject malformed `.MAPX` with clear failure behavior
- should keep accepting stock `.MAP`

## Minimal `swosconv` Scope For A Prototype

The smallest useful tooling prototype is:

1. add internal support for an extended tile-count model
2. add `.MAPX` writer
3. add `.MAPX` reader
4. add roundtrip tests:
   - `RAW -> MAPX`
   - `MAPX -> RAW`
   - `BMP -> MAPX`
   - `MAPX -> BMP`
5. add synthetic over-limit fixture generation

Do not start by modifying legacy `.MAP` behavior.

## Suggested Internal Data Model

For implementation inside `swosconv`, use a generic in-memory map representation:

- `rows`
- `cols`
- `distinct_tile_count`
- `cell_indices[]`
- `tile_bytes[]`

Then provide two serializers:

- legacy MAP serializer
- MAPX serializer

This prevents the legacy format constraints from leaking into the higher-level conversion pipeline.

## Test Plan For `swosconv`

### Legacy Regression Tests

Keep all existing `.MAP` tests unchanged.

Explicitly verify:

- legacy `.MAP` still rejects `> 512` distinct tiles
- byte-for-byte behavior for current representable fixtures remains unchanged

### MAPX Tests

Add at least these cases:

- small representable input converted to `.MAPX`
- synthetic image with `513` distinct tiles converted to `.MAPX`
- synthetic image with `700+` distinct tiles converted to `.MAPX`
- `.MAPX -> RAW` roundtrip
- `.MAPX -> BMP` roundtrip
- malformed `.MAPX` header rejected
- malformed tile count rejected
- out-of-range cell index rejected

### Fixture Strategy

Do not rely only on hand-authored binary fixtures.

Prefer:

- generated fixtures for edge cases
- a few stable binary golden files for regression

## Amiga Executable Reverse-Engineering Checklist

The patching work should answer these questions in order.

### Loader Discovery

- Where is the pitch asset opened?
- Where is the file size read or inferred?
- Is file extension consulted, or only content/layout?

### Legacy Decode Path

- Where are the 4-byte MAP cells parsed?
- Where is `2 * c + (d == 0 ? 0 : 1)` effectively reconstructed?
- Are `a` and `b` ignored everywhere, or only in one stage?

### Tile Storage

- Where is the distinct tile count computed?
- Where is memory allocated for tile storage?
- Are there static buffers sized to `512 * 32` bytes or similar?

### Index Width Audit

- Are tile indices stored in byte, word, or packed bitfields?
- Are any operations masking with `0x1FF`, `0xFF`, or similar?
- Are there compare instructions enforcing `<= 511`?

### Renderer Path

- Does the renderer access tile indices directly during draw?
- Or does the loader expand the full pitch to a planar bitmap before rendering?
- Are there per-row or per-frame tile caches with fixed-size tables?

### Error Handling

- What happens today when a malformed pitch is loaded?
- Is there a safe way to fail and return to menu?
- Is there an existing "unsupported asset" path that can be reused?

## Amiga Patch Design Guidelines

### Keep The New Path Separate

Prefer a branch like:

- detect `.MAPX`
- call new decode routine
- otherwise fall through to legacy path

Do not rewrite the legacy path more than necessary.

### Avoid Widening Everything Blindly

First identify:

- exact buffers
- exact index representations
- exact bounds checks

Then patch only what is needed.

On an Amiga target, unnecessary widening can create memory and performance regressions.

### Expand Early If Possible

If feasible, convert `.MAPX` to the runtime's final internal representation as early as possible.

That is often safer than threading wider tile indices through many old code paths.

If the game already expands legacy MAP into a planar pitch buffer early, hook the patch there.

## Open Design Questions

These should be resolved before coding the runtime patch:

1. Does the game need random access to unique tiles after load, or only to expanded output?
2. Is the distinct tile set kept resident after pitch construction?
3. Is there enough memory headroom for significantly larger tile sets?
4. Does the game use the same MAP loader for all pitch-like assets?
5. Is extension-based dispatch acceptable in the existing file loading model?

## Suggested Execution Order

1. implement `.MAPX` read/write in `swosconv`
2. add over-limit automated tests in the tool
3. reverse engineer the Amiga MAP loader
4. identify all `512`-tile assumptions
5. patch loader and memory sizing
6. validate with one controlled `513+` test asset
7. only then expand to more ambitious tile counts

## Exit Criteria

The feature should be considered viable only when all of these hold:

- stock `.MAP` support is unchanged
- `.MAPX` roundtrips correctly in `swosconv`
- patched Amiga executable loads `.MAPX`
- a `> 512`-tile pitch renders correctly
- malformed `.MAPX` fails safely

## Short Practical Summary

The most practical route is:

- keep legacy `.MAP` exactly as-is
- add a separate `.MAPX` format with explicit header and `u16` cell indices
- patch the Amiga loader to recognize and decode `.MAPX`
- widen only the specific runtime assumptions that are truly tied to `512` tiles
