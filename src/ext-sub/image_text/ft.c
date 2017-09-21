#include "image_text.h"


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

static FT_Library ft_lib;
static int ft_initialized;


static int StrBuf_add_int32(StrBuf *s, uint32_t i)
{
    return fs->StrBuf_add(s, (const char*)&i, sizeof(i));
}

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

void ft_initialize()
{
    if (!ft_initialized) {
        FT_Init_FreeType(&ft_lib);
        ft_initialized = TRUE;
    }
}

static int get_reader_file_size(int *pret, Value reader)
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
        fs->throw_errorf(mod_ft, "FontError", "File size is too large");
        return FALSE;
    }
    *pret = ret;

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

int ft_open_face_stream(Ref *r, int font_index)
{
    FT_Face face;
    FT_Stream stream;
    FT_Open_Args args;
    int size;
    FT_Error ret;

    if (!get_reader_file_size(&size, r->v[FONT_INDEX_SRC])) {
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
            fs->throw_errorf(mod_ft, "FontError", "%s", ft_err_to_string(ret));
        }
        return FALSE;
    }
    r->v[FONT_INDEX_FACE] = ptr_Value(face);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

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
void get_glyph_indexes(StrBuf *indexes, const char *p, const char *end, FT_Face *a_face)
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

int get_char_positions(StrBuf *offsets, const uint32_t *indexes, FT_Face *a_face, long face_size, long *pcx)
{
    int cx = *pcx;
    int prev_glyph = 0;
    int i = 0;
    FT_Face prev_face = NULL;

    for (i = 0; a_face[i] != NULL; i++) {
        FT_Face f = a_face[i];
        int size = face_size;
        if (f->available_sizes != NULL) {
            int j;
            int min_diff = 65536;
            for (j = 0; j < f->num_fixed_sizes; j++) {
                long size_diff = face_size - f->available_sizes[j].size;
                if (size_diff < 0) {
                    size_diff = -size_diff;
                }
                if (min_diff > size_diff) {
                    min_diff = size_diff;
                    size = f->available_sizes[j].size;
                }
            }
        }
        FT_Error ret = FT_Set_Char_Size(f, 0, size, 0, 0);
        if (ret != 0) {
            fs->throw_errorf(mod_ft, "FontError", "%s", ft_err_to_string(ret));
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
            fs->throw_errorf(mod_ft, "FontError", "FT_Load_Glyph failed");
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////

int ftfont_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    FT_Face face = Value_ptr(r->v[FONT_INDEX_FACE]);

    if (face != NULL) {
        FT_Done_Face(face);
        r->v[FONT_INDEX_FACE] = VALUE_NULL;
    }

    return TRUE;
}
int ftfont_get_attr(Value *vret, Value *v, RefNode *node)
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
int ftfont_family_name(Value *vret, Value *v, RefNode *node)
{
    FT_Face face = Value_ftface(*v);
    *vret = fs->cstr_Value(fs->cls_str, face->family_name, -1);
    return TRUE;
}
int ftfont_to_str(Value *vret, Value *v, RefNode *node)
{
    FT_Face face = Value_ftface(*v);
    *vret = fs->printf_Value("FontFace(%s)", face->family_name);
    return TRUE;
}
int ft_quit(Value *vret, Value *v, RefNode *node)
{
    if (ft_initialized) {
        FT_Done_FreeType(ft_lib);
    }
    return TRUE;
}
