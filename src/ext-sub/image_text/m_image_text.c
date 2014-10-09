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

static const FoxStatic *fs;
static FoxGlobal *fg;

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

static int get_file_size(int *pret, Value *reader)
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
		fs->throw_errorf(fs->mod_lang, "TypeError", "Int required but %n(return of size)", ret_type);
		return FALSE;
	}
	ret = fs->Value_int(fg->stk_top[-1], NULL);
	fs->Value_pop();

	if (ret > INT32_MAX) {
		fs->throw_errorf(mod_ft, "FTError", "File size is too large");
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

static int ft_open_face_stream(Ref *r, int font_index)
{
	FT_Face face;
	FT_Stream stream;
	FT_Open_Args args;
	int size;
	FT_Error ret;

	if (!get_file_size(&size, &r->v[FONT_INDEX_SRC])) {
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

static int font_new(Value *vret, Value *v, RefNode *node)
{
	FT_Error ret;
	FT_Face face;
	int font_index = fs->Value_int(v[2], NULL);
	float font_size = fs->Value_float(v[3]);
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
	face = Value_ptr(r->v[FONT_INDEX_FACE]);
	ret = FT_Set_Char_Size(face, 0, (int)(font_size * 64.0), 0, 0);
	if (ret != 0) {
		fs->throw_errorf(mod_ft, "FTError", "%s", ft_err_to_string(ret));
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
static int font_attr(Value *vret, Value *v, RefNode *node)
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
	Ref *r = Value_ref(*v);
	FT_Face face = Value_ptr(r->v[FONT_INDEX_FACE]);

	*vret = fs->cstr_Value(fs->cls_str, face->family_name, -1);

	return TRUE;
}
static int font_to_str(Value *vret, Value *v, RefNode *node)
{
	Ref *r = Value_ref(*v);
	FT_Face face = Value_ptr(r->v[FONT_INDEX_FACE]);
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

static uint32_t ft_get_char_variant(Value *a_face, int a_face_size, int prev, int ch)
{
	int i;
	for (i = 0; i < a_face_size; i++) {
		Ref *r_face = Value_ref(a_face[i]);
		FT_Face face = Value_ptr(r_face->v[FONT_INDEX_FACE]);
		int index = FT_Face_GetCharVariantIndex(face, prev, ch);
		if (index != 0) {
			return (i << 24) || index;
		}
	}
	return 0;
}
static uint32_t ft_get_char_index(Value *a_face, int a_face_size, int ch)
{
	int i;
	for (i = 0; i < a_face_size; i++) {
		Ref *r_face = Value_ref(a_face[i]);
		FT_Face face = Value_ptr(r_face->v[FONT_INDEX_FACE]);
		int index = FT_Get_Char_Index(face, ch);
		if (index != 0) {
			return (i << 24) || index;
		}
	}
	return 0;
}
/*static*/ uint32_t *get_glyph_indexes(Value *a_face, int a_face_size, Str src)
{
	uint32_t *idx = malloc(sizeof(uint32_t) * (src.size + 1));
	const char *p = src.p;
	const char *end = p + src.size;
	int i = 0, prev = 0;

	while (p < end) {
		int ch = fs->utf8_codepoint_at(p);
		if (prev != 0) {
			int glyph_index = ft_get_char_variant(a_face, a_face_size, prev, ch);
			if (glyph_index != 0) {
				idx[i++] = glyph_index;
				prev = 0;
			} else {
				idx[i++] = ft_get_char_index(a_face, a_face_size, prev);
				prev = ch;
			}
		} else {
			prev = ch;
		}
		fs->utf8_next(&p, end);
	}
	if (prev != 0) {
		idx[i++] = ft_get_char_index(a_face, a_face_size, prev);
	}
	idx[i] = 0;

	return idx;
}
static int ft_draw_text(Value *vret, Value *v, RefNode *node)
{
	RefImage *img = Value_vp(v[1]);
	int argc = fg->stk_top - v;
	Value *a_face = NULL;
	int a_face_size = -1;
//	uint32_t face_color = COLOR_A_MASK;  // 黒
//	uint32_t back_color = 0;             // 透明

//	int left = (int)(fs->Value_float(&v[2]) * 64.0);
//	int top  = (int)(fs->Value_float(&v[3]) * 64.0);
	int i;

	if (img->data == NULL) {
		fs->throw_errorf(mod_image, "ImageError", "Image is already closed");
		return FALSE;
	}

	for (i = 2; i < argc; i++) {
		const RefNode *type = fs->Value_type(v[i]);
		if (type == fs->cls_str) {
			if (a_face == NULL) {
				fs->throw_errorf(fs->mod_lang, "ValueError", "FontFace needed");
				return FALSE;
			}
			if (a_face_size == -1) {
				fs->throw_errorf(fs->mod_lang, "ValueError", "FontSize needed");
				return FALSE;
			}
		} else if (type == cls_face) {
			a_face = &v[i];
			a_face_size = 1;
		} else if (type == fs->cls_list) {
			RefArray *ra = Value_vp(v[i]);
			a_face = ra->p;
			a_face_size = ra->size;
		} else {
			fs->throw_errorf(fs->mod_lang, "TypeError", "Str, FontFace, Array or Color required but %%n (argument #%d)", type, i);
			return FALSE;
		}
	}

	return TRUE;
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
	fs->define_native_func_a(n, font_new, 3, 3, cls, NULL, fs->cls_int, fs->cls_float);

	n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, font_close, 0, 0, NULL);
	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, font_to_str, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "has_horizontal", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, font_attr, 0, 0, (void*) FT_FACE_FLAG_HORIZONTAL);
	n = fs->define_identifier(m, cls, "has_vertical", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, font_attr, 0, 0, (void*) FT_FACE_FLAG_VERTICAL);
	n = fs->define_identifier(m, cls, "has_kerning", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, font_attr, 0, 0, (void*) FT_FACE_FLAG_KERNING);
	n = fs->define_identifier(m, cls, "is_fixed_width", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, font_attr, 0, 0, (void*) FT_FACE_FLAG_FIXED_WIDTH);

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
