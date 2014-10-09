#include "m_number.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_json;


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

static RefNode *mod_json;


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
	fs->throw_errorf(mod_json, "JSONError", "Invalid number format");
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
			fs->throw_errorf(mod_json, "JSONError", "Unterminated string");
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
				fs->throw_errorf(mod_json, "JSONError", "Invalid string format");
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
				int ch2 = fs->parse_hex((const char**)&tk->p, tk->end, 4);
				if (ch2 < 0) {
					fs->throw_errorf(mod_json, "JSONError", "Illigal hexadecimal format");
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
						fs->throw_errorf(mod_json, "JSONError", "Missing Low Surrogate");
						tk->type = JS_TOK_ERR;
						return;
					}
					tk->p += 2;
					ch3 = fs->parse_hex((const char**)&tk->p, tk->end, 4);
					if (ch3 < 0) {
						fs->throw_errorf(mod_json, "JSONError", "Illigal hexadecimal format");
						tk->type = JS_TOK_ERR;
						return;
					}
					if (ch3 < 0xDC00 || ch3 >= 0xE000) {
						fs->throw_errorf(mod_json, "JSONError", "Missing Low Surrogate");
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
					fs->throw_errorf(mod_json, "JSONError", "Illigal codepoint (Surrogate) %U", ch2);
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
				if (isalnumu_fox(ch)) {
					fs->throw_errorf(mod_json, "JSONError", "Unknwon backslash sequence");
					tk->type = JS_TOK_ERR;
					return;
				} else if (*tk->p < ' ') {
					fs->throw_errorf(mod_json, "JSONError", "Unknwon character");
					tk->type = JS_TOK_ERR;
					return;
				}
				*dst++ = *tk->p++;
				break;
			}
			break;
		default:
			if ((*tk->p & 0xFF) < ' ') {
				fs->throw_errorf(mod_json, "JSONError", "Unknwon character %c", *tk->p);
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

	fs->throw_errorf(mod_json, "JSONError", "Unexpected token %q", top);
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
		if (isdigit_fox(*tk->p)) {
			JSTok_parse_digit(tk);
		} else {
			fs->throw_errorf(mod_json, "JSONError", "Unexpected character %c", *tk->p);
			tk->type = JS_TOK_ERR;
		}
		break;
	}
}

static int json_add_indent(StrBuf *buf, int level)
{
	int i;

	if (!fs->StrBuf_add_c(buf, '\n')) {
		return FALSE;
	}
	for (i = 0; i < level; i++) {
		if (!fs->StrBuf_add_c(buf, ' ')) {
			return FALSE;
		}
	}
	return TRUE;
}
static int has_complex_value(RefArray *ra)
{
	int i;
	for (i = 0; i < ra->size; i++) {
		const RefNode *type = fs->Value_type(ra->p[i]);
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
	const RefNode *type = fs->Value_type(v);

	if (type == fs->cls_str) {
		Str val = fs->Value_str(v);
		if (!fs->StrBuf_add_c(buf, '"')) {
			return FALSE;
		}
		fs->add_backslashes_sub(buf, val.p, val.size, mode);
		if (!fs->StrBuf_add_c(buf, '"')) {
			return FALSE;
		}
	} else if (type == fs->cls_int || type == fs->cls_float) {
		if (!fs->StrBuf_printf(buf, "%v", v)) {
			return FALSE;
		}
	} else if (type == fs->cls_list) {
		RefArray *ra = Value_vp(v);
		HashEntry *he = fs->Hash_get_add_entry(hash, mem, (RefStr*)ra);
		int has_complex;
		int i;

		if (he->p != NULL) {
			fs->throw_error_select(THROW_LOOP_REFERENCE);
			return FALSE;
		}
		he->p = (void*)1;
		if (!fs->StrBuf_add_c(buf, '[')) {
			return FALSE;
		}
		has_complex = has_complex_value(ra);
		if (level >= 0 && has_complex) {
			level += 2;
			json_add_indent(buf, level);
		}
		for (i = 0; i < ra->size; i++) {
			if (i > 0) {
				if (!fs->StrBuf_add_c(buf, ',')) {
					return FALSE;
				}
				if (level >= 0) {
					if (has_complex) {
						json_add_indent(buf, level);
					} else {
						if (!fs->StrBuf_add_c(buf, ' ')) {
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
		if (!fs->StrBuf_add_c(buf, ']')) {
			return FALSE;
		}
		he->p = NULL;
	} else if (type == fs->cls_map) {
		RefMap *rm = Value_vp(v);
		HashEntry *he = fs->Hash_get_add_entry(hash, mem, (RefStr*)rm);
		int i;
		int first = TRUE;

		if (he->p != NULL) {
			fs->throw_error_select(THROW_LOOP_REFERENCE);
			return FALSE;
		}
		he->p = (void*)1;

		if (!fs->StrBuf_add_c(buf, '{')) {
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
					const RefNode *ktype = fs->Value_type(ve->key);
					if (ktype != fs->cls_str) {
						fs->throw_errorf(fs->mod_lang, "TypeError", "Str required but %n (Map key)", ktype);
						return FALSE;
					}
					if (first) {
						first = FALSE;
					} else {
						if (!fs->StrBuf_add_c(buf, ',')) {
							return FALSE;
						}
						if (level >= 0) {
							json_add_indent(buf, level);
						}
					}
					json_encode_sub(buf, ve->key, mem, hash, mode, level);
					if (!fs->StrBuf_add_c(buf, ':')) {
						return FALSE;
					}
					if (level >= 0) {
						if (!fs->StrBuf_add_c(buf, ' ')) {
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
		if (!fs->StrBuf_add_c(buf, '}')) {
			return FALSE;
		}
		he->p = NULL;
	} else if (type == fs->cls_bool) {
		if (Value_bool(v)) {
			if (!fs->StrBuf_add(buf, "true", 4)) {
				return FALSE;
			}
		} else {
			if (!fs->StrBuf_add(buf, "false", 5)) {
				return FALSE;
			}
		}
	} else if (type == fs->cls_null) {
		if (!fs->StrBuf_add(buf, "null", 4)) {
			return FALSE;
		}
	} else {
		fs->throw_errorf(fs->mod_lang, "TypeError", "Int, Float, Str, Array, Map, Bool or Null required but %n", type);
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
				fs->throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", fm);
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
		fmt = fs->Value_str(v[2]);
	}
	if (!serialize_parse_format(&type, &pretty, fmt)) {
		return FALSE;
	}

	fs->Mem_init(&mem, 4096);
	fs->Hash_init(&hash, &mem, 64);
	fs->StrBuf_init(&buf, 0);
	ret = json_encode_sub(&buf, v[1], &mem, &hash, type, pretty ? 0 : -1);
	fs->Mem_close(&mem);
	if (pretty) {
		if (!fs->StrBuf_add_c(&buf, '\n')) {
			return FALSE;
		}
	}
	if (ret) {
		*vret = fs->cstr_Value(fs->cls_str, buf.p, buf.size);
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
		RefInt *mp = fs->buf_new(fs->cls_int, sizeof(RefInt));
		*v = vp_Value(mp);
		mp->mp = tk->mp_val;
		JSTok_next(tk);
		break;
	}
	case JS_FLOAT: {
		RefFloat *rd = fs->buf_new(fs->cls_float, sizeof(RefFloat));
		*v = vp_Value(rd);
		rd->d = tk->real_val;
		JSTok_next(tk);
		break;
	}
	case JS_STR:
		*v = fs->cstr_Value(fs->cls_str, tk->str_val.p, tk->str_val.size);
		JSTok_next(tk);
		break;
	case JS_LB: {  // Array
		// 途中でthrowした時のことを考える
		RefArray *r = fs->refarray_new(0);
		*v = vp_Value(r);
		JSTok_next(tk);
		if (tk->type != JS_RB) {
			for (;;) {
				Value *v2 = fs->refarray_push(r);
				if (!parse_json_sub(v2, tk)) {
					return FALSE;
				}
				if (tk->type == JS_RB) {
					break;
				} else if (tk->type == JS_COMMA) {
					JSTok_next(tk);
				} else {
					fs->throw_errorf(mod_json, "JSONError", "Unexpected token");
					return FALSE;
				}
			}
		}
		JSTok_next(tk);
		break;
	}
	case JS_LC: {  // Map
		// 途中でthrowした時のことを考える
		RefMap *rm = fs->refmap_new(32);
		*v = vp_Value(rm);
		JSTok_next(tk);
		if (tk->type != JS_RC) {
			for (;;) {
				Str key_s;
				Value key, val;
				HashValueEntry *ve;

				if (tk->type != JS_STR) {
					fs->throw_errorf(mod_json, "JSONError", "Map key must be string");
					return FALSE;
				}
				key_s = tk->str_val;
				JSTok_next(tk);
				if (tk->type != JS_COLON) {
					fs->throw_errorf(mod_json, "JSONError", "Unexpected token");
					return FALSE;
				}
				JSTok_next(tk);
				if (!parse_json_sub(&val, tk)) {
					return FALSE;
				}

				key = fs->cstr_Value(fs->cls_str, key_s.p, key_s.size);
				ve = fs->refmap_add(rm, key, TRUE, FALSE);
				fs->Value_dec(key);
				if (ve == NULL) {
					return FALSE;
				}
				ve->val = val;

				if (tk->type == JS_RC) {
					break;
				} else if (tk->type == JS_COMMA) {
					JSTok_next(tk);
				} else {
					fs->throw_errorf(mod_json, "JSONError", "Unexpected token");
					return FALSE;
				}
			}
		}
		JSTok_next(tk);
		break;
	}
	case JS_RANGE_ERR:
		fs->throw_errorf(fs->mod_lang, "FloatError", "Float value overflow.");
		return FALSE;
	case JS_COLON:
		fs->throw_errorf(mod_json, "JSONError", "Unexpected token ':'");
		return FALSE;
	case JS_RC:
		fs->throw_errorf(mod_json, "JSONError", "Unexpected token '}'");
		return FALSE;
	case JS_RB:
		fs->throw_errorf(mod_json, "JSONError", "Unexpected token ']'");
		return FALSE;
	case JS_EOS:
		fs->throw_errorf(mod_json, "JSONError", "Unexpected token [EOS]");
		return FALSE;
	case JS_TOK_ERR:
		return FALSE;
	default:
		fs->throw_errorf(fs->mod_lang, "InternalError", "Unexpected token");
		return FALSE;
	}
	return TRUE;
}
static int json_decode(Value *vret, Value *v, RefNode *node)
{
	RefStr *src = Value_vp(v[1]);
	JSTok tk;
	char *buf = fs->str_dup_p(src->c, src->size, NULL);

	JSTok_init(&tk, buf, src->size);
	JSTok_next(&tk);
	if (!parse_json_sub(vret, &tk)) {
		free(buf);
		return FALSE;
	}
	free(buf);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
	RefNode *cls;

	cls = fs->define_identifier(m, m, "JSONError", NODE_CLASS, 0);
	cls->u.c.n_memb = 2;
	fs->extends_method(cls, fs->cls_error);
}

static void define_func(RefNode *m)
{
	RefNode *n;

	n = fs->define_identifier(m, m, "json_encode", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, json_encode, 1, 2, NULL, NULL, fs->cls_str);

	n = fs->define_identifier(m, m, "json_decode", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, json_decode, 1, 1, NULL, fs->cls_str);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_json = m;

	define_class(m);
	define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}
