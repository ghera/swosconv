# SWOS2 Oversized MAP Patch

## Scope

This document is the canonical technical note for the SWOS2 runtime patch that allows oversized legacy pitch `.MAP` files under WHDLoad.

The patch keeps the original `.MAP` format. It does not introduce MAPX or any new runtime format.

## Supported Executables

The WHDLoad install uses two unpacked runtime modules:

| WHDLoad path | Size | MD5 | Role |
| --- | --- | ---: | --- | --- |
| `data/SWOS` | `351596` | `B3EB5352926CC2F89D1FFA060C15AC5F` | First module; not patched by this oversized MAP fix |
| `data/SWOS2` / `data/SWOS2.original` | `357226` | `75A7DD45CFE6EBE13F7CA81C28E0A31A` | Second module; patch target |

The oversized pitch `.MAP` loader used during match startup is in the second module, so this patch targets `SWOS2`, not `SWOS`.

Supported `SWOS2` source executables:

| File | Source | Size | MD5 | Status |
| --- | --- | ---: | --- | --- |
| `SWOS2_UNP_SWOSDE` | Official HDF from `sensiblesoccer.de` | `357226` | `75A7DD45CFE6EBE13F7CA81C28E0A31A` | Supported and runtime-tested |
| `SWOS2_UNP_WHDL` | Retroplay WHDLoad Packs | `357226` | `0B8C57E4B51F275E95DB04F5C4BF3A1E` | Supported by matching patch offsets |

Both executables have the same patch-relevant byte sequences and offsets.

Expected patched MD5 values:

| Source | Patched MD5 |
| --- | --- |
| `SWOS2_UNP_SWOSDE` -> `SWOS2_UNP_SWOSDE_PATCHED` | `910BEDEDBBD886763D785F18E3AF6404` |
| `SWOS2_UNP_WHDL` -> `SWOS2_UNP_WHDL_PATCHED` | `4F3B574B1F8E16F07FB643BF4EA07F1F` |

## Runtime Problem

The stock executable loads pitch `.MAP` files at `0x400`.

Large generated maps such as `SWCPICH7.MAP` and `SWCPICH8.MAP` exceed the stock loader size checks and, if those checks are simply raised, overwrite low-memory runtime state. The first confirmed failure was stack overlap:

- load base: `0x400`
- `SWCPICH7.MAP` size: `0x18818`
- copied range: `0x400..0x18C17`
- observed stack: around `0xCAB4`

So the fix must relocate the pitch buffer, not just raise size checks.

## Stable Patch Strategy

The stable patch does this:

- raises the pitch-specific file-size check to `0x1A000`
- raises the generic loader file-size check to `0x1A000`
- relocates the pitch header from `0x400` to `0x0C0000`
- relocates the pitch tile base from `0x2818` to `0x0C2418`
- patches the known consumers that read or rebuild pitch tile pointers

The current stable patch intentionally does not include:

- `12FB4C` normalization helper patch
- relocation to `0x0E0000`
- cleanup hooks around `0x2818`
- forced early calls into pre-match/display state machines

Those experimental changes caused regressions or were unnecessary.

## Patch Offsets

All offsets are file offsets from the unpacked `SWOS2` executable.

| Offset | Purpose | Original | Patched |
| ---: | --- | --- | --- |
| `0x002A16` | pitch loader destination | `00 00 04 00` | `00 0C 00 00` |
| `0x002A20` | pitch-specific size check | `0C 81 00 00 B2 20 63 0C` | `0C 81 00 01 A0 00 63 0C` |
| `0x002A38` | MAP fixup header base | `00 00 04 00` | `00 0C 00 00` |
| `0x002A46` | MAP fixup tile base | `00 00 28 18` | `00 0C 24 18` |
| `0x002A6A` | pitch page A base | `00 00 04 00` | `00 0C 00 00` |
| `0x002A70` | pitch page B base | `00 00 04 54` | `00 0C 00 54` |
| `0x002D44` | pitch DMA/source base | `00 00 04 00` | `00 0C 00 00` |
| `0x002FC2` | pitch scroll/source base | `00 00 04 00` | `00 0C 00 00` |
| `0x00599C` | generic loader size check | `0C 81 00 00 BB B0 62 00 00 92` | `0C 81 00 01 A0 00 62 00 00 92` |
| `0x02FA2A` | tile pointer table base | `00 00 04 00` | `00 0C 00 00` |
| `0x02FA34` | tile pointer lower bound | `00 00 04 00` | `00 0C 00 00` |
| `0x02FA3C` | tile pointer upper bound | `00 00 28 18` | `00 0C 24 18` |
| `0x02FA54` | tile pointer normalization subtract | `00 00 28 18` | `00 0C 24 18` |
| `0x02FA86` | tile pointer rebuild add | `00 00 28 18` | `00 0C 24 18` |
| `0x02FBEA` | tile pointer update source base | `00 00 04 00` | `00 0C 00 00` |

## Recreating The Patch

Generate a patched executable:

```sh
python patch/patch_swos2_oversized_map.py patch/SWOS2_UNP_SWOSDE patch/SWOS2_PATCHED
```

or:

```sh
python patch/patch_swos2_oversized_map.py patch/SWOS2_UNP_WHDL patch/SWOS2_PATCHED
```

Generate an IPS patch:

```sh
python patch/make_swos2_oversized_map_ips.py
```

The default IPS output is:

```text
patch/SWOS2_OVERSIZED_MAP.ips
```

The IPS patch is offset-based and can be applied to both supported executables because the relevant byte sequences are identical at the patch offsets.

## Runtime Validation

Confirmed on the `SWOS2_UNP_SWOSDE` executable:

- stock `.MAP` files still render correctly
- oversized `SWCPICH7.MAP` loads and starts gameplay
- oversized `SWCPICH8.MAP` loads and starts gameplay
- the earlier formation-screen flicker is not currently reproduced

`SWCPICH7.MAP` and `SWCPICH8.MAP` are validation assets, not additional runtime pitch slots. To use either one in-game, replace or rename it over one of the stock `SWCPICH1.MAP` through `SWCPICH6.MAP` files.

`SWOS2_UNP_WHDL` has matching patch offsets and byte patterns, but should still receive a full runtime pass before calling it equally tested.

## Historical Dead Ends

Avoid reintroducing these experiments:

- stack-only relocation around the loader
- buffer base `0x0E0000`
- `12FB4C` normalization helper patch
- cleanup hooks around `0x2818`
- forced early calls into `12FF14`, `12F720`, or display-transition routines

They were useful diagnostics but are not part of the stable patch.
