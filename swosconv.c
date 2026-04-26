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
#define LEGACY_MAP_FILE_SIZE_LIMIT 0xB220L
#define MAX_LEGACY_MAP_DISTINCT_TILES ((int)((LEGACY_MAP_FILE_SIZE_LIMIT - (TILES_MAP_CELLS * 4L)) / TILE_BYTES_NUM))

#define PITCH_WIDTH (TILES_MAP_COLS * TILE_WIDTH)
#define PITCH_HEIGHT (TILES_MAP_ROWS * TILE_HEIGHT)
#define BMP_PALETTE_ENTRIES 16
#define BMP_FILE_HEADER_SIZE 14
#define BMP_INFO_HEADER_SIZE 40
#define BMP_PALETTE_SIZE (BMP_PALETTE_ENTRIES * 4)
#define ILBM_ROW_BYTES (((PITCH_WIDTH + 15) / 16) * 2)
#define ILBM_BODY_ROW_BYTES (ILBM_ROW_BYTES * TILE_DEPTH)
#define ILBM_CMAP_SIZE (BMP_PALETTE_ENTRIES * 3)
#define SWOSCONV_VERSION "1.2.1"

typedef struct bitmap_format {
    int width;
    int height;
    int bitplanes;
    int row_bytes;
    int body_row_bytes;
} bitmap_format_t;

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

static const uint8_t simple_bmp_palette[BMP_PALETTE_ENTRIES][4] = {
    { 0x00, 0x00, 0x00, 0x00 },
    { 0x11, 0x11, 0x11, 0x00 },
    { 0x22, 0x22, 0x22, 0x00 },
    { 0x33, 0x33, 0x33, 0x00 },
    { 0x44, 0x44, 0x44, 0x00 },
    { 0x55, 0x55, 0x55, 0x00 },
    { 0x66, 0x66, 0x66, 0x00 },
    { 0x77, 0x77, 0x77, 0x00 },
    { 0x88, 0x88, 0x88, 0x00 },
    { 0x99, 0x99, 0x99, 0x00 },
    { 0xAA, 0xAA, 0xAA, 0x00 },
    { 0xBB, 0xBB, 0xBB, 0x00 },
    { 0xCC, 0xCC, 0xCC, 0x00 },
    { 0xDD, 0xDD, 0xDD, 0x00 },
    { 0xEE, 0xEE, 0xEE, 0x00 },
    { 0xFF, 0xFF, 0xFF, 0x00 }
};

static const uint8_t simple_ilbm_cmap[BMP_PALETTE_ENTRIES][3] = {
    { 0, 0, 0 },
    { 17, 17, 17 },
    { 34, 34, 34 },
    { 51, 51, 51 },
    { 68, 68, 68 },
    { 85, 85, 85 },
    { 102, 102, 102 },
    { 119, 119, 119 },
    { 136, 136, 136 },
    { 153, 153, 153 },
    { 170, 170, 170 },
    { 187, 187, 187 },
    { 204, 204, 204 },
    { 221, 221, 221 },
    { 238, 238, 238 },
    { 255, 255, 255 }
};
/* clang-format on */

static int load_raw_pixels(const char *input_path, byte **pixels_out);
static bool g_no_tile_limit = false;

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
static dword_t read_u32_be(const byte *data) {
    return ((dword_t)data[0] << 24) |
           ((dword_t)data[1] << 16) |
           ((dword_t)data[2] << 8)  |
           (dword_t)data[3];
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

static void write_u32_be(byte *data, dword_t value) {
    data[0] = (byte)((value >> 24) & 0xFF);
    data[1] = (byte)((value >> 16) & 0xFF);
    data[2] = (byte)((value >> 8) & 0xFF);
    data[3] = (byte)(value & 0xFF);
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

static const char *find_filename(const char *path) {
    const char *filename;

    filename = path;
    while (*path != '\0') {
        if (*path == '/' || *path == '\\') {
            filename = path + 1;
        }
        ++path;
    }

    return filename;
}

static int starts_with_ignore_case(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text == '\0' || tolower((unsigned char)*text) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        ++text;
        ++prefix;
    }

    return 1;
}

static int is_swcpich_path(const char *path) {
    return starts_with_ignore_case(find_filename(path), "SWCPICH");
}

static int ilbm_row_bytes_for_width(int width) {
    return ((width + 15) / 16) * 2;
}

static bitmap_format_t make_bitmap_format(int width, int height, int bitplanes) {
    bitmap_format_t format;

    format.width = width;
    format.height = height;
    format.bitplanes = bitplanes;
    format.row_bytes = ilbm_row_bytes_for_width(width);
    format.body_row_bytes = format.row_bytes * bitplanes;
    return format;
}

static int get_simple_raw_format_from_size(long input_size, bitmap_format_t *format) {
    if (input_size == 40960L) {
        *format = make_bitmap_format(320, 256, TILE_DEPTH);
        return 1;
    }

    if (input_size == 47872L) {
        *format = make_bitmap_format(345, 272, TILE_DEPTH);
        return 1;
    }

    return 0;
}

static int get_simple_bitmap_format_from_dimensions(long width, long height, bitmap_format_t *format) {
    if (width == 320L && height == 256L) {
        *format = make_bitmap_format(320, 256, TILE_DEPTH);
        return 1;
    }

    if (width == 345L && height == 272L) {
        *format = make_bitmap_format(345, 272, TILE_DEPTH);
        return 1;
    }

    return 0;
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

        if (run_length >= 3) {
            out_row[out_pos++] = (byte)(257 - run_length);
            out_row[out_pos++] = row[in_pos];
            in_pos += run_length;
        } else {
            int literal_start;
            int literal_length;

            literal_start = in_pos;
            literal_length = 0;
            while (in_pos < row_size && literal_length < 128) {
                run_length = 1;
                while (in_pos + run_length < row_size &&
                       run_length < 128 &&
                       row[in_pos + run_length] == row[in_pos]) {
                    ++run_length;
                }
                if (run_length >= 3) {
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

static int locate_ilbm_body(FILE *in_file,
                            long *body_offset,
                            dword_t *body_size,
                            byte *compression_out,
                            bitmap_format_t *format_out,
                            int pitch_only) {
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
            int width;
            int height;

            width = read_u16_be(bmhd + 0);
            height = read_u16_be(bmhd + 2);
            if (bmhd[8] != TILE_DEPTH || bmhd[9] != 0) {
                fprintf(stderr, "Unsupported ILBM bitmap format.\n");
                return 0;
            }
            if (pitch_only && (width != PITCH_WIDTH || height != PITCH_HEIGHT)) {
                fprintf(stderr, "Unsupported ILBM bitmap format.\n");
                return 0;
            }
            *format_out = make_bitmap_format(width, height, bmhd[8]);
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

static int convert_ilbm_to_raw_with_mode(const char *input_path, const char *output_path, int pitch_only) {
    FILE *in_file;
    FILE *out_file;
    long body_offset;
    dword_t body_size;
    byte compression;
    bitmap_format_t format;
    int y;

    body_size = 0;
    format = make_bitmap_format(0, 0, TILE_DEPTH);
    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    if (!locate_ilbm_body(in_file, &body_offset, &body_size, &compression, &format, pitch_only)) {
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

    for (y = 0; y < format.height; ++y) {
        byte row[ILBM_BODY_ROW_BYTES];

        if (format.body_row_bytes > ILBM_BODY_ROW_BYTES) {
            fclose(out_file);
            fclose(in_file);
            fprintf(stderr, "Unsupported ILBM bitmap width.\n");
            return 1;
        }

        if (compression == 0) {
            if (fread(row, 1, (size_t)format.body_row_bytes, in_file) != (size_t)format.body_row_bytes) {
                fclose(out_file);
                fclose(in_file);
                fprintf(stderr, "Unexpected end of ILBM BODY.\n");
                return 1;
            }
        } else {
            int bp;

            for (bp = 0; bp < format.bitplanes; ++bp) {
                if (!decode_byterun1_row(in_file, row + (bp * format.row_bytes), format.row_bytes)) {
                    fclose(out_file);
                    fclose(in_file);
                    fprintf(stderr, "Failed to decode ILBM BODY.\n");
                    return 1;
                }
            }
        }
        fwrite(row, 1, (size_t)format.body_row_bytes, out_file);
    }

    fclose(out_file);
    fclose(in_file);
    return 0;
}

static int convert_ilbm_to_raw(const char *input_path, const char *output_path) {
    return convert_ilbm_to_raw_with_mode(input_path, output_path, is_swcpich_path(input_path));
}

static int write_ilbm_from_raw_with_mode(const char *input_path, const char *output_path, int pitch_only) {
    FILE *in_file;
    FILE *out_file;
    long input_size;
    long expected_raw_size;
    bitmap_format_t format;
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

    if (pitch_only) {
        format = make_bitmap_format(PITCH_WIDTH, PITCH_HEIGHT, TILE_DEPTH);
    } else if (!get_simple_raw_format_from_size(input_size, &format)) {
        fclose(in_file);
        fprintf(stderr, "Unsupported simple RAW size: %ld bytes.\n", input_size);
        return 1;
    }

    expected_raw_size = (long)format.height * format.body_row_bytes;
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
    for (y = 0; y < format.height; ++y) {
        byte row[ILBM_BODY_ROW_BYTES];
        byte encoded[(ILBM_ROW_BYTES * 2) + 2];
        int bp;

        if (format.body_row_bytes > ILBM_BODY_ROW_BYTES) {
            free(body_data);
            fclose(in_file);
            fprintf(stderr, "Unsupported RAW bitmap width.\n");
            return 1;
        }

        if (fread(row, 1, (size_t)format.body_row_bytes, in_file) != (size_t)format.body_row_bytes) {
            free(body_data);
            fclose(in_file);
            fprintf(stderr, "Unexpected end of RAW data.\n");
            return 1;
        }

        for (bp = 0; bp < format.bitplanes; ++bp) {
            int encoded_size;

            encoded_size = encode_byterun1_row(row + (bp * format.row_bytes), format.row_bytes, encoded);
            memcpy(body_data + body_pos, encoded, (size_t)encoded_size);
            body_pos += encoded_size;
        }
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
        const uint8_t (*ilbm_cmap)[3];

        ilbm_cmap = pitch_only ? swos_ilbm_cmap : simple_ilbm_cmap;

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
        bmhd[0] = (byte)((format.width >> 8) & 0xFF);
        bmhd[1] = (byte)(format.width & 0xFF);
        bmhd[2] = (byte)((format.height >> 8) & 0xFF);
        bmhd[3] = (byte)(format.height & 0xFF);
        bmhd[8] = (byte)format.bitplanes;
        bmhd[10] = 1;
        bmhd[11] = 0;
        bmhd[14] = 44;
        bmhd[15] = 44;
        bmhd[16] = (byte)((format.width >> 8) & 0xFF);
        bmhd[17] = (byte)(format.width & 0xFF);
        bmhd[18] = (byte)((format.height >> 8) & 0xFF);
        bmhd[19] = (byte)(format.height & 0xFF);
        fwrite(bmhd, 1, 20, out_file);

        memcpy(chunk_header, "CMAP", 4);
        chunk_header[4] = 0;
        chunk_header[5] = 0;
        chunk_header[6] = 0;
        chunk_header[7] = ILBM_CMAP_SIZE;
        fwrite(chunk_header, 1, 8, out_file);
        fwrite(ilbm_cmap, 1, ILBM_CMAP_SIZE, out_file);

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

static int write_ilbm_from_raw(const char *input_path, const char *output_path) {
    return write_ilbm_from_raw_with_mode(input_path, output_path, is_swcpich_path(input_path));
}

static int load_ilbm_pixels(const char *input_path, byte **pixels_out) {
    char tmp_raw_path[L_tmpnam + 4];
    int result;

    *pixels_out = (byte *)0;
    if (tmpnam(tmp_raw_path) == (char *)0) {
        fprintf(stderr, "Unable to create temporary filename.\n");
        return 1;
    }

    result = convert_ilbm_to_raw_with_mode(input_path, tmp_raw_path, 1);
    if (result != 0) {
        remove(tmp_raw_path);
        return result;
    }

    result = load_raw_pixels(tmp_raw_path, pixels_out);
    remove(tmp_raw_path);
    return result;
}

static int load_map_tiles(const char *input_path,
                          tile_index_t tiles_map[TILES_MAP_ROWS][TILES_MAP_COLS],
                          byte **tiles_data_out,
                          int *distinct_tiles_out) {
    FILE *in_file;
    long input_size;
    long tile_data_size;
    tile_index_t max_tile_idx;
    int row;
    int col;

    *tiles_data_out = (byte *)0;
    *distinct_tiles_out = 0;

    in_file = fopen(input_path, "rb");
    if (in_file == (FILE *)0) {
        fprintf(stderr, "Unable to open input file.\n");
        return 1;
    }

    if (!read_file_size(in_file, &input_size)) {
        fclose(in_file);
        fprintf(stderr, "Unable to determine MAP file size.\n");
        return 1;
    }

    if (input_size < (long)(TILES_MAP_CELLS * 4)) {
        fclose(in_file);
        fprintf(stderr, "MAP file is too small.\n");
        return 1;
    }

    max_tile_idx = 0;
    for (row = 0; row < TILES_MAP_ROWS; ++row) {
        for (col = 0; col < TILES_MAP_COLS; ++col) {
            byte header_bytes[4];
            dword_t tile_offset;
            tile_index_t tile_idx;

            if (fread(header_bytes, 1, 4, in_file) != 4) {
                fclose(in_file);
                fprintf(stderr, "Unexpected end of MAP header.\n");
                return 1;
            }

            tile_offset = read_u32_be(header_bytes);
            if ((tile_offset % TILE_BYTES_NUM) != 0) {
                fclose(in_file);
                fprintf(stderr, "Invalid MAP tile offset alignment.\n");
                return 1;
            }

            tile_idx = (tile_index_t)(tile_offset / TILE_BYTES_NUM);
            tiles_map[row][col] = tile_idx;
            if (tile_idx > max_tile_idx) {
                max_tile_idx = tile_idx;
            }
        }
    }

    tile_data_size = input_size - (long)(TILES_MAP_CELLS * 4);
    if (((long)max_tile_idx + 1L) * TILE_BYTES_NUM > tile_data_size) {
        fclose(in_file);
        fprintf(stderr, "MAP tile offsets exceed tile data size.\n");
        return 1;
    }

    *distinct_tiles_out = (int)max_tile_idx + 1;
    *tiles_data_out = (byte *)malloc((size_t)(*distinct_tiles_out * TILE_BYTES_NUM));
    if (*tiles_data_out == (byte *)0) {
        fclose(in_file);
        fprintf(stderr, "Out of memory while loading MAP tiles.\n");
        return 1;
    }

    if (fread(*tiles_data_out, TILE_BYTES_NUM, (size_t)(*distinct_tiles_out), in_file) !=
        (size_t)(*distinct_tiles_out)) {
        free(*tiles_data_out);
        *tiles_data_out = (byte *)0;
        fclose(in_file);
        fprintf(stderr, "Unexpected end of MAP tile data.\n");
        return 1;
    }

    fclose(in_file);
    return 0;
}

static int convert_map_to_raw(const char *input_path, const char *output_path) {
    FILE *out_file;
    tile_index_t tiles_map[TILES_MAP_ROWS][TILES_MAP_COLS];
    byte *tiles_data;
    byte *tiles_row[TILES_MAP_COLS];
    int distinct_tiles;
    int row;
    int col;

    if (load_map_tiles(input_path, tiles_map, &tiles_data, &distinct_tiles) != 0) {
        return 1;
    }

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
    tile_index_t tiles_map[TILES_MAP_ROWS][TILES_MAP_COLS];
    byte *tiles_data;
    byte *pixels;
    int distinct_tiles;
    int row;
    int col;

    *pixels_out = (byte *)0;

    if (load_map_tiles(input_path, tiles_map, &tiles_data, &distinct_tiles) != 0) {
        return 1;
    }

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

static int load_bmp_pixels_with_mode(const char *input_path,
                                     byte **pixels_out,
                                     int pitch_only,
                                     bitmap_format_t *format_out) {
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

    if (pitch_only) {
        if (width != PITCH_WIDTH || height != PITCH_HEIGHT) {
            free(bmp_data);
            fprintf(stderr, "Unsupported BMP size: %ldx%ld, expected %dx%d.\n",
                    width,
                    height,
                    PITCH_WIDTH,
                    PITCH_HEIGHT);
            return 1;
        }
        *format_out = make_bitmap_format(PITCH_WIDTH, PITCH_HEIGHT, TILE_DEPTH);
    } else if (!get_simple_bitmap_format_from_dimensions(width, height, format_out)) {
        free(bmp_data);
        fprintf(stderr, "Unsupported simple BMP size: %ldx%ld.\n", width, height);
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

    pixels = (byte *)malloc((size_t)(format_out->width * format_out->height));
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

        for (x = 0; x < (width + 1) / 2; ++x) {
            byte packed_pixels;

            packed_pixels = src_row[x];
            dst_row[x * 2] = (byte)(packed_pixels >> 4);
            if ((x * 2 + 1) < width) {
                dst_row[x * 2 + 1] = (byte)(packed_pixels & 0x0F);
            }
        }
    }

    free(bmp_data);
    *pixels_out = pixels;
    return 0;
}

static int load_bmp_pixels(const char *input_path, byte **pixels_out) {
    bitmap_format_t format;

    return load_bmp_pixels_with_mode(input_path, pixels_out, 1, &format);
}

static int write_raw_from_pixels_with_format(const byte *pixels,
                                             const char *output_path,
                                             const bitmap_format_t *format) {
    FILE *out_file;
    int y;

    out_file = fopen(output_path, "wb");
    if (out_file == (FILE *)0) {
        fprintf(stderr, "Unable to open output file.\n");
        return 1;
    }

    for (y = 0; y < format->height; ++y) {
        int bp;

        if (format->body_row_bytes > RAW_ROW_BYTES) {
            fclose(out_file);
            fprintf(stderr, "Unsupported RAW bitmap width.\n");
            return 1;
        }

        for (bp = 0; bp < TILE_DEPTH; ++bp) {
            int byte_index;

            for (byte_index = 0; byte_index < format->row_bytes; ++byte_index) {
                byte out_byte;
                int bit;

                out_byte = 0;
                for (bit = 0; bit < 8; ++bit) {
                    long x;
                    byte color_index;

                    x = (long)(byte_index * 8 + bit);
                    color_index = 0;
                    if (x < format->width) {
                        color_index = pixels[y * format->width + x];
                    }
                    out_byte = (byte)((out_byte << 1) | ((color_index >> bp) & 1));
                }
                fwrite(&out_byte, 1, 1, out_file);
            }
        }
    }

    fclose(out_file);
    return 0;
}

static int load_raw_pixels_with_mode(const char *input_path,
                                     byte **pixels_out,
                                     int pitch_only,
                                     bitmap_format_t *format_out) {
    FILE *in_file;
    long input_size;
    long expected_raw_size;
    byte *pixels;
    int y;

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

    if (pitch_only) {
        *format_out = make_bitmap_format(PITCH_WIDTH, PITCH_HEIGHT, TILE_DEPTH);
    } else if (!get_simple_raw_format_from_size(input_size, format_out)) {
        fclose(in_file);
        fprintf(stderr, "Unsupported simple RAW size: %ld bytes.\n", input_size);
        return 1;
    }

    expected_raw_size = (long)format_out->height * format_out->body_row_bytes;
    if (input_size != expected_raw_size) {
        fclose(in_file);
        fprintf(stderr, "Unexpected RAW size: %ld bytes, expected %ld.\n", input_size, expected_raw_size);
        return 1;
    }

    pixels = (byte *)malloc((size_t)(format_out->width * format_out->height));
    if (pixels == (byte *)0) {
        fclose(in_file);
        fprintf(stderr, "Out of memory while expanding RAW pixels.\n");
        return 1;
    }
    memset(pixels, 0, (size_t)(format_out->width * format_out->height));

    for (y = 0; y < format_out->height; ++y) {
        byte pixel_row[RAW_ROW_BYTES];
        int bp;

        if (format_out->body_row_bytes > RAW_ROW_BYTES) {
            free(pixels);
            fclose(in_file);
            fprintf(stderr, "Unsupported RAW bitmap width.\n");
            return 1;
        }

        if (fread(pixel_row, 1, (size_t)format_out->body_row_bytes, in_file) !=
            (size_t)format_out->body_row_bytes) {
            free(pixels);
            fclose(in_file);
            fprintf(stderr, "Unexpected end of RAW data.\n");
            return 1;
        }

        for (bp = 0; bp < TILE_DEPTH; ++bp) {
            int bitplane_row_idx;
            int byte_index;

            bitplane_row_idx = bp * format_out->row_bytes;
            for (byte_index = 0; byte_index < format_out->row_bytes; ++byte_index) {
                byte packed_bits;
                int bit;

                packed_bits = pixel_row[bitplane_row_idx + byte_index];
                for (bit = 0; bit < 8; ++bit) {
                    long x;
                    byte plane_bit;

                    x = (long)(byte_index * 8 + bit);
                    if (x < format_out->width) {
                        plane_bit = (byte)((packed_bits >> (7 - bit)) & 1);
                        pixels[y * format_out->width + x] =
                            (byte)(pixels[y * format_out->width + x] | (plane_bit << bp));
                    }
                }
            }
        }
    }

    fclose(in_file);
    *pixels_out = pixels;
    return 0;
}

static int load_raw_pixels(const char *input_path, byte **pixels_out) {
    bitmap_format_t format;

    return load_raw_pixels_with_mode(input_path, pixels_out, 1, &format);
}

static int write_bmp_from_pixels_with_format(const byte *pixels,
                                             const char *output_path,
                                             const bitmap_format_t *format,
                                             const uint8_t (*bmp_palette)[4]) {
    FILE *out_file;
    long row_size;
    dword_t pixel_data_offset;
    dword_t file_size;
    byte file_header[BMP_FILE_HEADER_SIZE];
    byte info_header[BMP_INFO_HEADER_SIZE];
    int y;

    row_size = ((((long)format->width * 4L) + 31L) / 32L) * 4L;
    pixel_data_offset = BMP_FILE_HEADER_SIZE + BMP_INFO_HEADER_SIZE + BMP_PALETTE_SIZE;
    file_size = pixel_data_offset + (dword_t)(row_size * format->height);

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
    write_u32_le(info_header + 4, (dword_t)format->width);
    write_u32_le(info_header + 8, (dword_t)format->height);
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
    fwrite(bmp_palette, 1, BMP_PALETTE_SIZE, out_file);

    for (y = format->height - 1; y >= 0; --y) {
        byte row_data[(PITCH_WIDTH / 2) + 4];
        int x;

        memset(row_data, 0, sizeof(row_data));
        for (x = 0; x < format->width; x += 2) {
            byte left;
            byte right;

            left = pixels[y * format->width + x];
            right = 0;
            if ((x + 1) < format->width) {
                right = pixels[y * format->width + x + 1];
            }
            row_data[x / 2] = (byte)((left << 4) | (right & 0x0F));
        }
        fwrite(row_data, 1, (size_t)row_size, out_file);
    }

    fclose(out_file);
    return 0;
}

static int write_bmp_from_pixels(const byte *pixels, const char *output_path) {
    bitmap_format_t format;

    format = make_bitmap_format(PITCH_WIDTH, PITCH_HEIGHT, TILE_DEPTH);
    return write_bmp_from_pixels_with_format(pixels, output_path, &format, swos_bmp_palette);
}

static int convert_bmp_to_raw(const char *input_path, const char *output_path) {
    byte *pixels;
    bitmap_format_t format;
    int result;

    result = load_bmp_pixels_with_mode(input_path, &pixels, is_swcpich_path(input_path), &format);
    if (result != 0) {
        return result;
    }

    result = write_raw_from_pixels_with_format(pixels, output_path, &format);
    free(pixels);
    return result;
}

static int convert_raw_to_bmp(const char *input_path, const char *output_path) {
    byte *pixels;
    bitmap_format_t format;
    int result;

    result = load_raw_pixels_with_mode(input_path, &pixels, is_swcpich_path(input_path), &format);
    if (result != 0) {
        return result;
    }

    result = write_bmp_from_pixels_with_format(pixels,
                                               output_path,
                                               &format,
                                               is_swcpich_path(input_path) ? swos_bmp_palette : simple_bmp_palette);
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

    distinct_tiles = (byte *)malloc((size_t)(TILES_MAP_CELLS * TILE_BYTES_NUM));
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
                if (!g_no_tile_limit && distinct_tile_count >= MAX_LEGACY_MAP_DISTINCT_TILES) {
                    free(distinct_tiles);
                    fprintf(stderr,
                            "Image would exceed the stock SWOS MAP size limit: at most %d distinct tiles fit in a legacy MAP without --no-tile-limit.\n",
                            MAX_LEGACY_MAP_DISTINCT_TILES);
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
            write_u32_be(header_bytes, (dword_t)tile_idx * TILE_BYTES_NUM);
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

    result = write_ilbm_from_raw_with_mode(tmp_raw_path, output_path, 1);
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
            "  .IFF (ILBM) -> .RAW, .MAP\n"
            "\n"
            "Format selection:\n"
            "  Only SWCPICH* files support .RAW/.BMP/.IFF <-> .MAP (SWOS tilemapped).\n"
            "  Other 4-bitplane graphics use simple planar .RAW/.BMP/.IFF conversion.\n");
}

static void print_usage(FILE *stream) {
    fprintf(stream,
            "swosconv %s\n"
            "\n"
            "Usage: swosconv [-n|--no-tile-limit] -i <input> -o <output>\n"
            "       swosconv --help\n"
            "       swosconv --version\n",
            SWOSCONV_VERSION);
    print_supported_conversions(stream);
    fprintf(stream,
            "\n"
            "Options:\n"
            "  -n, --no-tile-limit\n"
            "      Experimental. Allows writing legacy .MAP files beyond the stock\n"
            "      SWOS size limit. These files are not compatible with the standard\n"
            "      Amiga executable.\n");
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
        } else if (strcmp(argv[i], "-n") == 0 ||
                   strcmp(argv[i], "--no-tile-limit") == 0 ||
                   strcmp(argv[i], "--notilelimit") == 0) {
            g_no_tile_limit = true;
            ++i;
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
