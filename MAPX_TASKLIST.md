# MAPX Task List

## Objective

Track the practical work needed to add `.MAPX` support to `swosconv` and make it usable with a patched Amiga SWOS runtime.

This task list assumes the following documents are the current reference:

- `TILE_LIMIT_EXTENSION_PLAN.md`
- `EXTENDED_MAP_IMPLEMENTATION_NOTES.md`
- `MAPX_BINARY_SPEC.md`

## Milestone 1: Freeze The Spec

- [x] review `MAPX_BINARY_SPEC.md` for any missing invariants
- [x] confirm that little-endian is the intended on-disk encoding
- [x] confirm that version `1` uses fixed geometry only
- [x] confirm that version `1` caps `distinct_tile_count` at `1024`
- [x] confirm that `u16` cell indices are sufficient
- [ ] decide whether strict readers reject trailing bytes after tile data
- [x] decide whether canonical offsets are mandatory for writers only or also for readers
- [x] mark the spec as accepted for implementation

## Milestone 2: Refactor `swosconv` Internal Model

- [ ] identify current code paths where legacy `.MAP` assumptions are baked in
- [ ] introduce an internal generic map representation:
  - [ ] rows
  - [ ] cols
  - [ ] distinct tile count
  - [ ] cell index buffer
  - [ ] tile data buffer
- [ ] separate in-memory representation from file serialization details
- [ ] keep current legacy `.MAP` behavior byte-for-byte where applicable

## Milestone 3: Add `.MAPX` Reader

- [x] add extension detection for `.mapx`
- [x] implement header parsing
- [x] implement checked little-endian reads for `u16` and `u32`
- [x] validate magic, version, flags, geometry, and reserved fields
- [x] validate `cell_table_offset`
- [x] validate `tile_data_offset`
- [x] validate file size coverage for cell table and tile block
- [x] load row-major `u16` cell indices
- [x] reject out-of-range cell indices
- [x] load tile data block
- [x] wire `.MAPX -> RAW`
- [x] wire `.MAPX -> BMP`
- [x] optionally wire `.MAPX -> .IFF (ILBM)` if still useful later

## Milestone 4: Add `.MAPX` Writer

- [x] build distinct tile list from input pixels
- [x] allow tile counts above `512`
- [x] reject tile counts above `1024`
- [x] emit canonical version `1` header
- [x] emit row-major `u16` cell table
- [x] emit tile data block in SWOS-compatible tile byte layout
- [x] wire `RAW -> .MAPX`
- [x] wire `BMP -> .MAPX`
- [x] wire `.IFF (ILBM) -> .MAPX` only if there is a concrete use case

## Milestone 5: CLI And Help

- [x] add `.MAPX` to extension-based mode detection
- [x] update `--help` supported conversions text
- [x] decide whether `.MAPX` should appear in README examples immediately or only after implementation is stable
- [x] document that legacy `.MAP` still rejects `> 512` distinct tiles
- [x] document that `.MAPX` is intended for patched runtimes

## Milestone 6: Validation And Tests

### Legacy Regression

- [x] verify all current `.MAP` tests still pass unchanged
- [x] verify all current `.RAW`, `.BMP`, and `.IFF` paths remain stable
- [x] add explicit regression asserting legacy `.MAP` rejects `513+` tiles

### MAPX Happy Path

- [x] add `RAW -> MAPX -> RAW` roundtrip test
- [ ] add `BMP -> MAPX -> BMP` roundtrip test
- [x] add `MAPX -> RAW` golden test
- [x] add `MAPX -> BMP` golden test
- [x] use `SWCPICH7` and `SWCPICH8` as real over-limit validation assets, since they are currently rejected by legacy `.MAP` generation

### MAPX Edge Cases

- [ ] add generated `513`-tile fixture
- [ ] add generated `1024`-tile fixture
- [ ] add generated `1025`-tile rejection case
- [ ] add malformed magic test
- [ ] add malformed version test
- [ ] add malformed geometry test
- [ ] add truncated cell table test
- [ ] add truncated tile block test
- [ ] add out-of-range cell index test
- [ ] add non-zero reserved/flags rejection tests

### Test Tooling

- [ ] decide whether fixture generation should be done in shell, C helper, or a separate script
- [ ] keep golden binary fixtures small where possible
- [ ] ensure tests can run without requiring Amiga-side tooling

## Milestone 7: Documentation

- [x] add `.MAPX` section to `README.md`
- [x] document the difference between legacy `.MAP` and `.MAPX`
- [x] document compatibility expectations with unpatched SWOS
- [x] document exact current support matrix
- [ ] cross-link the three design/spec docs if they remain in the repo

## Milestone 8: Amiga Executable Recon

### Loader

- [ ] identify the file loading entry point for pitch assets
- [ ] determine whether dispatch is extension-based, content-based, or hardcoded by asset slot
- [ ] identify where legacy `.MAP` buffer parsing begins

### Legacy Decode

- [ ] locate the code that reconstructs `2 * c + (d == 0 ? 0 : 1)`
- [ ] confirm whether header bytes `a` and `b` are ignored
- [ ] identify where tile count is derived

### Memory And Limits

- [ ] locate all buffers sized for `512` tiles
- [ ] locate any hard limits or compare instructions tied to `511` or `512`
- [ ] locate any packed or narrow index storage
- [ ] estimate memory headroom for larger tile sets

### Renderer

- [ ] determine whether the renderer uses tile indices directly after load
- [ ] determine whether the pitch is expanded to a final planar buffer early
- [ ] identify caching structures that may depend on the old limit

## Milestone 9: Amiga Patch Prototype

- [ ] choose runtime dispatch strategy:
  - [ ] separate `.MAPX` loader path
  - [ ] or detect `MAPX` magic after file load
- [ ] implement header validation in the patched loader
- [ ] implement cell table and tile block ingestion
- [ ] patch memory allocation sizes
- [ ] patch any index-width assumptions
- [ ] keep legacy `.MAP` loader path intact
- [ ] verify one controlled `513`-tile pitch renders successfully
- [ ] verify one controlled near-cap pitch, ideally `1024` tiles, renders successfully

## Milestone 10: Hardening

- [ ] test malformed `.MAPX` handling on the patched runtime
- [ ] confirm safe failure behavior
- [ ] test fallback to legacy assets
- [ ] verify no regressions on stock compatible pitches
- [ ] re-evaluate whether `1024` remains sufficient after real-asset testing

## Nice-To-Have

- [ ] add a `--mapx` explicit mode override if extension-based dispatch becomes awkward
- [ ] add a dump/debug mode to inspect `.MAPX` header and counts
- [ ] add a small fixture inspector for development
- [ ] add deterministic binary output checks for `.MAPX`

## Risks To Watch

- [ ] spec drift between docs and implementation
- [ ] hidden legacy assumptions in `swosconv` map code
- [ ] hidden `512`-tile assumptions in the Amiga executable
- [ ] runtime memory pressure even if the file format works
- [ ] testing only on tool side and not on real patched runtime

## Suggested Implementation Order

- [x] freeze spec
- [ ] refactor internal map model
- [x] implement `.MAPX` reader
- [x] implement `.MAPX` writer
- [x] add host-side tests
- [x] update help and README
- [ ] reverse engineer Amiga loader
- [ ] patch runtime
- [x] validate with `513+` real asset
- [ ] validate behavior at the chosen `1024`-tile v1 cap

## Done Definition

The work is done when all of the following are true:

- [x] `.MAPX` read/write works in `swosconv`
- [x] legacy `.MAP` behavior is unchanged
- [ ] automated tests cover happy path and malformed files
- [ ] patched Amiga runtime loads `.MAPX`
- [ ] at least one `> 512` distinct-tile pitch renders correctly
