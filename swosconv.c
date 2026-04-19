/* SPDX-License-Identifier: 0BSD */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t byte;
typedef uint16_t tile_index_t;
typedef uint32_t dword_t;

#define TILES_MAP_ROWS 55
#define TILES_MAP_COLS 42
#define TILE_WIDTH 16
#define TILE_HEIGHT 16
#define TILE_DEPTH 4
#define BYTES_PER_BITPLANE_PX_ROW (TILE_WIDTH / 8)
#define TILE_BYTES_FOR_PX_ROW (BYTES_PER_BITPLANE_PX_ROW * TILE_DEPTH)
#define TILE_BYTES_NUM (TILE_BYTES_FOR_PX_ROW * TILE_HEIGHT)
#define RAW_ROW_BYTES (TILE_BYTES_FOR_PX_ROW * TILES_MAP_COLS)
#define TILES_MAP_CELLS (TILES_MAP_ROWS * TILES_MAP_COLS)
#define MAX_DISTINCT_TILES 512

#define PITCH_WIDTH (TILES_MAP_COLS * TILE_WIDTH)
#define PITCH_HEIGHT (TILES_MAP_ROWS * TILE_HEIGHT)
#define BMP_PALETTE_ENTRIES 16
#define BMP_FILE_HEADER_SIZE 14
#define BMP_INFO_HEADER_SIZE 40
#define BMP_PALETTE_SIZE (BMP_PALETTE_ENTRIES * 4)
#define ILBM_ROW_BYTES (((PITCH_WIDTH + 15) / 16) * 2)
#define ILBM_BODY_ROW_BYTES (ILBM_ROW_BYTES * TILE_DEPTH)
#define ILBM_CMAP_SIZE (BMP_PALETTE_ENTRIES * 3)
#define SWOSCONV_VERSION "1.0.0"

/* clang-format off */
static const uint8_t swos_bmp_palette[BMP_PALETTE_ENTRIES][4] = {
    { 0x00, 0x90, 0x70, 0x00 },
    { 0x90, 0x90, 0x90, 0x00 },
    { 0xF0, 0xF0, 0xF0, 0x00 },
    { 0x00, 0x00, 0x00, 0x00 },
    { 0x10, 0x20, 0x70, 0x00 },
    { 0x00, 0x40, 0xA0, 0x00 },
    { 0x10, 0x70, 0xF0, 0x00 },
    { 0x00, 0x90, 0x60, 0x00 },
    { 0x00, 0x30, 0x00, 0x00 },
    { 0x00, 0x90, 0x90, 0x00 },
    { 0x00, 0x00, 0xF0, 0x00 },
    { 0xF0, 0x00, 0x00, 0x00 },
    { 0x20, 0x00, 0x70, 0x00 },
    { 0xF0, 0x80, 0x80, 0x00 },
    { 0x00, 0x80, 0x30, 0x00 },
    { 0x00, 0xF0, 0xF0, 0x00 }
};

static const uint8_t swos_ilbm_cmap[BMP_PALETTE_ENTRIES][3] = {
    { 112, 144, 0 },
    { 144, 144, 144 },
    { 240, 240, 240 },
    { 0, 0, 0 },
    { 112, 32, 16 },
    { 160, 64, 0 },
    { 240, 112, 16 },
    { 96, 144, 0 },
    { 0, 48, 0 },
    { 144, 144, 0 },
    { 240, 0, 0 },
    { 0, 0, 240 },
    { 112, 0, 32 },
    { 128, 128, 240 },
    { 48, 128, 0 },
    { 240, 240, 0 }
};
/* clang-format on */

static int load_raw_pixels(const char *input_path, byte **pixels_out);

static unsigned int read_u16_le(const byte *data) {
    return (unsigned int)data[0] | ((unsigned int)data[1] << 8);
}

static unsigned int read_u16_be(const byte *data) {
    return ((unsigned int)data[0] << 8) | (unsigned int)data[1];
}

/* clang-format off */
static dword_t read_u32_le(const byte *data) {
    return (dword_t)data[0]         |
           ((dword_t)data[1] << 8)  |
           ((dword_t)data[2] << 16) |
           ((dword_t)data[3] << 24);
}
/* clang-format on */

/* clang-format off */
static dword_t swap_u32(dword_t value) {
    return ((value & 0x000000FFUL) << 24) |
           ((value & 0x0000FF00UL) << 8)  |
           ((value & 0x00FF0000UL) >> 8)  |
           ((value & 0xFF000000UL) >> 24);
}
/* clang-format on */

static long read_s32_le(const byte *data) {
    dword_t value;

    value = read_u32_le(data);
    if (value & 0x80000000UL) {
        value = ((~value) + 1UL) & 0xFFFFFFFFUL;
        return -(long)value;
    }

    return (long)value;
}

static void write_u16_le(byte *data, unsigned int value) {
    data[0] = (byte)(value & 0xFF);
    data[1] = (byte)((value >> 8) & 0xFF);
}

static void write_u32_le(byte *data, dword_t value) {
    data[0] = (byte)(value & 0xFF);
    data[1] = (byte)((value >> 8) & 0xFF);
    data[2] = (byte)((value >> 16) & 0xFF);
    data[3] = (byte)((value >> 24) & 0xFF);
}

static void write_tiles_row(FILE *out_file,
                            byte **tiles,
                            int col_num,
                            int bitplanes_num,
                            int tile_height,
                            int bytes_for_bitplane_px_row) {
    int tile_bytes_for_px_row;
    int px_row;
    byte pixel_row[RAW_ROW_BYTES];

    tile_bytes_for_px_row = bytes_for_bitplane_px_row * bitplanes_num;
    for (px_row = 0; px_row < tile_height; ++px_row) {
        int bp;

        for (bp = 0; bp < bitplanes_num; ++bp) {
            int bitplane_tile_idx;
            int bitplane_row_idx;
            int px_col;

            bitplane_tile_idx = bytes_for_bitplane_px_row * bp;
            bitplane_row_idx = bitplane_tile_idx * col_num;
            for (px_col = 0; px_col < col_num; ++px_col) {
                int bitplane_col_idx;
                byte *tile_pixel_row;
                int bp_col;

                bitplane_col_idx = bitplane_row_idx + px_col * bytes_for_bitplane_px_row;
                tile_pixel_row = tiles[px_col] + px_row * tile_bytes_for_px_row;
                for (bp_col = 0; bp_col < bytes_for_bitplane_px_row; ++bp_col) {
                    pixel_row[bitplane_col_idx + bp_col] = tile_pixel_row[bitplane_tile_idx + bp_col];
                }
            }
        }

        fwrite(pixel_row, 1, (size_t)(tile_bytes_for_px_row * col_num), out_file);
    }
}

static const char *find_extension(const char *path) {
    const char *last_dot;
    const char *last_sep;

    last_dot = (const char *)0;
    last_sep = (const char *)0;
    while (*path != '\0') {
        if (*path == '.') {
            last_dot = path;
        } else if (*path == '/' || *path == '\\') {
            last_sep = path;
            last_dot = (const char *)0;
        }
        ++path;
    }

    if (last_dot == (const char *)0 || last_dot == last_sep) {
        return "";
    }

    return last_dot;
}

static int extension_equals_ignore_case(const char *path, const char *expected) {
    const char *extension;

    extension = find_extension(path);
    while (*extension != '\0' && *expected != '\0') {
        if (tolower((unsigned char)*extension) != tolower((unsigned char)*expected)) {
            return 0;
        }
        ++extension;
        ++expected;
    }

    return *extension == '\0' && *expected == '\0';
}

static int read_file_size(FILE *file, long *size) {
    if (fseek(file, 0L, SEEK_END) != 0) {
        return 0;
    }

    *size = ftell(file);
    if (*size < 0) {
        return 0;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        return 0;
    }

    return 1;
}

static int decode_byterun1_row(FILE *in_file, byte *out_row, int row_size) {
    int written;

    written = 0;
    while (written < row_size) {
        int control;

        control = fgetc(in_file);
        if (control == EOF) {
            return 0;
        }

        if (control <= 127) {
            int count;
            int i;

            count = control + 1;
            if (written + count > row_size) {
                return 0;
            }
            for (i = 0; i < count; ++i) {
                int value;

                value = fgetc(in_file);
                if (value == EOF) {
                    return 0;
                }
                out_row[written++] = (byte)value;
            }
        } else if (control != 128) {
            int count;
            int value;
            int i;

            count = 257 - control;
            if (written + count > row_size) {
                return 0;
            }
            value = fgetc(in_file);
            if (value == EOF) {
                return 0;
            }
            for (i = 0; i < count; ++i) {
                out_row[written++] = (byte)value;
            }
        }
    }

    return 1;
}

static int encode_byterun1_row(const byte *row, int row_size, byte *out_row) {
    int in_pos;
    int out_pos;

    in_pos = 0;
    out_pos = 0;
    while (in_pos < row_size) {
        int run_length;
        int i;

        run_length = 1;
        while (in_pos + run_length < row_size &&
               run_length < 128 &&
               row[in_pos + run_length] == row[in_pos]) {
            ++run_length;
        }

        if (run_length >= 2) {
            out_row[out_pos++] = (byte)(257 - run_length);
            out_row[out_pos++] = row[in_pos];
            in_pos += run_length;
        } else {
            int literal_start;
            int literal_length;

            literal_start = in_pos;
            literal_length = 1;
            ++in_pos;

            while (in_pos < row_size && literal_length < 128) {
                run_length = 1;
                while (in_pos + run_length < row_size &&
                       run_length < 128 &&
                       row[in_pos + run_length] == row[in_pos]) {
                    ++run_length;
                }
                if (run_length >= 2) {
                    break;
                }
                ++in_pos;
                ++literal_length;
            }

            out_row[out_pos++] = (byte)(literal_length - 1);
            for (i = 0; i < literal_length; ++i) {
                out_row[out_pos++] = row[literal_start + i];
            }
        }
    }

    return out_pos;
}

static int locate_ilbm_body(FILE *in_file, long *body_offset, dword_t *body_size, byte *compression_out) {
    byte form_header[12];

    if (fread(form_header, 1, 12, in_file) != 12) {
        fprintf(stderr, "Unexpected end of ILBM header.\n");
        return 0;
    }

    if (memcmp(form_header, "FORM", 4) != 0 || memcmp(form_header + 8, "ILBM", 4) != 0) {
        fprintf(stderr, "Unsupported ILBM file type.\n");
        return 0;
    }

    *compression_out = 255;
    for (;;) {
        byte chunk_header[8];
        dword_t chunk_size;
        long next_pos;

        if (fread(chunk_header, 1, 8, in_file) != 8) {
            break;
        }

        chunk_size = swap_u32(read_u32_le(chunk_header + 4));
        next_pos = ftell(in_file) + (long)chunk_size + (long)(chunk_size & 1UL);

        if (memcmp(chunk_header, "BMHD", 4) == 0) {
            byte bmhd[20];
            if (chunk_size < 20 || fread(bmhd, 1, 20, in_file) != 20) {
                fprintf(stderr, "Invalid BMHD chunk.\n");
                return 0;
            }
            if (read_u16_be(bmhd + 0) != PITCH_WIDTH ||
                read_u16_be(bmhd + 2) != PITCH_HEIGHT ||
                bmhd[8] != TILE_DEPTH ||
                bmhd[9] != 0) {
                fprintf(stderr, "Unsupported ILBM bitmap format.\n");
                return 0;
            }
            *compression_out = bmhd[10];
        } else if (memcmp(chunk_header, "BODY", 4) == 0) {
            *body_offset = ftell(in_file);
            *body_size = chunk_size;
            if (*compression_out == 255) {
                fprintf(stderr, "BMHD chunk missing before BODY.\n");
                return 0;
            }
            return 1;
        }

        if (fseek(in_file, next_pos, SEEK_SET) != 0) {
            fprintf(stderr, "Invalid ILBM chunk layout.\n");
            return 0;
        }
    }

    fprintf(stderr, "BODY chunk not found in ILBM file.\n");
    return 0;
}

static int convert_ilbm_to_raw(const char *input_path, const char *output_path) {
    FILE *in_file;
    FILE *out_file;
    long body_offset;
    dword_t body_size;
    byte compression;
    int y;

    body_size = 0;
    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    if (!locate_ilbm_body(in_file, &body_offset, &body_size, &compression)) {
        fclose(in_file);
        return 1;
    }

    if (compression != 0 && compression != 1) {
        fclose(in_file);
        fprintf(stderr, "Unsupported ILBM compression: %u.\n", compression);
        return 1;
    }

    if (fseek(in_file, body_offset, SEEK_SET) != 0) {
        fclose(in_file);
        fprintf(stderr, "Unable to seek to ILBM BODY.\n");
        return 1;
    }

    out_file = fopen(output_path, "wb");
    if (out_file == (FILE *)0) {
        fclose(in_file);
        fprintf(stderr, "Unable to open output file.\n");
        return 1;
    }

    for (y = 0; y < PITCH_HEIGHT; ++y) {
        byte row[ILBM_BODY_ROW_BYTES];

        if (compression == 0) {
            if (fread(row, 1, ILBM_BODY_ROW_BYTES, in_file) != ILBM_BODY_ROW_BYTES) {
                fclose(out_file);
                fclose(in_file);
                fprintf(stderr, "Unexpected end of ILBM BODY.\n");
                return 1;
            }
        } else {
            if (!decode_byterun1_row(in_file, row, ILBM_BODY_ROW_BYTES)) {
                fclose(out_file);
                fclose(in_file);
                fprintf(stderr, "Failed to decode ILBM BODY.\n");
                return 1;
            }
        }
        fwrite(row, 1, ILBM_BODY_ROW_BYTES, out_file);
    }

    fclose(out_file);
    fclose(in_file);
    return 0;
}

static int write_ilbm_from_raw(const char *input_path, const char *output_path) {
    FILE *in_file;
    FILE *out_file;
    long input_size;
    long expected_raw_size;
    byte *body_data;
    dword_t body_size;
    dword_t form_size;
    int y;
    int body_pos;

    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    if (!read_file_size(in_file, &input_size)) {
        fclose(in_file);
        fprintf(stderr, "Unable to determine RAW file size.\n");
        return 1;
    }

    expected_raw_size = (long)PITCH_HEIGHT * ILBM_BODY_ROW_BYTES;
    if (input_size != expected_raw_size) {
        fclose(in_file);
        fprintf(stderr, "Unexpected RAW size: %ld bytes, expected %ld.\n", input_size, expected_raw_size);
        return 1;
    }

    body_data = (byte *)malloc((size_t)(expected_raw_size * 2));
    if (body_data == (byte *)0) {
        fclose(in_file);
        fprintf(stderr, "Out of memory while encoding ILBM BODY.\n");
        return 1;
    }

    body_pos = 0;
    for (y = 0; y < PITCH_HEIGHT; ++y) {
        byte row[ILBM_BODY_ROW_BYTES];
        byte encoded[(ILBM_BODY_ROW_BYTES * 2) + 2];
        int encoded_size;

        if (fread(row, 1, ILBM_BODY_ROW_BYTES, in_file) != ILBM_BODY_ROW_BYTES) {
            free(body_data);
            fclose(in_file);
            fprintf(stderr, "Unexpected end of RAW data.\n");
            return 1;
        }

        encoded_size = encode_byterun1_row(row, ILBM_BODY_ROW_BYTES, encoded);
        memcpy(body_data + body_pos, encoded, (size_t)encoded_size);
        body_pos += encoded_size;
    }

    fclose(in_file);
    body_size = (dword_t)body_pos;
    form_size = 4 + (8 + 20) + (8 + ILBM_CMAP_SIZE) + (8 + 4) + (8 + 4) + (8 + body_size + (body_size & 1UL));

    out_file = fopen(output_path, "wb");
    if (out_file == (FILE *)0) {
        free(body_data);
        fprintf(stderr, "Unable to open output file.\n");
        return 1;
    }

    {
        byte header[12];
        byte bmhd[20];
        byte chunk_header[8];

        memcpy(header, "FORM", 4);
        write_u32_le(header + 4, swap_u32(form_size));
        memcpy(header + 8, "ILBM", 4);
        fwrite(header, 1, 12, out_file);

        memcpy(chunk_header, "BMHD", 4);
        chunk_header[4] = 0;
        chunk_header[5] = 0;
        chunk_header[6] = 0;
        chunk_header[7] = 20;
        fwrite(chunk_header, 1, 8, out_file);
        memset(bmhd, 0, sizeof(bmhd));
        bmhd[0] = (byte)((PITCH_WIDTH >> 8) & 0xFF);
        bmhd[1] = (byte)(PITCH_WIDTH & 0xFF);
        bmhd[2] = (byte)((PITCH_HEIGHT >> 8) & 0xFF);
        bmhd[3] = (byte)(PITCH_HEIGHT & 0xFF);
        bmhd[8] = TILE_DEPTH;
        bmhd[10] = 1;
        bmhd[11] = 128;
        bmhd[14] = 44;
        bmhd[15] = 44;
        bmhd[16] = 1;
        bmhd[17] = 64;
        bmhd[18] = 1;
        bmhd[19] = 0;
        fwrite(bmhd, 1, 20, out_file);

        memcpy(chunk_header, "CMAP", 4);
        chunk_header[4] = 0;
        chunk_header[5] = 0;
        chunk_header[6] = 0;
        chunk_header[7] = ILBM_CMAP_SIZE;
        fwrite(chunk_header, 1, 8, out_file);
        fwrite(swos_ilbm_cmap, 1, ILBM_CMAP_SIZE, out_file);

        memcpy(chunk_header, "CAMG", 4);
        chunk_header[4] = 0;
        chunk_header[5] = 0;
        chunk_header[6] = 0;
        chunk_header[7] = 4;
        fwrite(chunk_header, 1, 8, out_file);
        fputc(0x00, out_file);
        fputc(0x02, out_file);
        fputc(0x10, out_file);
        fputc(0x00, out_file);

        memcpy(chunk_header, "DPI ", 4);
        chunk_header[4] = 0;
        chunk_header[5] = 0;
        chunk_header[6] = 0;
        chunk_header[7] = 4;
        fwrite(chunk_header, 1, 8, out_file);
        fputc(0x00, out_file);
        fputc(0x2C, out_file);
        fputc(0x00, out_file);
        fputc(0x2C, out_file);

        memcpy(chunk_header, "BODY", 4);
        write_u32_le(chunk_header + 4, swap_u32(body_size));
        fwrite(chunk_header, 1, 8, out_file);
        fwrite(body_data, 1, body_size, out_file);
        if (body_size & 1UL) {
            fputc(0, out_file);
        }
    }

    fclose(out_file);
    free(body_data);
    return 0;
}

static int load_ilbm_pixels(const char *input_path, byte **pixels_out) {
    char tmp_raw_path[L_tmpnam + 4];
    int result;

    *pixels_out = (byte *)0;
    if (tmpnam(tmp_raw_path) == (char *)0) {
        fprintf(stderr, "Unable to create temporary filename.\n");
        return 1;
    }

    result = convert_ilbm_to_raw(input_path, tmp_raw_path);
    if (result != 0) {
        remove(tmp_raw_path);
        return result;
    }

    result = load_raw_pixels(tmp_raw_path, pixels_out);
    remove(tmp_raw_path);
    return result;
}

static int convert_map_to_raw(const char *input_path, const char *output_path) {
    FILE *in_file;
    FILE *out_file;
    tile_index_t tiles_map[TILES_MAP_ROWS][TILES_MAP_COLS];
    tile_index_t max_tile_idx;
    byte *tiles_data;
    byte *tiles_row[TILES_MAP_COLS];
    int distinct_tiles;
    int row;
    int col;

    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    max_tile_idx = 0;
    for (row = 0; row < TILES_MAP_ROWS; ++row) {
        for (col = 0; col < TILES_MAP_COLS; ++col) {
            byte header_bytes[4];
            tile_index_t tile_idx;

            if (fread(header_bytes, 1, 4, in_file) != 4) {
                fclose(in_file);
                fprintf(stderr, "Unexpected end of MAP header.\n");
                return 1;
            }

            tile_idx = (tile_index_t)(header_bytes[2] * 2 + (header_bytes[3] == 0 ? 0 : 1));
            tiles_map[row][col] = tile_idx;
            if (tile_idx > max_tile_idx) {
                max_tile_idx = tile_idx;
            }
        }
    }

    distinct_tiles = (int)max_tile_idx + 1;
    tiles_data = (byte *)malloc((size_t)(distinct_tiles * TILE_BYTES_NUM));
    if (tiles_data == (byte *)0) {
        fclose(in_file);
        fprintf(stderr, "Out of memory while loading MAP tiles.\n");
        return 1;
    }

    if (fread(tiles_data, TILE_BYTES_NUM, (size_t)distinct_tiles, in_file) != (size_t)distinct_tiles) {
        free(tiles_data);
        fclose(in_file);
        fprintf(stderr, "Unexpected end of MAP tile data.\n");
        return 1;
    }

    fclose(in_file);

    out_file = fopen(output_path, "wb");
    if (out_file == (FILE *)0) {
        free(tiles_data);
        fprintf(stderr, "Unable to open output file.\n");
        return 1;
    }

    for (row = 0; row < TILES_MAP_ROWS; ++row) {
        for (col = 0; col < TILES_MAP_COLS; ++col) {
            tiles_row[col] = tiles_data + ((size_t)tiles_map[row][col] * TILE_BYTES_NUM);
        }
        write_tiles_row(out_file,
                        tiles_row,
                        TILES_MAP_COLS,
                        TILE_DEPTH,
                        TILE_HEIGHT,
                        BYTES_PER_BITPLANE_PX_ROW);
    }

    fclose(out_file);
    free(tiles_data);

    printf("Distinct tiles found: %d\n", distinct_tiles);
    printf("Tiles successfully read: %d\n", distinct_tiles);
    return 0;
}

static int load_map_pixels(const char *input_path, byte **pixels_out) {
    FILE *in_file;
    tile_index_t tiles_map[TILES_MAP_ROWS][TILES_MAP_COLS];
    tile_index_t max_tile_idx;
    byte *tiles_data;
    byte *pixels;
    int distinct_tiles;
    int row;
    int col;

    *pixels_out = (byte *)0;

    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    max_tile_idx = 0;
    for (row = 0; row < TILES_MAP_ROWS; ++row) {
        for (col = 0; col < TILES_MAP_COLS; ++col) {
            byte header_bytes[4];
            tile_index_t tile_idx;

            if (fread(header_bytes, 1, 4, in_file) != 4) {
                fclose(in_file);
                fprintf(stderr, "Unexpected end of MAP header.\n");
                return 1;
            }

            tile_idx = (tile_index_t)(header_bytes[2] * 2 + (header_bytes[3] == 0 ? 0 : 1));
            tiles_map[row][col] = tile_idx;
            if (tile_idx > max_tile_idx) {
                max_tile_idx = tile_idx;
            }
        }
    }

    distinct_tiles = (int)max_tile_idx + 1;
    tiles_data = (byte *)malloc((size_t)(distinct_tiles * TILE_BYTES_NUM));
    if (tiles_data == (byte *)0) {
        fclose(in_file);
        fprintf(stderr, "Out of memory while loading MAP tiles.\n");
        return 1;
    }

    if (fread(tiles_data, TILE_BYTES_NUM, (size_t)distinct_tiles, in_file) != (size_t)distinct_tiles) {
        free(tiles_data);
        fclose(in_file);
        fprintf(stderr, "Unexpected end of MAP tile data.\n");
        return 1;
    }

    fclose(in_file);

    pixels = (byte *)malloc((size_t)(PITCH_WIDTH * PITCH_HEIGHT));
    if (pixels == (byte *)0) {
        free(tiles_data);
        fprintf(stderr, "Out of memory while expanding MAP pixels.\n");
        return 1;
    }

    memset(pixels, 0, (size_t)(PITCH_WIDTH * PITCH_HEIGHT));
    for (row = 0; row < TILES_MAP_ROWS; ++row) {
        for (col = 0; col < TILES_MAP_COLS; ++col) {
            const byte *tile_data;
            int px_row;

            tile_data = tiles_data + ((size_t)tiles_map[row][col] * TILE_BYTES_NUM);
            for (px_row = 0; px_row < TILE_HEIGHT; ++px_row) {
                int bp;

                for (bp = 0; bp < TILE_DEPTH; ++bp) {
                    int bitplane_tile_idx;
                    int bp_col;

                    bitplane_tile_idx = px_row * TILE_BYTES_FOR_PX_ROW + BYTES_PER_BITPLANE_PX_ROW * bp;
                    for (bp_col = 0; bp_col < BYTES_PER_BITPLANE_PX_ROW; ++bp_col) {
                        byte packed_bits;
                        int bit;

                        packed_bits = tile_data[bitplane_tile_idx + bp_col];
                        for (bit = 0; bit < 8; ++bit) {
                            long x;
                            long y;
                            byte plane_bit;

                            x = (long)(col * TILE_WIDTH + bp_col * 8 + bit);
                            y = (long)(row * TILE_HEIGHT + px_row);
                            plane_bit = (byte)((packed_bits >> (7 - bit)) & 1);
                            pixels[y * PITCH_WIDTH + x] =
                                (byte)(pixels[y * PITCH_WIDTH + x] | (plane_bit << bp));
                        }
                    }
                }
            }
        }
    }

    free(tiles_data);
    *pixels_out = pixels;
    return 0;
}

static int load_bmp_pixels(const char *input_path, byte **pixels_out) {
    FILE *in_file;
    long input_size;
    byte *bmp_data;
    byte *pixels;
    dword_t pixel_data_offset;
    dword_t dib_header_size;
    long width;
    long signed_height;
    long height;
    unsigned int planes;
    unsigned int bits_per_pixel;
    dword_t compression;
    dword_t colors_used;
    long row_size;
    bool bottom_up;
    int y;

    *pixels_out = (byte *)0;

    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    if (!read_file_size(in_file, &input_size)) {
        fclose(in_file);
        fprintf(stderr, "Unable to determine BMP file size.\n");
        return 1;
    }

    bmp_data = (byte *)malloc((size_t)input_size);
    if (bmp_data == (byte *)0) {
        fclose(in_file);
        fprintf(stderr, "Out of memory while loading BMP file.\n");
        return 1;
    }

    if (fread(bmp_data, 1, (size_t)input_size, in_file) != (size_t)input_size) {
        free(bmp_data);
        fclose(in_file);
        fprintf(stderr, "Unexpected end of BMP file.\n");
        return 1;
    }

    fclose(in_file);

    if (input_size < 54 || bmp_data[0] != 'B' || bmp_data[1] != 'M') {
        free(bmp_data);
        fprintf(stderr, "Unsupported BMP file header.\n");
        return 1;
    }

    pixel_data_offset = read_u32_le(bmp_data + 10);
    dib_header_size = read_u32_le(bmp_data + 14);
    width = (long)read_u32_le(bmp_data + 18);
    signed_height = read_s32_le(bmp_data + 22);
    planes = read_u16_le(bmp_data + 26);
    bits_per_pixel = read_u16_le(bmp_data + 28);
    compression = read_u32_le(bmp_data + 30);
    colors_used = read_u32_le(bmp_data + 46);

    if (dib_header_size < 40) {
        free(bmp_data);
        fprintf(stderr, "Unsupported BMP DIB header size.\n");
        return 1;
    }

    if (signed_height < 0) {
        bottom_up = false;
        height = -signed_height;
    } else {
        bottom_up = true;
        height = signed_height;
    }

    if (width != PITCH_WIDTH || height != PITCH_HEIGHT) {
        free(bmp_data);
        fprintf(stderr, "Unsupported BMP size: %ldx%ld, expected %dx%d.\n",
                width,
                height,
                PITCH_WIDTH,
                PITCH_HEIGHT);
        return 1;
    }

    if (planes != 1 || bits_per_pixel != 4 || compression != 0) {
        free(bmp_data);
        fprintf(stderr, "Unsupported BMP format: planes=%u bpp=%u compression=%lu.\n",
                planes,
                bits_per_pixel,
                (unsigned long)compression);
        return 1;
    }

    if (colors_used != 0 && colors_used != BMP_PALETTE_ENTRIES) {
        free(bmp_data);
        fprintf(stderr, "Unsupported BMP palette size: %lu.\n", (unsigned long)colors_used);
        return 1;
    }

    row_size = (((width * bits_per_pixel) + 31L) / 32L) * 4L;
    if (pixel_data_offset >= (dword_t)input_size ||
        pixel_data_offset + (dword_t)(row_size * height) > (dword_t)input_size) {
        free(bmp_data);
        fprintf(stderr, "BMP pixel data is truncated.\n");
        return 1;
    }

    pixels = (byte *)malloc((size_t)(width * height));
    if (pixels == (byte *)0) {
        free(bmp_data);
        fprintf(stderr, "Out of memory while expanding BMP pixels.\n");
        return 1;
    }

    for (y = 0; y < height; ++y) {
        const byte *src_row;
        byte *dst_row;
        long src_y;
        long x;

        src_y = bottom_up ? (height - 1 - y) : y;
        src_row = bmp_data + pixel_data_offset + src_y * row_size;
        dst_row = pixels + y * width;

        for (x = 0; x < width / 2; ++x) {
            byte packed_pixels;

            packed_pixels = src_row[x];
            dst_row[x * 2] = (byte)(packed_pixels >> 4);
            dst_row[x * 2 + 1] = (byte)(packed_pixels & 0x0F);
        }
    }

    free(bmp_data);
    *pixels_out = pixels;
    return 0;
}

static int write_raw_from_pixels(const byte *pixels, const char *output_path) {
    FILE *out_file;
    int y;

    out_file = fopen(output_path, "wb");
    if (out_file == (FILE *)0) {
        fprintf(stderr, "Unable to open output file.\n");
        return 1;
    }

    for (y = 0; y < PITCH_HEIGHT; ++y) {
        int tile_col;
        int bp;

        for (bp = 0; bp < TILE_DEPTH; ++bp) {
            for (tile_col = 0; tile_col < TILES_MAP_COLS; ++tile_col) {
                int byte_index;

                for (byte_index = 0; byte_index < BYTES_PER_BITPLANE_PX_ROW; ++byte_index) {
                    byte out_byte;
                    int bit;

                    out_byte = 0;
                    for (bit = 0; bit < 8; ++bit) {
                        long x;
                        byte color_index;

                        x = (long)(tile_col * TILE_WIDTH + byte_index * 8 + bit);
                        color_index = pixels[y * PITCH_WIDTH + x];
                        out_byte = (byte)((out_byte << 1) | ((color_index >> bp) & 1));
                    }
                    fwrite(&out_byte, 1, 1, out_file);
                }
            }
        }
    }

    fclose(out_file);
    return 0;
}

static int load_raw_pixels(const char *input_path, byte **pixels_out) {
    FILE *in_file;
    long input_size;
    long expected_raw_size;
    byte *pixels;
    int tile_row;

    *pixels_out = (byte *)0;

    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    if (!read_file_size(in_file, &input_size)) {
        fclose(in_file);
        fprintf(stderr, "Unable to determine RAW file size.\n");
        return 1;
    }

    expected_raw_size = (long)TILES_MAP_ROWS * TILE_HEIGHT * RAW_ROW_BYTES;
    if (input_size != expected_raw_size) {
        fclose(in_file);
        fprintf(stderr, "Unexpected RAW size: %ld bytes, expected %ld.\n", input_size, expected_raw_size);
        return 1;
    }

    pixels = (byte *)malloc((size_t)(PITCH_WIDTH * PITCH_HEIGHT));
    if (pixels == (byte *)0) {
        fclose(in_file);
        fprintf(stderr, "Out of memory while expanding RAW pixels.\n");
        return 1;
    }
    memset(pixels, 0, (size_t)(PITCH_WIDTH * PITCH_HEIGHT));

    for (tile_row = 0; tile_row < TILES_MAP_ROWS; ++tile_row) {
        int px_row;

        for (px_row = 0; px_row < TILE_HEIGHT; ++px_row) {
            byte pixel_row[RAW_ROW_BYTES];
            int bp;
            int y;

            if (fread(pixel_row, 1, RAW_ROW_BYTES, in_file) != RAW_ROW_BYTES) {
                free(pixels);
                fclose(in_file);
                fprintf(stderr, "Unexpected end of RAW data.\n");
                return 1;
            }

            y = tile_row * TILE_HEIGHT + px_row;
            for (bp = 0; bp < TILE_DEPTH; ++bp) {
                int bitplane_tile_idx;
                int bitplane_row_idx;
                int tile_col;

                bitplane_tile_idx = BYTES_PER_BITPLANE_PX_ROW * bp;
                bitplane_row_idx = bitplane_tile_idx * TILES_MAP_COLS;
                for (tile_col = 0; tile_col < TILES_MAP_COLS; ++tile_col) {
                    int bitplane_col_idx;
                    int byte_index;

                    bitplane_col_idx = bitplane_row_idx + tile_col * BYTES_PER_BITPLANE_PX_ROW;
                    for (byte_index = 0; byte_index < BYTES_PER_BITPLANE_PX_ROW; ++byte_index) {
                        byte packed_bits;
                        int bit;

                        packed_bits = pixel_row[bitplane_col_idx + byte_index];
                        for (bit = 0; bit < 8; ++bit) {
                            long x;
                            byte plane_bit;

                            x = (long)(tile_col * TILE_WIDTH + byte_index * 8 + bit);
                            plane_bit = (byte)((packed_bits >> (7 - bit)) & 1);
                            pixels[y * PITCH_WIDTH + x] =
                                (byte)(pixels[y * PITCH_WIDTH + x] | (plane_bit << bp));
                        }
                    }
                }
            }
        }
    }

    fclose(in_file);
    *pixels_out = pixels;
    return 0;
}

static int write_bmp_from_pixels(const byte *pixels, const char *output_path) {
    FILE *out_file;
    long row_size;
    dword_t pixel_data_offset;
    dword_t file_size;
    byte file_header[BMP_FILE_HEADER_SIZE];
    byte info_header[BMP_INFO_HEADER_SIZE];
    int y;

    row_size = (((PITCH_WIDTH * 4L) + 31L) / 32L) * 4L;
    pixel_data_offset = BMP_FILE_HEADER_SIZE + BMP_INFO_HEADER_SIZE + BMP_PALETTE_SIZE;
    file_size = pixel_data_offset + (dword_t)(row_size * PITCH_HEIGHT);

    out_file = fopen(output_path, "wb");
    if (out_file == (FILE *)0) {
        fprintf(stderr, "Unable to open output file.\n");
        return 1;
    }

    memset(file_header, 0, sizeof(file_header));
    memset(info_header, 0, sizeof(info_header));

    file_header[0] = 'B';
    file_header[1] = 'M';
    write_u32_le(file_header + 2, file_size);
    write_u32_le(file_header + 10, pixel_data_offset);

    write_u32_le(info_header + 0, BMP_INFO_HEADER_SIZE);
    write_u32_le(info_header + 4, (dword_t)PITCH_WIDTH);
    write_u32_le(info_header + 8, (dword_t)PITCH_HEIGHT);
    write_u16_le(info_header + 12, 1);
    write_u16_le(info_header + 14, 4);
    write_u32_le(info_header + 16, 0);
    write_u32_le(info_header + 20, 0);
    write_u32_le(info_header + 24, 44);
    write_u32_le(info_header + 28, 44);
    write_u32_le(info_header + 32, BMP_PALETTE_ENTRIES);
    write_u32_le(info_header + 36, 0);

    fwrite(file_header, 1, sizeof(file_header), out_file);
    fwrite(info_header, 1, sizeof(info_header), out_file);
    fwrite(swos_bmp_palette, 1, sizeof(swos_bmp_palette), out_file);

    for (y = PITCH_HEIGHT - 1; y >= 0; --y) {
        byte row_data[(PITCH_WIDTH / 2) + 4];
        int x;

        memset(row_data, 0, sizeof(row_data));
        for (x = 0; x < PITCH_WIDTH; x += 2) {
            byte left;
            byte right;

            left = pixels[y * PITCH_WIDTH + x];
            right = pixels[y * PITCH_WIDTH + x + 1];
            row_data[x / 2] = (byte)((left << 4) | (right & 0x0F));
        }
        fwrite(row_data, 1, (size_t)row_size, out_file);
    }

    fclose(out_file);
    return 0;
}

static int convert_bmp_to_raw(const char *input_path, const char *output_path) {
    byte *pixels;
    int result;

    result = load_bmp_pixels(input_path, &pixels);
    if (result != 0) {
        return result;
    }

    result = write_raw_from_pixels(pixels, output_path);
    free(pixels);
    return result;
}

static int convert_raw_to_bmp(const char *input_path, const char *output_path) {
    byte *pixels;
    int result;

    result = load_raw_pixels(input_path, &pixels);
    if (result != 0) {
        return result;
    }

    result = write_bmp_from_pixels(pixels, output_path);
    free(pixels);
    return result;
}

static int find_tile_index(const byte *distinct_tiles, int distinct_tile_count, const byte *tile_data) {
    int tile_idx;

    for (tile_idx = 0; tile_idx < distinct_tile_count; ++tile_idx) {
        if (memcmp(distinct_tiles + ((size_t)tile_idx * TILE_BYTES_NUM), tile_data, TILE_BYTES_NUM) == 0) {
            return tile_idx;
        }
    }

    return -1;
}

static int write_map_from_pixels(const byte *pixels, const char *output_path) {
    FILE *out_file;
    tile_index_t tiles_map[TILES_MAP_ROWS][TILES_MAP_COLS];
    byte *distinct_tiles;
    int distinct_tile_count;
    int row;
    int col;

    distinct_tiles = (byte *)malloc((size_t)(MAX_DISTINCT_TILES * TILE_BYTES_NUM));
    if (distinct_tiles == (byte *)0) {
        fprintf(stderr, "Out of memory while building MAP tiles.\n");
        return 1;
    }

    distinct_tile_count = 0;
    for (row = 0; row < TILES_MAP_ROWS; ++row) {
        byte row_tiles[TILES_MAP_COLS * TILE_BYTES_NUM];
        int px_row;

        memset(row_tiles, 0, sizeof(row_tiles));

        for (px_row = 0; px_row < TILE_HEIGHT; ++px_row) {
            int bp;
            int y;

            y = row * TILE_HEIGHT + px_row;
            for (bp = 0; bp < TILE_DEPTH; ++bp) {
                int bitplane_tile_idx;
                int tile_pixel_row_idx;

                bitplane_tile_idx = BYTES_PER_BITPLANE_PX_ROW * bp;
                tile_pixel_row_idx = px_row * TILE_BYTES_FOR_PX_ROW + bitplane_tile_idx;
                for (col = 0; col < TILES_MAP_COLS; ++col) {
                    int bp_col;

                    for (bp_col = 0; bp_col < BYTES_PER_BITPLANE_PX_ROW; ++bp_col) {
                        byte packed_bits;
                        int bit;

                        packed_bits = 0;
                        for (bit = 0; bit < 8; ++bit) {
                            long x;
                            byte color_index;

                            x = (long)(col * TILE_WIDTH + bp_col * 8 + bit);
                            color_index = pixels[y * PITCH_WIDTH + x];
                            packed_bits = (byte)((packed_bits << 1) | ((color_index >> bp) & 1));
                        }
                        row_tiles[col * TILE_BYTES_NUM + tile_pixel_row_idx + bp_col] = packed_bits;
                    }
                }
            }
        }

        for (col = 0; col < TILES_MAP_COLS; ++col) {
            const byte *tile_data;
            int tile_idx;

            tile_data = row_tiles + ((size_t)col * TILE_BYTES_NUM);
            tile_idx = find_tile_index(distinct_tiles, distinct_tile_count, tile_data);
            if (tile_idx < 0) {
                if (distinct_tile_count >= MAX_DISTINCT_TILES) {
                    free(distinct_tiles);
                    fprintf(stderr,
                            "RAW contains more than %d distinct tiles, but MAP format supports at most %d.\n",
                            MAX_DISTINCT_TILES,
                            MAX_DISTINCT_TILES);
                    return 1;
                }

                memcpy(distinct_tiles + ((size_t)distinct_tile_count * TILE_BYTES_NUM), tile_data, TILE_BYTES_NUM);
                tile_idx = distinct_tile_count;
                ++distinct_tile_count;
            }

            tiles_map[row][col] = (tile_index_t)tile_idx;
        }
    }

    out_file = fopen(output_path, "wb");
    if (out_file == (FILE *)0) {
        free(distinct_tiles);
        fprintf(stderr, "Unable to open output file.\n");
        return 1;
    }

    for (row = 0; row < TILES_MAP_ROWS; ++row) {
        for (col = 0; col < TILES_MAP_COLS; ++col) {
            byte header_bytes[4];
            tile_index_t tile_idx;

            tile_idx = tiles_map[row][col];
            header_bytes[0] = 0;
            header_bytes[1] = 0;
            header_bytes[2] = (byte)(tile_idx / 2);
            header_bytes[3] = (byte)((tile_idx % 2) == 0 ? 0 : 128);
            fwrite(header_bytes, 1, 4, out_file);
        }
    }

    fwrite(distinct_tiles, TILE_BYTES_NUM, (size_t)distinct_tile_count, out_file);

    fclose(out_file);
    free(distinct_tiles);

    printf("Distinct tiles found: %d\n", distinct_tile_count);
    return 0;
}

static int convert_raw_to_map(const char *input_path, const char *output_path) {
    byte *pixels;
    int result;

    result = load_raw_pixels(input_path, &pixels);
    if (result != 0) {
        return result;
    }

    result = write_map_from_pixels(pixels, output_path);
    free(pixels);
    return result;
}

static int convert_bmp_to_map(const char *input_path, const char *output_path) {
    byte *pixels;
    int result;

    result = load_bmp_pixels(input_path, &pixels);
    if (result != 0) {
        return result;
    }

    result = write_map_from_pixels(pixels, output_path);
    free(pixels);
    return result;
}

static int convert_map_to_bmp(const char *input_path, const char *output_path) {
    byte *pixels;
    int result;

    result = load_map_pixels(input_path, &pixels);
    if (result != 0) {
        return result;
    }

    result = write_bmp_from_pixels(pixels, output_path);
    free(pixels);
    return result;
}

static int convert_ilbm_to_map(const char *input_path, const char *output_path) {
    byte *pixels;
    int result;

    result = load_ilbm_pixels(input_path, &pixels);
    if (result != 0) {
        return result;
    }

    result = write_map_from_pixels(pixels, output_path);
    free(pixels);
    return result;
}

static int convert_map_to_ilbm(const char *input_path, const char *output_path) {
    char tmp_raw_path[L_tmpnam + 4];
    int result;

    if (tmpnam(tmp_raw_path) == (char *)0) {
        fprintf(stderr, "Unable to create temporary filename.\n");
        return 1;
    }

    result = convert_map_to_raw(input_path, tmp_raw_path);
    if (result != 0) {
        remove(tmp_raw_path);
        return result;
    }

    result = write_ilbm_from_raw(tmp_raw_path, output_path);
    remove(tmp_raw_path);
    return result;
}

static void print_supported_conversions(FILE *stream) {
    fprintf(stream,
            "\n"
            "Supported conversions:\n"
            "  .RAW -> .MAP, .BMP, .IFF (ILBM)\n"
            "  .MAP -> .RAW, .BMP, .IFF (ILBM)\n"
            "  .BMP -> .RAW, .MAP\n"
            "  .IFF (ILBM) -> .RAW, .MAP\n");
}

static void print_usage(FILE *stream) {
    fprintf(stream,
            "swosconv %s\n"
            "\n"
            "Usage: swosconv -i <input> -o <output>\n"
            "       swosconv --help\n"
            "       swosconv --version\n",
            SWOSCONV_VERSION);
    print_supported_conversions(stream);
}

int main(int argc, char *argv[]) {
    const char *input_path;
    const char *output_path;
    int i;

    input_path = (const char *)0;
    output_path = (const char *)0;

    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(stdout);
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf("swosconv %s\n", SWOSCONV_VERSION);
        return 0;
    }

    if (argc < 5) {
        print_usage(stderr);
        return 1;
    }

    i = 1;
    while (i + 1 < argc) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) {
            input_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            output_path = argv[i + 1];
            i += 2;
        } else {
            ++i;
        }
    }

    if (input_path == (const char *)0 || output_path == (const char *)0) {
        fprintf(stderr,
                "Both input and output must be specified.\n"
                "swosconv %s\n"
                "Usage: swosconv -i <input> -o <output>\n",
                SWOSCONV_VERSION);
        return 1;
    }

    if (extension_equals_ignore_case(input_path, ".raw") &&
        extension_equals_ignore_case(output_path, ".map")) {
        return convert_raw_to_map(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".raw") &&
        extension_equals_ignore_case(output_path, ".bmp")) {
        return convert_raw_to_bmp(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".raw") &&
        extension_equals_ignore_case(output_path, ".iff")) {
        return write_ilbm_from_raw(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".map") &&
        extension_equals_ignore_case(output_path, ".raw")) {
        return convert_map_to_raw(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".map") &&
        extension_equals_ignore_case(output_path, ".bmp")) {
        return convert_map_to_bmp(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".map") &&
        extension_equals_ignore_case(output_path, ".iff")) {
        return convert_map_to_ilbm(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".bmp") &&
        extension_equals_ignore_case(output_path, ".raw")) {
        return convert_bmp_to_raw(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".bmp") &&
        extension_equals_ignore_case(output_path, ".map")) {
        return convert_bmp_to_map(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".iff") &&
        extension_equals_ignore_case(output_path, ".raw")) {
        return convert_ilbm_to_raw(input_path, output_path);
    }

    if (extension_equals_ignore_case(input_path, ".iff") &&
        extension_equals_ignore_case(output_path, ".map")) {
        return convert_ilbm_to_map(input_path, output_path);
    }

    fprintf(stderr, "Unsupported conversion.\n");
    print_usage(stderr);
    return 1;
}
