# Exceeding The 512-Tile MAP Limit

## Context

The current SWOS MAP representation stores one tile reference per cell in a `55 x 42` grid.

Each cell is 4 bytes, currently interpreted as:

- `a = 0`
- `b = 0`
- `tile_index = 2 * c + (d == 0 ? 0 : 1)`

That encoding exposes only 9 useful bits for the tile index, so the largest representable value is `511`.
As a consequence, a MAP file can reference at most `512` distinct `16 x 16` tiles.

This is not a `swosconv` implementation limit first. It is a format and game-runtime limit.

## Main Conclusion

If the goal is to support more than `512` distinct tiles while preserving a MAP-like workflow, the Amiga game executable would almost certainly need to be patched as well.

`swosconv` alone can emit a different file format, but the original game binary will not understand it unless its map-loading and pitch-rendering logic are updated to decode the extended tile references.

## Development Options

### Option 1: Define an Extended MAP Format

The cleanest approach is to define a new format variant, for example:

- `.MP2`
- `.MAPX`
- `.MAP` with a custom magic/version header

Suggested cell layout:

- `a = low 8 bits of tile index`
- `b = high 8 bits of tile index`
- `c = reserved`
- `d = reserved`

This gives a full 16-bit tile index range, far beyond current needs.

Pros:

- simple encoder/decoder logic
- no ambiguity
- future-proof
- avoids abusing undocumented semantics in the legacy 4-byte cell

Cons:

- incompatible with the stock game
- requires executable patching
- requires a clear versioning rule so tools can distinguish old vs new maps

### Option 2: Keep The Existing File Size But Reinterpret The 4 Bytes

Another option is to keep the `55 x 42 x 4` header size unchanged and redefine all four bytes as a larger tile index field.

For example:

- `tile_index = a | (b << 8) | (c << 16) | (d << 24)`

Pros:

- same overall header size
- no external side tables required

Cons:

- fully incompatible with the original meaning
- definitely requires a patched loader
- loses any chance of partial compatibility with the legacy MAP structure

In practice this is only attractive if the original four bytes are known to be ignored except for the current `c/d` pairing.

### Option 3: Add An External Tile Bank / Sidecar

Instead of extending the cell index width, a map could keep legacy indices and add:

- multiple banks of 512 tiles
- a per-row bank selector
- a per-cell bank selector in sidecar metadata

Example:

- `pitch.map` keeps legacy tile references
- `pitch.mbx` stores bank selection data

Pros:

- can reduce disruption to the original tile reference model
- may map better onto existing rendering loops if bank switching can be inserted cheaply

Cons:

- higher implementation complexity
- still requires game patching
- introduces synchronization problems between two files
- harder to debug than a single explicit extended format

This is likely worse than Option 1 unless reverse engineering reveals a very convenient hook in the game.

### Option 4: Do Not Extend MAP, Bypass It

If the real need is only "use richer pitches", the most pragmatic solution may be to stop targeting MAP for those assets and load:

- planar RAW directly
- ILBM directly
- another custom bitmap resource

Pros:

- avoids redesigning the MAP format
- potentially much simpler if the engine can be patched to read a full bitmap instead of tile references

Cons:

- larger assets
- more invasive runtime changes
- may conflict with memory, streaming, or decompression assumptions in the original game

This may still be the best engineering choice if the MAP structure is deeply wired into gameplay or renderer code.

## Recommended Direction

If the requirement is specifically "support more than 512 distinct tiles", the most defensible approach is:

1. introduce a clearly versioned extended map format
2. keep legacy `.MAP` support unchanged
3. patch the game to recognize and decode the new format
4. keep `swosconv` able to emit both legacy and extended variants

That gives the safest migration path:

- stock assets continue to work unchanged
- modded assets can opt into the extended format
- failures stay explicit instead of silently producing broken maps

## What Would Need To Be Patched In The Amiga Executable

At minimum, the game binary would likely need changes in these areas:

### 1. File Identification

The loader must distinguish:

- legacy MAP
- extended MAP

Without a format marker, the patched game cannot know which decoding rule to apply.

### 2. Tile Reference Decoding

Current logic likely reconstructs the tile index from the legacy `c/d` rule.

That logic must be replaced or branched so it can decode the wider tile index from the new format.

### 3. Tile Data Lookup

Once a tile index is no longer capped at `511`, any code that computes:

- tile offsets
- tile counts
- allocation sizes
- bounds checks

must be audited.

The key risk is code that assumes:

- `index <= 511`
- `tile_count <= 512`
- 9-bit or 10-bit temporary storage

### 4. Memory Allocation / Buffer Sizing

A larger distinct-tile set means more tile data in memory or on disk.

Any static buffer sized for 512 tiles must be found and expanded.
This is usually where old executables break first after a format extension.

### 5. Rendering And Cache Paths

If the engine caches decoded tiles, blitter inputs, or row pointers, those structures may also assume the old maximum.

Even if the loader is patched, rendering may still fail unless the cache/index structures are widened consistently.

## Reverse Engineering Notes

Before implementing anything, the executable patching work should answer these questions:

1. Where is the pitch MAP loaded?
2. Where is the tile index reconstructed from the 4-byte cell?
3. Where is the tile data count derived?
4. Are there fixed-size buffers for `512` tiles?
5. Is the renderer fed by tile indices or by already expanded planar rows?

If the runtime expands MAP to a full bitmap early, patching may be simpler than expected.
If it keeps tile references alive through much of the frame pipeline, the patch will be broader.

## Suggested Prototype Plan

### Phase 1: Tooling Prototype

- add an internal `extended map` model to `swosconv`
- emit a new format such as `.MAPX`
- allow tile counts above `512`
- add tests with synthetic inputs above the legacy limit

This phase proves the format design independently from the game.

### Phase 2: Executable Recon

- locate the MAP loader in the Amiga executable
- trace tile-index decode logic
- identify buffers and assumptions tied to 512 tiles
- document all touched routines before patching

### Phase 3: Minimal Runtime Patch

- add format detection
- add extended tile-index decode
- patch buffer sizes and bounds checks
- verify that a >512-tile pitch renders correctly

### Phase 4: Compatibility Hardening

- confirm legacy maps still load unchanged
- confirm extended maps fail cleanly on unpatched binaries
- define a stable authoring workflow for modders

## Risks

- hidden hard-coded assumptions about `512` tiles in multiple routines
- memory pressure on the Amiga target
- loader patch succeeds but renderer/cache still truncates indices
- extended assets become incompatible with original tools and game builds

## Practical Assessment

Yes: in all realistic scenarios, exceeding the `511` tile index limit means patching the Amiga executable too.

`swosconv` can help by defining and generating an extended format, but it cannot make the original game understand that format on its own.
