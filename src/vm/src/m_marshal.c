#include "fox_vm.h"
#include <string.h>
#include <errno.h>


enum {
	JS_EOS,
	JS_TOK_ERR,
	JS_RANGE_ERR,
	JS_INT,
	JS_BIGINT,
	JS_FLOAT,
	JS_STR,
	JS_LB,
	JS_RB,
	JS_LC,
	JS_RC,
	JS_COLON,
	JS_COMMA,
	JS_NULL,
	JS_TRUE,
	JS_FALSE,
};

typedef struct {
	char *p;
	const char *end;
	int type;

	mp_int mp_val;
	int32_t int_val;
	double real_val;
	Str str_val;
} JSTok;

typedef struct {
	RefHeader rh;

	Mem mem;
	Hash hash;
} RefLoopValidator;


static RefNode *mod_marshal;
static RefNode *cls_loopvalidator;

#define FOX_OBJECT_MAGIC_SIZE 8
static const char *FOX_OBJECT_MAGIC = "\x89\x66\x6f\x78\r\n\0\0";

static void JSTok_init(JSTok *tk, char *src, int size)
{
	tk->p = src;
	tk->end = src + size;
}

static void JSTok_parse_digit(JSTok *tk)
{
	int is_float = FALSE;
	const char *top = tk->p;

	if (*tk->p == '-') {
		tk->p++;
	}
	while (isdigit(*tk->p)) {
		tk->p++;
	}
	if (*tk->p == '.') {
		is_float = TRUE;
		tk->p++;
		if (!isdigit(*tk->p)) {
			goto ERROR_END;
		}
		while (isdigit(*tk->p)) {
			tk->p++;
		}
	}
	if (*tk->p == 'e' || *tk->p == 'E') {
		is_float = TRUE;
		tk->p++;
		if (*tk->p == '+' || *tk->p == '-') {
			tk->p++;
		}
		if (!isdigit(*tk->p)) {
			goto ERROR_END;
		}
		while (isdigit(*tk->p)) {
			tk->p++;
		}
	}

	if (is_float) {
		// 小数
		char c = *tk->p;
		*tk->p = '\0';
		errno = 0;
		tk->real_val = strtod(top, NULL);
		*tk->p = c;
		if (errno != 0) {
			tk->type = JS_RANGE_ERR;
			return;
		}
		tk->type = JS_FLOAT;
	} else {
		// 整数
		char c = *tk->p;
		*tk->p = '\0';
		errno = 0;
		tk->int_val = strtol(top, NULL, 10);
		if (errno != 0 || tk->int_val == INT32_MIN) {
			mp_init(&tk->mp_val);
			mp_read_radix(&tk->mp_val, (unsigned char*)top, 10);
			tk->type = JS_BIGINT;
		} else {
			tk->type = JS_INT;
		}
		*tk->p = c;
	}
	return;

ERROR_END:
	throw_errorf(mod_marshal, "JSONError", "Invalid number format");
	tk->type = JS_TOK_ERR;
}
static void JSTok_parse_str(JSTok *tk)
{
	const char *top = tk->p;
	char *dst = tk->p;
	int ch;
	tk->p++;

	for (;;) {
		if (tk->p >= tk->end) {
			throw_errorf(mod_marshal, "JSONError", "Unterminated string");
			tk->type = JS_TOK_ERR;
			return;
		}
		switch (*tk->p) {
		case '"':
			tk->p++;
			goto BREAK;
		case '\\':
			tk->p++;
			if (tk->p >= tk->end) {
				throw_errorf(mod_marshal, "JSONError", "Invalid string format");
				tk->type = JS_TOK_ERR;
				return;
			}
			ch = *tk->p++;

			/*
			 * \0        NUL
			 * \b        bell
			 * \f        formfeed (hex 0C)
			 * \n        linefeed (hex 0A)
			 * \r        carriage return (hex 0D)
			 * \t        tab (hex 09)
			 * \uXXXX    character with ucs2 code U+XXXX
			 */
			switch (ch) {
			case 'b':
				*dst++ = '\b';
				break;
			case 'f':
				*dst++ = '\f';
				break;
			case 'n':
				*dst++ = '\n';
				break;
			case 'r':
				*dst++ = '\r';
				break;
			case 't':
				*dst++ = '\t';
				break;
			case 'u': {
				int ch2 = parse_hex((const char**)&tk->p, tk->end, 4);
				if (ch2 < 0) {
					throw_errorf(mod_marshal, "JSONError", "Illigal hexadecimal format");
					tk->type = JS_TOK_ERR;
					return;
				} else if (ch2 < 0x80) {
					*dst++ = ch2;
				} else if (ch2 < 0x800) {
					*dst++ = 0xC0 | (ch2 >> 6);
					*dst++ = 0x80 | (ch2 & 0x3F);
				} else if (ch2 < 0xD800) {
					*dst++ = 0xE0 | (ch2 >> 12);
					*dst++ = 0x80 | ((ch2 >> 6) & 0x3F);
					*dst++ = 0x80 | (ch2 & 0x3F);
				} else if (ch2 < 0xDC00) {
					// サロゲート
					int ch3;
					if (tk->p + 3 > tk->end || tk->p[0] != '\\' || tk->p[1] != 'u') {
						throw_errorf(mod_marshal, "JSONError", "Missing Low Surrogate");
						tk->type = JS_TOK_ERR;
						return;
					}
					tk->p += 2;
					ch3 = parse_hex((const char**)&tk->p, tk->end, 4);
					if (ch3 < 0) {
						throw_errorf(mod_marshal, "JSONError", "Illigal hexadecimal format");
						tk->type = JS_TOK_ERR;
						return;
					}
					if (ch3 < 0xDC00 || ch3 >= 0xE000) {
						throw_errorf(mod_marshal, "JSONError", "Missing Low Surrogate");
						tk->type = JS_TOK_ERR;
						return;
					}
					ch2 = (ch2 - 0xD800) * 0x400 + 0x10000;
					ch3 = ch2 | (ch3 - 0xDC00);

					*dst++ = 0xF0 | (ch3 >> 18);
					*dst++ = 0x80 | ((ch3 >> 12) & 0x3F);
					*dst++ = 0x80 | ((ch3 >> 6) & 0x3F);
					*dst++ = 0x80 | (ch3 & 0x3F);
				} else if (ch2 < 0xE000) {
					// 下位サロゲート
					throw_errorf(mod_marshal, "JSONError", "Illigal codepoint (Surrogate) %U", ch2);
					tk->type = JS_TOK_ERR;
					return;
				} else {
					*dst++ = 0xE0 | (ch2 >> 12);
					*dst++ = 0x80 | ((ch2 >> 6) & 0x3F);
					*dst++ = 0x80 | (ch2 & 0x3F);
				}
				break;
			}
			default:
				if (isalnum(ch)) {
					throw_errorf(mod_marshal, "JSONError", "Unknwon backslash sequence");
					tk->type = JS_TOK_ERR;
					return;
				} else if (*tk->p < ' ') {
					throw_errorf(mod_marshal, "JSONError", "Unknwon character");
					tk->type = JS_TOK_ERR;
					return;
				}
				*dst++ = *tk->p++;
				break;
			}
			break;
		default:
			if ((*tk->p & 0xFF) < ' ') {
				throw_errorf(mod_marshal, "JSONError", "Unknwon character %c", *tk->p);
				tk->type = JS_TOK_ERR;
				return;
			}
			*dst++ = *tk->p++;
			break;
		}
	}
BREAK:
	tk->str_val = Str_new(top, dst - top);
	tk->type = JS_STR;
}
static void throw_unexpected_token(JSTok *tk)
{
	const char *top = tk->p;
	while (tk->p < tk->end && isalnum(*tk->p)) {
		tk->p++;
	}
	*tk->p = '\0';

	throw_errorf(mod_marshal, "JSONError", "Unexpected token %q", top);
	tk->type = JS_TOK_ERR;
}
static void JSTok_next(JSTok *tk)
{
	while (tk->p < tk->end && isspace(*tk->p)) {
		tk->p++;
	}
	if (tk->p >= tk->end) {
		tk->type = JS_EOS;
		return;
	}

	switch (*tk->p) {
	case '[':
		tk->p++;
		tk->type = JS_LB;
		break;
	case ']':
		tk->p++;
		tk->type = JS_RB;
		break;
	case '{':
		tk->p++;
		tk->type = JS_LC;
		break;
	case '}':
		tk->p++;
		tk->type = JS_RC;
		break;
	case ':':
		tk->p++;
		tk->type = JS_COLON;
		break;
	case ',':
		tk->p++;
		tk->type = JS_COMMA;
		break;
	case '"':
		JSTok_parse_str(tk);
		break;
	case 'f':
		if (tk->p + 5 <= tk->end && memcmp(tk->p, "false", 5) == 0) {
			tk->type = JS_FALSE;
			tk->p += 5;
		} else {
			throw_unexpected_token(tk);
		}
		break;
	case 't':
		if (tk->p + 4 <= tk->end && memcmp(tk->p, "true", 4) == 0) {
			tk->type = JS_TRUE;
			tk->p += 4;
		} else {
			throw_unexpected_token(tk);
		}
		break;
	case 'n':
		if (tk->p + 4 <= tk->end && memcmp(tk->p, "null", 4) == 0) {
			tk->type = JS_NULL;
			tk->p += 4;
		} else {
			throw_unexpected_token(tk);
		}
		break;
	case '-':
		JSTok_parse_digit(tk);
		break;
	default:
		if (isdigit(*tk->p)) {
			JSTok_parse_digit(tk);
		} else {
			throw_errorf(mod_marshal, "JSONError", "Unexpected character %c", *tk->p);
			tk->type = JS_TOK_ERR;
		}
		break;
	}
}

static int json_add_indent(StrBuf *buf, int level)
{
	int i;

	if (!StrBuf_add_c(buf, '\n')) {
		return FALSE;
	}
	for (i = 0; i < level; i++) {
		if (!StrBuf_add_c(buf, ' ')) {
			return FALSE;
		}
	}
	return TRUE;
}
static int has_complex_value(RefArray *ra)
{
	int i;
	for (i = 0; i < ra->size; i++) {
		const RefNode *type = Value_type(ra->p[i]);
		if (type == fs->cls_list) {
			RefArray *a = Value_vp(ra->p[i]);
			if (a->size > 0) {
				return TRUE;
			}
		} else if (type == fs->cls_map) {
			RefMap *m = Value_vp(ra->p[i]);
			if (m->count > 0) {
				return TRUE;
			}
		}
	}
	return FALSE;
}
static int json_encode_sub(StrBuf *buf, Value v, Mem *mem, Hash *hash, int mode, int level)
{
	const RefNode *type = Value_type(v);

	if (type == fs->cls_str) {
		Str val = Value_str(v);
		if (!StrBuf_add_c(buf, '"')) {
			return FALSE;
		}
		add_backslashes_sub(buf, val.p, val.size, mode);
		if (!StrBuf_add_c(buf, '"')) {
			return FALSE;
		}
	} else if (type == fs->cls_int || type == fs->cls_float) {
		if (!StrBuf_add_v(buf, v)) {
			return FALSE;
		}
	} else if (type == fs->cls_list) {
		RefArray *ra = Value_vp(v);
		HashEntry *he = Hash_get_add_entry(hash, mem, (RefStr*)ra);
		int has_complex;
		int i;

		if (he->p != NULL) {
			throw_error_select(THROW_LOOP_REFERENCE);
			return FALSE;
		}
		he->p = (void*)1;
		if (!StrBuf_add_c(buf, '[')) {
			return FALSE;
		}
		has_complex = has_complex_value(ra);
		if (level >= 0 && has_complex) {
			level += 2;
			json_add_indent(buf, level);
		}
		for (i = 0; i < ra->size; i++) {
			if (i > 0) {
				if (!StrBuf_add_c(buf, ',')) {
					return FALSE;
				}
				if (level >= 0) {
					if (has_complex) {
						json_add_indent(buf, level);
					} else {
						if (!StrBuf_add_c(buf, ' ')) {
							return FALSE;
						}
					}
				}
			}
			if (!json_encode_sub(buf, ra->p[i], mem, hash, mode, level)) {
				return FALSE;
			}
		}
		if (level >= 0 && has_complex) {
			level -= 2;
			json_add_indent(buf, level);
		}
		if (!StrBuf_add_c(buf, ']')) {
			return FALSE;
		}
		he->p = NULL;
	} else if (type == fs->cls_map) {
		RefMap *rm = Value_vp(v);
		HashEntry *he = Hash_get_add_entry(hash, mem, (RefStr*)rm);
		int i;
		int first = TRUE;

		if (he->p != NULL) {
			throw_error_select(THROW_LOOP_REFERENCE);
			return FALSE;
		}
		he->p = (void*)1;

		if (!StrBuf_add_c(buf, '{')) {
			return FALSE;
		}
		if (rm->count > 0) {
			if (level >= 0) {
				level += 2;
				json_add_indent(buf, level);
			}
			for (i = 0; i < rm->entry_num; i++) {
				HashValueEntry *ve = rm->entry[i];
				for (; ve != NULL; ve = ve->next) {
					const RefNode *ktype = Value_type(ve->key);
					if (ktype != fs->cls_str) {
						throw_errorf(fs->mod_lang, "TypeError", "Str required but %n (Map key)", ktype);
						return FALSE;
					}
					if (first) {
						first = FALSE;
					} else {
						if (!StrBuf_add_c(buf, ',')) {
							return FALSE;
						}
						if (level >= 0) {
							json_add_indent(buf, level);
						}
					}
					json_encode_sub(buf, ve->key, mem, hash, mode, level);
					if (!StrBuf_add_c(buf, ':')) {
						return FALSE;
					}
					if (level >= 0) {
						if (!StrBuf_add_c(buf, ' ')) {
							return FALSE;
						}
					}
					if (!json_encode_sub(buf, ve->val, mem, hash, mode, level)) {
						return FALSE;
					}
				}
			}
			if (level >= 0) {
				level -= 2;
				json_add_indent(buf, level);
			}
		}
		if (!StrBuf_add_c(buf, '}')) {
			return FALSE;
		}
		he->p = NULL;
	} else if (type == fs->cls_bool) {
		if (Value_bool(v)) {
			if (!StrBuf_add(buf, "true", 4)) {
				return FALSE;
			}
		} else {
			if (!StrBuf_add(buf, "false", 5)) {
				return FALSE;
			}
		}
	} else if (type == fs->cls_null) {
		if (!StrBuf_add(buf, "null", 4)) {
			return FALSE;
		}
	} else {
		throw_errorf(fs->mod_lang, "TypeError", "Int, Float, Str, Array, Map, Bool or Null required but %n", type);
		return FALSE;
	}
	return TRUE;
}

static int serialize_parse_format(int *type, int *pretty, Str fmt)
{
	const char *ptr = fmt.p;
	const char *pend = ptr + fmt.size;

	while (ptr < pend) {
		Str fm;
		int found = FALSE;

		// fmtをトークン分解
		while (ptr < pend && isspace_fox(*ptr)) {
			ptr++;
		}
		fm.p = ptr;
		while (ptr < pend) {
			int c = *ptr;
			if (islexspace_fox(c)) {
				break;
			}
			ptr++;
		}
		fm.size = ptr - fm.p;
		if (fm.size > 0) {
			switch (fm.p[0]) {
			case 'a':
				if (fm.size == 1 || Str_eq_p(fm, "ascii")) {
					*type = ADD_BACKSLASH_U_UCS2;
					found = TRUE;
				}
				break;
			case 'p':
				if (fm.size == 1 || Str_eq_p(fm, "pretty")) {
					*pretty = TRUE;
					found = TRUE;
				}
				break;
			}
			if (!found) {
				throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", fm);
				return FALSE;
			}
		}
	}
	return TRUE;
}

static int json_encode(Value *vret, Value *v, RefNode *node)
{
	Mem mem;
	StrBuf buf;
	Hash hash;
	int ret;
	Str fmt = fs->Str_EMPTY;
	int type = ADD_BACKSLASH_UCS2;
	int pretty = FALSE;

	if (fg->stk_top > v + 2) {
		fmt = Value_str(v[2]);
	}
	if (!serialize_parse_format(&type, &pretty, fmt)) {
		return FALSE;
	}

	Mem_init(&mem, 4096);
	Hash_init(&hash, &mem, 64);
	StrBuf_init(&buf, 0);
	ret = json_encode_sub(&buf, v[1], &mem, &hash, type, pretty ? 0 : -1);
	Mem_close(&mem);
	if (pretty) {
		if (!StrBuf_add_c(&buf, '\n')) {
			return FALSE;
		}
	}
	if (ret) {
		*vret = cstr_Value(fs->cls_str, buf.p, buf.size);
	}
	StrBuf_close(&buf);

	return ret;
}

static int parse_json_sub(Value *v, JSTok *tk)
{
	switch (tk->type) {
	case JS_NULL:
		*v = VALUE_NULL;
		JSTok_next(tk);
		break;
	case JS_TRUE:
		*v = VALUE_TRUE;
		JSTok_next(tk);
		break;
	case JS_FALSE:
		*v = VALUE_FALSE;
		JSTok_next(tk);
		break;
	case JS_INT:
		*v = int32_Value(tk->int_val);
		JSTok_next(tk);
		break;
	case JS_BIGINT: {
		RefInt *mp = new_buf(fs->cls_int, sizeof(RefInt));
		*v = vp_Value(mp);
		mp->mp = tk->mp_val;
		JSTok_next(tk);
		break;
	}
	case JS_FLOAT: {
		RefFloat *rd = new_buf(fs->cls_float, sizeof(RefFloat));
		*v = vp_Value(rd);
		rd->d = tk->real_val;
		JSTok_next(tk);
		break;
	}
	case JS_STR:
		*v = cstr_Value(fs->cls_str, tk->str_val.p, tk->str_val.size);
		JSTok_next(tk);
		break;
	case JS_LB: {  // Array
		// 途中でthrowした時のことを考える
		RefArray *r = refarray_new(0);
		*v = vp_Value(r);
		JSTok_next(tk);
		if (tk->type != JS_RB) {
			for (;;) {
				Value *v2 = refarray_push(r);
				if (!parse_json_sub(v2, tk)) {
					return FALSE;
				}
				if (tk->type == JS_RB) {
					break;
				} else if (tk->type == JS_COMMA) {
					JSTok_next(tk);
				} else {
					throw_errorf(mod_marshal, "JSONError", "Unexpected token");
					return FALSE;
				}
			}
		}
		JSTok_next(tk);
		break;
	}
	case JS_LC: {  // Map
		// 途中でthrowした時のことを考える
		RefMap *rm = refmap_new(32);
		*v = vp_Value(v);
		JSTok_next(tk);
		if (tk->type != JS_RC) {
			for (;;) {
				Str key_s;
				Value key, val;
				HashValueEntry *ve;

				if (tk->type != JS_STR) {
					throw_errorf(mod_marshal, "JSONError", "Map key must be string");
					return FALSE;
				}
				key_s = tk->str_val;
				JSTok_next(tk);
				if (tk->type != JS_COLON) {
					throw_errorf(mod_marshal, "JSONError", "Unexpected token");
					return FALSE;
				}
				JSTok_next(tk);
				if (!parse_json_sub(&val, tk)) {
					return FALSE;
				}

				key = cstr_Value(fs->cls_str, key_s.p, key_s.size);
				ve = refmap_add(rm, key, TRUE, FALSE);
				Value_dec(key);
				if (ve == NULL) {
					return FALSE;
				}
				ve->val = val;

				if (tk->type == JS_RC) {
					break;
				} else if (tk->type == JS_COMMA) {
					JSTok_next(tk);
				} else {
					throw_errorf(mod_marshal, "JSONError", "Unexpected token");
					return FALSE;
				}
			}
		}
		JSTok_next(tk);
		break;
	}
	case JS_RANGE_ERR:
		throw_errorf(fs->mod_lang, "FloatError", "Float value overflow.");
		return FALSE;
	case JS_COLON:
		throw_errorf(mod_marshal, "JSONError", "Unexpected token ':'");
		return FALSE;
	case JS_RC:
		throw_errorf(mod_marshal, "JSONError", "Unexpected token '}'");
		return FALSE;
	case JS_RB:
		throw_errorf(mod_marshal, "JSONError", "Unexpected token ']'");
		return FALSE;
	case JS_EOS:
		throw_errorf(mod_marshal, "JSONError", "Unexpected token [EOS]");
		return FALSE;
	case JS_TOK_ERR:
		return FALSE;
	default:
		throw_errorf(fs->mod_lang, "InternalError", "Unexpected token");
		return FALSE;
	}
	return TRUE;
}
static int json_decode(Value *vret, Value *v, RefNode *node)
{
	RefStr *src = Value_vp(v[1]);
	JSTok tk;
	char *buf = str_dup_p(src->c, src->size, NULL);

	JSTok_init(&tk, buf, src->size);
	JSTok_next(&tk);
	if (!parse_json_sub(vret, &tk)) {
		free(buf);
		return FALSE;
	}
	free(buf);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static int validate_fox_object(Value src)
{
	char magic[8];
	int size = FOX_OBJECT_MAGIC_SIZE;

	if (!stream_read_data(src, NULL, magic, &size, FALSE, FALSE)) {
		return FALSE;
	}
	if (size != 8 || memcmp(magic, FOX_OBJECT_MAGIC, FOX_OBJECT_MAGIC_SIZE) != 0) {
		throw_errorf(fs->mod_io, "FileTypeError", "Not a fox object file");
		return FALSE;
	}
	return TRUE;
}
static void init_marshaldumper(Value *v, Value *src)
{
	RefLoopValidator *lv;
	Ref *r = new_ref(fs->cls_marshaldumper);
	*v = vp_Value(r);
	r->v[INDEX_MARSHALDUMPER_SRC] = *src;

	lv = new_buf(cls_loopvalidator, sizeof(RefLoopValidator));
	r->v[INDEX_MARSHALDUMPER_CYCLREF] = vp_Value(lv);
	Mem_init(&lv->mem, 256);
	Hash_init(&lv->hash, &lv->mem, 16);
}
static int marshal_load(Value *vret, Value *v, RefNode *node)
{
	Value reader;

	if (!value_to_streamio(&reader, v[1], FALSE, 0)) {
		return FALSE;
	}
	if (!validate_fox_object(reader)) {
		Value_dec(reader);
		return FALSE;
	}
	// readerの所有権がMarshalWriterに移る
	init_marshaldumper(fg->stk_top, &reader);
	fg->stk_top++;

	if (!call_member_func(fs->str_read, 0, TRUE)) {
		return FALSE;
	}
	fg->stk_top--;
	*vret = *fg->stk_top;

	return TRUE;
}
static int marshal_save(Value *vret, Value *v, RefNode *node)
{
	Value writer;

	if (!value_to_streamio(&writer, v[1], TRUE, 0)) {
		return FALSE;
	}
	stream_write_data(writer, FOX_OBJECT_MAGIC, FOX_OBJECT_MAGIC_SIZE);

	// writerの所有権がMarshalWriterに移る
	init_marshaldumper(fg->stk_top, &writer);
	fg->stk_top++;
	Value_push("v", v[2]);
	if (!call_member_func(fs->str_write, 1, TRUE)) {
		return FALSE;
	}
	Value_pop();

	return TRUE;
}

int read_int32(uint32_t *val, Value r)
{
	int size = 4;
	uint8_t buf[4];

	if (!stream_read_data(r, NULL, (char*)buf, &size, FALSE, TRUE)) {
		return FALSE;
	}
	*val = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	return TRUE;
}
static char *read_str(Str *val, Value r)
{
	uint8_t size;
	int32_t rd_size;
	char *ptr = NULL;

	rd_size = 1;
	if (!stream_read_data(r, NULL, (char*)&size, &rd_size, FALSE, TRUE)) {
		return NULL;
	}
	if (size == 0) {
		throw_errorf(mod_marshal, "MarshalLoadError", "MarshalDumpHeader is broken");
		return NULL;
	}

	rd_size = size;
	ptr = malloc(size);
	if (!stream_read_data(r, NULL, ptr, &rd_size, FALSE, TRUE)) {
		return NULL;
	}
	*val = Str_new(ptr, size);
	return ptr;
}
static int validate_module_name(Str s)
{
	int i;
	int head = TRUE;

	for (i = 0; i < s.size; i++) {
		int ch = s.p[i];
		if (head) {
			if (!islower_fox(ch)) {
				return FALSE;
			}
			head = FALSE;
		} else {
			if (ch == '.') {
				head = TRUE;
			} else if (!islower_fox(ch) && !isdigit_fox(ch) && ch != '_') {
				return FALSE;
			}
		}
	}
	if (head) {
		return FALSE;
	}
	return TRUE;
}
static int validate_class_name(Str s)
{
	int i;
	int valid = FALSE;

	if (s.size == 0) {
		return FALSE;
	}
	if (!isupper_fox(s.p[0])) {
		return FALSE;
	}
	for (i = 1; i < s.size; i++) {
		int ch = s.p[i] & 0xFF;
		if (isalnum(ch)) {
			if (islower(ch)) {
				valid = TRUE;
			}
		} else if (ch == '_') {
		} else {
			return FALSE;
		}
	}

	return valid;
}
int write_int32(uint32_t val, Value w)
{
	uint8_t buf[4];
	buf[0] = (val >> 24) & 0xFF;
	buf[1] = (val >> 16) & 0xFF;
	buf[2] = (val >> 8) & 0xFF;
	buf[3] = val & 0xFF;

	if (!stream_write_data(w, (char*)buf, 4)) {
		return FALSE;
	}
	return TRUE;
}
static int write_str(RefStr *rs, Value w)
{
	uint8_t size;
	if (rs->size > 255) {
		throw_errorf(mod_marshal, "MarshalSaveError", "Class name or RefNode name too long (> 255)");
		return FALSE;
	}

	size = rs->size;
	if (!stream_write_data(w, (const char*)&size, 1)) {
		return FALSE;
	}
	if (!stream_write_data(w, rs->c, rs->size)) {
		return FALSE;
	}

	return TRUE;
}

static int marshaldump_read(Value *vret, Value *v, RefNode *node)
{
	Ref *r = Value_vp(*v);
	Value rd = r->v[INDEX_MARSHALDUMPER_SRC];
	char *ptr;
	Str name_s;
	RefStr *name_r;
	RefNode *cls;
	RefNode *mod;

	ptr = read_str(&name_s, rd);
	if (ptr == NULL) {
		goto ERROR_END;
	}
	if (!validate_module_name(name_s)) {
		goto ERROR_END;
	}
	mod = get_module_by_name(name_s.p, name_s.size, FALSE, TRUE);
	free(ptr);
	if (mod == NULL) {
		goto ERROR_END;
	}

	ptr = read_str(&name_s, rd);
	if (ptr == NULL) {
		goto ERROR_END;
	}
	if (!validate_class_name(name_s)) {
		goto ERROR_END;
	}
	name_r = intern(name_s.p, name_s.size);
	cls = Hash_get_p(&mod->u.m.h, name_r);
	if (cls == NULL) {
		throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, mod, name_r);
		free(ptr);
		goto ERROR_END;
	}
	free(ptr);

	if (cls == fs->cls_module || cls == fs->cls_class) {
		RefNode *cls2;
		RefNode *mod2;
		ptr = read_str(&name_s, rd);
		if (ptr == NULL) {
			goto ERROR_END;
		}
		if (!validate_module_name(name_s)) {
			goto ERROR_END;
		}
		mod2 = get_module_by_name(name_s.p, name_s.size, FALSE, TRUE);
		free(ptr);
		if (mod2 == NULL) {
			goto ERROR_END;
		}
		if (cls == fs->cls_module) {
			*vret = vp_Value(mod2);
		} else {
			ptr = read_str(&name_s, rd);
			if (ptr == NULL) {
				goto ERROR_END;
			}
			if (!validate_class_name(name_s)) {
				goto ERROR_END;
			}
			name_r = intern(name_s.p, name_s.size);
			cls2 = Hash_get(&mod2->u.m.h, name_s.p, name_s.size);
			if (cls2 == NULL) {
				throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, mod, name_r);
				free(ptr);
				goto ERROR_END;
			}
			free(ptr);
			*vret = vp_Value(cls2);
		}
	} else {
		Value_push("rv", cls, *v);
		if (!call_member_func(fs->str_marshal_read, 1, TRUE)) {
			return FALSE;
		}
		fg->stk_top--;
		*vret = *fg->stk_top;
	}

	return TRUE;

ERROR_END:
	if (fg->error == VALUE_NULL) {
		// Errorがセットされていないが、エラーで飛んできたので、何かセットする
		throw_errorf(mod_marshal, "MarshalLoadError", "MarshalDumpHeader is broken");
	}
	return FALSE;
}
static int marshaldump_write(Value *vret, Value *v, RefNode *node)
{
	Ref *r = Value_vp(*v);
	Value w = r->v[INDEX_MARSHALDUMPER_SRC];
	Value v1 = v[1];

	if (Value_isref(v1)) {
		RefLoopValidator *lv = Value_vp(r->v[INDEX_MARSHALDUMPER_CYCLREF]);
		Ref *r1 = Value_vp(v1);
		HashEntry *he = Hash_get_add_entry(&lv->hash, &lv->mem, (RefStr*)r1);

		if (he->p != NULL) {
			throw_error_select(THROW_LOOP_REFERENCE);
			return FALSE;
		} else {
			he->p = (void*)1;
		}
	}

	{
		const RefNode *cls = Value_type(v1);
		RefStr *mod_name = cls->defined_module->name;

		if (mod_name == fs->str_toplevel) {
			throw_errorf(fs->mod_lang, "ValueError", "Cannot save object which belongs no modules");
			return FALSE;
		}

		if (!write_str(mod_name, w)) {
			return FALSE;
		}
		if (!write_str(cls->name, w)) {
			return FALSE;
		}

		if (cls == fs->cls_module) {
			RefNode *mod2 = Value_vp(v1);
			if (!write_str(mod2->name, w)) {
				return FALSE;
			}
		} else if (cls == fs->cls_class) {
			RefNode *cls2 = Value_vp(v1);
			if (!write_str(cls2->defined_module->name, w)) {
				return FALSE;
			}
			if (!write_str(cls2->name, w)) {
				return FALSE;
			}
		} else {
			Value_push("vv", v1, *v);
			if (!call_member_func(fs->str_marshal_write, 1, TRUE)) {
				return FALSE;
			}
			Value_pop();
		}
	}

	return TRUE;
}
static int marshaldump_stream(Value *vret, Value *v, RefNode *node)
{
	Ref *r = Value_vp(*v);
	*vret = Value_cp(r->v[INDEX_MARSHALDUMPER_SRC]);
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * v:Ref値を渡す
 */
static int value_has_loop_sub(Value v, Hash *hash, Mem *mem)
{
	HashEntry *he;
	Ref *r = Value_vp(v);
	const RefNode *type = r->rh.type;

	if (type == NULL) {
		return FALSE;
	}
	he = Hash_get_add_entry(hash, mem, (RefStr*)r);

	if (he->p != NULL) {
		return TRUE;
	} else {
		he->p = (void*)1;
	}

	if (type == fs->cls_list) {
		int i;
		RefArray *ra = Value_vp(v);
		for (i = 0; i < ra->size; i++) {
			Value v1 = ra->p[i];
			if (Value_isref(v1) && value_has_loop_sub(v1, hash, mem)) {
				return TRUE;
			}
		}
	} else if (type == fs->cls_map) {
		// SetはMapとして構築される
		int i;
		RefMap *rm = Value_vp(v);

		for (i = 0; i < rm->entry_num; i++) {
			HashValueEntry *ve = rm->entry[i];
			for (; ve != NULL; ve = ve->next) {
				if (Value_isref(ve->key) && value_has_loop_sub(ve->key, hash, mem)) {
					return TRUE;
				}
				if (Value_isref(ve->val) && value_has_loop_sub(ve->val, hash, mem)) {
					return TRUE;
				}
			}
		}
	} else {
		int i;
		for (i = 0; i < type->u.c.n_memb; i++) {
			Value v1 = r->v[i];
			if (Value_isref(v1) && value_has_loop_sub(v1, hash, mem)) {
				return TRUE;
			}
		}
	}
	return FALSE;
}
static int marshal_validate_loop_ref(Value *vret, Value *v, RefNode *node)
{
	int ret = FALSE;

	if (Value_isref(v[1])) {
		Mem mem;
		Hash hash;

		Mem_init(&mem, 1024);
		Hash_init(&hash, &mem, 128);
		ret = value_has_loop_sub(v[1], &hash, &mem);

		Mem_close(&mem);
	}
	if (ret) {
		throw_error_select(THROW_LOOP_REFERENCE);
		return FALSE;
	}

	return TRUE;
}
static int loopref_new(Value *vret, Value *v, RefNode *node)
{
	RefLoopValidator *lv = new_buf(cls_loopvalidator, sizeof(RefLoopValidator));
	*vret = vp_Value(lv);

	Mem_init(&lv->mem, 1024);
	Hash_init(&lv->hash, &lv->mem, 128);

	return TRUE;
}
static int loopref_dispose(Value *vret, Value *v, RefNode *node)
{
	RefLoopValidator *lv = Value_vp(*v);

	if (lv->mem.p != NULL) {
		Mem_close(&lv->mem);
		lv->mem.p = NULL;
	}
	return TRUE;
}
static int loopref_push(Value *vret, Value *v, RefNode *node)
{
	RefLoopValidator *lv = Value_vp(*v);
	Value v1 = v[1];

	if (Value_isref(v1)) {
		Ref *r = Value_vp(v1);
		HashEntry *he = Hash_get_add_entry(&lv->hash, &lv->mem, (RefStr*)r);

		if (he->p != NULL) {
			throw_error_select(THROW_LOOP_REFERENCE);
			return FALSE;
		} else {
			he->p = (void*)1;
		}
	}
	return TRUE;
}
static int loopref_pop(Value *vret, Value *v, RefNode *node)
{
	RefLoopValidator *lv = Value_vp(*v);
	Value v1 = v[1];

	if (Value_isref(v1)) {
		Ref *r = Value_vp(v1);
		HashEntry *he = Hash_get_add_entry(&lv->hash, &lv->mem, (RefStr*)r);

		if (he->p == NULL) {
			throw_errorf(fs->mod_lang, "ValueError", "Wrong value given");
			return FALSE;
		} else {
			he->p = (void*)0;
		}
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static void define_marshal_func(RefNode *m)
{
	RefNode *n;

	n = define_identifier(m, m, "json_encode", NODE_FUNC_N, 0);
	define_native_func_a(n, json_encode, 1, 2, NULL, NULL, fs->cls_str);

	n = define_identifier(m, m, "json_decode", NODE_FUNC_N, 0);
	define_native_func_a(n, json_decode, 1, 1, NULL, fs->cls_str);


	n = define_identifier(m, m, "marshal_load", NODE_FUNC_N, 0);
	define_native_func_a(n, marshal_load, 1, 1, NULL, NULL);

	n = define_identifier(m, m, "marshal_save", NODE_FUNC_N, 0);
	define_native_func_a(n, marshal_save, 2, 2, NULL, NULL, NULL);


	// 循環参照しているか調べる
	n = define_identifier(m, m, "validate_loop_ref", NODE_FUNC_N, 0);
	define_native_func_a(n, marshal_validate_loop_ref, 1, 1, NULL, NULL);
}

void define_marshal_class(RefNode *m)
{
	RefNode *cls, *cls2;
	RefNode *n;

	cls_loopvalidator = define_identifier(m, m, "LoopValidator", NODE_CLASS, 0);
	cls = cls_loopvalidator;
	n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	define_native_func_a(n, loopref_new, 0, 0, NULL);

	n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
	define_native_func_a(n, loopref_dispose, 0, 0, NULL);
	n = define_identifier(m, cls, "push", NODE_FUNC_N, 0);
	define_native_func_a(n, loopref_push, 1, 1, NULL, NULL);
	n = define_identifier(m, cls, "pop", NODE_FUNC_N, 0);
	define_native_func_a(n, loopref_pop, 1, 1, NULL, NULL);
	extends_method(cls, fs->cls_obj);


	cls = fs->cls_marshaldumper;
	n = define_identifier_p(m, cls, fs->str_read, NODE_FUNC_N, 0);
	define_native_func_a(n, marshaldump_read, 0, 0, NULL);
	n = define_identifier_p(m, cls, fs->str_write, NODE_FUNC_N, 0);
	define_native_func_a(n, marshaldump_write, 1, 1, NULL, NULL);
	n = define_identifier(m, cls, "stream", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, marshaldump_stream, 1, 1, NULL, NULL);
	cls->u.c.n_memb = INDEX_MARSHALDUMPER_NUM;
	extends_method(cls, fs->cls_obj);


	cls = define_identifier(m, m, "JSONError", NODE_CLASS, 0);
	define_error_class(cls, fs->cls_error, m);

	cls = define_identifier(m, m, "MarshalError", NODE_CLASS, NODEOPT_ABSTRACT);
	define_error_class(cls, fs->cls_error, m);
	cls2 = cls;

	cls = define_identifier(m, m, "MarshalLoadError", NODE_CLASS, 0);
	define_error_class(cls, cls2, m);
}

void define_marshal_module(RefNode *m)
{
	define_marshal_class(m);
	define_marshal_func(m);
	mod_marshal = m;
}
