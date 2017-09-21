#define DEFINE_GLOBALS
#include "fox.h"
#include "m_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <jpeglib.h>
#include "gif_lib.h"


typedef struct {
    struct jpeg_source_mgr pub; /* public fields */

    Value reader;
    char *buffer;
    int start;
} JpegSrcManager;

typedef struct {
    struct jpeg_destination_mgr pub;    /* public fields */

    Value writer;
    char *buffer;
} JpegDstManager;

static const FoxStatic *fs;
static FoxGlobal *fg;
static RefNode *mod_image;
static RefNode *mod_webimage;


static int get_map_double(int *has_value, double *value, RefMap *v_map, const char *key)
{
    HashValueEntry *ret = fs->refmap_get_strkey(v_map, key, -1);

    if (ret != NULL) {
        const RefNode *type = fs->Value_type(ret->val);
        if (type != fs->cls_float) {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Float required but %n", type);
            return FALSE;
        }
        *value = fs->Value_float(ret->val);
        *has_value = TRUE;
    } else {
        *has_value = FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////

static void extract_palette3(uint8_t *dst, const uint8_t *src, int width, uint32_t *pal)
{
    int i;
    for (i = 0; i < width; i++) {
        uint32_t c = pal[src[i]];
        *dst++ = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
        *dst++ = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
        *dst++ = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
    }
}
static void remove_alpha_channel1(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0; i < width; i++) {
        *dst++ = *src++;
        src++;
    }
}
static void remove_alpha_channel3(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0; i < width; i++) {
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = *src++;
        src++;
    }
}

static void fatal_jpeg_error(j_common_ptr cinfo)
{
    char cbuf[JMSG_LENGTH_MAX];

    cinfo->err->format_message(cinfo, cbuf);
    fs->throw_errorf(mod_image, "ImageError", "%s", cbuf);
}
static void throw_jpeg_error(j_common_ptr cinfo)
{
    char cbuf[JMSG_LENGTH_MAX];

    cinfo->err->format_message(cinfo, cbuf);
    fs->throw_errorf(mod_image, "ImageError", "%s", cbuf);
}

////////////////////////////////////////////////////////////////////////////////////////////////

static void png_read_callback(png_structp png_ptr, png_bytep buf, png_size_t size)
{
    Value *r = (Value*)png_get_io_ptr(png_ptr);
    int sz = size;

    if (!fs->stream_read_data(*r, NULL, (char*)buf, &sz, FALSE, TRUE)) {
        longjmp(png_jmpbuf(png_ptr), 1);
    }
}
static void png_error_callback(png_structp png_ptr, png_const_charp error_msg)
{
    fs->throw_errorf(mod_image, "ImageError", "%s", error_msg);
    longjmp(png_jmpbuf(png_ptr), 1);
}
static void png_warn_callback(png_structp png_ptr, png_const_charp error_msg)
{
}

static int load_png_sub(RefImage *image, Value r, int info_only)
{
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    int compression_type,filter_type;
    png_colorp palette;
    int num_palette;
    png_uint_32 rowbytes;
    png_bytep data = NULL;
    png_bytepp rows = NULL;
    int i;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_error_callback, png_warn_callback);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        free(data);
        free(rows);
        return FALSE;
    }

    png_set_read_fn(png_ptr, &r, png_read_callback);

    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_type);

    if (width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fs->throw_errorf(mod_image, "ImageError", "Image size too large (max:%d)", MAX_IMAGE_SIZE);
        return FALSE;
    }

    image->width = width;
    image->height = height;

    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    } else if (bit_depth < 8) {
        png_set_packing(png_ptr);
    }

    switch (color_type) {
    case PNG_COLOR_TYPE_GRAY:
        image->bands = BAND_L;
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        image->bands = BAND_LA;
        break;
    case PNG_COLOR_TYPE_PALETTE: {
        uint32_t *col;

        image->bands = BAND_P;

        // パレットの読み込み
        col = malloc(sizeof(uint32_t) * PALETTE_NUM);
        for (i = 0; i < PALETTE_NUM; i++) {
            col[i] = COLOR_A_MASK;
        }
        image->palette = col;
        png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
        if (num_palette > PALETTE_NUM) {
            fs->throw_errorf(mod_image, "ImageError", "Invalid PNG format");
            return FALSE;
        }

        for (i = 0; i < num_palette; i++) {
            col[i] = (palette[i].red << COLOR_R_SHIFT) | (palette[i].green << COLOR_G_SHIFT) | (palette[i].blue << COLOR_B_SHIFT) | COLOR_A_MASK;
        }

        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_bytep trans;
            int num_trans;

            png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
            for (i = 0; i < num_trans; ++i) {
                col[i] &= ~COLOR_A_MASK;
                col[i] |= trans[i] << COLOR_A_SHIFT;
            }
        }
        break;
    }
    case PNG_COLOR_TYPE_RGB:
        image->bands = BAND_RGB;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        image->bands = BAND_RGBA;
        break;
    default:
        break;
    }
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (bit_depth < 8) {
        rowbytes *= 8 / bit_depth;
    }
    image->pitch = rowbytes;

    if (!info_only) {
        if (rowbytes * height > fs->max_alloc) {
            fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }
        data = malloc(rowbytes * height);
        rows = malloc(sizeof(png_bytep) * height);
        for (i = 0; i < height; i++) {
            rows[i] = data + rowbytes * i;
        }
        png_read_image(png_ptr, rows);
        free(rows);

        image->data = data;
        png_read_end(png_ptr, info_ptr);
    }
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return TRUE;
}

static int load_image_png(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(v[1]);
    int info_only = Value_bool(v[3]);

    if (!load_png_sub(image, v[2], info_only)) {
        return FALSE;
    }
    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void jpeg_init_source_callback(j_decompress_ptr cinfo)
{
    JpegSrcManager *src = (JpegSrcManager*)cinfo->src;
    src->start = TRUE;
}
static boolean fill_input_buffer_callback(j_decompress_ptr cinfo)
{
    JpegSrcManager *src = (JpegSrcManager*)cinfo->src;
    int read_size = 0;

    while (read_size < BUFFER_SIZE) {
        int rd = BUFFER_SIZE - read_size;
        if (!fs->stream_read_data(src->reader, NULL, (char*)src->buffer + read_size, &rd, FALSE, FALSE)) {
            // TODO 例外発生時の巻き戻し
        }
        if (rd <= 0) {
            break;
        }
        read_size += rd;
    }

    // ダミーデータ挿入
    if (read_size <= 0) {
        if (src->start) {
            src->buffer[0] = 0xFF;
            src->buffer[1] = JPEG_EOI;
            read_size = 2;
        }
    }
    src->pub.next_input_byte = (uint8_t*)src->buffer;
    src->pub.bytes_in_buffer = read_size;
    src->start = FALSE;

    return TRUE;
}
static void skip_input_data_callback(j_decompress_ptr cinfo, long num_bytes)
{
    JpegSrcManager *src = (JpegSrcManager*)cinfo->src;
    if (num_bytes > 0) {
        while (num_bytes > (long)src->pub.bytes_in_buffer) {
            num_bytes -= src->pub.bytes_in_buffer;
            fill_input_buffer_callback(cinfo);
        }
        src->pub.next_input_byte += num_bytes;
        src->pub.bytes_in_buffer -= num_bytes;
    }
}
static void term_source_callback(j_decompress_ptr cinfo)
{
}

static void jpeg_init_source(struct jpeg_decompress_struct *cinfo, Value r)
{
    JpegSrcManager *src = (JpegSrcManager *)cinfo->src;

    if (src == NULL) {
        src = cinfo->mem->alloc_small((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(JpegSrcManager));
        src->buffer = cinfo->mem->alloc_small((j_common_ptr)cinfo, JPOOL_PERMANENT, BUFFER_SIZE);
        cinfo->src = (struct jpeg_source_mgr *)src;
    }
    src->pub.init_source = jpeg_init_source_callback;
    src->pub.fill_input_buffer = fill_input_buffer_callback;
    src->pub.skip_input_data = skip_input_data_callback;
    src->pub.resync_to_restart = jpeg_resync_to_restart;
    src->pub.term_source = term_source_callback;
    src->reader = r;
    src->pub.bytes_in_buffer = 0;
    src->pub.next_input_byte = NULL;
}
static void CMYK_to_RGB(uint8_t *dst, uint8_t *src, int width, int inverted)
{
    int i;

    if (inverted) {
        for (i = 0; i < width; i++) {
            int c = src[i * 4 + 0];
            int m = src[i * 4 + 1];
            int y = src[i * 4 + 2];
            int k = src[i * 4 + 3];
            dst[i * 3 + 0] = c * k / 255;
            dst[i * 3 + 1] = m * k / 255;
            dst[i * 3 + 2] = y * k / 255;
        }
    } else {
        for (i = 0; i < width; i++) {
            int c = src[i * 4 + 0];
            int m = src[i * 4 + 1];
            int y = src[i * 4 + 2];
            int k = src[i * 4 + 3];
            dst[i * 3 + 0] = (255 - c) * (255 - k) / 255;
            dst[i * 3 + 1] = (255 - m) * (255 - k) / 255;
            dst[i * 3 + 2] = (255 - y) * (255 - k) / 255;
        }
    }
}
static int load_jpeg_sub(RefImage *image, Value r, int info_only)
{
    int channels;
    int inverted = FALSE;
    int rowbytes;
    uint8_t *data;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = fatal_jpeg_error;
    jerr.output_message = throw_jpeg_error;
    jpeg_create_decompress(&cinfo);
    jpeg_init_source(&cinfo, r);

    jpeg_save_markers(&cinfo, JPEG_APP0 + 14, 256);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        if (fg->error == VALUE_NULL) {
            fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format");
        }
        goto ERROR_END;
    }
    if (cinfo.image_width > MAX_IMAGE_SIZE || cinfo.image_height > MAX_IMAGE_SIZE) {
        fs->throw_errorf(mod_image, "ImageError", "Image size too large (max:%d)", MAX_IMAGE_SIZE);
        goto ERROR_END;
    }

    image->width = cinfo.image_width;
    image->height = cinfo.image_height;

    if (!jpeg_start_decompress(&cinfo)) {
        if (fg->error == VALUE_NULL) {
            fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (jpeg_start_decompress failed)");
        }
        goto ERROR_END;
    }
    if (image->width != cinfo.output_width || image->height != cinfo.output_height) {
        fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format");
        goto ERROR_END;
    }

    switch (cinfo.jpeg_color_space) {
    case JCS_CMYK:
    case JCS_YCCK:
        cinfo.out_color_space = JCS_CMYK;
        break;
    case JCS_GRAYSCALE:
        cinfo.out_color_space = JCS_GRAYSCALE;
        break;
    default:
        cinfo.out_color_space = JCS_RGB;
        break;
    }

    if (cinfo.out_color_space == JCS_RGB) {
        if (cinfo.output_components != 3) {
            fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (output_components %d, expected 3)", cinfo.output_components);
            goto ERROR_END;
        }
        image->bands = BAND_RGB;
        channels = 3;
    } else if (cinfo.out_color_space == JCS_CMYK) {
        jpeg_saved_marker_ptr marker;
        if (cinfo.output_components != 4) {
            fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (output_components %d, expected 4)", cinfo.output_components);
            goto ERROR_END;
        }
        for (marker = cinfo.marker_list; marker != NULL; marker = marker->next) {
            if (marker->marker == (JPEG_APP0 + 14) && marker->data_length >= 12 && strncmp((const char*)marker->data, "Adobe", 5) == 0) {
                inverted = TRUE;
                break;
            }
        }
        image->bands = BAND_RGB;
        channels = 3;
    } else if (cinfo.out_color_space == JCS_GRAYSCALE) {
        if (cinfo.output_components != 1) {
            fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (output_components %d, expected 1)", cinfo.output_components);
            goto ERROR_END;
        }
        image->bands = BAND_L;
        channels = 1;
    } else {
        fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (Not suppoeter color_space)");
        goto ERROR_END;
    }

    rowbytes = image->width * channels;
    image->pitch = rowbytes;

    if (!info_only) {
        if (rowbytes * image->height > fs->max_alloc) {
            fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            goto ERROR_END;
        }
        data = malloc(rowbytes * image->height);

        if (cinfo.out_color_space == JCS_RGB || cinfo.out_color_space == JCS_GRAYSCALE) {
            JSAMPROW rows[1];
            int i;

            for (i = 0; i < image->height; i++) {
                rows[0] = data + rowbytes * i;
                if (jpeg_read_scanlines(&cinfo, rows, 1) != 1) {
                    if (fg->error == VALUE_NULL) {
                        fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (jpeg_read_scanlines failed)");
                    }
                    goto ERROR_END;
                }
            }
        } else if (cinfo.out_color_space == JCS_CMYK) {
            JSAMPROW rows[1];
            int i;
            uint8_t *cmyk = malloc(image->width * 4);
            rows[0] = cmyk;

            for (i = 0; i < image->height; i++) {
                // CMYK CMYK ...
                if (jpeg_read_scanlines(&cinfo, rows, 1) != 1) {
                    if (fg->error == VALUE_NULL) {
                        fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (jpeg_read_scanlines failed)");
                    }
                    goto ERROR_END;
                }
                CMYK_to_RGB(data + rowbytes * i, cmyk, image->width, inverted);
            }
            free(cmyk);
        }
        image->data = data;
    }
    jpeg_destroy_decompress(&cinfo);

    return TRUE;

ERROR_END:
    jpeg_destroy_decompress(&cinfo);
    return FALSE;
}
static int load_image_jpeg(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(v[1]);
    int info_only = Value_bool(v[3]);

    if (!load_jpeg_sub(image, v[2], info_only)) {
        return FALSE;
    }
    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int gif_read_callback(GifFileType* gif_file, GifByteType* buf, int count)
{
    Value *r = gif_file->UserData;

    if (!fs->stream_read_data(*r, NULL, (char*)buf, &count, FALSE, FALSE)) {
        return -1;
    }
    return count;
}

static int get_transparent_index(const SavedImage *si)
{
    if (si->ExtensionBlocks != NULL) {
        int i;
        for (i = 0; i < si->ExtensionBlockCount; i++) {
            ExtensionBlock *b = &si->ExtensionBlocks[i];
            if (b->Function == GRAPHICS_EXT_FUNC_CODE && b->ByteCount >= 4) {
                if ((b->Bytes[0] & 0x01) != 0) {
                    return b->Bytes[3] & 0xFF;
                }
            }
        }
    }
    return -1;
}
static int get_transparent_index_from_image(const RefImage *img)
{
    int i;
    const uint32_t *pal = img->palette;

    if (pal != NULL) {
        for (i = PALETTE_NUM - 1; i >= 0; i--) {
            if ((pal[i] & COLOR_A_MASK) == 0) {
                return i;
            }
        }
    }
    return -1;
}

/*
 * GIFの最初の1枚を読む
 */
static int load_gif_sub(RefImage *image, Value r, int info_only)
{
    int err = 0;
    int rowbytes;
    GifFileType *gif = DGifOpen(&r, gif_read_callback, &err);

    if (gif == NULL) {
        fs->throw_errorf(mod_image, "ImageError", "Failed to read GIF image");
        return FALSE;
    }

    if (gif->SWidth > MAX_IMAGE_SIZE || gif->SHeight > MAX_IMAGE_SIZE) {
        fs->throw_errorf(mod_image, "ImageError", "Image size too large (max:%d)", MAX_IMAGE_SIZE);
        DGifCloseFile(gif, NULL);
        return FALSE;
    }
    image->bands = BAND_P;
    image->width = gif->SWidth;
    image->height = gif->SHeight;
    rowbytes = image->width;
    image->pitch = rowbytes;

    if (!info_only) {
        const ColorMapObject *cmap;
        const SavedImage *si;

        if (DGifSlurp(gif) == GIF_ERROR || gif->ImageCount <= 0) {
            fs->throw_errorf(mod_image, "ImageError", "Failed to read GIF image");
            return FALSE;
        }
        si = &gif->SavedImages[0];
        // パレット読み込み
        if (si->ImageDesc.ColorMap != NULL) {
            cmap = si->ImageDesc.ColorMap;
        } else {
            cmap = gif->SColorMap;
        }
        if (cmap != NULL) {
            int i;
            int color_key;
            uint32_t *col = malloc(sizeof(uint32_t) * PALETTE_NUM);

            for (i = 0; i < PALETTE_NUM; i++) {
                col[i] = COLOR_A_MASK;
            }
            for (i = 0; i < cmap->ColorCount; i++) {
                const GifColorType *c = &cmap->Colors[i];
                col[i] = (c->Red << COLOR_R_SHIFT) | (c->Green << COLOR_G_SHIFT) | (c->Blue << COLOR_B_SHIFT) | COLOR_A_MASK;
            }
            color_key = get_transparent_index(si);
            if (color_key >= 0) {
                col[color_key] &= ~COLOR_A_MASK;
            }

            image->palette = col;
        } else {
            // Illigal
            image->bands = BAND_L;
        }
        if (si->RasterBits != NULL) {
            int y;
            int size_alloc = rowbytes * image->height;
            const GifImageDesc *id = &si->ImageDesc;
            uint8_t *data;

            if (size_alloc > fs->max_alloc) {
                fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
                goto ERROR_END;
            }
            if (id->Left < 0 || id->Left + id->Width > image->width || id->Top < 0 || id->Top + id->Height > image->height) {
                fs->throw_errorf(mod_image, "ImageError", "Invalid GIF format");
                goto ERROR_END;
            }

            data = malloc(size_alloc);
            memset(data, gif->SBackGroundColor, size_alloc);
            for (y = 0; y < id->Height; y++) {
                memcpy(data + rowbytes * (y + id->Top) + id->Left, si->RasterBits + id->Width * y, id->Width);
            }
            image->data = data;
        }
    }

    DGifCloseFile(gif, NULL);
    return TRUE;

ERROR_END:
    DGifCloseFile(gif, NULL);
    return FALSE;
}
static int load_image_gif(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(v[1]);
    int info_only = Value_bool(v[3]);

    if (!load_gif_sub(image, v[2], info_only)) {
        return FALSE;
    }
    return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

static void png_write_callback(png_structp png_ptr, png_bytep buf, png_size_t size)
{
    Value *w = (Value*)png_get_io_ptr(png_ptr);

    if (!fs->stream_write_data(*w, (const char*)buf, size)) {
        longjmp(png_jmpbuf(png_ptr), 1);
    }
}
static void png_flush_callback(png_structp png_ptr)
{
}
static int palette_has_alpha(uint32_t *pal)
{
    int i;
    for (i = 0; i < PALETTE_NUM; i++) {
        if ((pal[i] & COLOR_A_MASK) != COLOR_A_MASK) {
            return TRUE;
        }
    }
    return FALSE;
}
static int save_png_sub(RefImage *image, Value w)
{
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_bytepp rows = malloc(sizeof(png_bytep) * image->height);
    png_bytep data = (png_bytep)image->data;
    int color_type = PNG_COLOR_TYPE_RGB;
    int i;

    png_set_write_fn(png_ptr, &w, png_write_callback, png_flush_callback);

    switch (image->bands) {
    case BAND_L:
        color_type = PNG_COLOR_TYPE_GRAY;
        break;
    case BAND_LA:
        color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
        break;
    case BAND_P:
        color_type = PNG_COLOR_TYPE_PALETTE;
        break;
    case BAND_RGB:
        color_type = PNG_COLOR_TYPE_RGB;
        break;
    case BAND_RGBA:
        color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        break;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr,  &info_ptr);
        free(rows);
        return FALSE;
    }

    png_set_IHDR(png_ptr, info_ptr,
            image->width, image->height, 8, color_type,
            PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    for (i = 0; i < image->height; i++) {
        rows[i] = data + image->pitch * i;
    }

    if (image->bands == BAND_P) {
        uint32_t *pal = image->palette;
        uint8_t trans[PALETTE_NUM];
        png_color pcolor[PALETTE_NUM];
        if (palette_has_alpha(pal)) {
            for (i = 0; i < PALETTE_NUM; i++) {
                trans[i] = (pal[i] & COLOR_A_MASK) >> COLOR_A_SHIFT;
            }
            png_set_tRNS(png_ptr, info_ptr, trans, PALETTE_NUM, NULL);
        }
        for (i = 0; i < PALETTE_NUM; i++) {
            pcolor[i].red   = (pal[i] & COLOR_R_MASK) >> COLOR_R_SHIFT;
            pcolor[i].green = (pal[i] & COLOR_G_MASK) >> COLOR_G_SHIFT;
            pcolor[i].blue  = (pal[i] & COLOR_B_MASK) >> COLOR_B_SHIFT;
        }
        png_set_PLTE(png_ptr, info_ptr, pcolor, PALETTE_NUM);
    }

    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    free(rows);

    return TRUE;
}
static int save_image_png(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(v[1]);

    if (!save_png_sub(image, v[2])) {
        return FALSE;
    }
    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////

static void init_destination_callback(j_compress_ptr cinfo)
{
}
static boolean empty_output_buffer_callback(j_compress_ptr cinfo)
{
    JpegDstManager *dst = (JpegDstManager *)cinfo->dest;

    if (!fs->stream_write_data(dst->writer, dst->buffer, BUFFER_SIZE)) {
        //
    }
    dst->pub.next_output_byte = (uint8_t *)dst->buffer;
    dst->pub.free_in_buffer = BUFFER_SIZE;

    return TRUE;
}
static void term_destination_callback(j_compress_ptr cinfo)
{
    JpegDstManager *dst = (JpegDstManager *)cinfo->dest;
    int write_size = BUFFER_SIZE - dst->pub.free_in_buffer;

    if (write_size > 0) {
        if (!fs->stream_write_data(dst->writer, dst->buffer, write_size)) {
            //
        }
    }
}
static void jpeg_init_destination(j_compress_ptr cinfo, Value w)
{
    JpegDstManager *dst = (JpegDstManager *)cinfo->dest;

    if (dst == NULL) {
        dst = (*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(JpegDstManager));
        dst->buffer = cinfo->mem->alloc_small((j_common_ptr) cinfo, JPOOL_IMAGE, BUFFER_SIZE);
        dst->pub.next_output_byte = (uint8_t *)dst->buffer;
        dst->pub.free_in_buffer = BUFFER_SIZE;

        cinfo->dest = (struct jpeg_destination_mgr *)dst;
    }
    dst->pub.init_destination = init_destination_callback;
    dst->pub.empty_output_buffer = empty_output_buffer_callback;
    dst->pub.term_destination = term_destination_callback;
    dst->writer = w;
}

static int save_jpeg_sub(RefImage *image, Value w, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    int extract_palette = FALSE;
    int remove_alpha = FALSE;
    uint8_t *data = image->data;

    memset(&cinfo, 0, sizeof(cinfo));
    memset(&jerr, 0, sizeof(jerr));

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = fatal_jpeg_error;
    jerr.output_message = throw_jpeg_error;
    jpeg_create_compress(&cinfo);

    switch (image->bands) {
    case BAND_L:
        cinfo.input_components = 1;
        cinfo.in_color_space = JCS_GRAYSCALE;
        break;
    case BAND_LA:
        // アルファチャンネルを無視
        cinfo.input_components = 1;
        cinfo.in_color_space = JCS_GRAYSCALE;
        remove_alpha = TRUE;
        break;
    case BAND_P:
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        extract_palette = TRUE;
        break;
    case BAND_RGB:
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        break;
    case BAND_RGBA:
        // アルファチャンネルを無視
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        remove_alpha = TRUE;
        break;
    default:
        fs->throw_errorf(mod_image, "ImageError", "Not supported type");
        goto ERROR_END;
    }
    cinfo.image_width = image->width;
    cinfo.image_height = image->height;
    jpeg_set_defaults(&cinfo);

    if (quality >= 0) {
        jpeg_set_quality(&cinfo, quality, TRUE);
    }
    jpeg_init_destination(&cinfo, w);
    jpeg_start_compress(&cinfo, TRUE);

    if (extract_palette) {
        JSAMPROW rows[1];
        int i;
        uint8_t *row = malloc(image->width * 3);
        rows[0] = row;

        for (i = 0; i < image->height; i++) {
            extract_palette3(row, data + image->pitch * i, image->width, image->palette);
            if (jpeg_write_scanlines(&cinfo, rows, 1) != 1) {
                fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (jpeg_write_scanlines failed)");
                free(row);
                goto ERROR_END;
            }
        }
        free(row);
    } else if (remove_alpha) {
        JSAMPROW rows[1];
        int i;
        uint8_t *row = malloc(image->width * 3);
        rows[0] = row;

        for (i = 0; i < image->height; i++) {
            if (image->bands == BAND_RGBA) {
                remove_alpha_channel3(row, data + image->pitch * i, image->width);
            } else {
                remove_alpha_channel1(row, data + image->pitch * i, image->width);
            }
            if (jpeg_write_scanlines(&cinfo, rows, 1) != 1) {
                fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (jpeg_write_scanlines failed)");
                free(row);
                goto ERROR_END;
            }
        }
        free(row);
    } else {
        JSAMPROW rows[1];
        int i;
        for (i = 0; i < image->height; i++) {
            rows[0] = data + image->pitch * i;
            if (jpeg_write_scanlines(&cinfo, rows, 1) != 1) {
                fs->throw_errorf(mod_image, "ImageError", "Invalid JPEG format (jpeg_write_scanlines failed)");
                goto ERROR_END;
            }
        }
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return TRUE;

ERROR_END:
    jpeg_destroy_compress(&cinfo);
    return FALSE;
}
static int save_image_jpeg(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(v[1]);
    RefMap *rm_param = Value_vp(v[3]);
    int q = -1;
    double dq;
    int has_value;

    if (!get_map_double(&has_value, &dq, rm_param, "quality")) {
        return FALSE;
    }
    if (has_value) {
        if (dq < 0.0) {
            q = 0;
        } else if (dq > 1.0) {
            q = 100;
        } else {
            q = dq * 100.0;
        }
    }

    if (!save_jpeg_sub(image, v[2], q)) {
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////

static int gif_write_callback(GifFileType* gif_file, const GifByteType* buf, int count)
{
    Value *w = gif_file->UserData;

    if (!fs->stream_write_data(*w, (const char*)buf, count)) {
        //
    }
    return count;
}
static void fox_image_free(RefImage *img)
{
    if (img != NULL) {
        free(img->palette);
        free(img->data);
        free(img);
    }
}
static int call_quantize(Value *dst, Value img, int bands)
{
    fs->Value_push("vs", img, (bands == BAND_RGBA ? "colorkey" : "rgb"));
    if (!fs->call_member_func(fs->intern("quantize", -1), 1, TRUE)) {
        return FALSE;
    }
    fg->stk_top--;
    *dst = *fg->stk_top;

    return TRUE;
}
/*
 * GIFを1枚書き込む
 */
static int save_gif_sub(RefImage *image, Value w)
{
    GifFileType *gif;
    int gray_scale = FALSE;
    int err = 0;
    ColorMapObject cmap;
    int y;
    RefImage *img_p = NULL;

    switch (image->bands) {
    case BAND_L:
        gray_scale = TRUE;
        break;
    case BAND_LA:
        gray_scale = TRUE;
        // TODO:alpha channelを除去
        // TODO:透明ピクセルがあれば255色に減色して透明色を割り当て
        break;
    case BAND_P:
        break;
    }

    gif = EGifOpen(&w, gif_write_callback, &err);
    if (gif == NULL) {
        fs->throw_errorf(mod_image, "ImageError", "EGifOpen failed");
        fox_image_free(img_p);
        return FALSE;
    }

    cmap.ColorCount = PALETTE_NUM;
    cmap.BitsPerPixel = 8;
    cmap.SortFlag = FALSE;
    cmap.Colors = malloc(sizeof(GifColorType) * PALETTE_NUM);

    if (gray_scale) {
        int i;
        for (i = 0; i < PALETTE_NUM; i++) {
            GifColorType *c = &cmap.Colors[i];
            c->Red = i;
            c->Green = i;
            c->Blue = i;
        }
    } else {
        int i;
        const uint32_t *col = image->palette;
        for (i = 0; i < PALETTE_NUM; i++) {
            GifColorType *c = &cmap.Colors[i];
            c->Red   = (col[i] & COLOR_R_MASK) >> COLOR_R_SHIFT;
            c->Green = (col[i] & COLOR_G_MASK) >> COLOR_G_SHIFT;
            c->Blue  = (col[i] & COLOR_B_MASK) >> COLOR_B_SHIFT;
        }
    }

    if (EGifPutScreenDesc(gif, image->width, image->height, PALETTE_NUM, 0, &cmap) == GIF_ERROR) {
        fs->throw_errorf(mod_image, "ImageError", "EGifPutScreenDesc failed");
        goto ERROR_END;
    }
    {
        int color_key = get_transparent_index_from_image(image);
        if (color_key >= 0) {
            uint8_t ebuf[4];
            ebuf[0] = 0x01;
            ebuf[1] = 0x0A;
            ebuf[2] = 0x00;
            ebuf[3] = color_key;
            EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, 4, ebuf);
        }
    }
    if (EGifPutImageDesc(gif, 0, 0, image->width, image->height, FALSE, NULL) == GIF_ERROR) {
        fs->throw_errorf(mod_image, "ImageError", "EGifPutImageDesc failed");
        goto ERROR_END;
    }

    for (y = 0; y < image->height; y++) {
        if (EGifPutLine(gif, (GifPixelType*)image->data + y * image->pitch, image->width) == GIF_ERROR) {
            fs->throw_errorf(mod_image, "ImageError", "EGifPutLine failed");
            goto ERROR_END;
        }
    }

    EGifCloseFile(gif, NULL);
    fox_image_free(img_p);
    return TRUE;

ERROR_END:
    EGifCloseFile(gif, NULL);
    fox_image_free(img_p);
    return FALSE;
}
static int save_image_gif(Value *vret, Value *v, RefNode *node)
{
    Value img = v[1];
    RefImage *image = Value_vp(img);
    Value tmp;

    if (image->bands == BAND_RGB || image->bands == BAND_RGBA) {
        if (!call_quantize(&tmp, img, image->bands)) {
            return FALSE;
        }
        image = Value_vp(tmp);
    } else {
        tmp = VALUE_NULL;
    }

    if (!save_gif_sub(image, v[2])) {
        fs->unref(tmp);
        return FALSE;
    }
    fs->unref(tmp);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "load_image_jpeg", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, load_image_jpeg, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);

    n = fs->define_identifier(m, m, "load_image_gif", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, load_image_gif, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);

    n = fs->define_identifier(m, m, "load_image_png", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, load_image_png, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);

    n = fs->define_identifier(m, m, "save_image_jpeg", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, save_image_jpeg, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_map);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);

    n = fs->define_identifier(m, m, "save_image_gif", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, save_image_gif, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_map);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);

    n = fs->define_identifier(m, m, "save_image_png", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, save_image_png, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_map);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_webimage = m;

    mod_image = fs->get_module_by_name("image", -1, TRUE, FALSE);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    if (buf == NULL) {
        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\nlibpng\t" PNG_LIBPNG_VER_STRING "\nlibjpeg\t%d\ngiflib\t%d.%d.%d\n",
                JPEG_LIB_VERSION, GIFLIB_MAJOR, GIFLIB_MINOR, GIFLIB_RELEASE);
    }
    return buf;
}
