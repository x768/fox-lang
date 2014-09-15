#include "fox_vm.h"
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>

#ifndef WIN32
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif


struct MemChunk
{
	struct MemChunk *next;
	char buf[];
};

////////////////////////////////////////////////////////////////////////////////

int align_pow2(int sz, int min)
{
	if (sz <= 0) {
		return 0;
	}
	while (min < sz) {
		min *= 2;
	}
	return min;
}
int char2hex(char ch)
{
	if (isdigit_fox(ch)) {
		return ch - '0';
	} else if (ch >= 'A' && ch <= 'F') {
		return 10 + ch - 'A';
	} else if (ch >= 'a' && ch <= 'f') {
		return 10 + ch - 'a';
	} else {
		return -1;
	}
}
int write_p(int fd, const char *str)
{
	if (str != NULL) {
		return write_fox(fd, str, strlen(str));
	} else {
		return 0;
	}
}
/**
 * example:
 * 16 777 216
 * 256K
 * 16 M
 */
int parse_memory_size(Str s)
{
	enum {
		MAX_MEMORY_SIZE = 1024 * 1024 * 1024,
	};
	int ret = FALSE;
	int64_t val = 0LL;
	const char *p = s.p;
	const char *end = p + s.size;

	while (p < end) {
		if (isdigit_fox(*p)) {
			val = val * 10 + (*p - '0');
			ret = TRUE;
		} else if (!isspace_fox(*p)) {
			break;
		}
		p++;
	}
	if (p < end) {
		if (*p == 'K') {
			val *= 1024;
			p++;
		} else if (*p == 'M') {
			val *= 1024 * 1024;
			p++;
		}
	}
	while (p < end && isspace_fox(*p)) {
		p++;
	}
	if (p < end) {
		ret = FALSE;
	} else if (val > MAX_MEMORY_SIZE) {
		ret = FALSE;
	}
	return ret ? val : -1;
}

/**
 * 16進数として解釈可能なところまでポインタを進める
 *
 * end : 文字列終端(\0終端なら NULL)
 * n   : 最大文字数 or -1
 *
 * 変換できなければ-1
 * オーバーフローしたら-2
 */
int parse_hex(const char **pp, const char *end, int n)
{
	const char *p = *pp;
	int ret = 0;
	int i = 0;

	if (end != NULL) {
		if (p >= end || !isxdigit(*p)) {
			return -1;
		}

		while (p < end && isxdigit(*p)) {
			if ((ret & 0xF0000000) != 0) {
				return -2;
			}
			ret = ret << 4 | char2hex(*p);
			p++;
			if (n >= 0) {
				if (++i == n) {
					break;
				}
			}
		}
	} else {
		if (!isxdigit(*p)) {
			return -1;
		}

		while (isxdigit(*p)) {
			if ((ret & 0xF0000000) != 0) {
				return -2;
			}
			ret = ret << 4 | char2hex(*p);
			p++;
			if (n >= 0) {
				if (++i == n) {
					break;
				}
			}
		}
	}

	*pp = p;
	return ret;
}
char hex2lchar(int i)
{
	if (i < 10) {
		return i + '0';
	} else {
		return i + 'a' - 10;
	}
}
char hex2uchar(int i)
{
	if (i < 10) {
		return i + '0';
	} else {
		return i + 'A' - 10;
	}
}

/**
 * 整数のみ
 * エラー:-1
 */
int parse_int(Str src, int max)
{
	int ret = 0;
	const char *p = src.p;
	const char *end = p + src.size;

	if (p >= end || !isdigit_fox(*p)) {
		return -1;
	}

	while (p < end && isdigit_fox(*p)) {
		ret = ret * 10 + (*p - '0');
		if (ret > max) {
			ret = max;
			break;
		}
		p++;
	}

	return ret;
}

char *str_dup_p(const char *p, int size, Mem *mem)
{
	char *dst;

	if (size < 0) {
		size = strlen(p);
	}
	dst = Mem_get(mem, size + 1);
	memcpy(dst, p, size);
	dst[size] = '\0';

	return dst;
}
char *str_printf(const char *fmt, ...)
{
	StrBuf buf;
	va_list va;

	StrBuf_init(&buf, 256);
	va_start(va, fmt);
	StrBuf_vprintf(&buf, fmt, va);
	va_end(va);

	StrBuf_add_c(&buf, '\0');
	return buf.p;
}

////////////////////////////////////////////////////////////////////////////////

void Mem_init(Mem *mem, int chunk_size)
{
	MemChunk *p = malloc(sizeof(MemChunk) + chunk_size);
	p->next = NULL;

	mem->p = p;
	mem->max = chunk_size;
	mem->pos = 0;
}
void *Mem_get(Mem *mem, int size)
{
	if (mem == NULL) {
		return malloc(size);
	}
	size = (size + 3) & ~3;

#if NO_DEBUG
	if (mem->pos + size > mem->max) {
		int sz = sizeof(MemChunk) + (size > mem->max ? size : mem->max);
		MemChunk *p = malloc(sz);
		p->next = mem->p;
		mem->p = p;
		mem->pos = size;
		return p->buf;
	} else {
		char *ret = mem->p->buf + mem->pos;
		mem->pos += size;
		return ret;
	}
#else
	{
		int sz = sizeof(MemChunk) + size;
		MemChunk *p = malloc(sz);
		p->next = mem->p;
		mem->p = p;
		return p->buf;
	}
#endif
}
void Mem_close(Mem *mem)
{
	MemChunk *p = mem->p;

	while (p != NULL) {
		MemChunk *pre = p;
		p = p->next;
		free(pre);
	}
	mem->p = NULL;
}

////////////////////////////////////////////////////////////////////////////////

int Str_cmp(Str s, Str t)
{
	int i;
	int n = (s.size < t.size ? s.size : t.size);

	for (i = 0; i < n; i++) {
		int c1 = s.p[i] & 0xFF;
		int c2 = t.p[i] & 0xFF;
		if (c1 < c2) {
			return -1;
		} else if (c1 > c2) {
			return 1;
		}
	}

	if (s.size < t.size) {
		return -1;
	} else if (s.size > t.size) {
		return 1;
	}
	return 0;
}
int Str_split(Str src, const char **ret, int n, char c)
{
	const char *p = src.p;
	const char *end = p + src.size;
	int i = 0;

	n--;
	for (;;) {
		const char *top = p;
		while (p < end) {
			if (i < n && *p == c) {
				break;
			}
			p++;
		}
		if (p > top) {
			ret[i++] = str_dup_p(top, p - top, &fg->st_mem);
		} else {
			ret[i++] = NULL;
		}
		if (p < end) {
			p++;
		} else {
			break;
		}
	}
	return i;
}
Str Str_trim(Str src)
{
	while (src.size > 0) {
		if ((src.p[src.size - 1] & 0xFF) <= ' ') {
			src.size--;
		} else {
			break;
		}
	}
	while (src.size > 0) {
		if ((src.p[0] & 0xFF) <= ' ') {
			src.p++;
			src.size--;
		} else {
			break;
		}
	}
	return src;
}

////////////////////////////////////////////////////////////////////////////////

void StrBuf_init(StrBuf *s, int size)
{
	int max = 32;
	while (max <= size) {
		max *= 2;
	}

	s->max = max;
	s->size = 0;
	s->p = malloc(max);
}
void StrBuf_init_refstr(StrBuf *s, int size)
{
	RefStr *rs;
	int max = 32;
	size += offsetof(RefStr, c);

	while (max <= size) {
		max *= 2;
	}

	s->max = max;
	s->size = offsetof(RefStr, c);
	rs = malloc(max);

	rs->rh.n_memb = 0;
	rs->rh.nref = 1;
	rs->rh.weak_ref = NULL;
	rs->size = 0;
	s->p = (char*)rs;
}
void StrBuf_alloc(StrBuf *s, int size)
{
	if (size >= s->max) {
		int max = 32;
		while (max <= size) {
			max *= 2;
		}

		s->max = max;
		s->p = realloc(s->p, max);
	}
	s->size = size;
}
Value StrBuf_str_Value(StrBuf *s, RefNode *type)
{
	RefStr *rs = realloc(s->p, s->size + 1);
	rs->rh.type = type;
	rs->size = s->size - offsetof(RefStr, c);
	rs->c[rs->size] = '\0';
	return vp_Value(rs);
}
int StrBuf_add(StrBuf *s, const char *p, int size)
{
	if (size < 0) {
		size = strlen(p);
	}
	if (s->max <= s->size + size) {
		while (s->max <= s->size + size) {
			s->max *= 2;
		}
		if (s->max > fs->max_alloc) {
			throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
			return FALSE;
		}
		s->p = realloc(s->p, s->max);
	}
	memcpy(s->p + s->size, p, size);
	s->size += size;

	return TRUE;
}
int StrBuf_add_r(StrBuf *s, RefStr *r)
{
	if (s->max <= s->size + r->size) {
		while (s->max <= s->size + r->size) {
			s->max *= 2;
		}
		if (s->max > fs->max_alloc) {
			throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
			return FALSE;
		}
		s->p = realloc(s->p, s->max);
	}
	memcpy(s->p + s->size, r->c, r->size);
	s->size += r->size;

	return TRUE;
}
int StrBuf_add_c(StrBuf *s, char c)
{
	if (s->max <= s->size + 1) {
		s->max *= 2;
		if (s->max > fs->max_alloc) {
			throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
			return FALSE;
		}
		s->p = realloc(s->p, s->max);
	}
	s->p[s->size++] = c;

	return TRUE;
}
int StrBuf_printf(StrBuf *s, const char *fmt, ...)
{
	int ret;
	va_list va;

	va_start(va, fmt);
	ret = StrBuf_vprintf(s, fmt, va);
	va_end(va);
	return ret;
}
int StrBuf_vprintf(StrBuf *s, const char *fmt, va_list va)
{
	const char *p = fmt;

	while (*p != '\0') {
		if (*p == '%') {
			p++;
			switch (*p) {
			case '\0':
				break;
			case '%':    // %% -> %
				if (!StrBuf_add_c(s, '%')) {
					return FALSE;
				}
				break;
			case 's':    // %s -> (const char*)そのまま
				if (!StrBuf_add(s, va_arg(va, const char*), -1)) {
					return FALSE;
				}
				break;
			case 'S': {  // %S -> (Str)そのまま
				Str str = va_arg(va, Str);
				if (!StrBuf_add(s, str.p, str.size)) {
					return FALSE;
				}
				break;
			}
			case 'q': {  // %q -> (const char*)制御コードをquoted
				const char *q = va_arg(va, const char*);
				if (!StrBuf_add_c(s, '"')) {
					return FALSE;
				}
				add_backslashes_bin(s, q, -1);
				if (!StrBuf_add_c(s, '"')) {
					return FALSE;
				}
				break;
			}
			case 'Q': {  // %Q -> (Str)制御コードをquoted
				Str q = va_arg(va, Str);
				if (!StrBuf_add_c(s, '"')) {
					return FALSE;
				}
				add_backslashes_bin(s, q.p, q.size);
				if (!StrBuf_add_c(s, '"')) {
					return FALSE;
				}
				break;
			}
			case 'c': {  // %c -> (int)char型1文字quoted
				char c = va_arg(va, int);
				if (!StrBuf_add_c(s, '\'')) {
					return FALSE;
				}
				add_backslashes_bin(s, &c, 1);
				if (!StrBuf_add_c(s, '\'')) {
					return FALSE;
				}
				break;
			}
			case 'd': {  // %d -> (int)10進
				char ctmp[48];
				sprintf(ctmp, "%d", va_arg(va, int));
				if (!StrBuf_add(s, ctmp, -1)) {
					return FALSE;
				}
				break;
			}
			case 'i': {  // %i -> (int)10進2桁
				char ctmp[48];
				sprintf(ctmp, "%02d", va_arg(va, int));
				if (!StrBuf_add(s, ctmp, -1)) {
					return FALSE;
				}
				break;
			}
			case 'x': {  // %x -> (int)16進8桁
				char ctmp[48];
				sprintf(ctmp, "%08x", va_arg(va, int));
				if (!StrBuf_add(s, ctmp, -1)) {
					return FALSE;
				}
				break;
			}
			case 'D': {  // %D -> (int64_t)10進
				char ctmp[48];
				sprintf(ctmp, "%" FMT_INT64_PREFIX "d", (long long int)va_arg(va, int64_t));
				if (!StrBuf_add(s, ctmp, -1)) {
					return FALSE;
				}
				break;
			}
			case 'n': {  // %n -> (const Node*)名称
				RefNode *n = va_arg(va, RefNode*);
				if (!StrBuf_add_r(s, n->name)) {
					return FALSE;
				}
				break;
			}
			case 'r': {  // %r -> (RefStr*)
				RefStr *rs = va_arg(va, RefStr*);
				if (!StrBuf_add_r(s, rs)) {
					return FALSE;
				}
				break;
			}
			case 'U': {  // %n -> (int)U+?????? (コードポイント16進)
				char ctmp[16];
				int c = va_arg(va, int);
				sprintf(ctmp, "U+%06X", c);
				if (!StrBuf_add(s, ctmp, -1)) {
					return FALSE;
				}
				break;
			}
			case 'v': {  // %v -> (Value)to_str
				Value v = va_arg(va, Value);
				if (Value_type(v) == fs->cls_str) {
					RefStr *rs = Value_vp(v);
					StrBuf_add_c(s, '"');
					add_backslashes_sub(s, rs->c, rs->size, ADD_BACKSLASH_UCS4);
					StrBuf_add_c(s, '"');
				} else if (!StrBuf_add_v(s, v)) {
					return FALSE;
				}
				break;
			}
			}
			p++;
		} else {
			if (!StrBuf_add_c(s, *p)) {
				return FALSE;
			}
			p++;
		}
	}

	return TRUE;
}

int StrBuf_add_v(StrBuf *s, Value v)
{
	if (Value_isint(v)) {
		char cbuf[32];
		sprintf(cbuf, "%d", (int32_t)(v >> 32));
		if (!StrBuf_add(s, cbuf, -1)) {
			return FALSE;
		}
	} else {
		RefNode *type = Value_type(v);
		if (type == fs->cls_str) {
			RefStr *rs = Value_vp(v);
			StrBuf_add_r(s, rs);
		} else if (type == fs->cls_bool) {
			StrBuf_add(s, Value_bool(v) ? "true" : "false", -1);
		} else if (type == fs->cls_module) {
			RefNode *nd = Value_type(v);
			StrBuf_add(s, "Module(", -1);
			StrBuf_add_r(s, nd->name);
			StrBuf_add_c(s, ')');
		} else if (type != fs->cls_null) {
			RefNode *fn = Hash_get_p(&type->u.c.h, fs->str_tostr);
			Value vret;
			RefNode *ret_type;

			if (fn == NULL) {
				throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, type, fs->str_tostr);
				return FALSE;
			}
			if (fn->type != NODE_FUNC && fn->type != NODE_FUNC_N) {
				throw_errorf(fs->mod_lang, "NameError", "to_str is not a member function");
				return FALSE;
			}

			Value_push("v", v);
			if (!call_function(fn, 0)) {
				return FALSE;
			}

			vret = fg->stk_top[-1];
			ret_type = Value_type(fg->stk_top[-1]);
			// 戻り値チェック
			if (ret_type != fs->cls_str) {
				throw_errorf(fs->mod_lang, "TypeError", "Return value must be Str but %n", ret_type);
				return FALSE;
			}
			if (!StrBuf_add_r(s, Value_vp(vret))) {
				return FALSE;
			}
			Value_pop();
		}
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

/*
 * fnameは完全パスに限る
 * \0終端を保証
 */
char *read_from_file(int *psize, const char *path, Mem *mem)
{
	int rd;
	char *p;
	int64_t size;

	FileHandle fd = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
	if (fd == -1) {
		return NULL;
	}
	size = get_file_size(fd);
	if (size == -1 || size >= fs->max_alloc) {
		close_fox(fd);
		return NULL;
	}
	p = Mem_get(mem, size + 1);
	rd = read_fox(fd, p, size);

	if (rd >= 0) {
		p[size] = '\0';
	} else {
		if (mem == NULL) {
			free(p);
		}
		p = NULL;
	}
	close_fox(fd);

	if (psize != NULL) {
		*psize = size;
	}

	return p;
}
int read_all_from_file(StrBuf *buf, int fd, int limit)
{
	char *cbuf = (char*)malloc(BUFFER_SIZE);
	int rd;

	while ((rd = read_fox(fd, cbuf, BUFFER_SIZE)) > 0) {
		if (buf->size + rd > limit) {
			free(cbuf);
			return FALSE;
		}
		StrBuf_add(buf, cbuf, rd);
	}
	free(cbuf);

	return TRUE;
}
int write_to_file(const char *path, Str s, int append)
{
	int fd = open_fox(path, (append ? O_CREAT|O_WRONLY|O_APPEND : O_CREAT|O_WRONLY|O_TRUNC), DEFAULT_PERMISSION);
	int ret;

	if (fd == -1) {
		return FALSE;
	}
	ret = (write_fox(fd, s.p, s.size) == s.size);
	close_fox(fd);

	return ret;
}

int make_directory(const char *path)
{
	int result = TRUE;
	char *cp = str_dup_p(path, -1, NULL);
	char *cp_end = &cp[strlen(cp)];
	char *path_ptr = cp_end - 1;
	int type;

	// 既に存在している
	type = exists_file(cp);
	if (type != EXISTS_NONE) {
		free(cp);
		return type == EXISTS_DIR;
	}

	// ディレクトリが存在する階層を探す
	while (path_ptr > cp) {
		if (*path_ptr == SEP_C) {
			*path_ptr = '\0';
			type = exists_file(cp);
			if (type == EXISTS_DIR) {
				break;
			} else if (type == EXISTS_FILE) {
				free(cp);
				return FALSE;
			}
		}
		path_ptr--;
	}

	// ディレクトリを作成する
	while (path_ptr < cp_end) {
		if (*path_ptr == '\0') {
			*path_ptr = SEP_C;
			if (mkdir_fox(cp, DEFAULT_PERMISSION_DIR) != 0) {
				result = FALSE;
				break;
			}
		}
		path_ptr++;
	}

	free(cp);
	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

int IniTok_load(IniTok *tk, const char *path)
{
	int size;
	tk->buf = read_from_file(&size, path, NULL);

	if (tk->buf != NULL) {
		tk->p = tk->buf;
		tk->end = tk->p + size;
		tk->key = fs->Str_EMPTY;
		tk->val = fs->Str_EMPTY;
		return TRUE;
	} else {
		return FALSE;
	}
}
void IniTok_close(IniTok *tk)
{
	if (tk->buf != NULL) {
		free(tk->buf);
		tk->buf = NULL;
	}
}
/**
 * INITYPE_NONE   : 空行など
 * INITYPE_EOF    : 終了
 * INITYPE_ERROR  : エラー
 * INITYPE_STRING : abc=foo  abc:foo  など
 * INITYPE_FILE   : abc
 */
int IniTok_next(IniTok *tk)
{
	if (tk->p == tk->end) {
		tk->type = INITYPE_EOF;
		return FALSE;
	}
	// #ではじまる行はコメント
	if (*tk->p == '#') {
		goto NEXT_NL;
	}

	// 先頭のスペースを飛ばす
	while (tk->p < tk->end && isspace_fox(*tk->p)) {
		tk->p++;
	}
	if (tk->p == tk->end) {
		tk->type = INITYPE_EOF;
		return FALSE;
	}
	// key値を取得
	tk->key.p = tk->p;
	while (tk->p < tk->end) {
		int ch = *tk->p;
		if (ch == '=' || ch == ':' || ch == '\n') {
			break;
		}
		tk->p++;
	}
	tk->key.size = tk->p - tk->key.p;

	if (tk->p == tk->end || *tk->p == '\n') {
		tk->type = INITYPE_FILE;
		// 末尾の\rを削除
		if (tk->key.size > 0 && tk->key.p[tk->key.size - 1] == '\r') {
			tk->key.size--;
		}
	} else {
		char ch = *tk->p;
		char *top;
		char *end;
		tk->p++;

		// val値の取得
		top = tk->p;
		while (tk->p < tk->end && *tk->p != '\n') {
			tk->p++;
		}
		end = tk->p;
		// 末尾の\rを除去
		if (end > top && end[-1] == '\r') {
			end--;
		}
		if (tk->p < tk->end) {
			tk->p++;
		}

		switch (ch) {
		case ':': // raw string
			tk->type = INITYPE_STRING;
			tk->val = Str_new(top, end - top);
			break;
		case '=': { // backslash escaped
			int size = escape_backslashes_sub(top, top, end - top, FALSE);
			tk->type = INITYPE_STRING;
			tk->val = Str_new(top, size);
			break;
		}
		default:
			tk->type = INITYPE_ERROR;
			break;
		}

	}

	return TRUE;

NEXT_NL:
	// 改行の次に進める
	while (tk->p < tk->end && *tk->p != '\n') {
		tk->p++;
	}
	if (tk->p < tk->end) {
		tk->p++;
	}

	if (tk->p == tk->end) {
		tk->type = INITYPE_EOF;
		return FALSE;
	} else {
		tk->type = INITYPE_NONE;
		return TRUE;
	}
}
/**
 * 入力文字列のバックスラッシュシーケンスを解釈
 * UTF-8文字列として扱う
 *
 * dst  : 出力先(少なくともsrcの長さは必要
 * src  : 入力(dstと同一でも構わない)
 * size : 入力長
 * bin  : バイト列
 *
 * return : 結果の長さ
 */
int escape_backslashes_sub(char *dst, const char *src, int size, int bin)
{
	const char *end = src + size;
	char *dst_top = dst;

	while (src < end) {
		char ch = *src;
		if (ch == '\\') {
			src++;
			if (src < end) {
				char ch2 = *src++;
				switch (ch2) {
				case '0':
					*dst++ = '\0';
					break;
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
				case 'x':
					if (!bin && *src == '{') {
						int ch3;
						src++;
						ch3 = parse_hex(&src, end, -1);
						if (src < end && *src == '}') {
							src++;
							str_add_codepoint(&dst, ch3, NULL);
						}
					} else {
						int ch3 = parse_hex(&src, end, 2);
						if (bin) {
							if (ch3 >= 0) {
								*dst++ = ch3;
							}
						} else {
							if (ch3 >= 0 && ch3 < 0x80) {
								*dst++ = ch3;
							}
						}
					}
					break;
				default:
					if (!isalnum(ch2)) {
						*dst++ = ch2;
					}
					break;
				}
			}
		} else {
			src++;
			*dst++ = ch;
		}
	}

	return dst - dst_top;
}
