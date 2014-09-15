#include "fox_vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <errno.h>

#ifdef WIN32
#include <windows.h>
#endif


static int load_charset_file(IniTok *tk, RefStr *name)
{
	char *path = str_printf("%S" SEP_S "encoding" SEP_S "%r.txt", fs->fox_home, name);
	int ret = IniTok_load(tk, path);
	free(path);
	return ret;
}
static RefCharset *load_charset(RefStr *name_r)
{
	RefCharset *cs = Mem_get(&fg->st_mem, sizeof(RefCharset));
	IniTok tk;

	cs->rh.type = fs->cls_charset;
	cs->rh.nref = -1;
	cs->rh.n_memb = 0;
	cs->rh.weak_ref = NULL;

	cs->utf8 = FALSE;
	cs->ascii = TRUE;
	cs->name = name_r;
	cs->iana = name_r;
	cs->ic_name = name_r;

	if (load_charset_file(&tk, name_r)) {
		while (IniTok_next(&tk)) {
			if (tk.type == INITYPE_STRING) {
				if (Str_eq_p(tk.key, "iana")) {
					cs->iana = intern(tk.val.p, tk.val.size);
				} else if (Str_eq_p(tk.key, "iconv")) {
					cs->ic_name = intern(tk.val.p, tk.val.size);
				} else if (Str_eq_p(tk.key, "ascii_compat")) {
					cs->ascii = Str_eq_p(tk.key, "true");
				}
			}
		}
		IniTok_close(&tk);
	}

	return cs;
}

// 返す値はst_memから取得したバッファ
static RefStr *resolve_charset_alias(const char *src_p, int src_size)
{
	static Hash charsets;

	RefStr *ret;
	char buf[32];
	int i, j = 0;

	if (src_size < 0) {
		src_size = strlen(src_p);
	}

	for (i = 0; i < src_size; i++) {
		char c = src_p[i];
		if (isalnum(c)) {
			buf[j++] = toupper(c);
			if (j >= sizeof(buf) - 1) {
				break;
			}
		}
	}
	buf[j] = '\0';
	if (strcmp(buf, "UTF8") == 0) {
		return intern("UTF-8", -1);
	}

	if (charsets.entry == NULL) {
		Hash_init(&charsets, &fg->st_mem, 64);
		load_aliases_file(&charsets, "data" SEP_S "charset.txt");
	}
	ret = Hash_get(&charsets, buf, j);

	if (ret != NULL) {
		return ret;
	} else {
		return NULL;
	}
}
RefCharset *get_charset_from_name(const char *name_p, int name_size)
{
	static Hash charsets;
	RefStr *name_r;
	RefCharset *cs;

	name_r = resolve_charset_alias(name_p, name_size);
	if (name_r == NULL) {
		return NULL;
	}

	if (charsets.entry == NULL) {
		Hash_init(&charsets, &fg->st_mem, 32);
	}

	cs = Hash_get(&charsets, name_r->c, name_r->size);
	if (cs == NULL) {
		if (strcmp(name_r->c, "UTF-8") == 0) {
			cs = Mem_get(&fg->st_mem, sizeof(RefCharset));
			cs->rh.type = fs->cls_charset;
			cs->rh.nref = -1;
			cs->rh.n_memb = 0;
			cs->rh.weak_ref = NULL;

			cs->name = name_r;
			cs->iana = name_r;
			cs->ic_name = name_r;
			cs->utf8 = TRUE;
			cs->ascii = TRUE;
		} else {
			cs = load_charset(name_r);
		}
		if (cs != NULL) {
			Hash_add_p(&charsets, &fg->st_mem, name_r, cs);
		}
	}
	return cs;
}
RefCharset *get_charset_from_cp(int cp)
{
	char buf[32];
	sprintf(buf, "CP%d", cp);
	return get_charset_from_name(buf, -1);
}

RefCharset *Value_to_charset(Value v, int argn)
{
	const RefNode *v_type = Value_type(v);

	if (v_type == fs->cls_charset) {
		return Value_vp(v);
	} else if (v_type == fs->cls_str) {
		RefStr *rs = Value_vp(v);
		RefCharset *cs = get_charset_from_name(rs->c, rs->size);
		if (cs == NULL) {
			throw_errorf(fs->mod_lang, "ValueError", "Unknown encoding name %q", rs->c);
			return NULL;
		}
		return cs;
	} else {
		throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_charset, fs->cls_str, v_type, argn + 1);
		return NULL;
	}
}

int convert_str_to_bin_sub(Value *dst, StrBuf *dst_buf, const char *src_p, int src_size, RefCharset *cs, int alt_b)
{
	if (cs->utf8) {
		// そのまま
		if (dst != NULL) {
			*dst = cstr_Value(fs->cls_bytes, src_p, src_size);
		}
		if (dst_buf != NULL) {
			if (!StrBuf_add(dst_buf, src_p, src_size)) {
				return FALSE;
			}
		}
		return TRUE;
	} else {
		// UTF-8 -> 任意の文字コード変換
		IconvIO ic;
		StrBuf *buf;
		StrBuf str_buf;
		char *c_buf = malloc(BUFFER_SIZE);
		int result = TRUE;

		if (!IconvIO_open(&ic, fs->cs_utf8, cs, alt_b)) {
			return FALSE;
		}

		if (dst_buf == NULL) {
			StrBuf_init(&str_buf, 0);
			buf = &str_buf;
		} else {
			buf = dst_buf;
		}
		if (!IconvIO_conv_b(&ic, buf, src_p, src_size)) {
			result = FALSE;
		}
		IconvIO_close(&ic);

		if (dst != NULL) {
			*dst = cstr_Value(fs->cls_bytes, buf->p, buf->size);
		}
		if (dst_buf == NULL) {
			StrBuf_close(&str_buf);
		}
		free(c_buf);

		return result;
	}
}

// dst : optional
// dst_buf : optional
// src   : Str|Bytes
// arg   : Charset|Str (default : UTF-8)
// arg+1 : Bool(optional)
int convert_str_to_bin(Value *dst, StrBuf *dst_buf, Value src, int arg)
{
	Value *v = fg->stk_base;
	const RefNode *src_type = Value_type(src);
	RefCharset *cs;
	RefStr *rs;
	int alt_b;

	if (fg->stk_top > v + arg + 1) {
		if (src_type != fs->cls_str) {
			throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, src_type, arg);
			return FALSE;
		}
		cs = Value_to_charset(v[arg + 1], arg);
		if (cs == NULL) {
			return FALSE;
		}
	} else {
		if (src_type != fs->cls_bytes) {
			throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_bytes, src_type, arg);
			return FALSE;
		}
		cs = fs->cs_utf8;
	}
	if (fg->stk_top > v + arg + 2) {
		alt_b = Value_bool(v[arg + 2]);
	} else {
		alt_b = FALSE;
	}

	rs = Value_vp(src);
	return convert_str_to_bin_sub(dst, dst_buf, rs->c, rs->size, cs, alt_b);
}

/*
 * 出力先にdstとdst_bufどちらか一方を指定
 */
int convert_bin_to_str_sub(Value *dst, StrBuf *dst_buf, const char *src_p, int src_size, RefCharset *cs, int alt_b)
{
	if (src_size < 0) {
		src_size = strlen(src_p);
	}
	if (cs->utf8) {
		// なるべくそのまま
		if (invalid_utf8_pos(src_p, src_size) >= 0) {
			if (alt_b) {
				if (dst_buf != NULL) {
					StrBuf_alloc(dst_buf, src_size);
					alt_utf8_string(dst_buf, src_p, src_size);
					*dst = cstr_Value(fs->cls_str, dst_buf->p, dst_buf->size);
				}
				if (dst != NULL) {
					StrBuf d_buf;
					StrBuf_init(&d_buf, src_size);
					alt_utf8_string(&d_buf, src_p, src_size);
					*dst = cstr_Value(fs->cls_str, d_buf.p, d_buf.size);
					StrBuf_close(&d_buf);
				}
			} else {
				throw_error_select(THROW_INVALID_UTF8);
				return FALSE;
			}
		} else {
			if (dst_buf != NULL) {
				StrBuf_add(dst_buf, src_p, src_size);
			}
			if (dst != NULL) {
				*dst = cstr_Value(fs->cls_str, src_p, src_size);
			}
		}
		return TRUE;
	} else {
		IconvIO ic;
		StrBuf d_buf;
		int result = TRUE;

		if (!IconvIO_open(&ic, cs, fs->cs_utf8, alt_b)) {
			return FALSE;
		}

		if (dst_buf == NULL) {
			StrBuf_init(&d_buf, 0);
		}
		if (!IconvIO_conv_s(&ic, (dst_buf != NULL ? dst_buf : &d_buf), src_p, src_size)) {
			result = FALSE;
		}
		IconvIO_close(&ic);

		if (dst_buf == NULL) {
			*dst = cstr_Value(fs->cls_str, d_buf.p, d_buf.size);
			StrBuf_close(&d_buf);
		}
		return result;
	}
}

// dst : needed
// src   : optional
// src_str : needed if src == NULL
// arg   : Charset|Str (省略時はBytesのまま)
// arg+1 : Bool(optional)
int convert_bin_to_str(Value *dst, Value *src, const Str *src_str, int arg)
{
	Value *v = fg->stk_base;
	RefCharset *cs;
	int alt_b;
	const char *src_p;
	int src_size;

	if (fg->stk_top > v + arg + 1) {
		cs = Value_to_charset(v[arg + 1], arg);
		if (cs == NULL) {
			return FALSE;
		}
	} else {
		// そのまま返す
		if (src != NULL) {
			RefStr *rs = Value_vp(*src);
			*dst = cstr_Value(fs->cls_bytes, rs->c, rs->size);
		} else {
			*dst = cstr_Value(fs->cls_bytes, src_str->p, src_str->size);
		}
		return TRUE;
	}
	if (fg->stk_top > v + arg + 2) {
		alt_b = Value_bool(v[arg + 2]);
	} else {
		alt_b = FALSE;
	}
	if (src != NULL) {
		RefStr *rs = Value_vp(*src);
		src_p = rs->c;
		src_size = rs->size;
	} else {
		src_p = src_str->p;
		src_size = src_str->size;
	}
	return convert_bin_to_str_sub(dst, NULL, src_p, src_size, cs, alt_b);
}
int get_charset_from_args(RefCharset **cs, int *alt_b, int arg)
{
	Value *v = fg->stk_base;

	if (fg->stk_top > v + arg + 1) {
		*cs = Value_to_charset(v[arg + 1], arg);
		if (*cs == NULL) {
			return FALSE;
		}
		if (fg->stk_top > v + arg + 2) {
			*alt_b = Value_bool(v[arg + 2]);
		} else {
			*alt_b = FALSE;
		}
	} else {
		*cs = NULL;
		*alt_b = FALSE;
	}
	return TRUE;
}

static int charset_new(Value *vret, Value *v, RefNode *node)
{
	RefCharset *cs = Value_to_charset(v[1], 0);

	if (cs == NULL) {
		return FALSE;
	}
	*vret = vp_Value(cs);
	return TRUE;
}
static int charset_marshal_read(Value *vret, Value *v, RefNode *node)
{
	Value r = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);
	RefCharset *cs;
	uint32_t size;
	int rd_size;
	char cbuf[64];

	if (!read_int32(&size, r)) {
		goto ERROR_END;
	}
	if (size > 63) {
		throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
		goto ERROR_END;
	}
	rd_size = size;
	if (!stream_read_data(r, NULL, cbuf, &rd_size, FALSE, TRUE)) {
		goto ERROR_END;
	}
	cbuf[rd_size] = '\0';

	cs = get_charset_from_name(cbuf, -1);
	if (cs == NULL) {
		throw_errorf(fs->mod_lang, "ValueError", "Unknown encoding name %q", cbuf);
		goto ERROR_END;
	}
	*vret = vp_Value(cs);

	return TRUE;

ERROR_END:
	return FALSE;
}
static int charset_marshal_write(Value *vret, Value *v, RefNode *node)
{
	RefCharset *cs = Value_vp(*v);
	RefStr *name = cs->name;
	Value w = Value_ref_memb(v[1], INDEX_MARSHALDUMPER_SRC);

	if (!write_int32(name->size, w)) {
		return FALSE;
	}
	if (!stream_write_data(w, name->c, name->size)) {
		return FALSE;
	}
	return TRUE;
}
static int charset_tostr(Value *vret, Value *v, RefNode *node)
{
	RefCharset *cs = Value_vp(*v);
	RefStr *name = cs->name;
	*vret = printf_Value("Charset(%r)", name);
	return TRUE;
}
static int charset_name(Value *vret, Value *v, RefNode *node)
{
	RefCharset *cs = Value_vp(*v);
	*vret = vp_Value(cs->name);
	return TRUE;
}
static int charset_iana(Value *vret, Value *v, RefNode *node)
{
	RefCharset *cs = Value_vp(*v);
	*vret = vp_Value(cs->iana);
	return TRUE;
}

static int charset_get_stdio(Value *vret, Value *v, RefNode *node)
{
	*vret = ref_cp_Value(&fs->cs_stdio->rh);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void RefStr_copy(char *dst, RefStr *src, int max)
{
	int size = src->size;
	if (size > max - 1) {
		size = max - 1;
	}
	memcpy(dst, src->c, size);
	dst[size] = '\0';
}
int IconvIO_open(IconvIO *ic, RefCharset *from, RefCharset *to, int trans)
{
	enum {
		MAX_LEN = 32,
	};
	char c_src[MAX_LEN];
	char c_dst[MAX_LEN];

	ic->cs_from = from;
	ic->cs_to = to;
	ic->trans = trans;

	RefStr_copy(c_src, from->ic_name, MAX_LEN);
	RefStr_copy(c_dst, to->ic_name, MAX_LEN);

	ic->ic = iconv_open(c_dst, c_src);

	if (ic->ic == (iconv_t)-1) {
		throw_errorf(fs->mod_lang, "InternalError", "iconv_open fault");
		return FALSE;
	}
	return TRUE;
}
void IconvIO_close(IconvIO *ic)
{
	if (ic->ic != (iconv_t)-1) {
		iconv_close(ic->ic);
		ic->ic = (iconv_t)-1;
	}
}

int IconvIO_next(IconvIO *ic)
{
	errno = 0;
	if (iconv(ic->ic, (char**)&ic->inbuf, &ic->inbytesleft, &ic->outbuf, &ic->outbytesleft) == (size_t)-1) {
		switch (errno) {
		case EINVAL:
			return ICONV_OK;
		case E2BIG:
			return ICONV_OUTBUF;
		case EILSEQ:
			return ICONV_INVALID;
		}
	}
	return ICONV_OK;
}
// Bytes -> Str
int IconvIO_conv_s(IconvIO *ic, StrBuf *dst, const char *src_p, int src_size)
{
	enum {
		ALT_SIZE = 3,
	};
	const char *alt = "\xEF\xBF\xBD";
	char *tmp_buf = malloc(BUFFER_SIZE);

	ic->inbuf = src_p;
	ic->inbytesleft = src_size;
	ic->outbuf = tmp_buf;
	ic->outbytesleft = BUFFER_SIZE;

	for (;;) {
		switch (IconvIO_next(ic)) {
		case ICONV_OK:
			if (ic->inbuf != NULL) {
				ic->inbuf = NULL;
			} else {
				goto BREAK;
			}
			break;
		case ICONV_OUTBUF:
			if (!StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
				return FALSE;
			}
			ic->outbuf = tmp_buf;
			ic->outbytesleft = BUFFER_SIZE;
			break;
		case ICONV_INVALID:
			if (ic->trans) {
				if (ic->outbytesleft < ALT_SIZE) {
					if (!StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
						return FALSE;
					}
					ic->outbuf = tmp_buf;
					ic->outbytesleft = BUFFER_SIZE;
				}
				{
					char *pdst = ic->outbuf;

					memcpy(pdst, alt, ALT_SIZE);
					ic->outbuf += ALT_SIZE;
					ic->outbytesleft -= ALT_SIZE;

					ic->inbuf++;
					ic->inbytesleft--;
				}
			} else {
				throw_errorf(fs->mod_lang, "CharsetError", "Invalid byte sequence detected (%r)", ic->cs_from->name);
				free(tmp_buf);
				return FALSE;
			}
			break;
		}
	}
BREAK:
	if (!StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
		free(tmp_buf);
		return FALSE;
	}
	free(tmp_buf);

	if (ic->inbytesleft > 0) {
		if (ic->trans) {
			if (!StrBuf_add(dst, alt, ALT_SIZE)) {
				return FALSE;
			}
		} else {
			throw_errorf(fs->mod_lang, "CharsetError", "Invalid byte sequence detected (%r)", ic->cs_from->name);
			return FALSE;
		}
	}
	return TRUE;
}
// Str -> Bytes
int IconvIO_conv_b(IconvIO *ic, StrBuf *dst, const char *src_p, int src_size)
{
	char *tmp_buf = malloc(BUFFER_SIZE);

	ic->inbuf = src_p;
	ic->inbytesleft = src_size;
	ic->outbuf = tmp_buf;
	ic->outbytesleft = BUFFER_SIZE;

	for (;;) {
		switch (IconvIO_next(ic)) {
		case ICONV_OK:
			if (ic->inbuf != NULL) {
				ic->inbuf = NULL;
			} else {
				goto BREAK;
			}
			break;
		case ICONV_OUTBUF:
			if (!StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
				return FALSE;
			}
			ic->outbuf = tmp_buf;
			ic->outbytesleft = BUFFER_SIZE;
			break;
		case ICONV_INVALID:
			if (ic->trans) {
				if (ic->outbytesleft < 1) {
					if (!StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
						return FALSE;
					}
					ic->outbuf = tmp_buf;
					ic->outbytesleft = BUFFER_SIZE;
				}
				{
					const char *psrc = ic->inbuf;
					char *pdst = ic->outbuf;

					*pdst = '?';
					ic->outbuf++;
					ic->outbytesleft--;

					// 1文字進める
					utf8_next(&psrc, psrc + ic->inbytesleft);
					ic->inbytesleft -= psrc - ic->inbuf;
					ic->inbuf = psrc;
				}
			} else {
				int ch = utf8_codepoint_at(ic->inbuf);
				throw_errorf(fs->mod_lang, "CharsetError", "Cannot convert to %r (%U)", ic->cs_to->name, ch);
				free(tmp_buf);
				return FALSE;
			}
			break;
		}
	}
BREAK:
	if (!StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
		return FALSE;
	}
	free(tmp_buf);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void define_charset_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

	fs->cs_utf8 = get_charset_from_name("UTF8", 4);

	// Charset
	cls = fs->cls_charset;
	n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	define_native_func_a(n, charset_new, 1, 1, NULL, NULL);
	n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
	define_native_func_a(n, charset_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

	n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	define_native_func_a(n, charset_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	define_native_func_a(n, object_eq, 1, 1, NULL, fs->cls_charset);
	n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
	define_native_func_a(n, charset_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
	n = define_identifier(m, cls, "name", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, charset_name, 0, 0, NULL);
	n = define_identifier(m, cls, "iana", NODE_FUNC_N, NODEOPT_PROPERTY);
	define_native_func_a(n, charset_iana, 0, 0, NULL);

	n = define_identifier(m, cls, "UTF8", NODE_CONST, 0);
	n->u.k.val = vp_Value(fs->cs_utf8);

	n = define_identifier(m, cls, "STDIO", NODE_CONST_U_N, 0);
	define_native_func_a(n, charset_get_stdio, 0, 0, NULL);

	extends_method(cls, fs->cls_obj);


	cls = define_identifier(m, m, "CharsetError", NODE_CLASS, 0);
	define_error_class(cls, fs->cls_error, m);
}