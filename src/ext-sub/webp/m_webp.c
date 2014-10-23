#define DEFINE_GLOBALS
#include "m_image.h"
#include <stdio.h>
#include <stdlib.h>

#include <webp/decode.h>
#include <webp/encode.h>


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_webp;
static RefNode *mod_image;


static int get_map_bool(int *has_value, int *value, RefMap *v_map, const char *key)
{
    HashValueEntry *ret = fs->refmap_get_strkey(v_map, key, -1);

    if (ret != NULL) {
        RefNode *type = fs->Value_type(ret->val);
        if (type != fs->cls_bool) {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Bool required but %n", type);
            return FALSE;
        }
        *value = Value_bool(ret->val);
        *has_value = TRUE;
    } else {
        *has_value = FALSE;
    }
    return TRUE;
}
static int get_map_double(int *has_value, double *value, RefMap *v_map, const char *key, double lower, double upper)
{
    HashValueEntry *ret = fs->refmap_get_strkey(v_map, key, -1);

    if (ret != NULL) {
        RefNode *type = fs->Value_type(ret->val);
        double val;
        if (type != fs->cls_float) {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Float required but %n", type);
            return FALSE;
        }
        val = fs->Value_float(ret->val);
        if (val < lower) {
            val = lower;
        } else if (val > upper) {
            val = upper;
        }
        *value = val;
        *has_value = TRUE;
    } else {
        *has_value = FALSE;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static const char* const kStatusMessages[] = {
    "OK", "OUT_OF_MEMORY", "INVALID_PARAM", "BITSTREAM_ERROR",
    "UNSUPPORTED_FEATURE", "SUSPENDED", "USER_ABORT", "NOT_ENOUGH_DATA"
};

static int stream_read_all(StrBuf *sb, Value v)
{
    int size = BUFFER_SIZE;
    char *buf = malloc(BUFFER_SIZE);

    for (;;) {
        if (!fs->stream_read_data(v, NULL, buf, &size, FALSE, FALSE)) {
            return FALSE;
        }
        if (size <= 0) {
            break;
        }
        if (!fs->StrBuf_add(sb, buf, size)) {
            return FALSE;
        }
    }

    free(buf);
    return TRUE;
}

static int load_webp_sub(RefImage *image, Value r, int info_only)
{
    VP8StatusCode status;
    StrBuf sb;
    WebPBitstreamFeatures bf;
    int size;
    int failed;

    fs->StrBuf_init(&sb, 4096);
    if (!stream_read_all(&sb, r)) {
        return FALSE;
    }
    status = WebPGetFeatures((const uint8_t*)sb.p, sb.size, &bf);
    if (status != VP8_STATUS_OK) {
        goto ERROR_END;
    }
    if (bf.has_animation) {
        fs->throw_errorf(mod_webp, "WebPError", "Animated WebP file is not supported.");
        return FALSE;
    }
    if (bf.width > MAX_IMAGE_SIZE || bf.height > MAX_IMAGE_SIZE) {
        fs->throw_errorf(mod_image, "ImageError", "Image size too large (max:%d)", MAX_IMAGE_SIZE);
        return FALSE;
    }
    image->width = bf.width;
    image->height = bf.height;
    if (bf.has_alpha) {
        image->bands = BAND_RGBA;
        image->pitch = bf.width * 4;
        size = bf.width * bf.height * 4;
    } else {
        image->bands = BAND_RGB;
        image->pitch = bf.width * 3;
        size = bf.width * bf.height * 3;
    }

    if (info_only) {
        return TRUE;
    }
    image->data = malloc(size);
    if (bf.has_alpha) {
        failed = (WebPDecodeRGBAInto((const uint8_t*)sb.p, sb.size, image->data, size, image->pitch) == NULL);
    } else {
        failed = (WebPDecodeRGBInto((const uint8_t*)sb.p, sb.size, image->data, size, image->pitch) == NULL);
    }
    if (failed) {
        fs->throw_errorf(mod_webp, "WebPError", "Failed to load image");
        return FALSE;
    }

    return TRUE;

ERROR_END:
    fs->throw_errorf(mod_webp, "WebPError", "%s", kStatusMessages[status]);
    return FALSE;
}
static int load_image_webp(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(v[1]);
    int info_only = Value_bool(v[3]);

    if (!load_webp_sub(image, v[2], info_only)) {
        return FALSE;
    }
    return TRUE;
}

static int writer_callback(const uint8_t *data, size_t data_size, const WebPPicture *picture)
{
    Value *w = picture->custom_ptr;

    if (!fs->stream_write_data(*w, (const char*)data, data_size)) {
        return FALSE;
    }
    return TRUE;
}
static int save_webp_sub(RefImage *image, Value w, int lossless, int no_alpha, double quality, int alpha_quality)
{
    WebPConfig config;
    WebPPicture pic;
    int x, y;
    int status;

    WebPConfigInit(&config);
    config.lossless = lossless;
    config.quality = quality;
    if (alpha_quality >= 0) {
        config.alpha_quality = alpha_quality;
    }

    if (!WebPPictureInit(&pic)) {
        fs->throw_errorf(mod_webp, "WebPError", "WebPPictureInit failed");
        return FALSE;
    }
    pic.use_argb = TRUE;
    pic.width = image->width;
    pic.height = image->height;
    pic.argb_stride = image->width;
    if (!WebPPictureAlloc(&pic)) {
        fs->throw_errorf(mod_webp, "WebPError", "WebPPictureAlloc failed");
        return FALSE;
    }

    for (y = 0; y < image->height; y++) {
        const uint8_t *src = image->data + y * image->pitch;
        uint32_t *dst = pic.argb + y * pic.argb_stride;

        switch (image->bands) {
        case BAND_L:
            for (x = 0; x < pic.width; x++) {
                uint8_t c = src[x];
                dst[x] = c | (c << 8) | (c << 16) | 0xFF000000;
            }
            break;
        case BAND_P:
            if (no_alpha) {
                const uint32_t *pal = image->palette;
                for (x = 0; x < pic.width; x++) {
                    uint32_t c = pal[src[x]];
                    int r = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
                    int g = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
                    int b = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
                    dst[x] = b | (g << 8) | (r << 16) | 0xFF000000;
                }
            } else {
                const uint32_t *pal = image->palette;
                for (x = 0; x < pic.width; x++) {
                    uint32_t c = pal[src[x]];
                    int r = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
                    int g = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
                    int b = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
                    int a = (c & COLOR_A_MASK) >> COLOR_A_SHIFT;
                    dst[x] = b | (g << 8) | (r << 16) | (a << 24);
                }
            }
            break;
        case BAND_LA:
            if (no_alpha) {
                for (x = 0; x < pic.width; x++) {
                    uint8_t c = src[x * 2];
                    dst[x] = c | (c << 8) | (c << 16) | 0xFF000000;
                }
            } else {
                for (x = 0; x < pic.width; x++) {
                    uint8_t c = src[x * 2];
                    dst[x] = c | (c << 8) | (c << 16) | (src[x * 2 + 1] << 24);
                }
            }
            break;
        case BAND_RGB:
            for (x = 0; x < pic.width; x++) {
                dst[x] = src[x * 3 + 2] | (src[x * 3 + 1] << 8) | (src[x * 3 + 0] << 16) | 0xFF000000;
            }
            break;
        case BAND_RGBA:
            if (no_alpha) {
                for (x = 0; x < pic.width; x++) {
                    dst[x] = src[x * 4 + 2] | (src[x * 4 + 1] << 8) | (src[x * 4 + 0] << 16) | 0xFF000000;
                }
            } else {
                for (x = 0; x < pic.width; x++) {
                    dst[x] = src[x * 4 + 2] | (src[x * 4 + 1] << 8) | (src[x * 4 + 0] << 16) | (src[x * 4 + 3] << 24);
                }
            }
            break;
        }
    }
    pic.writer = writer_callback;
    pic.custom_ptr = &w;

    status = WebPEncode(&config, &pic);
    WebPPictureFree(&pic);
    if (!status) {
        if (fg->error == VALUE_NULL) {
            fs->throw_errorf(mod_webp, "WebPError", "WebPEncode failed");
        }
        return FALSE;
    }

    return TRUE;
}
static int save_image_webp(Value *vret, Value *v, RefNode *node)
{
    RefImage *image = Value_vp(v[1]);
    RefMap *rm_param = Value_vp(v[3]);
    int has_value;
    int lossless = TRUE;
    int no_alpha = FALSE;
    double quality = 75.0;
    double dquality;
    int a_quality = -1;
    double da_quality;

    if (!get_map_bool(&has_value, &lossless, rm_param, "lossless")) {
        return FALSE;
    }
    if (!get_map_bool(&has_value, &no_alpha, rm_param, "no_alpha")) {
        return FALSE;
    }
    if (!get_map_double(&has_value, &dquality, rm_param, "quality", 0.0, 1.0)) {
        return FALSE;
    }
    if (has_value) {
        quality = dquality * 100.0;
    }
    if (!get_map_double(&has_value, &da_quality, rm_param, "alpha_quality", 0.0, 1.0)) {
        return FALSE;
    }
    if (has_value) {
        a_quality = da_quality * 100.0;
    }
    if (!save_webp_sub(image, v[2], lossless, no_alpha, quality, a_quality)) {
        return FALSE;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "load_image_x_webp", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, load_image_webp, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_bool);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);

    n = fs->define_identifier(m, m, "save_image_x_webp", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, save_image_webp, 3, 3, NULL, NULL, fs->cls_streamio, fs->cls_map);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);
}

static void define_class(RefNode *m)
{
    RefNode *cls;

    cls = fs->define_identifier(m, m, "WebPError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_webp = m;

    mod_image = fs->get_module_by_name("image", -1, TRUE, FALSE);
    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    if (buf == NULL) {
        int webp_ver = WebPGetDecoderVersion();
        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\nlibwebp\t%d.%d.%d\n",
                (webp_ver >> 16) & 0xFF, (webp_ver >> 8) & 0xFF, webp_ver & 0xFF);
    }
    return buf;
}
