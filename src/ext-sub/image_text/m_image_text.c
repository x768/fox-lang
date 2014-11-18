#define DEFINE_GLOBALS
#include "fox_io.h"
#include "m_image.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s )  { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, 0 } };

static const struct
{
    int          err_code;
    const char*  err_msg;
} ft_errors[] =
#include FT_ERRORS_H

enum {
    FONT_INDEX_FACE,
    FONT_INDEX_SRC,
    FONT_INDEX_NUM,
};

#define Value_ftface(val) ((FT_Face)(Value_ptr(Value_ref(val)->v[FONT_INDEX_FACE])))
#define FLOAT_TO_26_6(f) ((long)((f) * 64.0))

static const FoxStatic *fs;
static FoxGlobal *fg;

static const ImageStatic *ist;

static RefNode *mod_ft;
static RefNode *mod_image;
static const RefNode *cls_fileio;
static RefNode *cls_face;
static RefNode *cls_color;

static FT_Library ft_lib;
static int ft_initialized;

static const char *ft_err_to_string(FT_Error err)
{
    int i;
    for (i = 0; i < (int)(sizeof(ft_errors) / sizeof(ft_errors[0])); i++) {
        if (ft_errors[i].err_code == err) {
            return ft_errors[i].err_msg;
        }
    }
    return "?";
}

static int get_file_size(int *pret, Value reader)
{
    const RefNode *ret_type;
    int64_t ret;

    fs->Value_push("v", reader);
    if (!fs->call_property(fs->intern("size", -1))) {
        return FALSE;
    }
    // 戻り値を除去
    ret_type = fs->Value_type(fg->stk_top[-1]);
    if (ret_type != fs->cls_int) {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Int required but %n (%n#size)", ret_type, fs->Value_type(reader));
        return FALSE;
    }
    ret = fs->Value_int64(fg->stk_top[-1], NULL);
    fs->Value_pop();

    if (ret > INT32_MAX) {
        fs->throw_errorf(mod_ft, "FTError", "File size is too large");
        return FALSE;
    }
    *pret = ret;

    return TRUE;
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

static unsigned long ft_stream_io_func(
    FT_Stream stream, unsigned long offset, unsigned char *buf, unsigned long count)
{
    if (count > 0) {
        Ref *r = stream->descriptor.pointer;
        Value v = r->v[FONT_INDEX_SRC];
        int sz = count;

        if (!fs->stream_seek_sub(v, offset)) {
            return 0;
        }
        if (!fs->stream_read_data(v, NULL, (char*)buf, &sz, FALSE, FALSE)) {
            return 0;
        }
        return sz;
    }
    return 0;
}

static void ft_stream_close_func(FT_Stream stream)
{
    free(stream);
}

static int ft_open_face_stream(Ref *r, int font_index)
{
    FT_Face face;
    FT_Stream stream;
    FT_Open_Args args;
    int size;
    FT_Error ret;

    if (!get_file_size(&size, r->v[FONT_INDEX_SRC])) {
        return FALSE;
    }
    stream = (FT_Stream)malloc(sizeof(FT_StreamRec));
    memset(stream, 0, sizeof(FT_StreamRec));
    stream->base = 0;
    stream->size = size;
    stream->pos = 0;
    stream->descriptor.pointer = r;
    stream->pathname.pointer = NULL;
    stream->read = ft_stream_io_func;
    stream->close = ft_stream_close_func;

    memset(&args, 0, sizeof(args));
    args.flags = FT_OPEN_STREAM;
    args.stream = stream;
    args.driver = 0;
    args.num_params = 0;
    args.params = NULL;

    ret = FT_Open_Face(ft_lib, &args, font_index, &face);
    if (ret != 0) {
        if (fg->error == VALUE_NULL) {
            fs->throw_errorf(mod_ft, "FTError", "%s", ft_err_to_string(ret));
        }
        return FALSE;
    }
    r->v[FONT_INDEX_FACE] = ptr_Value(face);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int StrBuf_add_int32(StrBuf *s, uint32_t i)
{
    return fs->StrBuf_add(s, (const char*)&i, sizeof(i));
}
static const uint32_t *StrBuf_get_int32(StrBuf *s)
{
    return (const uint32_t*)s->p;
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

///////////////////////////////////////////////////////////////////////////////////////////

static int font_new(Value *vret, Value *v, RefNode *node)
{
    int font_index = fs->Value_int64(v[2], NULL);
    Ref *r = fs->ref_new(cls_face);
    *vret = vp_Value(r);

    if (!ft_initialized) {
        FT_Init_FreeType(&ft_lib);
        ft_initialized = TRUE;
    }

    if (!fs->value_to_streamio(&r->v[FONT_INDEX_SRC], v[1], FALSE, 0)) {
        return FALSE;
    }

    if (!ft_open_face_stream(r, font_index)) {
        return FALSE;
    }
    return TRUE;
}
static int font_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    FT_Face face = Value_ptr(r->v[FONT_INDEX_FACE]);

    if (face != NULL) {
        FT_Done_Face(face);
        r->v[FONT_INDEX_FACE] = VALUE_NULL;
    }

    return TRUE;
}
static int font_get_attr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    FT_Face face = Value_ptr(r->v[FONT_INDEX_FACE]);
    int mask = FUNC_INT(node);

    if ((face->face_flags & mask) != 0) {
        *vret = VALUE_TRUE;
    } else {
        *vret = VALUE_FALSE;
    }

    return TRUE;
}
static int font_family_name(Value *vret, Value *v, RefNode *node)
{
    FT_Face face = Value_ftface(*v);
    *vret = fs->cstr_Value(fs->cls_str, face->family_name, -1);
    return TRUE;
}
static int font_to_str(Value *vret, Value *v, RefNode *node)
{
    FT_Face face = Value_ftface(*v);
    *vret = fs->printf_Value("FontFace(%s)", face->family_name);
    return TRUE;
}
static int ft_quit(Value *vret, Value *v, RefNode *node)
{
    if (ft_initialized) {
        FT_Done_FreeType(ft_lib);
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t ft_get_char_index(FT_Face *a_face, int ch)
{
    int i;
    for (i = 0; a_face[i] != NULL; i++) {
        int index = FT_Get_Char_Index(a_face[i], ch);
        if (index != 0) {
            return (i << 24) | index;
        }
    }
    return 0;
}
static void get_glyph_indexes(StrBuf *indexes, const char *p, const char *end, FT_Face *a_face)
{
    int prev = 0;

    while (p < end) {
        int ch = fs->utf8_codepoint_at(p);
        if (prev != 0) {
            int face_idx = prev >> 24;
            int variant_index = FT_Face_GetCharVariantIndex(a_face[face_idx], prev & 0xFFffFF, ch);
            if (variant_index != 0) {
                StrBuf_add_int32(indexes, variant_index | face_idx << 24);
                prev = 0;
            } else {
                int glyph_index = ft_get_char_index(a_face, prev);
                if (glyph_index != 0) {
                    StrBuf_add_int32(indexes, glyph_index);
                }
                prev = ch;
            }
        } else {
            prev = ch;
        }
        fs->utf8_next(&p, end);
    }
    if (prev != 0) {
        int glyph_index = ft_get_char_index(a_face, prev);
        if (glyph_index != 0) {
            StrBuf_add_int32(indexes, glyph_index);
        }
    }
    StrBuf_add_int32(indexes, 0);
}

static int get_char_positions(StrBuf *offsets, const uint32_t *indexes, FT_Face *a_face, long face_size, long *pcx)
{
    int cx = *pcx;
    int prev_glyph = 0;
    int i = 0;
    FT_Face prev_face = NULL;

    for (i = 0; a_face[i] != NULL; i++) {
        FT_Error ret = FT_Set_Char_Size(a_face[i], 0, face_size, 96, 96);
        if (ret != 0) {
            fs->throw_errorf(mod_ft, "FTError", "%s", ft_err_to_string(ret));
            return FALSE;
        }
    }

    for (i = 0; indexes[i] != 0; i++) {
        int idx = indexes[i];
        FT_Face face = a_face[idx >> 24];
        int glyph_index = idx & 0xFFffFF;

        if (face == prev_face) {
            int use_kerning = FT_HAS_KERNING(face);

            if (use_kerning && prev_glyph != 0 && glyph_index != 0) {
                FT_Vector delta;
                FT_Get_Kerning(face, prev_glyph, glyph_index, FT_KERNING_DEFAULT, &delta);
                cx += delta.x;
            }
        }
        StrBuf_add_int32(offsets, cx);

        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP) != 0) {
            fs->throw_errorf(mod_ft, "FTError", "FT_Load_Glyph failed");
            return FALSE;
        }
        {
            FT_GlyphSlot slot = face->glyph;
            cx += slot->advance.x;
            prev_glyph = glyph_index;
            prev_face = face;
        }
    }
    *pcx = cx;
    return TRUE;
}
int render_text(RefImage *img_dst, RefImage *img_src, CopyParam *cp, const uint32_t *indexes, const uint32_t *offsets, long offsety, FT_Face *a_face)
{
    int i;

    for (i = 0; indexes[i] != 0; i++) {
        int idx = indexes[i];
        FT_Face face = a_face[idx >> 24];
        int glyph_index = idx & 0xFFffFF;

        FT_GlyphSlot slot;
        FT_Bitmap *bm;

        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP) != 0) {
            fs->throw_errorf(mod_ft, "FTError", "FT_Load_Glyph failed");
            return FALSE;
        }
        slot = face->glyph;
        if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0) {
            fs->throw_errorf(mod_ft, "FTError", "FT_Render_Glyph failed");
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
                }
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
    FT_Face *a_face = NULL;

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
            if (a_face == NULL) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "FontFace needed");
                goto ERROR_END;
            }
            if (a_face_size == -1) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "FontSize needed");
                goto ERROR_END;
            }
            indexes.size = 0;
            offsets.size = 0;
            get_glyph_indexes(&indexes, rs->c, rs->c + rs->size, a_face);
            if (!get_char_positions(&offsets, StrBuf_get_int32(&indexes), a_face, a_face_size, &cx)) {
                goto ERROR_END;
            }
            if (!render_text(img, &src, &cp, StrBuf_get_int32(&indexes), StrBuf_get_int32(&offsets), cy, a_face)) {
                goto ERROR_END;
            }
        } else if (type == fs->cls_list) {
            int j;
            RefArray *ra = Value_vp(v[i]);

            a_face = realloc(a_face, sizeof(FT_Face*) * (ra->size + 1));
            for (j = 0; j < ra->size; j++) {
                RefNode *a_type = fs->Value_type(ra->p[j]);
                if (a_type != cls_face) {
                    fs->throw_errorf(fs->mod_lang, "TypeError", "List(FontFace) needed but %n", a_type);
                    goto ERROR_END;
                }
                a_face[j] = Value_ftface(ra->p[j]);
            }
            a_face[j] = NULL;
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
    free(a_face);
    free(src.palette);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier_p(m, m, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ft_quit, 0, 0, NULL);

    n = fs->define_identifier(m, m, "draw_text", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ft_draw_text, 3, -1, NULL, NULL, fs->cls_float, fs->cls_float);
    fs->add_unresolved_args(m, mod_image, "Image", n, 0);
}

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_face = fs->define_identifier(m, m, "FontFace", NODE_CLASS, 0);


    cls = cls_face;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, font_new, 2, 2, cls, NULL, fs->cls_int);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, font_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, font_to_str, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "has_horizontal", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, font_get_attr, 0, 0, (void*) FT_FACE_FLAG_HORIZONTAL);
    n = fs->define_identifier(m, cls, "has_vertical", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, font_get_attr, 0, 0, (void*) FT_FACE_FLAG_VERTICAL);
    n = fs->define_identifier(m, cls, "has_kerning", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, font_get_attr, 0, 0, (void*) FT_FACE_FLAG_KERNING);
    n = fs->define_identifier(m, cls, "is_fixed_width", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, font_get_attr, 0, 0, (void*) FT_FACE_FLAG_FIXED_WIDTH);

    n = fs->define_identifier(m, cls, "family_name", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, font_family_name, 0, 0, NULL);

    cls->u.c.n_memb = FONT_INDEX_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "FTError", NODE_CLASS, 0);
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
