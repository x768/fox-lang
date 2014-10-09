#include "fox_vm.h"



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

int32_t str_hash(const char *p, int size)
{
	const char *c;
	const char *end;
	uint32_t h = 0;

	if (size < 0) {
		size = strlen(p);
	}
	end = p + size;

	for (c = p; c < end; c++) {
		h = h * 31 + *c;
	}
	return h & INT32_MAX;
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

	if ((end != NULL && p >= end) || !isxdigit(*p)) {
		return -1;
	}
	while ((p == NULL || p < end) && isxdigit(*p)) {
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

	*pp = p;
	return ret;
}

/**
 * 整数のみ
 * エラー:-1
 */
int parse_int(const char *src_p, int src_size, int max)
{
	int ret = 0;
	const char *p = src_p;
	const char *end;

	if (src_size < 0) {
		src_size = strlen(src_p);
	}

	end = p + src_size;
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

////////////////////////////////////////////////////////////////////////////////////////////

// plus = FALSE : RFC3986
// plus = TRUE  : RFC3986 + (' ' => '+')
void urlencode_sub(StrBuf *buf, const char *p, int size, int plus)
{
	int i;

	for (i = 0; i < size; i++) {
		char ch = p[i];

		if ((ch & 0x80) == 0 && (isalnum(ch) || ch == '.' || ch == '_' || ch == '-')) {
			StrBuf_add_c(buf, ch);
		} else if (plus && ch == ' ') {
			StrBuf_add_c(buf, '+');
		} else {
			StrBuf_add_c(buf, '%');
			StrBuf_add_c(buf, hex2uchar((ch >> 4) & 0xF));
			StrBuf_add_c(buf, hex2uchar(ch & 0xF));
		}
	}
}
void urldecode_sub(StrBuf *buf, const char *p, int size)
{
	int i;

	for (i = 0; i < size; ) {
		if (p[i] == '%') {
			// %XX
			char c;
			i++;
			c = p[i];
			if (isxdigit_fox(c)) {
				int ch = char2hex(c);
				i++;
				c = p[i];
				if (isxdigit_fox(c)) {
					ch <<= 4;
					ch |= char2hex(c);
					i++;
				}
				StrBuf_add_c(buf, ch);
			}
		} else if (p[i] == '+') {
			StrBuf_add_c(buf, ' ');
			i++;
		} else {
			StrBuf_add_c(buf, p[i]);
			i++;
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////

int encode_b64_sub(char *dst, const char *p, int size, int url)
{
	const char *table = (url ?
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" :
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
	char *top = dst;
	const char *src = p;
	const char *e = src + size - 2;
	const char *end = src + size;

	// srcを3バイトずつ
	// dstを4バイトずつ
	while (src < e) {
		unsigned char a = src[0];
		unsigned char b = src[1];
		unsigned char c = src[2];

		dst[0] = table[(a >> 2) & 63];
		dst[1] = table[((a << 4) | (b >> 4)) & 63];
		dst[2] = table[((b << 2) | (c >> 6)) & 63];
		dst[3] = table[c & 63];

		src += 3;
		dst += 4;
	}

	// 残りの部分
	switch (end - src) {
	case 1: {
		unsigned char a = src[0];

		dst[0] = table[(a >> 2) & 63];
		dst[1] = table[(a << 4) & 63];
		if (!url) {
			dst[2] = '=';
			dst[3] = '=';
			dst += 4;
		} else {
			dst += 2;
		}
		break;
	}
	case 2: {
		unsigned char a = src[0];
		unsigned char b = src[1];

		dst[0] = table[(a >> 2) & 63];
		dst[1] = table[((a << 4) | (b >> 4)) & 63];
		dst[2] = table[(b << 2) & 63];
		if (!url) {
			dst[3] = '=';
			dst += 4;
		} else {
			dst += 3;
		}
		break;
	}
	}
	return dst - top;
}

int decode_b64_sub(char *dst, const char *p, int size, int strict)
{
	static const unsigned char * const table = (const unsigned char*)
			/* 00 */ "\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE"
			/* 10 */ "\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE\xFE"
			/* 20 */ "\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x3E\xFF\x3E\xFF\x3F"
			/* 30 */ "\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\xFF\xFF\xFF\xFE\xFF\xFF"
			/* 40 */ "\xFF\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E"
			/* 50 */ "\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xFF\xFF\xFF\xFF\x3F"
			/* 60 */ "\xFF\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x25\x26\x27\x28"
			/* 70 */ "\x29\x2A\x2B\x2C\x2D\x2E\x2F\x30\x31\x32\x33\xFF\xFF\xFF\xFF\xFF"
			;
	int i, j = 0;
	int tmp = 0;
	int bits = 0;

	for (i = 0; i < size; i++) {
		int ch = p[i] & 0xFF;
		if (ch < 0x80) {
			int val = table[ch];
			if (val == 0xFF) {
				if (strict) {
					return -1;
				}
			} else if (val != 0xFE) {
				tmp = (tmp << 6) | val;
				bits += 6;
				if (bits >= 8) {
					bits -= 8;
					dst[j++] = (tmp >> bits);
					// 下から (bits - 8) bitを残す
					tmp &= ~(~1 << bits);
				}
			}
		} else {
			if (strict) {
				return -1;
			}
		}
	}

	return j;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void add_backslashes_sub(StrBuf *buf, const char *src_p, int src_size, int mode)
{
	if (src_size < 0) {
		src_size = strlen(src_p);
	}

	if (mode == ADD_BACKSLASH_UCS2 || mode == ADD_BACKSLASH_UCS4) {
		int i;
		for (i = 0; i < src_size; i++) {
			switch (src_p[i]) {
			case '\0':
				if (mode == ADD_BACKSLASH_UCS4) {
					StrBuf_add(buf, "\\0", 2);
				} else {
					StrBuf_add(buf, "\\u0000", 6);
				}
				break;
			case '\b':
				StrBuf_add(buf, "\\b", 2);
				break;
			case '\f':
				StrBuf_add(buf, "\\f", 2);
				break;
			case '\n':
				StrBuf_add(buf, "\\n", 2);
				break;
			case '\r':
				StrBuf_add(buf, "\\r", 2);
				break;
			case '\t':
				StrBuf_add(buf, "\\t", 2);
				break;
			case '\'':
				StrBuf_add(buf, "\\'", 2);
				break;
			case '\"':
				StrBuf_add(buf, "\\\"", 2);
				break;
			case '\\':
				StrBuf_add(buf, "\\\\", 2);
				break;
			default: {
				int c = src_p[i] & 0xFF;
				if (c < ' ') {
					char c_buf[8];
					if (mode == ADD_BACKSLASH_UCS4) {
						sprintf(c_buf, "\\u%02x", c);
						StrBuf_add(buf, c_buf, 4);
					} else {
						sprintf(c_buf, "\\u%04x", c);
						StrBuf_add(buf, c_buf, 6);
					}
				} else {
					StrBuf_add_c(buf, c);
				}
				break;
			}
			}
		}
	} else {
		const unsigned char *p = p = (unsigned char*)src_p;
		const unsigned char *end = p + src_size;

		while (p < end) {
			int c = *p;
			int code;

			// UTF-8をコードポイントにする
			if ((c & 0x80) == 0) {
				code = c;
				p++;
			} else if ((c & 0xE0) == 0xC0) {
				code = ((c & 0x1F) << 6) | (p[1] & 0x3F);
				p += 2;
			} else if ((c & 0xF0) == 0xE0) {
				code = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
				p += 3;
			} else {
				code = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12)  | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
				p += 4;
			}
			switch (code) {
			case '\0':
				if (mode == ADD_BACKSLASH_U_UCS4) {
					StrBuf_add(buf, "\\0", 2);
				} else {
					StrBuf_add(buf, "\\u0000", 6);
				}
				break;
			case '\b':
				StrBuf_add(buf, "\\b", 2);
				break;
			case '\f':
				StrBuf_add(buf, "\\f", 2);
				break;
			case '\n':
				StrBuf_add(buf, "\\n", 2);
				break;
			case '\r':
				StrBuf_add(buf, "\\r", 2);
				break;
			case '\t':
				StrBuf_add(buf, "\\t", 2);
				break;
			case '\'':
				StrBuf_add(buf, "\\'", 2);
				break;
			case '\"':
				StrBuf_add(buf, "\\\"", 2);
				break;
			case '/':
				if (mode == ADD_BACKSLASH_U_UCS2) {
					StrBuf_add(buf, "\\/", 2);
				} else {
					StrBuf_add_c(buf, '/');
				}
				break;
			case '\\':
				StrBuf_add(buf, "\\\\", 2);
				break;
			default:
				if (code >= ' ' && code < 0x7F) {
					StrBuf_add_c(buf, c);
				} else if (code < 0x10000) {  // BMP
					char c_buf[8];
					sprintf(c_buf, "\\u%04x", code);
					StrBuf_add(buf, c_buf, 6);
				} else {  // サロゲート
					char c_buf[16];
					int hi, lo;
					code -= 0x10000;
					hi = code / 0x400 + 0xD800;
					lo = code % 0x400 + 0xDC00;
					sprintf(c_buf, "\\u%04x\\u%04x", hi, lo);
					StrBuf_add(buf, c_buf, 12);
				}
				break;
			}
		}
	}
}
void add_backslashes_bin(StrBuf *buf, const char *src_p, int src_size)
{
	int i;

	if (src_size < 0) {
		src_size = strlen(src_p);
	}
	for (i = 0; i < src_size; i++) {
		switch (src_p[i]) {
		case '\0':
			StrBuf_add(buf, "\\0", 2);
			break;
		case '\b':
			StrBuf_add(buf, "\\b", 2);
			break;
		case '\f':
			StrBuf_add(buf, "\\f", 2);
			break;
		case '\n':
			StrBuf_add(buf, "\\n", 2);
			break;
		case '\r':
			StrBuf_add(buf, "\\r", 2);
			break;
		case '\t':
			StrBuf_add(buf, "\\t", 2);
			break;
		case '\'':
			StrBuf_add(buf, "\\'", 2);
			break;
		case '\"':
			StrBuf_add(buf, "\\\"", 2);
			break;
		case '\\':
			StrBuf_add(buf, "\\\\", 2);
			break;
		default: {
			int c = src_p[i] & 0xFF;
			if (c < ' ' || c >= 0x7f) {
				char c_buf[8];
				sprintf(c_buf, "\\x%02x", c);
				StrBuf_add(buf, c_buf, 4);
			} else {
				StrBuf_add_c(buf, c);
			}
			break;
		}
		}
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

///////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * <>&!"を変換
 * br = TRUE の場合は改行を<br>に変換
 */
void convert_html_entity(StrBuf *buf, const char *p, int size, int br)
{
	const char *end;

	if (size < 0) {
		size = strlen(p);
	}
	end = p + size;

	for (; p < end; p++) {
		switch (*p) {
		case '<':
			StrBuf_add(buf, "&lt;", 4);
			break;
		case '>':
			StrBuf_add(buf, "&gt;", 4);
			break;
		case '&':
			StrBuf_add(buf, "&amp;", 5);
			break;
		case '"':
			StrBuf_add(buf, "&quot;", 6);
			break;
		case '\n':
			if (br) {
				StrBuf_add(buf, "<br>", 4);
			} else {
				StrBuf_add_c(buf, '\n');
			}
			break;
		default:
			StrBuf_add_c(buf, *p);
			break;
		}
	}
}

int str_add_codepoint(char **pdst, int ch, const char *error_type)
{
	char *dst = *pdst;

	if (ch < 0) {
		if (error_type != NULL) {
			throw_errorf(fs->mod_lang, error_type, "Invalid codepoint (< 0)");
		}
		return FALSE;
	} else if (ch < 0x80) {
		*dst++ = ch;
	} else if (ch < 0x800) {
		*dst++ = 0xC0 | (ch >> 6);
		*dst++ = 0x80 | (ch & 0x3F);
	} else if (ch < 0xD800) {
		*dst++ = 0xE0 | (ch >> 12);
		*dst++ = 0x80 | ((ch >> 6) & 0x3F);
		*dst++ = 0x80 | (ch & 0x3F);
	} else if (ch < 0xE000) {
		if (error_type != NULL) {
			throw_errorf(fs->mod_lang, error_type, "Invalid codepoint (Surrogate) %U", ch);
		}
		return FALSE;
	} else if (ch < 0x10000) {
		*dst++ = 0xE0 | (ch >> 12);
		*dst++ = 0x80 | ((ch >> 6) & 0x3F);
		*dst++ = 0x80 | (ch & 0x3F);
	} else if (ch < 0x110000) {
		*dst++ = 0xF0 | (ch >> 18);
		*dst++ = 0x80 | ((ch >> 12) & 0x3F);
		*dst++ = 0x80 | ((ch >> 6) & 0x3F);
		*dst++ = 0x80 | (ch & 0x3F);
	} else {
		if (error_type != NULL) {
			throw_errorf(fs->mod_lang, error_type, "Invalid codepoint (> 0x10FFFF)");
		}
		return FALSE;
	}
	*pdst = dst;
	return TRUE;
}

int is_string_only_ascii(const char *p, int size, const char *except)
{
	int i;

	if (size < 0) {
		size = strlen(p);
	}
	for (i = 0; i < size; i++) {
		int c = p[i] & 0xFF;
		if (c < ' ' || c >= 0x7F) {
			return FALSE;
		}
		if (except != NULL) {
			if (strchr(except, c) != NULL) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 不正なシーケンスが見つかったらその位置を返す
 * 最短形式UTF8文字列なら-1を返す
 */
int invalid_utf8_pos(const char *src_p, int size)
{
	const char *p = src_p;
	const char *end;
	int last = 0;
	if (size < 0) {
		size = strlen(p);
	}
	end = p + size;

	while (p < end) {
		const char *cend;
		int c = *p;

		if ((c & 0x80) == 0) {
			p++;
			continue;
		} else if ((c & 0xE0) == 0xC0) {
			// 非最短型
			if ((c & 0xFF) < 0xC2) {
				return p - src_p;
			}
			last = 1;
		} else if ((c & 0xF0) == 0xE0) {
			// サロゲートのチェック
			cend = p + 3;
			if (cend <= end){
				switch (c & 0xFF) {
				case 0xE0:
					// 非最短型
					if ((p[1] & 0xFF) < 0xA0) {
						return p - src_p;
					}
					break;
				case 0xED:
					if ((p[1] << 8 | p[2]) >= 0xA080) {  // Surrogate開始(D800)
						return p - src_p;
					}
					break;
				case 0xEE:
					if ((p[1] << 8 | p[2]) < 0x8080) {   // Surrogate終了(E000)
						return p - src_p;
					}
					break;
				}
			} else {
				return p - src_p;
			}
			last = 2;
		} else if ((c & 0xF8) == 0xF0) {
			if ((c & 0xFF) == 0xF0) {
				// 非最短型
				if ((p[1] & 0xFF) < 0x90) {
					return p - src_p;
				}
			}
			last = 3;
		} else {
			return p - src_p;
		}

		p++;
		cend = p + last;
		if (cend > end) {
			return p - src_p;
		}

		while (p < cend) {
			if ((*p & 0xC0) != 0x80) {
				return p - src_p;
			}
			p++;
		}
	}

	return -1;
}

/**
 * エラーのある箇所を代替文字に置換
 */
void alt_utf8_string(StrBuf *dst, const char *p, int size)
{
	enum {
		ALT_SIZE = 3,
	};
	const char *alt = "\xEF\xBF\xBD";

	for (;;) {
		int pos = invalid_utf8_pos(p, size);
		if (pos >= 0) {
			StrBuf_add(dst, p, pos);
			StrBuf_add(dst, alt, ALT_SIZE);
			if (pos < size) {
				pos++;
			}
			// 文字の境界まで飛ばす
			while (pos < size && (p[pos] & 0xC0) == 0x80){
				pos++;
			}
			p += pos;
			size -= pos;
		} else {
			StrBuf_add(dst, p, size);
			break;
		}
	}
}
/**
 * utf8_next(&ptr, end);
 * ptrを、次のコードポイントの位置に動かす
 */
void utf8_next(const char **pp, const char *end)
{
	const char *p = *pp;

	if (p < end) {
		if ((*p & 0x80) == 0) {
			p += 1;
		} else {
			p++;
			while (p < end && (*p & 0xC0) == 0x80) {
				p++;
			}
		}
		*pp = p;
	}
}

/**
 * UTF-8の正当性チェックは行わない
 */
int utf8_codepoint_at(const char *p)
{
	int c = *p;
	int code;

	if ((c & 0x80) == 0) {
		code = c & 0x7F;
	} else if ((c & 0xE0) == 0xC0) {
		code = ((c & 0x1F) << 6) | (p[1] & 0x3F);
	} else if ((c & 0xF0) == 0xE0) {
		code = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
	} else {
		code = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12)  | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
	}
	return code;
}

/**
 * 文字数インデックスをbytesインデックスに変換する
 * idx >= 0 の場合は先頭から数える
 * 範囲外の場合はsrc.size
 * idx < 0 の場合は末尾から数える
 * 範囲外の場合は-1
 */
int utf8_position(const char *p, int size, int idx)
{
	if (idx >= 0) {
		int i;
		for (i = 0; i < size; i++) {
			// 文字の境界
			if ((p[i] & 0xC0) != 0x80){
				if (idx == 0) {
					return i;
				}
				idx--;
			}
		}
		return size;
	} else {
		int i;
		idx = -idx - 1;

		for (i = size - 1; i >= 0; i--) {
			// 文字の境界
			if ((p[i] & 0xC0) != 0x80){
				if (idx == 0) {
					return i;
				}
				idx--;
			}
		}
		return -1;
	}
}

/**
 * 泣き別れになっている場合、その部分を取り除く
 */
Str fix_utf8_part(Str s)
{
	const char *p = s.p;
	const char *end = p + s.size;

	if (s.size == 0) {
		return s;
	}

	// 文字列の最初
	while (p < end && (*p & 0xC0) == 0x80) {
		p++;
	}
	s.p = p;
	s.size = end - p;

	// 文字列の最後
	if (p < end) {
		const char *ptr = end - 1;
		int c;

		if ((*ptr & 0x80) == 0) {
			return s;
		}

		while (ptr >= p && (*ptr & 0xC0) == 0x80) {
			ptr--;
		}
		if (ptr < p) {
			return fs->Str_EMPTY;
		}
		c = *ptr;
		if ((c & 0xE0) == 0xC0){
			if (end - ptr == 2) {
				return s;
			}
		} else if ((c & 0xF0) == 0xE0){
			if (end - ptr == 3) {
				return s;
			}
		} else if ((c & 0xF8) == 0xF0){
			if (end - ptr == 4) {
				return s;
			}
		}
		s.size = ptr - s.p;
	}
	return s;
}

/**
 * UTF8のコードポイント数
 * 不正・非最短形式の場合は未定義
 */
int strlen_utf8(const char *ptr, int len)
{
	const unsigned char *p = (const unsigned char*)ptr;
	const unsigned char *end = p + len;
	int count = 0;

	for (; p < end; p++) {
		if ((*p & 0xC0) != 0x80){
			count++;
		}
	}
	return count;
}

