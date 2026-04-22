#!/usr/bin/env bash

set -eu

EXE="${1:-./swosconv.exe}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEMP_ROOT="$SCRIPT_DIR/.tmp-tests"

assert_file_equal() {
    actual_path="$1"
    expected_path="$2"
    label="$3"

    if ! cmp -s "$actual_path" "$expected_path"; then
        printf '%s\n' "$label mismatch" >&2
        exit 1
    fi
}

invoke_converter() {
    input_path="$1"
    output_path="$2"
    label="$3"
    shift 3
    stdout_path="$TEMP_ROOT/$label.stdout.txt"
    stderr_path="$TEMP_ROOT/$label.stderr.txt"

    if ! "$EXE" "$@" -i "$input_path" -o "$output_path" >"$stdout_path" 2>"$stderr_path"; then
        printf '%s failed.\n' "$label" >&2
        cat "$stdout_path" "$stderr_path" >&2
        exit 1
    fi
}

assert_rejected_for_tile_limit() {
    input_path="$1"
    output_path="$2"
    label="$3"
    stdout_path="$TEMP_ROOT/$label.stdout.txt"
    stderr_path="$TEMP_ROOT/$label.stderr.txt"

    if "$EXE" -i "$input_path" -o "$output_path" >"$stdout_path" 2>"$stderr_path"; then
        printf '%s unexpectedly succeeded.\n' "$label" >&2
        exit 1
    fi

    if ! grep -q "stock SWOS MAP size limit" "$stdout_path" "$stderr_path"; then
        printf '%s failed for the wrong reason.\n' "$label" >&2
        cat "$stdout_path" "$stderr_path" >&2
        exit 1
    fi
}

cleanup() {
    rm -rf "$TEMP_ROOT"
}

trap cleanup EXIT

cd "$REPO_ROOT"

if [ ! -f "$EXE" ]; then
    printf 'Executable not found: %s\n' "$EXE" >&2
    exit 1
fi

rm -rf "$TEMP_ROOT"
mkdir -p "$TEMP_ROOT"

for n in 1 2 3 4 5 6; do
    map_path="test/map/SWCPICH${n}.MAP"
    raw_path="test/raw/SWCPICH${n}.RAW"
    bmp_path="test/bmp/SWCPICH${n}.BMP"
    ilbm_path="test/ilbm/SWCPICH${n}.IFF"

    raw_from_map="$TEMP_ROOT/SWCPICH${n}.from_map.raw"
    map_from_raw="$TEMP_ROOT/SWCPICH${n}.from_raw.map"
    bmp_from_map="$TEMP_ROOT/SWCPICH${n}.from_map.bmp"
    map_from_bmp="$TEMP_ROOT/SWCPICH${n}.from_bmp.map"
    ilbm_from_map="$TEMP_ROOT/SWCPICH${n}.from_map.iff"
    raw_from_ilbm="$TEMP_ROOT/SWCPICH${n}.from_ilbm.raw"

    invoke_converter "$map_path" "$raw_from_map" "swcpich${n}_map_to_raw"
    assert_file_equal "$raw_from_map" "$raw_path" "SWCPICH${n} MAP->RAW"
    printf 'PASS SWCPICH%s MAP->RAW\n' "$n"

    invoke_converter "$raw_path" "$map_from_raw" "swcpich${n}_raw_to_map"
    assert_file_equal "$map_from_raw" "$map_path" "SWCPICH${n} RAW->MAP"
    printf 'PASS SWCPICH%s RAW->MAP\n' "$n"

    invoke_converter "$map_path" "$bmp_from_map" "swcpich${n}_map_to_bmp"
    assert_file_equal "$bmp_from_map" "$bmp_path" "SWCPICH${n} MAP->BMP"
    printf 'PASS SWCPICH%s MAP->BMP\n' "$n"

    invoke_converter "$bmp_path" "$map_from_bmp" "swcpich${n}_bmp_to_map"
    assert_file_equal "$map_from_bmp" "$map_path" "SWCPICH${n} BMP->MAP"
    printf 'PASS SWCPICH%s BMP->MAP\n' "$n"

    invoke_converter "$map_path" "$ilbm_from_map" "swcpich${n}_map_to_ilbm"
    assert_file_equal "$ilbm_from_map" "$ilbm_path" "SWCPICH${n} MAP->ILBM"
    printf 'PASS SWCPICH%s MAP->ILBM\n' "$n"

    invoke_converter "$ilbm_path" "$raw_from_ilbm" "swcpich${n}_ilbm_to_raw"
    assert_file_equal "$raw_from_ilbm" "$raw_path" "SWCPICH${n} ILBM->RAW"
    printf 'PASS SWCPICH%s ILBM->RAW\n' "$n"
done

for n in 7 8; do
    map_path="test/map/SWCPICH${n}.MAP"
    raw_path="test/raw/SWCPICH${n}.RAW"
    bmp_path="test/bmp/SWCPICH${n}.BMP"
    ilbm_path="test/ilbm/SWCPICH${n}.IFF"

    raw_from_bmp="$TEMP_ROOT/SWCPICH${n}.from_bmp.raw"
    bmp_from_raw="$TEMP_ROOT/SWCPICH${n}.from_raw.bmp"
    raw_from_ilbm="$TEMP_ROOT/SWCPICH${n}.from_ilbm.raw"
    ilbm_from_raw="$TEMP_ROOT/SWCPICH${n}.from_raw.iff"
    raw_roundtrip="$TEMP_ROOT/SWCPICH${n}.roundtrip.raw"
    map_from_raw="$TEMP_ROOT/SWCPICH${n}.from_raw.map"
    map_from_bmp="$TEMP_ROOT/SWCPICH${n}.from_bmp.map"
    map_from_ilbm="$TEMP_ROOT/SWCPICH${n}.from_ilbm.map"
    raw_from_map="$TEMP_ROOT/SWCPICH${n}.from_map.raw"
    bmp_from_map="$TEMP_ROOT/SWCPICH${n}.from_map.bmp"

    invoke_converter "$bmp_path" "$raw_from_bmp" "swcpich${n}_bmp_to_raw"
    assert_file_equal "$raw_from_bmp" "$raw_path" "SWCPICH${n} BMP->RAW"
    printf 'PASS SWCPICH%s BMP->RAW\n' "$n"

    invoke_converter "$raw_path" "$bmp_from_raw" "swcpich${n}_raw_to_bmp"
    assert_file_equal "$bmp_from_raw" "$bmp_path" "SWCPICH${n} RAW->BMP"
    printf 'PASS SWCPICH%s RAW->BMP\n' "$n"

    invoke_converter "$ilbm_path" "$raw_from_ilbm" "swcpich${n}_ilbm_to_raw"
    assert_file_equal "$raw_from_ilbm" "$raw_path" "SWCPICH${n} ILBM->RAW"
    printf 'PASS SWCPICH%s ILBM->RAW\n' "$n"

    invoke_converter "$raw_path" "$map_from_raw" "swcpich${n}_raw_to_map" --no-tile-limit
    assert_file_equal "$map_from_raw" "$map_path" "SWCPICH${n} RAW->MAP (--no-tile-limit)"
    printf 'PASS SWCPICH%s RAW->MAP (--no-tile-limit)\n' "$n"

    invoke_converter "$bmp_path" "$map_from_bmp" "swcpich${n}_bmp_to_map" --no-tile-limit
    assert_file_equal "$map_from_bmp" "$map_path" "SWCPICH${n} BMP->MAP (--no-tile-limit)"
    printf 'PASS SWCPICH%s BMP->MAP (--no-tile-limit)\n' "$n"

    invoke_converter "$ilbm_path" "$map_from_ilbm" "swcpich${n}_ilbm_to_map" --no-tile-limit
    assert_file_equal "$map_from_ilbm" "$map_path" "SWCPICH${n} ILBM->MAP (--no-tile-limit)"
    printf 'PASS SWCPICH%s ILBM->MAP (--no-tile-limit)\n' "$n"

    invoke_converter "$map_path" "$raw_from_map" "swcpich${n}_map_to_raw"
    assert_file_equal "$raw_from_map" "$raw_path" "SWCPICH${n} MAP->RAW"
    printf 'PASS SWCPICH%s MAP->RAW\n' "$n"

    invoke_converter "$map_path" "$bmp_from_map" "swcpich${n}_map_to_bmp"
    assert_file_equal "$bmp_from_map" "$bmp_path" "SWCPICH${n} MAP->BMP"
    printf 'PASS SWCPICH%s MAP->BMP\n' "$n"

    invoke_converter "$raw_path" "$ilbm_from_raw" "swcpich${n}_raw_to_ilbm"
    invoke_converter "$ilbm_from_raw" "$raw_roundtrip" "swcpich${n}_raw_ilbm_raw"
    assert_file_equal "$raw_roundtrip" "$raw_path" "SWCPICH${n} RAW->ILBM->RAW"
    printf 'PASS SWCPICH%s RAW->ILBM->RAW\n' "$n"

    assert_rejected_for_tile_limit "$raw_path" "$TEMP_ROOT/SWCPICH${n}.raw.map" "swcpich${n}_raw_to_map_reject"
    printf 'PASS SWCPICH%s RAW->MAP rejected\n' "$n"

    assert_rejected_for_tile_limit "$bmp_path" "$TEMP_ROOT/SWCPICH${n}.bmp.map" "swcpich${n}_bmp_to_map_reject"
    printf 'PASS SWCPICH%s BMP->MAP rejected\n' "$n"

    assert_rejected_for_tile_limit "$ilbm_path" "$TEMP_ROOT/SWCPICH${n}.ilbm.map" "swcpich${n}_ilbm_to_map_reject"
    printf 'PASS SWCPICH%s ILBM->MAP rejected\n' "$n"
done

for name in CJCGRAFS MENUS2 LOADER1 MENUBG; do
    iff_path="test/ilbm/${name}.IFF"
    raw_path="test/raw/${name}.RAW"
    bmp_path="test/bmp/${name}.BMP"

    if [ ! -f "$iff_path" ] || [ ! -f "$raw_path" ] || [ ! -f "$bmp_path" ]; then
        continue
    fi

    raw_from_ilbm="$TEMP_ROOT/${name}.from_ilbm.raw"
    ilbm_from_raw="$TEMP_ROOT/${name}.from_raw.iff"
    bmp_from_raw="$TEMP_ROOT/${name}.from_raw.bmp"
    raw_from_bmp="$TEMP_ROOT/${name}.from_bmp.raw"

    invoke_converter "$iff_path" "$raw_from_ilbm" "${name}_ilbm_to_raw"
    assert_file_equal "$raw_from_ilbm" "$raw_path" "$name ILBM->RAW"
    printf 'PASS %s ILBM->RAW\n' "$name"

    invoke_converter "$raw_path" "$bmp_from_raw" "${name}_raw_to_bmp"
    assert_file_equal "$bmp_from_raw" "$bmp_path" "$name RAW->BMP"
    printf 'PASS %s RAW->BMP\n' "$name"

    invoke_converter "$bmp_path" "$raw_from_bmp" "${name}_bmp_to_raw"
    assert_file_equal "$raw_from_bmp" "$raw_path" "$name BMP->RAW"
    printf 'PASS %s BMP->RAW\n' "$name"

    invoke_converter "$raw_path" "$ilbm_from_raw" "${name}_raw_to_ilbm"
    assert_file_equal "$ilbm_from_raw" "$iff_path" "$name RAW->ILBM"
    printf 'PASS %s RAW->ILBM\n' "$name"
done

printf 'All conversion tests passed.\n'
