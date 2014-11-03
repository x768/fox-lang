#include "image_raw.h"
#include <string.h>
#include <stdlib.h>


// Windowsビットマップの入出力
// 圧縮形式 : 圧縮なし
// 色数     : 1bit, 4bit, 8bit (Palette) / 16bit, 24bit (Full color) / 32bit (Full color with alpha)

enum {
    BITMAPFILEHEADER_SIZE = 14,
    BITMAPINFOHEADER_SIZE = 40,
    BITMAPV4HEADER_SIZE = 108,
    BITMAPV5HEADER_SIZE = 124,
    BMPHEADER_TOTAL_SIZE = BITMAPFILEHEADER_SIZE + BITMAPINFOHEADER_SIZE,
    
    RGBTRIPLE_SIZE = 3,
    RGBQUAD_SIZE = 4,
    PIX_PER_METER = 2835,
};
enum {
    BI_RGB = 0,
    BI_RLE8 = 1,
    BI_RLE4 = 2,
    BI_BITFIELDS = 3,
};

/*
typedef struct tagBITMAPFILEHEADER {
    WORD bfType;             // 0
    DWORD bfSize;            // 2
    WORD bfReserved1;        // 6
    WORD bfReserved2;        // 8
    DWORD bfOffBits;         // 10
} BITMAPFILEHEADER;
*/

/*
typedef struct tagBITMAPINFOHEADER {
    DWORD  biSize;           // 14
    LONG   biWidth;          // 18
    LONG   biHeight;         // 22
    WORD   biPlanes;         // 26
    WORD   biBitCount;       // 28
    DWORD  biCompression;    // 30
    DWORD  biSizeImage;      // 34
    LONG   biXPelsPerMeter;  // 38
    LONG   biYPelsPerMeter;  // 42
    DWORD  biClrUsed;        // 46
    DWORD  biClrImportant;   // 50
    DWORD  bV4RedMask;       //  0    // infoheader
    DWORD  bV4GreenMask;     //  4
    DWORD  bV4BlueMask;      //  8
    DWORD  bV4AlphaMask;     // 12
    DWORD  bV4CSType;        // 16
    CIEXYZTRIPLE bV4Endpoints;
    DWORD  bV4GammaRed;
    DWORD  bV4GammaGreen;
    DWORD  bV4GammaBlue;
} BITMAPINFOHEADER;
*/

static uint16_t ptr_read_uint16_le(const char *p)
{
    return ((uint8_t)p[0]) | ((uint8_t)p[1] << 8);
}
static uint32_t ptr_read_uint32_le(const char *p)
{
    return ((uint8_t)p[0]) | ((uint8_t)p[1] << 8) | ((uint8_t)p[2] << 16) | ((uint8_t)p[3] << 24);
}

static void ptr_write_uint16_le(char *p, uint16_t val)
{
    p[0] = val & 0xFF;
    p[1] = val >> 8;
}
static void ptr_write_uint32_le(char *p, uint32_t val)
{
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
}

static int get_shift_n(uint32_t mask)
{
    if (mask == 0 || mask == 0xFF) {
        return 0;
    } else if ((mask & 0xFFFFff80) == 0) {
        int ret = 0;
        while ((mask & 0x80) == 0) {
            mask <<= 1;
            ret--;
        }
        return ret;
    } else {
        int ret = 0;
        while ((mask & 0x100) != 0 || (mask & 0x80) == 0) {
            mask >>= 1;
            ret++;
        }
        return ret;
    }
}

static void bmp_convert_1_to_8(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0 ; i < width; i++) {
        if (src[i / 8] & (1 << (7 - (width % 8)))) {
            dst[i] = 1;
        } else {
            dst[i] = 0;
        }
    }
}
static void bmp_convert_4_to_8(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0 ; i < width; i++) {
        if (i % 2 == 0) {
            dst[i] = src[i / 2] >> 4;
        } else {
            dst[i] = src[i / 2] & 0xF;
        }
    }
}
static void bmp_convert_16_to_24(uint8_t *dst, const uint8_t *src, int width, uint32_t rmask, int rshift, uint32_t gmask, int gshift, uint32_t bmask, int bshift)
{
    int i;
    for (i = 0 ; i < width; i++) {
        uint32_t c = src[i * 2] | (src[i * 2 + 1] << 8);
        if (rshift >= 0) {
            dst[i * 3 + 0] = (c & rmask) >> rshift;
        } else {
            dst[i * 3 + 0] = (c & rmask) << -rshift;
        }
        if (gshift >= 0) {
            dst[i * 3 + 1] = (c & gmask) >> gshift;
        } else {
            dst[i * 3 + 1] = (c & gmask) << -gshift;
        }
        if (bshift >= 0) {
            dst[i * 3 + 2] = (c & bmask) >> bshift;
        } else {
            dst[i * 3 + 2] = (c & bmask) << -bshift;
        }
    }
}
static void bmp_convert_24_to_24(uint8_t *dst, const uint8_t *src, int width, uint32_t rmask, int rshift, uint32_t gmask, int gshift, uint32_t bmask, int bshift)
{
    int i;
    for (i = 0 ; i < width; i++) {
        uint32_t c = src[i * 3] | (src[i * 3 + 1] << 8) | (src[i * 3 + 2] << 16);
        dst[i * 3 + 0] = (c & rmask) >> rshift;
        dst[i * 3 + 1] = (c & gmask) >> gshift;
        dst[i * 3 + 2] = (c & bmask) >> bshift;
    }
}
static void bmp_convert_32_to_32(uint8_t *dst, const uint8_t *src, int width, uint32_t rmask, int rshift, uint32_t gmask, int gshift, uint32_t bmask, int bshift, uint32_t amask, int ashift)
{
    int i;
    for (i = 0 ; i < width; i++) {
        uint32_t c = src[i * 4] | (src[i * 4 + 1] << 8) | (src[i * 4 + 2] << 16) | (src[i * 4 + 3] << 24);
        dst[i * 4 + 0] = (c & rmask) >> rshift;
        dst[i * 4 + 1] = (c & gmask) >> gshift;
        dst[i * 4 + 2] = (c & bmask) >> bshift;
        dst[i * 4 + 3] = (c & amask) >> ashift;
    }
}
static void bmp_swap_rgb(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0 ; i < width; i++) {
        dst[i * 3 + 0] = src[i * 3 + 2];
        dst[i * 3 + 1] = src[i * 3 + 1];
        dst[i * 3 + 2] = src[i * 3 + 0];
    }
}
static void bmp_swap_rgba(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0 ; i < width; i++) {
        dst[i * 4 + 0] = src[i * 4 + 2];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 0];
        dst[i * 4 + 3] = src[i * 4 + 3];
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////

int load_image_bmp_sub(RefImage *image, Value r, int info_only)
{
    char *buf = malloc(RGBQUAD_SIZE * PALETTE_NUM);
    uint32_t off_bits;
    uint32_t bi_size;
    uint32_t bi_compression;
    uint16_t bit_count;
    uint32_t rmask = 0, gmask = 0, bmask = 0, amask = 0;
    int32_t width, height;
    int top_to_bottom = FALSE;
    int pitch, pitch_read;
    int palette_count = 0;
    int read_total = 0;
    int read_size = BMPHEADER_TOTAL_SIZE;

    if (!fs->stream_read_data(r, NULL, buf, &read_size, FALSE, TRUE)) {
        free(buf);
        return FALSE;
    }
    read_total += read_size;

    if (buf[0] != 'B' || buf[1] != 'M') {
        goto ERROR_INVALID_FORMAT;
    }
    off_bits = ptr_read_uint32_le(buf + 10);
    bi_size = ptr_read_uint32_le(buf + 14);
    width = ptr_read_uint32_le(buf + 18);
    height = ptr_read_uint32_le(buf + 22);

    if (height < 0) {
        height = -height;
        top_to_bottom = TRUE;
    }

    if (width == 0 || height == 0) {
        goto ERROR_INVALID_FORMAT;
    }
    if (width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fs->throw_errorf(mod_image, "ImageError", "Image size too large (max:%d)", MAX_IMAGE_SIZE);
        free(buf);
        return FALSE;
    }
    image->width = width;
    image->height = height;

    bi_compression = ptr_read_uint32_le(buf + 30);
    bit_count = ptr_read_uint16_le(buf + 28);
    palette_count = ptr_read_uint32_le(buf + 46);

    if (bi_compression != BI_RGB && bi_compression != BI_BITFIELDS) {
        goto ERROR_INVALID_FORMAT;
    }
    if (palette_count < 0 || palette_count > PALETTE_NUM) {
        goto ERROR_INVALID_FORMAT;
    }

    switch (bit_count) {
    case 1:
        image->bands = BAND_P;
        pitch = width;
        pitch_read = (width + 7) / 8;
        if (palette_count == 0) {
            palette_count = 2;
        }
        break;
    case 4:
        image->bands = BAND_P;
        pitch = width;
        pitch_read = (width + 1) / 2;
        if (palette_count == 0) {
            palette_count = 16;
        }
        break;
    case 8:
        image->bands = BAND_P;
        pitch = width;
        pitch_read = width;
        if (palette_count == 0) {
            palette_count = 256;
        }
        break;
    case 15:
    case 16:
        image->bands = BAND_RGB;
        pitch = width * 3;
        pitch_read = width * 2;
        amask = 0x0000;
        rmask = 0x7C00;
        gmask = 0x03E0;
        bmask = 0x001F;
        break;
    case 24:
        image->bands = BAND_RGB;
        pitch = width * 3;
        pitch_read = width * 3;
        amask = 0x00000000;
        rmask = 0x00FF0000;
        gmask = 0x0000FF00;
        bmask = 0x000000FF;
        break;
    case 32:
        image->bands = BAND_RGBA;
        pitch = width * 4;
        pitch_read = width * 4;
        amask = 0xFF000000;
        rmask = 0x00FF0000;
        gmask = 0x0000FF00;
        bmask = 0x000000FF;
        break;
    default:
        fs->throw_errorf(mod_image, "ImageError", "Invalid Windows BMP format (Unknown bit_count %d)", bit_count);
        free(buf);
        return FALSE;
    }
    pitch = (pitch + 3) & ~3;
    pitch_read = (pitch_read + 3) & ~3;
    image->pitch = pitch;

    if (bi_size >= BITMAPINFOHEADER_SIZE && bi_size < BITMAPV4HEADER_SIZE) {
        // BITMAPV4HEADERに満たない場合
        if (bi_size > BITMAPINFOHEADER_SIZE) {
            // 空読み
            read_size = bi_size - BITMAPINFOHEADER_SIZE;
            if (!fs->stream_read_data(r, NULL, NULL, &read_size, FALSE, TRUE)) {
                free(buf);
                return FALSE;
            }
            read_total += read_size;
        }
    } else if (bi_size == BITMAPV4HEADER_SIZE || bi_size == BITMAPV5HEADER_SIZE) {
        // BITMAPV4HEADER
        // BITMAPV5HEADER
        read_size = bi_size - BITMAPINFOHEADER_SIZE;
        if (!fs->stream_read_data(r, NULL, buf, &read_size, FALSE, TRUE)) {
            free(buf);
            return FALSE;
        }
        if (bi_compression == BI_BITFIELDS) {
            rmask = ptr_read_uint32_le(buf + 0);
            gmask = ptr_read_uint32_le(buf + 4);
            bmask = ptr_read_uint32_le(buf + 8);
            amask = ptr_read_uint32_le(buf + 12);
        }
        read_total += read_size;
    } else {
        goto ERROR_INVALID_FORMAT;
    }

    // パレットの読み込み
    if (palette_count > 0) {
        // RGBQUAD
        int i;
        uint32_t *palette = malloc(sizeof(uint32_t) * PALETTE_NUM);
        read_size = RGBQUAD_SIZE * palette_count;
        if (!fs->stream_read_data(r, NULL, buf, &read_size, FALSE, TRUE)) {
            free(buf);
            return FALSE;
        }
        read_total += read_size;
        memset(palette, 0, sizeof(uint32_t) * PALETTE_NUM);
        
        for (i = 0; i < palette_count; i++) {
            palette[i] = (((uint8_t)buf[i * 4 + 2]) << COLOR_R_SHIFT) | (((uint8_t)buf[i * 4 + 1]) << COLOR_G_SHIFT) | (((uint8_t)buf[i * 4 + 0]) << COLOR_B_SHIFT) | COLOR_A_MASK;
        }
        image->palette = palette;
    }
    if (off_bits < read_total) {
        goto ERROR_INVALID_FORMAT;
    } else if (off_bits > read_total) {
        // 空読み
        read_size = off_bits - read_total;
        if (!fs->stream_read_data(r, NULL, NULL, &read_size, FALSE, TRUE)) {
            free(buf);
            return FALSE;
        }
    }

    // 画像データの読み込み
    if (!info_only) {
        int y;
        int rshift = get_shift_n(rmask);
        int gshift = get_shift_n(gmask);
        int bshift = get_shift_n(bmask);
        int ashift = get_shift_n(amask);
        uint8_t *src = malloc(pitch_read);
        uint8_t *data = malloc(pitch * height);

        for (y = 0; y < height; y++) {
            uint8_t *dst;
            if (top_to_bottom) {
                dst = data + pitch * y;
            } else {
                dst = data + pitch * (height - y - 1);
            }

            read_size = pitch_read;
            if (!fs->stream_read_data(r, NULL, (char*)src, &read_size, FALSE, TRUE)) {
                free(buf);
                return FALSE;
            }
            read_total += read_size;
            if (read_size != pitch_read) {
                free(src);
                free(data);
                goto ERROR_INVALID_FORMAT;
            }

            switch (bit_count) {
            case 1:
                bmp_convert_1_to_8(dst, src, width);
                break;
            case 4:
                bmp_convert_4_to_8(dst, src, width);
                break;
            case 8:
                memcpy(dst, src, width);
                break;
            case 15:
            case 16:
                bmp_convert_16_to_24(dst, src, width, rmask, rshift, gmask, gshift, bmask, bshift);
                break;
            case 24:
                bmp_convert_24_to_24(dst, src, width, rmask, rshift, gmask, gshift, bmask, bshift);
                break;
            case 32:
                bmp_convert_32_to_32(dst, src, width, rmask, rshift, gmask, gshift, bmask, bshift, amask, ashift);
                break;
            }
        }
        free(src);
        image->data = data;
    }

    return TRUE;

ERROR_INVALID_FORMAT:
    fs->throw_errorf(mod_image, "ImageError", "Invalid Windows BMP format");
    free(buf);
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

int save_image_bmp_sub(RefImage *image, Value w)
{
    char *buf;
    int total_size = 0;
    int off_bits = 0;
    int bit_count;
    int pitch = image->pitch;
    int pitch_align = (pitch + 3) & ~3;

    switch (image->bands) {
    case BAND_L:
        bit_count = 8;
        break;
    case BAND_P:
        bit_count = 8;
        break;
    case BAND_RGB:
        bit_count = 24;
        break;
    case BAND_RGBA:
        bit_count = 32;
        break;
    default:
        fs->throw_errorf(mod_image, "ImageError", "Not supported type");
        return FALSE;
    }

    buf = malloc(RGBQUAD_SIZE * PALETTE_NUM);
    off_bits = BMPHEADER_TOTAL_SIZE + (bit_count == 8 ? PALETTE_NUM * 4 : 0);
    total_size = off_bits + pitch_align * image->height;

    memset(buf, 0, RGBQUAD_SIZE * PALETTE_NUM);

    buf[0] = 'B';
    buf[1] = 'M';
    ptr_write_uint32_le(buf + 2, total_size);
    ptr_write_uint32_le(buf + 10, off_bits);

    ptr_write_uint32_le(buf + 14, 40);                           // biSize
    ptr_write_uint32_le(buf + 18, image->width);
    ptr_write_uint32_le(buf + 22, image->height);
    ptr_write_uint16_le(buf + 26, 1);                            // biPlanes
    ptr_write_uint16_le(buf + 28, bit_count);
    ptr_write_uint32_le(buf + 30, BI_RGB);
    ptr_write_uint32_le(buf + 34, pitch_align * image->height);  // biSizeImage
    ptr_write_uint32_le(buf + 38, PIX_PER_METER);
    ptr_write_uint32_le(buf + 42, PIX_PER_METER);
    ptr_write_uint32_le(buf + 46, (bit_count == 8 ? 256 : 0));   // biClrUsed
    ptr_write_uint32_le(buf + 50, 0);                            // biCirImportant

    if (!fs->stream_write_data(w, buf, BMPHEADER_TOTAL_SIZE)) {
        free(buf);
        return FALSE;
    }

    // パレット
    if (bit_count == 8) {
        int i;
        if (image->palette != NULL) {
            const uint32_t *palette = image->palette;
            for (i = 0; i < PALETTE_NUM; i++) {
                uint32_t c = palette[i];
                buf[i * 4 + 0] = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
                buf[i * 4 + 1] = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
                buf[i * 4 + 2] = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
                buf[i * 4 + 3] = 0;
            }
        } else {
            // グレイスケール画像
            for (i = 0; i < PALETTE_NUM; i++) {
                buf[i * 4 + 0] = i;
                buf[i * 4 + 1] = i;
                buf[i * 4 + 2] = i;
                buf[i * 4 + 3] = 0;
            }
        }
        if (!fs->stream_write_data(w, buf, PALETTE_NUM * RGBQUAD_SIZE)) {
            free(buf);
            return FALSE;
        }
    }

    // 画像情報
    {
        int y;
        int width = image->width;
        int height = image->height;
        uint8_t *dst = malloc(pitch_align);
        memset(dst, 0, pitch_align);

        for (y = 0; y < height; y++) {
            uint8_t *src = (uint8_t*)(image->data + (height - y - 1) * pitch);
            switch (bit_count) {
            case 8:
                memcpy(dst, src, pitch);
                break;
            case 24:
                bmp_swap_rgb(dst, src, width);
                break;
            case 32:
                bmp_swap_rgba(dst, src, width);
                break;
            }
            if (!fs->stream_write_data(w, (const char*)dst, pitch_align)) {
                free(dst);
                free(buf);
                return FALSE;
            }
        }
        free(dst);
    }
    free(buf);

    return TRUE;
}
