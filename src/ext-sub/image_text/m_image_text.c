#define DEFINE_GLOBALS
#include "image_text.h"


static const uint32_t *StrBuf_get_int32(StrBuf *s)
{
    return (const uint32_t*)s->p;
}

static int check_type_and_throw_error(Value v, RefNode *type, const char *param)
{
    RefNode *v_type = fs->Value_type(v);
    if (v_type != type) {
        fs->throw_errorf(fs->mod_lang, "TypeError", "%n required but %n (%s)", type, v_type, param);
        return FALSE;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

/*
void Image_blt(RefImage *img, int x, int y, int w, const uint8_t *buf, int bits)
{
    if (y < 0 || y >= img->height) {
        return;
    }
    if (x < 0) {
        buf -= x;
        w += x;
        x = 0;
    }
    if (w > img->width - x) {
        w = img->width - x;
    }
    if (w > 0) {
        uint8_t *p = &img->data[y * img->width + x];
        if (bits == 8) {
            memcpy(p, buf, w);
        } else {
            int i;
            for (i = 0; i < w; i++) {
                p[i] = (buf[i / 8] & (1 << (7 - i % 8))) ? 255 : 0;
            }
        }
    }
}
*/
static uint32_t *make_palette(uint32_t color)
{
    int i;
    int alpha = (color & COLOR_A_MASK) >> COLOR_A_SHIFT;
    uint32_t c_rgb = color & ~COLOR_A_MASK;
    uint32_t *p = malloc(sizeof(uint32_t) * PALETTE_NUM);

    p[0] = 0;
    for (i = 1; i < PALETTE_NUM; i++) {
        int a = alpha * i / 255;
        p[i] = (a << COLOR_A_SHIFT) | c_rgb;
    }
    return p;
}

static int render_text(RefImage *img_dst, RefImage *img_src, CopyParam *cp, const uint32_t *indexes, const uint32_t *offsets, long offsety, FT_Face *a_face)
{
    int i;

    for (i = 0; indexes[i] != 0; i++) {
        int idx = indexes[i];
        FT_Face face = a_face[idx >> 24];
        int glyph_index = idx & 0xFFffFF;

        FT_GlyphSlot slot;
        FT_Bitmap *bm;

        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP) != 0) {
            fs->throw_errorf(mod_ft, "FontError", "FT_Load_Glyph failed");
            return FALSE;
        }
        slot = face->glyph;
        if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
            fs->throw_errorf(mod_ft, "FontError", "FT_Render_Glyph failed");
            return FALSE;
        }
        bm = &slot->bitmap;

        {
            int cx = (offsets[i] >> 6) + slot->bitmap_left;
            int cy = (offsety >> 6) - slot->bitmap_top;
            int top = 0;
            int bottom = bm->rows;
            int left = 0;
            int right = bm->width;

            if (cx < 0) {
                left = -cx;
            }
            if (cy < 0) {
                top = -cy;
            }
            if (right > img_dst->width - cx) {
                right = img_dst->width - cx;
            }
            if (bottom > img_dst->height - cy) {
                bottom = img_dst->height - cy;
            }
            if (left < right) {
                int y;

                if (bm->pixel_mode == FT_PIXEL_MODE_MONO) {
                    int width = right - left;
                    uint8_t *src = malloc(width);
                    for (y = top; y < bottom; y++) {
                        uint8_t *dst = img_dst->data + img_dst->pitch * (top + y + cy) + cp->dc * (left + cx);
                        const uint8_t *src_bm = bm->buffer + bm->pitch * (top + y);
                        int x;
                        int bit = 7 - (left % 8);
                        src_bm += left / 8;

                        for (x = 0; x < width; x++) {
                            if ((*src_bm & (1 << bit)) != 0) {
                                src[x] = 255;
                            } else {
                                src[x] = 0;
                            }
                            bit--;
                            if (bit < 0) {
                                bit = 7;
                                src_bm++;
                            }
                        }
                        ist->copy_image_line(dst, img_dst, src, img_src, width, cp, TRUE);
                    }
                    free(src);
                } else {
                    img_src->data = bm->buffer;
                    img_src->width = bm->width;
                    img_src->height = bm->rows;
                    img_src->pitch = bm->pitch;

                    for (y = top; y < bottom; y++) {
                        uint8_t *dst = img_dst->data + img_dst->pitch * (top + y + cy) + cp->dc * (left + cx);
                        const uint8_t *src = bm->buffer + bm->pitch * (top + y) + left;
                        ist->copy_image_line(dst, img_dst, src, img_src, right - left, cp, TRUE);
                    }
                }
            }
        }
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int ftfont_new(Value *vret, Value *v, RefNode *node)
{
    int font_index = fs->Value_int64(v[2], NULL);
    Ref *r = fs->ref_new(cls_ftface);
    *vret = vp_Value(r);

    ft_initialize();

    if (!fs->value_to_streamio(&r->v[FONT_INDEX_SRC], v[1], FALSE, 0, FALSE)) {
        return FALSE;
    }

    if (!ft_open_face_stream(r, font_index)) {
        return FALSE;
    }
    return TRUE;
}

static int ft_draw_text(Value *vret, Value *v, RefNode *node)
{
    RefImage *img = Value_vp(v[1]);
    RefImage src;
    CopyParam cp;
    int argc = fg->stk_top - v;
    uint32_t color = COLOR_A_MASK;  // 黒
    long a_face_size = -1;
    StrBuf indexes;
    StrBuf offsets;
    FT_Face *a_ftface = NULL;

    int i;
    int ret = FALSE;
    long cx = FLOAT_TO_26_6(Value_float2(v[2]));
    long cy = FLOAT_TO_26_6(Value_float2(v[3]));

    fs->StrBuf_init(&indexes, 64);
    fs->StrBuf_init(&offsets, 64);

    if (img->data == NULL) {
        fs->throw_errorf(mod_image, "ImageError", "Image is already closed");
        goto ERROR_END;
    }
    src.bands = BAND_P;
    src.palette = make_palette(color);
    ist->CopyParam_init(&cp, img, &src, TRUE, FALSE);

    for (i = 4; i < argc; i++) {
        const RefNode *type = fs->Value_type(v[i]);
        if (type == fs->cls_str) {
            RefStr *rs = Value_vp(v[i]);
            if (a_ftface == NULL) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "FontFace needed");
                goto ERROR_END;
            }
            if (a_face_size == -1) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "FontSize needed");
                goto ERROR_END;
            }
            indexes.size = 0;
            offsets.size = 0;
            get_glyph_indexes(&indexes, rs->c, rs->c + rs->size, a_ftface);
            if (!get_char_positions(&offsets, StrBuf_get_int32(&indexes), a_ftface, a_face_size, &cx)) {
                goto ERROR_END;
            }
            if (!render_text(img, &src, &cp, StrBuf_get_int32(&indexes), StrBuf_get_int32(&offsets), cy, a_ftface)) {
                goto ERROR_END;
            }
        } else if (type == fs->cls_list) {
            int j;
            RefArray *ra = Value_vp(v[i]);

            a_ftface = realloc(a_ftface, sizeof(FT_Face*) * (ra->size + 1));
            for (j = 0; j < ra->size; j++) {
                RefNode *a_type = fs->Value_type(ra->p[j]);
                if (a_type != cls_ftface) {
                    fs->throw_errorf(fs->mod_lang, "TypeError", "List(FontFace) needed but %n", a_type);
                    goto ERROR_END;
                }
                a_ftface[j] = Value_ftface(ra->p[j]);
            }
            a_ftface[j] = NULL;
        } else if (type == fs->cls_map) {
            HashValueEntry *ve;
            RefMap *rm = Value_vp(v[i]);

            if ((ve = fs->refmap_get_strkey(rm, "color", -1)) != NULL) {
                if (!check_type_and_throw_error(ve->val, cls_color, "color")) {
                    goto ERROR_END;
                }
                free(src.palette);
                src.palette = make_palette(Value_integral(ve->val));
                ist->CopyParam_init(&cp, img, &src, TRUE, FALSE);
            }
            if ((ve = fs->refmap_get_strkey(rm, "size", -1)) != NULL) {
                if (!check_type_and_throw_error(ve->val, fs->cls_float, "size")) {
                    goto ERROR_END;
                }
                a_face_size = FLOAT_TO_26_6(Value_float2(ve->val));
            }
        } else {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Str, List or Map required but %n (argument #%d)", type, i);
            goto ERROR_END;
        }
    }
    ret = TRUE;

ERROR_END:
    StrBuf_close(&indexes);
    StrBuf_close(&offsets);
    free(a_ftface);
    free(src.palette);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier_p(m, m, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ft_quit, 0, 0, NULL);

    n = fs->define_identifier(m, m, "draw_text", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ft_draw_text, 3, -1, NULL, NULL, fs->cls_float, fs->cls_float);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);
}

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_ftface = fs->define_identifier(m, m, "FontFace", NODE_CLASS, 0);


    cls = cls_ftface;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, ftfont_new, 2, 2, cls, NULL, fs->cls_int);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ftfont_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ftfont_to_str, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "has_horizontal", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ftfont_get_attr, 0, 0, (void*) FT_FACE_FLAG_HORIZONTAL);
    n = fs->define_identifier(m, cls, "has_vertical", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ftfont_get_attr, 0, 0, (void*) FT_FACE_FLAG_VERTICAL);
    n = fs->define_identifier(m, cls, "has_kerning", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ftfont_get_attr, 0, 0, (void*) FT_FACE_FLAG_KERNING);
    n = fs->define_identifier(m, cls, "is_fixed_width", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ftfont_get_attr, 0, 0, (void*) FT_FACE_FLAG_FIXED_WIDTH);

    n = fs->define_identifier(m, cls, "family_name", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ftfont_family_name, 0, 0, NULL);

    cls->u.c.n_memb = FONT_INDEX_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "FontError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_ft = m;

    mod_image = fs->get_module_by_name("image", -1, TRUE, FALSE);
    fs->add_unresolved_ptr(m, mod_image, "Color", &cls_color);
    cls_fileio = fs->Hash_get(&fs->mod_io->u.m.h, "FileIO", -1);
    ist = mod_image->u.m.ext;

    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }

    if (buf == NULL) {
        FT_Library lib;
        FT_Int major, minor, patch;
        FT_Init_FreeType(&lib);
        FT_Library_Version(lib, &major, &minor, &patch);
        FT_Done_FreeType(lib);

        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\nFreeType\t%d.%d.%d\n", major, minor, patch);
    }
    return buf;
}
