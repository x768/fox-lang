#define DEFINE_GLOBALS
#include "m_xml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>



enum {
	TK_EOS,
	TK_ERROR,

	TK_TAG_START,     // <
	TK_TAG_END,       // >
	TK_TAG_END_CLOSE, // />
	TK_TAG_CLOSE,     // </abc>
	TK_CMD_START,     // <?xml
	TK_CMD_END,       // ?>
	TK_DOCTYPE,       // <!hoge

	TK_ATTR_NAME,     // key
	TK_STRING,        // "value"
	TK_EQUAL,         // =

	TK_TEXT,
};
enum {
	LOAD_XML = 1,
	LOAD_FILE = 2,
};
enum {
	TTYPE_NO_ENDTAG = 1,
	TTYPE_HEAD = 2,
	TTYPE_PCDATA = 4,
	TTYPE_RAW = 8,
	TTYPE_BLOCK = 16,     // ブロック要素
	TTYPE_C_INLINE = 32,  // 非ブロック要素だけを含む
	TTYPE_NO_SELF = 64,   // 自身と同じ要素を直下に含むことができない
};

typedef struct {
	const char *name;
	int code;
} Entity;

typedef struct {
	int type;
	Str val;
	char *p;
	const char *end;
	int line;
	int xml;
	int loose;
} XMLTok;

typedef struct {
	Value *html;
	RefArray *html_ra;
	Value *html_vm;

	Value *head;
	RefArray *head_ra;
	Value *head_vm;

	Value *title;
	RefArray *title_ra;
	Value *title_vm;

	Value *body;
	RefArray *body_ra;
	Value *body_vm;
} HTMLDoc;

static Str get_elem_name(XMLTok *tk)
{
	int first = TRUE;
	Str ret;
	ret.p = tk->p;
	
	// 最初の1文字:Letter
	// 2文字目以降:Letter Digit - _ : .
	if (tk->p < tk->end) {
		for (;;) {
			int ch = fs->utf8_codepoint_at(tk->p);
			int type = uni->ch_get_category(ch);

			if (first) {
				if (type < CATE_Ll || type > CATE_Mn) {
					break;
				}
			} else {
				if (type < CATE_Ll || type > CATE_No) {
					if (ch != '-' && ch != '_' && ch != ':' && ch != '.') {
						break;
					}
				}
			}

			fs->utf8_next((const char**)&tk->p, tk->end);
			first = FALSE;
		}
	}
	ret.size = tk->p - ret.p;

	return ret;
}
static int is_valid_elem_name(Str s)
{
	const char *p = s.p;
	const char *end = p + s.size;
	int first = TRUE;

	while (p < end) {
		int ch = fs->utf8_codepoint_at(p);
		int type = uni->ch_get_category(ch);

		if (first) {
			if (type < CATE_Ll || type > CATE_Mn) {
				return FALSE;
			}
		} else {
			if (type < CATE_Ll || type > CATE_No) {
				if (ch != '-' && ch != '_' && ch != ':' && ch != '.') {
					return FALSE;
				}
			}
		}

		fs->utf8_next(&p, end);
		first = FALSE;
	}
	return TRUE;
}

static int char2hex(char ch)
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
static int Str_eqi_head(Str s, const char *p)
{
	int i;

	for (i = 0; i < s.size; i++) {
		if (tolower_fox(s.p[i]) != tolower_fox(p[i])) {
			return FALSE;
		}
	}
	return TRUE;
}
static int Str_isascii(Str s)
{
	int i;

	for (i = 0; i < s.size; i++) {
		if ((s.p[i] & 0x80) != 0) {
			return FALSE;
		}
	}
	return TRUE;
}
static int Str_isspace(Str s)
{
	int i;

	for (i = 0; i < s.size; i++) {
		if (!isspace_fox(s.p[i])) {
			return FALSE;
		}
	}
	return TRUE;
}

static Str Str_trim(Str src)
{
	int i, j;
	Str s;

	// 左の空白を除去
	for (i = 0; i < src.size; i++) {
		if (!isspace_fox(src.p[i])) {
			break;
		}
	}
	// 右の空白を除去
	for (j = src.size; j > i; j--) {
		if (!isspace_fox(src.p[j - 1])) {
			break;
		}
	}
	s.p = src.p + i;
	s.size = j - i;
	return s;
}

/**
 * 16進数として解釈可能なところまでポインタを進める
 *
 * n   : 最大文字数 or -1
 *
 * 変換できなければ-1
 * オーバーフローしたら-2
 */
static int parse_hex(char **pp, int n)
{
	char *p = *pp;
	int ret = 0;
	int i = 0;

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

	*pp = p;
	return ret;
}

static NativeFunc get_function_ptr(RefNode *node, RefStr *name)
{
	RefNode *n = fs->get_node_member(fs->cls_list, name, NULL);
	if (n == NULL || n->type != NODE_FUNC_N) {
		fs->fatal_errorf(NULL, "Initialize module 'xml' failed at get_function_ptr(%r)", name);
	}
	return n->u.f.u.fn;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int resolve_entity(const char *p, const char *end)
{
	static const Entity entities[] = {
			{"AElig", 198},
			{"Aacute", 193},
			{"Acirc", 194},
			{"Agrave", 192},
			{"Alpha", 913},
			{"Aring", 197},
			{"Atilde", 195},
			{"Auml", 196},
			{"Beta", 914},
			{"Ccedil", 199},
			{"Chi", 935},
			{"Dagger", 8225},
			{"Delta", 916},
			{"ETH", 208},
			{"Eacute", 201},
			{"Ecirc", 202},
			{"Egrave", 200},
			{"Epsilon", 917},
			{"Eta", 919},
			{"Euml", 203},
			{"Gamma", 915},
			{"Iacute", 205},
			{"Icirc", 206},
			{"Igrave", 204},
			{"Iota", 921},
			{"Iuml", 207},
			{"Kappa", 922},
			{"Lambda", 923},
			{"Mu", 924},
			{"Ntilde", 209},
			{"Nu", 925},
			{"OElig", 338},
			{"Oacute", 211},
			{"Ocirc", 212},
			{"Ograve", 210},
			{"Omega", 937},
			{"Omicron", 927},
			{"Oslash", 216},
			{"Otilde", 213},
			{"Ouml", 214},
			{"Phi", 934},
			{"Pi", 928},
			{"Prime", 8243},
			{"Psi", 936},
			{"Rho", 929},
			{"Scaron", 352},
			{"Sigma", 931},
			{"THORN", 222},
			{"Tau", 932},
			{"Theta", 920},
			{"Uacute", 218},
			{"Ucirc", 219},
			{"Ugrave", 217},
			{"Upsilon", 933},
			{"Uuml", 220},
			{"Xi", 926},
			{"Yacute", 221},
			{"Yuml", 376},
			{"Zeta", 918},
			{"aacute", 225},
			{"acirc", 226},
			{"acute", 180},
			{"aelig", 230},
			{"agrave", 224},
			{"alefsym", 8501},
			{"alpha", 945},
			{"amp", 38},
			{"and", 8743},
			{"ang", 8736},
			{"apos", 39},
			{"aring", 229},
			{"asymp", 8776},
			{"atilde", 227},
			{"auml", 228},
			{"bdquo", 8222},
			{"beta", 946},
			{"brvbar", 166},
			{"bull", 8226},
			{"cap", 8745},
			{"ccedil", 231},
			{"cedil", 184},
			{"cent", 162},
			{"chi", 967},
			{"circ", 710},
			{"clubs", 9827},
			{"cong", 8773},
			{"copy", 169},
			{"crarr", 8629},
			{"cup", 8746},
			{"curren", 164},
			{"dArr", 8659},
			{"dagger", 8224},
			{"darr", 8595},
			{"deg", 176},
			{"delta", 948},
			{"diams", 9830},
			{"divide", 247},
			{"eacute", 233},
			{"ecirc", 234},
			{"egrave", 232},
			{"empty", 8709},
			{"emsp", 8195},
			{"ensp", 8194},
			{"epsilon", 949},
			{"equiv", 8801},
			{"eta", 951},
			{"eth", 240},
			{"euml", 235},
			{"euro", 8364},
			{"ewierp", 8472},
			{"exist", 8707},
			{"fnof", 402},
			{"forall", 8704},
			{"frac12", 189},
			{"frac14", 188},
			{"frac34", 190},
			{"frasl", 8260},
			{"gamma", 947},
			{"ge", 8805},
			{"gt", 62},
			{"hArr", 8660},
			{"harr", 8596},
			{"hearts", 9829},
			{"hellip", 8230},
			{"iacute", 237},
			{"icirc", 238},
			{"iexcl", 161},
			{"igrave", 236},
			{"image", 8465},
			{"infin", 8734},
			{"int", 8747},
			{"iota", 953},
			{"iquest", 191},
			{"isin", 8712},
			{"iuml", 239},
			{"kappa", 954},
			{"lArr", 8656},
			{"lambda", 955},
			{"lang", 9001},
			{"laquo", 171},
			{"larr", 8592},
			{"lceil", 8968},
			{"ldquo", 8220},
			{"le", 8804},
			{"lfloor", 8970},
			{"lowast", 8727},
			{"loz", 9674},
			{"lrm", 8206},
			{"lsaquo", 8249},
			{"lsquo", 8216},
			{"lt", 60},
			{"macr", 175},
			{"mdash", 8212},
			{"micro", 181},
			{"middot", 183},
			{"minus", 8722},
			{"mu", 956},
			{"nabla", 8711},
			{"nbsp", 160},
			{"ndash", 8211},
			{"ne", 8800},
			{"ni", 8715},
			{"not", 172},
			{"notin", 8713},
			{"nsub", 8836},
			{"ntilde", 241},
			{"nu", 957},
			{"oacute", 243},
			{"ocirc", 244},
			{"oelig", 339},
			{"ograve", 242},
			{"oline", 8254},
			{"omega", 969},
			{"omicron", 959},
			{"oplus", 8853},
			{"or", 8744},
			{"ordf", 170},
			{"ordm", 186},
			{"oslash", 248},
			{"otilde", 245},
			{"otimes", 8855},
			{"ouml", 246},
			{"para", 182},
			{"part", 8706},
			{"permil", 8240},
			{"perp", 8869},
			{"phi", 966},
			{"pi", 960},
			{"piv", 982},
			{"plusmn", 177},
			{"pound", 163},
			{"prime", 8242},
			{"prod", 8719},
			{"prop", 8733},
			{"psi", 968},
			{"quot", 34},
			{"rArr", 8658},
			{"radic", 8730},
			{"rang", 9002},
			{"raquo", 187},
			{"rarr", 8594},
			{"rceil", 8969},
			{"rdquo", 8221},
			{"real", 8476},
			{"reg", 174},
			{"rfloor", 8971},
			{"rho", 961},
			{"rlm", 8207},
			{"rsaquo", 8250},
			{"rsquo", 8217},
			{"sbquo", 8218},
			{"scaron", 353},
			{"sdot", 8901},
			{"sect", 167},
			{"shy", 173},
			{"sigma", 963},
			{"sigmaf", 962},
			{"sim", 8764},
			{"spades", 9824},
			{"sub", 8834},
			{"sube", 8838},
			{"sum", 8721},
			{"sup", 8835},
			{"sup1", 185},
			{"sup2", 178},
			{"sup3", 179},
			{"supe", 8839},
			{"szlig", 223},
			{"tau", 964},
			{"there4", 8756},
			{"theta", 952},
			{"thetasym", 977},
			{"thinsp", 8201},
			{"thorn", 254},
			{"tilde", 732},
			{"times", 215},
			{"trade", 8482},
			{"uArr", 8657},
			{"uacute", 250},
			{"uarr", 8593},
			{"ucirc", 251},
			{"ugrave", 249},
			{"uml", 168},
			{"upsih", 978},
			{"upsilon", 965},
			{"uuml", 252},
			{"xi", 958},
			{"yacute", 253},
			{"yen", 165},
			{"yuml", 255},
			{"zeta", 950},
			{"zwj", 8205},
			{"zwnj", 8204},
	};

	int size = end - p;
	int lo = 0;
	int hi = sizeof(entities) / sizeof(entities[0]) - 1;

	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		const Entity *e = &entities[mid];
		int cmp = memcmp(p, e->name, size);
		if (cmp < 0) {
			hi = mid - 1;
		} else if (cmp > 0) {
			lo = mid + 1;
		} else {
			return e->code;
		}
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////

static void xml_elem_init(Value *v, RefArray **p_ra, Value **p_vm, const char *name_p, int name_size)
{
	Ref *r = fs->new_ref(cls_elem);
	RefArray *ra = fs->refarray_new(0);

	r->v[INDEX_CHILDREN] = vp_Value(ra);
	*v = vp_Value(r);

	ra->rh.type = cls_nodelist;
	if (p_ra != NULL) {
		*p_ra = Value_vp(r->v[INDEX_CHILDREN]);
	}

	r->v[INDEX_ATTR] = vp_Value(fs->refmap_new(0));
	if (p_vm != NULL) {
		*p_vm = &r->v[INDEX_ATTR];
	}

	r->v[INDEX_NAME] = fs->cstr_Value(fs->cls_str, name_p, name_size);
}
/**
 *  vの子要素にvbaseがあればTRUE
 */
static int xml_elem_has_elem_sub(Value vbase, RefArray *ra)
{
	int i;
	for (i = 0; i < ra->size; i++) {
		Value pv = ra->p[i];
		if (fs->Value_type(pv) == cls_elem) {
			Ref *r2 = Value_vp(pv);
			RefArray *ra2 = Value_vp(r2->v[INDEX_CHILDREN]);
			if (xml_elem_has_elem_sub(vbase, ra2)) {
				return TRUE;
			}
		}
	}
	return FALSE;
}
static int xml_elem_has_elem(Value vbase, Value v)
{
	if (vbase == v) {
		return TRUE;
	}
	if (fs->Value_type(v) == cls_elem) {
		Ref *r = Value_vp(v);
		RefArray *ra = Value_vp(r->v[INDEX_CHILDREN]);
		if (xml_elem_has_elem_sub(vbase, ra)) {
			return TRUE;
		}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////

static void XMLTok_init(XMLTok *tk, char *src, const char *end, int xml, int loose)
{
	tk->p = src;
	tk->end = end;
	tk->type = TK_EOS;
	tk->xml = xml;
	tk->loose = loose;
	tk->line = 1;
}

static int resolve_xml_entity(const char *p, const char *end)
{
	int len = end - p;

	switch (len) {
	case 2:
		if (memcmp(p, "lt", 2) == 0) {
			return '<';
		} else if (memcmp(p, "gt", 2) == 0) {
			return '>';
		}
		break;
	case 3:
		if (memcmp(p, "amp", 3) == 0) {
			return '&';
		}
		break;
	case 4:
		if (memcmp(p, "quot", 4) == 0) {
			return '&';
		} else if (memcmp(p, "apos", 4) == 0) {
			return '\'';
		}
		break;
	}
	return -1;
}
static int XMLTok_parse_entity(char **ppdst, XMLTok *tk)
{
	char *dst = *ppdst;
	char *tmp = tk->p;
	int code = -1;

	tk->p++;
	if (*tk->p == '#') {
		tk->p++;
		if (*tk->p == 'x' || *tk->p == 'X') {
			tk->p++;
			code = parse_hex(&tk->p, 8);
			if (code >= 0) {
				if (*tk->p == ';') {
					tk->p++;
				} else {
					code = -1;
				}
			}
		} else if (isdigit_fox(*tk->p)){
			code = 0;
			while (isdigit_fox(*tk->p)) {
				if (code >= 0) {
					code = code * 10 + (*tk->p - '0');
					if (code >= 0x110000) {
						code = -1;
					}
				}
				tk->p++;
			}
			if (code >= 0) {
				if (*tk->p == ';') {
					tk->p++;
				} else {
					code = -1;
				}
			}
		}
	} else {
		const char *top = tk->p;
		while (tk->p < tk->end) {
			int ch = *tk->p;
			if (!isdigit_fox(ch) && !isupper_fox(ch) && !islower_fox(ch)) {
				break;
			}
			tk->p++;
		}
		if (*tk->p == ';') {
			if (tk->loose) {
				code = resolve_entity(top, tk->p);
			} else {
				code = resolve_xml_entity(top, tk->p);
			}
			tk->p++;
		} else {
			code = -1;
		}
	}
	if (code >= 0) {
		// #???;をUTF-8にする
		if (code < 0x80) {
			*dst++ = code;
		} else if (code < 0x800) {
			*dst++ = 0xC0 | (code >> 6);
			*dst++ = 0x80 | (code & 0x3F);
		} else if (code < 0xD800) {
			*dst++ = 0xE0 | (code >> 12);
			*dst++ = 0x80 | ((code >> 6) & 0x3F);
			*dst++ = 0x80 | (code & 0x3F);
		} else if (code < 0xDC00) {
			// サロゲート(不正なコード)
			if (!tk->loose) {
				return FALSE;
			}
			memcpy(dst, "\xEF\xBF\xBD", 3);
			dst += 3;
		} else if (code < 0x10000) {
			*dst++ = 0xE0 | (code >> 12);
			*dst++ = 0x80 | ((code >> 6) & 0x3F);
			*dst++ = 0x80 | (code & 0x3F);
		} else if (code < 0x110000) {
			*dst++ = 0xF0 | (code >> 18);
			*dst++ = 0x80 | ((code >> 12) & 0x3F);
			*dst++ = 0x80 | ((code >> 6) & 0x3F);
			*dst++ = 0x80 | (code & 0x3F);
		} else {
			if (!tk->loose) {
				return FALSE;
			}
			// 不正なコード
			memcpy(dst, "\xEF\xBF\xBD", 3);
			dst += 3;
		}
	} else {
		if (!tk->loose) {
			return FALSE;
		}
		tk->p = tmp + 1;
		*dst++ = '&';
	}

	*ppdst = dst;
	return TRUE;
}
static void XMLTok_parse_text(XMLTok *tk)
{
	char *dst = tk->p;
	tk->val.p = dst;

	while (*tk->p != '\0') {
		int ch = *tk->p;

		if (ch == '<') {
			if (tk->p[1] == '!' && tk->p[2] == '-' && tk->p[3] == '-') {
				// コメントを飛ばす
				tk->p += 4;
				while (*tk->p != '\0') {
					if (tk->p[0] == '-' && tk->p[1] == '-' && tk->p[2] == '>') {
						tk->p += 3;
						break;
					}
					tk->p++;
				}
				if (*tk->p == '\0' && !tk->loose) {
					// コメントが閉じていない
					tk->type = TK_ERROR;
					return;
				}
			} else if (tk->xml) {
				// <![CDATA[が出現したら ]]>まで飛ばす
				if (memcmp(tk->p + 1, "![CDATA[", 8) == 0) {
					tk->p += 9;
					while (*tk->p != '\0') {
						if (tk->p[0] == ']' && tk->p[1] == ']' && tk->p[2] == '>') {
							tk->p += 3;
							break;
						}
						if (*tk->p == '\n') {
							tk->line++;
						}
						*dst++ = *tk->p;
						tk->p++;
					}
					if (*tk->p == '\0' && !tk->loose) {
						// 閉じていない
						tk->type = TK_ERROR;
						return;
					}
				} else {
					break;
				}
			} else {
				break;
			}
		} else if (ch == '&') {
			if (!XMLTok_parse_entity(&dst, tk)) {
				tk->type = TK_ERROR;
				return;
			}
		} else {
			if (ch == '\n') {
				tk->line++;
			}
			*dst++ = ch;
			tk->p++;
		}
	}

	tk->val.size = dst - tk->val.p;
	tk->type = TK_TEXT;
}
static void XMLTok_parse_string(XMLTok *tk, int term)
{
	char *dst = tk->p;
	tk->val.p = dst;

	while (*tk->p != '\0') {
		int ch = *tk->p;

		if (ch == term) {
			tk->p++;
			tk->val.size = dst - tk->val.p;
			tk->type = TK_STRING;
			return;
		} else if (ch == '&') {
			XMLTok_parse_entity(&dst, tk);
		} else {
			if (ch == '\n') {
				tk->line++;
			}
			*dst++ = ch;
			tk->p++;
		}
	}
}
static void XMLTok_tolower(XMLTok *tk)
{
	char *p = (char*)tk->val.p;
	const char *end = p + tk->val.size;

	while (p < end) {
		*p = tolower_fox(*p);
		p++;
	}
}
/**
 * タグの外
 */
static void XMLTok_next(XMLTok *tk)
{
START:
	switch (*tk->p) {
	case '\0':
		tk->type = TK_EOS;
		return;
	case '<':
		switch (tk->p[1]) {
		case '!':
			if (tk->p[2] == '-' && tk->p[3] == '-') {
				// コメント
				tk->p += 4;
				while (tk->p[0] != '\0') {
					if (tk->p[0] == '-' && tk->p[1] == '-' && tk->p[2] == '>') {
						tk->p += 3;
						break;
					}
					tk->p++;
				}
				goto START;
			} else if (memcmp(tk->p + 1, "![CDATA[", 8) == 0) {
				XMLTok_parse_text(tk);
			} else {
				tk->p += 2;
				tk->type = TK_DOCTYPE;
			}
			break;
		case '?': // <?tag
			tk->p += 2;
			tk->val = get_elem_name(tk);
			if (tk->val.size == 0) {
				tk->type = TK_ERROR;
				return;
			}
			tk->type = TK_CMD_START;
			return;
		case '/':  // </tag>
			tk->p += 2;
			tk->val = get_elem_name(tk);
			if (tk->val.size == 0) {
				tk->type = TK_ERROR;
				return;
			}
			if (!tk->xml) {
				// HTMLはタグ名を小文字に変換
				XMLTok_tolower(tk);
			}
			while (isspace_fox(*tk->p)) {
				tk->p++;
			}
			if (*tk->p != '>') {
				tk->type = TK_ERROR;
				return;
			}
			tk->p++;
			tk->type = TK_TAG_CLOSE;
			break;
		default:
			tk->p++;
			tk->val = get_elem_name(tk);
			if (tk->val.size == 0) {
				tk->type = TK_ERROR;
				return;
			}
			if (!tk->xml) {
				// HTMLはタグ名を小文字に変換
				XMLTok_tolower(tk);
			}
			tk->type = TK_TAG_START;
			return;
		}
		break;
	default:
		XMLTok_parse_text(tk);
		break;
	}
}

/**
 * タグの中
 */
static void XMLTok_next_tag(XMLTok *tk)
{
	while (isspace_fox(*tk->p)) {
		if (*tk->p == '\n') {
			tk->line++;
		}
		tk->p++;
	}
	switch (*tk->p) {
	case '\0':
		tk->type = TK_EOS;
		return;
	case '>':
		tk->p++;
		tk->type = TK_TAG_END;
		break;
	case '/':
		if (tk->p[1] == '>') {
			tk->p += 2;
			tk->type = TK_TAG_END_CLOSE;
		}
		break;
	case '"':
		tk->p++;
		XMLTok_parse_string(tk, '"');
		break;
	case '\'':
		tk->p++;
		XMLTok_parse_string(tk, '\'');
		break;
	case '?':
		if (tk->p[1] == '>') {
			tk->p += 2;
			tk->type = TK_CMD_END;
		} else {
			tk->p++;
			tk->type = TK_ERROR;
		}
		break;
	case '=':
		tk->p++;
		tk->type = TK_EQUAL;
		break;
	default:
		tk->val = get_elem_name(tk);
		if (tk->val.size > 0) {
			tk->type = TK_ATTR_NAME;
		} else {
			tk->type = TK_ERROR;
		}
		break;
	}
}
/**
 * <script></script>
 */
static void XMLTok_next_raw(XMLTok *tk, Str tag_name)
{
	tk->val.p = tk->p;

	for (;;) {
		if (tk->p[0] == '<' && tk->p[1] == '/') {
			if (Str_eqi_head(tag_name, tk->p + 2)) {
				tk->val.size = tk->p - tk->val.p;
				tk->p += 2 + tag_name.size;
				break;
			} else {
				tk->p += 2;
			}
		} else if (*tk->p == '\0') {
			tk->val.size = tk->p - tk->val.p;
			break;
		} else {
			tk->p++;
		}
	}
	while (isspace_fox(*tk->p)) {
		tk->p++;
	}
	if (*tk->p == '>') {
		tk->p++;
	}
	tk->type = TK_TEXT;
}

static void XMLTok_skip_next(XMLTok *tk)
{
	while (isspace_fox(*tk->p)) {
		tk->p++;
	}
	XMLTok_next(tk);
}
static int XMLTok_isspace(XMLTok *tk)
{
	const char *p = tk->val.p;
	const char *end = p + tk->val.size;

	while (p < end) {
		if (!isspace_fox(*p)) {
			return FALSE;
		}
		p++;
	}
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 既定値 : インライン要素で、何でも含むことができる
 */
static int get_tag_type(Str s)
{
	if (s.size == 0) {
		return 0;
	}

	switch (s.p[0]) {
	case 'a':
		if (str_eq(s.p, s.size, "a", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "address", -1)) {
			// 内容はinlineとpのみ
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "area", -1)) {
			return TTYPE_NO_ENDTAG;
		}
		break;
	case 'b':
		if (str_eq(s.p, s.size, "br", -1)) {
			return TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "base", -1)) {
			return TTYPE_HEAD | TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "basefont", -1)) {
			return TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "bdo", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "b", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "big", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "body", -1)) {
			return TTYPE_BLOCK;
		}
		break;
	case 'c':
		if (str_eq(s.p, s.size, "center", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "caption", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "colgroup", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "col", -1)) {
			return TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "code", -1)) {
			return TTYPE_NO_ENDTAG;
		}
		break;
	case 'd':
		if (str_eq(s.p, s.size, "div", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "dfn", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "dl", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "dt", -1)) {
			return TTYPE_BLOCK | TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "dd", -1)) {
			return TTYPE_BLOCK | TTYPE_NO_SELF;
		}
		break;
	case 'e':
		if (str_eq(s.p, s.size, "em", -1)) {
			return TTYPE_C_INLINE;
		}
		break;
	case 'f':
		if (str_eq(s.p, s.size, "form", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "fieldset", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "font", -1)) {
			return TTYPE_C_INLINE;
		}
		break;
	case 'h':
		if (s.size == 2 && s.p[1] >= '1' && s.p[1] <= '6') {
			return TTYPE_BLOCK | TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "hr", -1)) {
			return TTYPE_BLOCK | TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "head", -1)) {
			return TTYPE_BLOCK;
		}
		break;
	case 'i':
		if (str_eq(s.p, s.size, "img", -1)) {
			return TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "input", -1)) {
			return TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "i", -1)) {
			return TTYPE_C_INLINE;
		}
		break;
	case 'l':
		if (str_eq(s.p, s.size, "link", -1)) {
			return TTYPE_HEAD | TTYPE_NO_ENDTAG;
		} else if (str_eq(s.p, s.size, "li", -1)) {
			return TTYPE_BLOCK | TTYPE_NO_SELF;
		} else if (str_eq(s.p, s.size, "legent", -1)) {
			return TTYPE_BLOCK;
		}
		break;
	case 'm':
		if (str_eq(s.p, s.size, "meta", -1)) {
			return TTYPE_HEAD | TTYPE_NO_ENDTAG;
		}
		break;
	case 'n':
		if (str_eq(s.p, s.size, "noscript", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "noframes", -1)) {
			return TTYPE_BLOCK;
		}
		break;
	case 'o':
		if (str_eq(s.p, s.size, "ol", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "optgroup", -1)) {
			return TTYPE_BLOCK;
		} else if (str_eq(s.p, s.size, "option", -1)) {
			return TTYPE_BLOCK;
		}
		break;
	case 'p':
		if (str_eq(s.p, s.size, "p", -1)) {
			return TTYPE_BLOCK | TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "pre", -1)) {
			return TTYPE_BLOCK | TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "param", -1)) {
			return TTYPE_NO_ENDTAG;
		}
		break;
	case 'q':
		if (str_eq(s.p, s.size, "q", -1)) {
			return TTYPE_C_INLINE;
		}
		break;
	case 's':
		if (str_eq(s.p, s.size, "script", -1)) {
			return TTYPE_BLOCK | TTYPE_RAW;
		} else if (str_eq(s.p, s.size, "style", -1)) {
			return TTYPE_HEAD | TTYPE_RAW;
		} else if (str_eq(s.p, s.size, "span", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "strong", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "sub", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "sup", -1)) {
			return TTYPE_C_INLINE;
		} else if (str_eq(s.p, s.size, "small", -1)) {
			return TTYPE_C_INLINE;
		}
		break;
	case 't':
		if (s.size == 2) {
			if (s.p[1] == 'r' || s.p[1] == 'd' || s.p[1] == 'h') {
				return TTYPE_BLOCK | TTYPE_NO_SELF;
			} else if (s.p[1] == 't') {
				return TTYPE_C_INLINE;
			}
		} else if (s.size == 5) {
			if (str_eq(s.p, s.size, "title", -1)) {
				return TTYPE_HEAD | TTYPE_PCDATA | TTYPE_BLOCK;
			} else if (str_eq(s.p, s.size, "table", -1)) {
				return TTYPE_BLOCK;
			} else if (str_eq(s.p, s.size, "thead", -1)) {
				return TTYPE_BLOCK;
			} else if (str_eq(s.p, s.size, "tfoot", -1)) {
				return TTYPE_BLOCK;
			} else if (str_eq(s.p, s.size, "tbody", -1)) {
				return TTYPE_BLOCK;
			}
		} else if (str_eq(s.p, s.size, "textarea", -1)) {
			return TTYPE_RAW;
		}
		break;
	case 'u':
		if (str_eq(s.p, s.size, "ul", -1)) {
			return TTYPE_BLOCK;
		}
		break;
	}
	return 0;
}
static int get_tag_type_icase(Str s)
{
	char cbuf[16];
	const char *p = s.p + s.size;
	int i;
	int size;

	while (p > s.p) {
		if (p[-1] == ':') {
			break;
		}
		p--;
	}
	size = s.size - (p - s.p);

	for (i = 0; i < size && i < 15; i++) {
		cbuf[i] = tolower_fox(p[i]);
	}
	cbuf[i] = '\0';
	return get_tag_type(Str_new(cbuf, -1));
}

static void parse_pcdata(RefArray *ra, Str tag_name, XMLTok *tk)
{
	if (ra->size > 0) {
		return;
	}

	for (;;) {
		switch (tk->type) {
		case TK_EOS:
			return;
		case TK_TAG_START:
			// ">" まで無視する
			XMLTok_next_tag(tk);
			for (;;) {
				switch (tk->type) {
				case TK_EOS:
					return;
				case TK_TAG_END:
					XMLTok_next(tk);
					goto BREAK_TAG;
				default:
					XMLTok_next_tag(tk);
					break;
				}
			}
			break;
		case TK_TAG_CLOSE:
			XMLTok_tolower(tk);
			if (str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
				XMLTok_next(tk);
				return;
			}
			XMLTok_next(tk);
			break;
		case TK_TEXT:
			if (tk->val.size > 0) {
				Value *vp = fs->refarray_push(ra);
				*vp = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
			}
			XMLTok_next(tk);
			break;
		}
	BREAK_TAG:
		;
	}
}
static void parse_raw_text(RefArray *ra, XMLTok *tk)
{
	if (tk->type == TK_TEXT) {
		Value *vp = fs->refarray_push(ra);
		*vp = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
		XMLTok_next(tk);
	}
}
/**
 * v, v_ra : 親要素
 * <p>aaa<p>bbbのように、閉じタグが省略されている場合、FALSEを返す
 */
static int parse_html_sub(Value *v, RefArray *v_ra, HTMLDoc *html, XMLTok *tk, Str parent_tag)
{
	switch (tk->type) {
	case TK_EOS:
		return TRUE;
	case TK_ERROR:
		// 無視
		XMLTok_next(tk);
		break;
	case TK_CMD_START:  // <?
		// ?>まで無視する
		XMLTok_next_tag(tk);

		for (;;) {
			switch (tk->type) {
			case TK_EOS:
				return TRUE;
			case TK_CMD_END:
				XMLTok_next(tk);
				goto BREAK_ALL;
			default:
				XMLTok_next_tag(tk);
				break;
			}
		}
		break;
	case TK_DOCTYPE:  // <!
		// >まで無視する
		XMLTok_next_tag(tk);

		for (;;) {
			switch (tk->type) {
			case TK_EOS:
				return TRUE;
			case TK_TAG_END:
				XMLTok_next(tk);
				goto BREAK_ALL;
			default:
				XMLTok_next_tag(tk);
				break;
			}
		}
		break;
	case TK_TAG_START: {
		RefArray *ra;
		Value *vm;
		Str tag_name = tk->val;
		int tag_type = get_tag_type(tag_name);

		if ((tag_type & TTYPE_BLOCK) != 0) {
			// ブロック要素を含むことができない要素の中にブロック要素が出現した
			int parent_type = get_tag_type(parent_tag);
			if ((parent_type & TTYPE_C_INLINE) != 0) {
				return FALSE;
			}
		}
		if ((tag_type & TTYPE_NO_SELF) != 0) {
			// 自身と同じ要素を含むことができない要素の中に自身と同じ要素が出現した
			if (str_eq(tag_name.p, tag_name.size, parent_tag.p, parent_tag.size)) {
				return FALSE;
			}
		}

		if (Str_eq_p(tag_name, "html")) {
			vm = html->html_vm;
			ra = html->html_ra;
		} else if (Str_eq_p(tag_name, "head")) {
			vm = html->head_vm;
			ra = html->head_ra;
		} else if (Str_eq_p(tag_name, "body")) {
			vm = html->body_vm;
			ra = html->body_ra;
		} else if (Str_eq_p(tag_name, "title")) {
			vm = html->title_vm;
			ra = html->title_ra;
		} else if ((tag_type & TTYPE_HEAD) != 0) {
			// titleを一番下に持ってくる
			Value *vp;
			ra = html->head_ra;
			fs->refarray_push(ra);
			vp = &ra->p[ra->size - 2];
			ra->p[ra->size - 1] = *vp;
			xml_elem_init(vp, &ra, &vm, tag_name.p, tag_name.size);
		} else {
			Value *vp = fs->refarray_push(v_ra);
			xml_elem_init(vp, &ra, &vm, tag_name.p, tag_name.size);
		}
		XMLTok_next_tag(tk);

		for (;;) {
			switch (tk->type) {
			case TK_EOS:
				return TRUE;
			case TK_TAG_END:
				if ((tag_type & TTYPE_RAW) != 0) {
					XMLTok_next_raw(tk, tag_name);
				} else {
					XMLTok_next(tk);
				}
				goto BREAK_TAG;
			case TK_TAG_END_CLOSE:
				XMLTok_next(tk);
				// 閉じタグを省略
				tag_type |= TTYPE_NO_ENDTAG;
				goto BREAK_TAG;
			case TK_ATTR_NAME: {
				Str skey;
				XMLTok_tolower(tk);
				skey = tk->val;
				XMLTok_next_tag(tk);
				if (tk->type == TK_EQUAL) {
					XMLTok_next_tag(tk);
					if (tk->type == TK_STRING || tk->type == TK_ATTR_NAME) {
						Value key = fs->cstr_Value(fs->cls_str, skey.p, skey.size);
						HashValueEntry *ve = fs->refmap_add(Value_vp(*vm), key, FALSE, FALSE);
						if (ve != NULL) {
							ve->val = fs->cstr_Value(fs->cls_str, tk->val.p, tk->val.size);
						}
						fs->Value_dec(key);
						XMLTok_next_tag(tk);
					}
				} else {
					Value key = fs->cstr_Value(fs->cls_str, skey.p, skey.size);
					HashValueEntry *ve = fs->refmap_add(Value_vp(*vm), key, FALSE, FALSE);
					if (ve != NULL) {
						ve->val = key;
					} else {
						fs->Value_dec(key);
					}
				}
				break;
			}
			default:
				XMLTok_next_tag(tk);
				break;
			}
		}
	BREAK_TAG:
		if ((tag_type & TTYPE_NO_ENDTAG) == 0) {
			if ((tag_type & TTYPE_PCDATA) != 0) {
				parse_pcdata(ra, tag_name, tk);
			} else if ((tag_type & TTYPE_RAW) != 0) {
				parse_raw_text(ra, tk);
			} else {
				while (tk->type != TK_TAG_CLOSE && tk->type != TK_EOS) {
					if (!parse_html_sub(vm, ra, html, tk, tag_name)) {
						// 閉じタグが省略されている
						goto BREAK_TAG2;
					}
				}
				if (tk->type != TK_EOS) {
					if (str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
						XMLTok_next(tk);
					}
				}
			BREAK_TAG2:
				;
			}
		}
		break;
	}
	case TK_TEXT: {
		if (!XMLTok_isspace(tk)) {
			Value *vp = fs->refarray_push(v_ra);
			*vp = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
		}
		XMLTok_next(tk);
		break;
	}
	default:
		// エラーは無視
		XMLTok_next(tk);
		break;
	}

BREAK_ALL:
	return TRUE;
}
static int parse_html_body(Value *html, XMLTok *tk)
{
	HTMLDoc ht;
	Str body = Str_new("body", 4);

	ht.html = html;
	xml_elem_init(ht.html, &ht.html_ra, &ht.html_vm, "html", 4);
	ht.head = fs->refarray_push(ht.html_ra);
	xml_elem_init(ht.head, &ht.head_ra, &ht.head_vm, "head", 4);
	ht.title = fs->refarray_push(ht.head_ra);
	xml_elem_init(ht.title, &ht.title_ra, &ht.title_vm, "title", 5);
	ht.body = fs->refarray_push(ht.html_ra);
	xml_elem_init(ht.body, &ht.body_ra, &ht.body_vm, body.p, body.size);

	while (tk->type != TK_EOS) {
		if (!parse_html_sub(ht.body, ht.body_ra, &ht, tk, body)) {
			return FALSE;
		}
	}
	return TRUE;
}
static int parse_xml_begin(Value *v, XMLTok *tk)
{
	if (tk->type == TK_CMD_START) {
		// ?>まで無視する
		XMLTok_next_tag(tk);

		for (;;) {
			switch (tk->type) {
			case TK_EOS:
				fs->throw_errorf(mod_xml, "XMLError", "Missing '?>'");
				return FALSE;
			case TK_ERROR:
				fs->throw_errorf(mod_xml, "XMLError", "Unknown token at line %d", tk->line);
				return FALSE;
			case TK_CMD_END:
				XMLTok_next(tk);
				goto BREAK_CMD;
			case TK_ATTR_NAME: {
				XMLTok_next_tag(tk);
				if (tk->type == TK_EQUAL) {
					XMLTok_next_tag(tk);
					if (tk->type == TK_STRING) {
						XMLTok_next_tag(tk);
					} else {
						fs->throw_errorf(mod_xml, "XMLError", "Missing string after '=' at line %d", tk->line);
						return FALSE;
					}
				}
				break;
			}
			default:
				fs->throw_errorf(mod_xml, "XMLError", "Unexpected token at line %d", tk->line);
				return FALSE;
			}
		}
	BREAK_CMD:
		;
	}
	if (tk->type == TK_TEXT && XMLTok_isspace(tk)) {
		XMLTok_next(tk);
	}
	if (tk->type == TK_DOCTYPE) {
		// >まで無視する
		XMLTok_next_tag(tk);

		for (;;) {
			switch (tk->type) {
			case TK_EOS:
				fs->throw_errorf(mod_xml, "XMLError", "Missing '>'");
				return FALSE;
			case TK_ERROR:
				fs->throw_errorf(mod_xml, "XMLError", "Unknown token at line %d", tk->line);
				return FALSE;
			case TK_TAG_END:
				XMLTok_next(tk);
				goto BREAK_DOCTYPE;
			case TK_ATTR_NAME:
				XMLTok_next_tag(tk);
				break;
			case TK_STRING:
				XMLTok_next_tag(tk);
				break;
			default:
				fs->throw_errorf(mod_xml, "XMLError", "Unexpected token at line %d", tk->line);
				return FALSE;
			}
		}
	BREAK_DOCTYPE:
		;
	}
	if (tk->type == TK_TEXT && XMLTok_isspace(tk)) {
		XMLTok_next(tk);
	}

	return TRUE;
}
/**
 * 一つの要素または1つのテキスト要素を解析する
 */
static int parse_xml_body(Value *v, XMLTok *tk, int first)
{
	switch (tk->type) {
	case TK_TAG_START: {
		RefArray *ra;
		Value *vm;
		Str tag_name = tk->val;
		xml_elem_init(v, &ra, &vm, tag_name.p, tag_name.size);
		XMLTok_next_tag(tk);

		for (;;) {
			switch (tk->type) {
			case TK_EOS:
				fs->throw_errorf(mod_xml, "XMLError", "Missing '>' at line %d", tk->line);
				return FALSE;
			case TK_ERROR:
				fs->throw_errorf(mod_xml, "XMLError", "Unknown token at line %d", tk->line);
				return FALSE;
			case TK_TAG_END_CLOSE:
				XMLTok_next(tk);
				goto BREAK_ALL;
			case TK_TAG_END:
				XMLTok_next(tk);
				for (;;) {
					switch (tk->type) {
					case TK_TAG_START:
					case TK_TEXT: {
						Value *ve = fs->refarray_push(ra);
						if (!parse_xml_body(ve, tk, FALSE)) {
							return FALSE;
						}
						break;
					}
					case TK_TAG_CLOSE:
						if (!str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
							if (!tk->loose) {
								fs->throw_errorf(mod_xml, "XMLError", "Tag name mismatch (<%S>...</%S>) at line %d",
										tag_name, tk->val, tk->line);
								return FALSE;
							}
							// 閉じタグが対応していない場合は、閉じタグが省略されているとみなす
							XMLTok_tolower(tk);
							if (str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
								// 大文字小文字を同一視(エラー処理)
								XMLTok_next(tk);
							}
						} else {
							XMLTok_next(tk);
						}
						goto BREAK_ALL;
					default:
						fs->throw_errorf(mod_xml, "XMLError", "Unknown token at line %d", tk->line);
						return FALSE;
					}
				}
			case TK_ATTR_NAME: {
				Str skey = tk->val;

				XMLTok_next_tag(tk);
				if (tk->type == TK_EQUAL) {
					XMLTok_next_tag(tk);
					if (tk->type == TK_STRING || (tk->loose && tk->type == TK_ATTR_NAME)) {
						Value key = fs->cstr_Value(fs->cls_str, skey.p, skey.size);
						HashValueEntry *ve = fs->refmap_add(Value_vp(*vm), key, tk->loose, TRUE);

						if (ve == NULL) {
							fs->Value_dec(key);
							return FALSE;
						}
						ve->val = fs->cstr_Value(fs->cls_str, tk->val.p, tk->val.size);
						fs->Value_dec(key);
						XMLTok_next_tag(tk);
					} else {
						fs->throw_errorf(mod_xml, "XMLError", "Missing string after '=' at line %d", tk->line);
						return FALSE;
					}
				} else {
					if (tk->loose) {
						Value key = fs->cstr_Value(fs->cls_str, skey.p, skey.size);
						HashValueEntry *ve = fs->refmap_add(Value_vp(*vm), key, tk->loose, TRUE);

						if (ve == NULL) {
							fs->Value_dec(key);
							return FALSE;
						}
						ve->val = key;
						fs->Value_dec(key);
					} else {
						fs->throw_errorf(mod_xml, "XMLError", "Missing '=' at line %d", tk->line);
						return FALSE;
					}
				}
				break;
			}
			default:
				fs->throw_errorf(mod_xml, "XMLError", "Unexpected token at line %d", tk->line);
				return FALSE;
			}
		}
		break;
	}
	case TK_TEXT:
		if (first) {
			fs->throw_errorf(mod_xml, "XMLError", "Unexpected token at line %d", tk->line);
			return FALSE;
		} else {
			*v = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
			XMLTok_next(tk);
		}
		break;
	case TK_EOS:
		break;
	case TK_ERROR:
		fs->throw_errorf(mod_xml, "XMLError", "Unknown token at line %d", tk->line);
		return FALSE;
	default:
		fs->throw_errorf(mod_xml, "XMLError", "Unexpected token at line %d", tk->line);
		return FALSE;
	}

BREAK_ALL:
	return TRUE;
}

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
	if (!fs->StrBuf_add_c(sb, '\0')) {
		return FALSE;
	}
	sb->size--;
	return TRUE;
}
static int file_read_all(StrBuf *sb, int fh)
{
	char *buf = malloc(BUFFER_SIZE);

	for (;;) {
		int size = read_fox(fh, buf, BUFFER_SIZE);
		if (size <= 0) {
			break;
		}
		if (!fs->StrBuf_add(sb, buf, size)) {
			return FALSE;
		}
	}
	if (!fs->StrBuf_add_c(sb, '\0')) {
		return FALSE;
	}
	sb->size--;
	return TRUE;
}
static int parse_xml_from_str(Value *vret, const char *src_p, int src_size)
{
	XMLTok tk;
	char *buf = fs->str_dup_p(src_p, src_size, NULL);

	// XML Strict
	XMLTok_init(&tk, buf, buf + src_size, TRUE, FALSE);
	XMLTok_skip_next(&tk);  // 最初のスペースは無視

	if (!parse_xml_body(vret, &tk, TRUE)) {
		free(buf);
		return FALSE;
	}
	free(buf);

	return TRUE;
}
static RefCharset *detect_bom(const char *p, int size)
{
	if (size >= 3) {
		if (memcmp(p, "\xEF\xBB\xBF", 3) == 0) {
			return fs->cs_utf8;
		}
	}
	if (size >= 2) {
		if (memcmp(p, "\xFE\xFF", 2) == 0) {
			// UTF-16BE
			return fs->get_charset_from_name("UTF-16", -1);
		} else if (memcmp(p, "\xFF\xFE", 2) == 0) {
			// UTF-16LE
			return fs->get_charset_from_name("UTF-16LE", -1);
		}
	}
	return NULL;
}
static RefCharset *detect_charset(const char *p, int size)
{
	// (charset|encoding)
	// =
	// ("|')?
	// [0-9A-Za-z\-_]+
	int i;

	if (size > 512) {
		size = 512;
	}

	for (i = 0; i < size - 10; i++) {
		if (memcmp(p + i, "charset", 7) == 0) {
			i += 7;
			goto FOUND;
		} else if (memcmp(p + i, "encoding", 8) == 0) {
			i += 8;
			goto FOUND;
		}
	}
	return fs->cs_utf8;

FOUND:
	while (i < size) {
		if (p[i] == '=') {
			RefCharset *cs;
			const char *begin;
			const char *end;

			i++;
			while (i < size && isspace_fox(p[i])) {
				i++;
			}
			if (i < size && (p[i] == '"' || p[i] == '\'')) {
				i++;
			}
			begin = p + i;
			while (i < size) {
				int ch = p[i];
				if (!isalnumu_fox(ch) && ch != '-') {
					break;
				}
				i++;
			}
			end = p + i;
			cs = fs->get_charset_from_name(begin, end - begin);
			if (cs != NULL) {
				return cs;
			} else {
				return fs->cs_utf8;
			}
		}
		i++;
	}
	return fs->cs_utf8;
}
static void convert_encoding_sub(StrBuf *sb, const char *p, int size, RefCharset *cs)
{
	fs->convert_bin_to_str_sub(NULL, sb, p, size, cs, TRUE);
	fs->StrBuf_add_c(sb, '\0');
}
static int parse_xml(Value *vret, Value *v, RefNode *node)
{
	int mode = FUNC_INT(node);
	int xml = (mode & LOAD_XML) != 0;
	XMLTok tk;
	char *buf = NULL;
	int buf_size = 0;
	Value v1 = v[1];
	RefNode *v1_type = fs->Value_type(v1);
	int loose = !xml;
	RefCharset *cs = NULL;
	StrBuf sb_conv;

	if (fg->stk_top > v + 2) {
		if (xml) {
			if (Value_bool(v[2])) {
				loose = TRUE;
			}
		} else {
			cs = Value_vp(v[2]);
		}
	}

	if ((mode & LOAD_FILE) != 0) {
		StrBuf sb;
		Str path_s;
		int fh = -1;
		char *path = fs->file_value_to_path(&path_s, v1, 0);

		if (path == NULL) {
			return FALSE;
		}
		fh = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
		if (fh == -1) {
			fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
			free(path);
			return FALSE;
		}
		fs->StrBuf_init(&sb, 0);
		if (!file_read_all(&sb, fh)) {
			close_fox(fh);
			free(path);
			return FALSE;
		}
		close_fox(fh);
		free(path);

		buf = sb.p;
		buf_size = sb.size;
	} else {
		if (v1_type == fs->cls_str || v1_type == fs->cls_bytes) {
			RefStr *src = Value_vp(v1);
			buf = fs->str_dup_p(src->c, src->size, NULL);
			buf_size = src->size;
			if (v1_type == fs->cls_str) {
				cs = fs->cs_utf8;
			}
		} else if (v1_type == fs->cls_bytesio) {
			RefBytesIO *mb = Value_vp(v1);
			buf = fs->str_dup_p(mb->buf.p, mb->buf.size, NULL);
			buf_size = mb->buf.size;
		} else if (fs->is_subclass(v1_type, fs->cls_streamio)) {
			StrBuf sb;
			fs->StrBuf_init(&sb, 0);
			if (!stream_read_all(&sb, v1)) {
				return FALSE;
			}
			buf = sb.p;
			buf_size = sb.size;
		} else {
			fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_sequence, fs->cls_streamio, v1_type, 1);
			return FALSE;
		}
	}

	{
		RefCharset *cs_bom = detect_bom(buf, buf_size);

		fs->StrBuf_init(&sb_conv, 0);
		if (cs_bom == fs->cs_utf8) {
			// UTF-8
			XMLTok_init(&tk, buf + 3, buf + buf_size, xml, loose);
		} else if (cs_bom != NULL) {
			// UTF-16
			convert_encoding_sub(&sb_conv, buf + 2, buf_size - 2, cs_bom);
			XMLTok_init(&tk, sb_conv.p, sb_conv.p + sb_conv.size, xml, loose);
		} else {
			if (cs == NULL) {
				cs = detect_charset(buf, buf_size);
			}
			if (cs == fs->cs_utf8) {
				XMLTok_init(&tk, buf, buf + buf_size, xml, loose);
			} else {
				convert_encoding_sub(&sb_conv, buf, buf_size, cs);
				XMLTok_init(&tk, sb_conv.p, sb_conv.p + sb_conv.size, xml, loose);
			}
		}
	}

	XMLTok_skip_next(&tk);  // 最初のスペースは無視

	if (tk.xml) {
		if (!parse_xml_begin(vret, &tk)) {
			goto ERROR_END;
		}
		if (!parse_xml_body(vret, &tk, TRUE)) {
			goto ERROR_END;
		}
	} else {
		if (!parse_html_body(vret, &tk)) {
			goto ERROR_END;
		}
	}
	free(buf);
	StrBuf_close(&sb_conv);

	return TRUE;

ERROR_END:
	free(buf);
	StrBuf_close(&sb_conv);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int text_convert_html(StrBuf *buf, Str src, int quot, int ascii)
{
	const char *p = src.p;
	const char *end = p + src.size;

	while (p < end) {
		switch (*p) {
		case '<':
			if (!fs->StrBuf_add(buf, "&lt;", 4)) {
				return FALSE;
			}
			p++;
			break;
		case '>':
			if (!fs->StrBuf_add(buf, "&gt;", 4)) {
				return FALSE;
			}
			p++;
			break;
		case '&':
			if (!fs->StrBuf_add(buf, "&amp;", 5)) {
				return FALSE;
			}
			p++;
			break;
		case '"':
			if (quot) {
				if (!fs->StrBuf_add(buf, "&quot;", 6)) {
					return FALSE;
				}
			} else {
				if (!fs->StrBuf_add_c(buf, '"')) {
					return FALSE;
				}
			}
			p++;
			break;
		default:
			if (ascii && (*p & 0x80) != 0) {
				char cbuf[16];
				int code = fs->utf8_codepoint_at(p);
				sprintf(cbuf, "&#%d;", code);
				if (!fs->StrBuf_add(buf, cbuf, -1)) {
					return FALSE;
				}
				fs->utf8_next(&p, end);
			} else {
				if (!fs->StrBuf_add_c(buf, *p)) {
					return FALSE;
				}
				p++;
			}
			break;
		}
	}
	return TRUE;
}
static int html_attr_no_value(Str name)
{
	static const char *list[] = {
		"checked", "disabled", "readonly", "multiple", "selected",
	};
	int i;

	for (i = 0; i < lengthof(list); i++) {
		if (str_eqi(name.p, name.size, list[i], -1)) {
			return TRUE;
		}
	}
	return FALSE;
}
/**
 * xmlを文字列化
 */
static int node_tostr_xml(StrBuf *buf, Value v, int html, int ascii, int level)
{
	if (fs->Value_type(v) == cls_text) {
		Str s = fs->Value_str(v);
		if (level >= 0) {
			s = Str_trim(s);
			if (s.size == 0) {
				s.p = " ";
				s.size = 1;
			}
		}
		if (!text_convert_html(buf, s, FALSE, ascii)) {
			return FALSE;
		}
	} else {
		Ref *r = Value_vp(v);
		RefArray *ra = Value_vp(r->v[INDEX_CHILDREN]);
		RefMap *rm = Value_vp(r->v[INDEX_ATTR]);
		Str tag_name = fs->Value_str(r->v[INDEX_NAME]);
		int i;

		if (ascii && !Str_isascii(tag_name)) {
			fs->throw_errorf(mod_xml, "XMLError", "Tag name contains not ascii chars.");
			return FALSE;
		}

		if (!fs->StrBuf_add_c(buf, '<') || !fs->StrBuf_add(buf, tag_name.p, tag_name.size)) {
			return FALSE;
		}

		for (i = 0; i < rm->entry_num; i++) {
			HashValueEntry *h = rm->entry[i];
			for (; h != NULL; h = h->next) {
				Str key = fs->Value_str(h->key);
				if (html && html_attr_no_value(key)) {
					if (!fs->StrBuf_add_c(buf, ' ') ||
						!fs->StrBuf_add(buf, key.p, key.size)) {
						return FALSE;
					}
				} else {
					if (!fs->StrBuf_add_c(buf, ' ') ||
						!fs->StrBuf_add(buf, key.p, key.size) ||
						!fs->StrBuf_add(buf, "=\"", 2)) {
						return FALSE;
					}
					if (!text_convert_html(buf, fs->Value_str(h->val), TRUE, ascii)) {
						return FALSE;
					}
					if (!fs->StrBuf_add_c(buf, '"')) {
						return FALSE;
					}
				}
			}
		}
		if (html) {
			int type = get_tag_type_icase(tag_name);

			if ((type & TTYPE_NO_ENDTAG) != 0) {
				if (!fs->StrBuf_add_c(buf, '>')) {
					return FALSE;
				}
				return TRUE;
			} else if ((type & TTYPE_RAW) != 0) {
				if (!fs->StrBuf_add_c(buf, '>')) {
					return FALSE;
				}
				for (i = 0; i < ra->size; i++) {
					if (fs->Value_type(ra->p[i]) == cls_text) {
						if (!fs->StrBuf_add_r(buf, Value_vp(ra->p[i]))) {
							return FALSE;
						}
					}
				}
				if (!fs->StrBuf_add(buf, "</", 2) || !fs->StrBuf_add(buf, tag_name.p, tag_name.size) || !fs->StrBuf_add_c(buf, '>')) {
					return FALSE;
				}
				return TRUE;
			}
		}

		if (ra->size > 0) {
			int prt;
			if (!fs->StrBuf_add_c(buf, '>')) {
				return FALSE;
			}

			if (level >= 0) {
				// 子要素が、ブロック要素を含まず、スペース以外のテキストを含む場合、整形せず表示
				int has_block = FALSE;
				int space_only = TRUE;
				prt = TRUE;

				for (i = 0; i < ra->size; i++) {
					Value vp = ra->p[i];
					if (fs->Value_type(vp) == cls_text) {
						if (!Str_isspace(fs->Value_str(vp))) {
							space_only = FALSE;
						}
					} else if (html) {
						Ref *rp = Value_vp(vp);
						int type2 = get_tag_type_icase(fs->Value_str(rp->v[INDEX_NAME]));
						if ((type2 & TTYPE_BLOCK) != 0) {
							has_block = TRUE;
						}
					} else {
						has_block = TRUE;
					}
				}
				if (!has_block && !space_only) {
					prt = FALSE;
				}
			} else {
				prt = FALSE;
			}
			if (prt) {
				int j;
				int lv = level + 2;
				for (i = 0; i < ra->size; i++) {
					if (fs->Value_type(ra->p[i]) == cls_text) {
						if (Str_isspace(fs->Value_str(ra->p[i]))) {
							continue;
						}
					}
					if (!fs->StrBuf_add_c(buf, '\n')) {
						return FALSE;
					}
					for (j = 0; j < lv; j++) {
						if (!fs->StrBuf_add_c(buf, ' ')) {
							return FALSE;
						}
					}
					if (!node_tostr_xml(buf, ra->p[i], html, ascii, lv)) {
						return FALSE;
					}
				}

				if (!fs->StrBuf_add_c(buf, '\n')) {
					return FALSE;
				}
				for (j = 0; j < level; j++) {
					if (!fs->StrBuf_add_c(buf, ' ')) {
						return FALSE;
					}
				}
			} else {
				for (i = 0; i < ra->size; i++) {
					if (!node_tostr_xml(buf, ra->p[i], html, ascii, level)) {
						return FALSE;
					}
				}
			}

			if (!fs->StrBuf_add(buf, "</", 2) ||
				!fs->StrBuf_add(buf, tag_name.p, tag_name.size) ||
				!fs->StrBuf_add_c(buf, '>')) {
				return FALSE;
			}
		} else if (html) {
			if (!fs->StrBuf_add(buf, "></", 3) ||
				!fs->StrBuf_add(buf, tag_name.p, tag_name.size) ||
				!fs->StrBuf_add_c(buf, '>')) {
					return FALSE;
				}
		} else {
			if (!fs->StrBuf_add(buf, " />", 3)) {
				return FALSE;
			}
		}
	}
	return TRUE;
}
static int xml_parse_format(int *html, int *ascii, int *pretty, Str fmt)
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
			if (isspace_fox(c)) {
				break;
			}
			ptr++;
		}
		fm.size = ptr - fm.p;
		if (fm.size > 0) {
			switch (fm.p[0]) {
			case 'a':
				if (fm.size == 1 || Str_eq_p(fm, "ascii")) {
					*ascii = TRUE;
					found = TRUE;
				}
				break;
			case 'h':
				if (fm.size == 1 || Str_eq_p(fm, "html")) {
					*html = TRUE;
					found = TRUE;
				}
				break;
			case 'x':
				if (fm.size == 1 || Str_eq_p(fm, "xml")) {
					*html = FALSE;
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
				fs->throw_errorf(fs->mod_lang, "FormatError", "Unknown format %S", fm);
				return FALSE;
			}
		}
	}
	return TRUE;
}
static int xml_node_tostr(Value *vret, Value *v, RefNode *node)
{
	StrBuf buf;
	Str fmt;
	int html = FALSE;
	int ascii = FALSE;
	int pretty = FALSE;

	if (fg->stk_top > v + 1) {
		fmt = fs->Value_str(v[1]);
	} else {
		const char *pf = FUNC_VP(node);
		if (pf != NULL) {
			fmt = Str_new(pf, -1);
		} else {
			fmt.p = NULL;
			fmt.size = 0;
		}
	}
	if (!xml_parse_format(&html, &ascii, &pretty, fmt)) {
		return FALSE;
	}

	fs->StrBuf_init_refstr(&buf, 0);
	if (!node_tostr_xml(&buf, *v, html, ascii, (pretty ? 0 : -1)) || (pretty && !fs->StrBuf_add_c(&buf, '\n'))) {
		StrBuf_close(&buf);
		return FALSE;
	}
	*vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

	return TRUE;
}

/**
 * node.save(dest, format, charcode)
 */
static int xml_node_save(Value *vret, Value *v, RefNode *node)
{

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_elem_new(Value *vret, Value *v, RefNode *node)
{
	RefArray *ra;
	int pos = 2;
	Ref *r = fs->new_ref(cls_elem);
	*vret = vp_Value(r);

	if (!is_valid_elem_name(fs->Value_str(v[1]))) {
		fs->throw_errorf(mod_xml, "XMLError", "Invalid element name");
		return FALSE;
	}

	r->v[INDEX_NAME] = fs->Value_cp(v[1]);

	ra = fs->refarray_new(0);
	r->v[INDEX_CHILDREN] = vp_Value(ra);
	ra->rh.type = cls_nodelist;

	r->v[INDEX_ATTR] = vp_Value(fs->refmap_new(0));

	if (fg->stk_top > v + 2) {
		Value pv = v[2];
		if (fs->Value_type(pv) == fs->cls_map) {
			RefMap *map = Value_vp(pv);
			RefMap *rm_dst = Value_vp(r->v[INDEX_ATTR]);
			int i;
			for (i = 0; i < map->entry_num; i++) {
				HashValueEntry *ep;
				for (ep = map->entry[i]; ep != NULL; ep = ep->next) {
					HashValueEntry *ve;
					if (fs->Value_type(ep->key) != fs->cls_str || fs->Value_type(ep->val) != fs->cls_str) {
						fs->throw_errorf(fs->mod_lang, "TypeError", "Argument #2 must be map of {str:str, str:str ...}");
						return FALSE;
					}
					ve = fs->refmap_add(rm_dst, ep->key, TRUE, FALSE);
					if (ve == NULL) {
						return FALSE;
					}
					ve->val = fs->Value_cp(ep->val);
				}
			}
			pos = 3;
		}
	}
	for (; v + pos < fg->stk_top; pos++) {
		RefNode *v_type = fs->Value_type(v[pos]);
		if (v_type == cls_elem || v_type == cls_text) {
			Value *pv = fs->refarray_push(ra);
			*pv = fs->Value_cp(v[pos]);
		} else if (v_type == fs->cls_str) {
			Value tmp;
			Value *pv;
			RefStr *rs = Value_vp(v[pos]);
			if (!parse_xml_from_str(&tmp, rs->c, rs->size)) {
				return FALSE;
			}
			pv = fs->refarray_push(ra);
			// moveなので参照カウンタは変化しない
			*pv = tmp;
		} else {
			fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, cls_node, v_type, pos + 1);
			return FALSE;
		}
	}

	return TRUE;
}
static int xml_elem_index_key(Value *vret, Value *v, RefNode *node)
{
	int i;
	Ref *r = Value_vp(*v);
	RefStr *key = Value_vp(v[1]);
	RefArray *ra = fs->refarray_new(0);
	RefArray *rch = Value_vp(r->v[INDEX_CHILDREN]);

	*vret = vp_Value(ra);
	ra->rh.type = cls_nodelist;

	for (i = 0; i < rch->size; i++) {
		Value vk = rch->p[i];
		if (fs->Value_type(vk) == cls_elem) {
			Ref *rk = Value_vp(vk);
			RefStr *k = Value_vp(rk->v[INDEX_NAME]);
			if (str_eq(key->c, key->size, k->c, k->size)) {
				Value *ve = fs->refarray_push(ra);
				*ve = fs->Value_cp(vk);
			}
		}
	}

	return TRUE;
}
/**
 * 子要素を追加
 */
static int xml_elem_push(Value *vret, Value *v, RefNode *node)
{
	Value *vp;
	Value tmp;
	int vp_size;
	RefNode *v1_type = fs->Value_type(v[1]);
	
	if (v1_type == cls_elem || v1_type == cls_text) {
		vp = &v[1];
		vp_size = 1;
		if (xml_elem_has_elem(*v, *vp)) {
			fs->throw_error_select(THROW_LOOP_REFERENCE);
			return FALSE;
		}
	} else if (v1_type == cls_nodelist) {
		RefArray *ra = Value_vp(v[1]);
		vp = ra->p;
		vp_size = ra->size;
		// 循環参照のチェック
		int i;
		for (i = 0; i < vp_size; i++) {
			if (xml_elem_has_elem(*v, vp[i])) {
				fs->throw_error_select(THROW_LOOP_REFERENCE);
				return FALSE;
			}
		}
	} else if (v1_type == fs->cls_str) {
		RefStr *rs = Value_vp(v[1]);
		if (!parse_xml_from_str(&tmp, rs->c, rs->size)) {
			return FALSE;
		}
		vp = &tmp;
		vp_size = 1;
	} else {
		fs->throw_errorf(fs->mod_lang, "TypeError", "XMLRefNode, XMLRefNodeList or Str required but %n (argument #1)", v1_type);
		return FALSE;
	}

	{
		Ref *r = Value_vp(*v);
		RefArray *ra = Value_vp(r->v[INDEX_CHILDREN]);

		if (fg->stk_top > v + 2) {
			// insert
		} else {
			if (FUNC_INT(node)) {
				// unshift
				int size = ra->size;
				int i;
				for (i = 0; i < vp_size; i++) {
					fs->refarray_push(ra);
				}
				memmove(ra->p + vp_size, ra->p, size * sizeof(Value));
				for (i = 0; i < vp_size; i++) {
					ra->p[i] = fs->Value_cp(vp[i]);
				}
			} else {
				// push
				int i;
				for (i = 0; i < vp_size; i++) {
					Value *vd = fs->refarray_push(ra);
					*vd = fs->Value_cp(vp[i]);
				}
			}
		}
	}
	if (v1_type == fs->cls_str) {
		fs->Value_dec(tmp);
	}

	return TRUE;
}
static int xml_elem_index(Value *vret, Value *v, RefNode *node)
{
	RefNode *v1_type = fs->Value_type(v[1]);

	if (v1_type == fs->cls_str) {
		return xml_elem_index_key(vret, v, node);
	} else if (v1_type == fs->cls_int) {
		Ref *r = Value_vp(*v);
		RefArray *ra = Value_vp(r->v[INDEX_CHILDREN]);
		int64_t idx = fs->Value_int(v[1], NULL);

		if (idx < 0) {
			idx += ra->size;
		}
		if (idx < 0 || idx >= ra->size) {
			fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, &v[1], ra->size);
			return FALSE;
		}
		*vret = fs->Value_cp(ra->p[idx]);
	} else {
		fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_int, v1_type, 1);
		return FALSE;
	}

	return TRUE;
}
static int xml_elem_iterator(Value *vret, Value *v, RefNode *node)
{
	int (*array_iterator)(Value *, Value *, RefNode *) = get_function_ptr(fs->cls_list, fs->str_iterator);
	Ref *r = Value_vp(*v);

	array_iterator(vret, &r->v[INDEX_CHILDREN], node);
	return TRUE;
}
static int xml_elem_tagname(Value *vret, Value *v, RefNode *node)
{
	Ref *r = Value_vp(*v);
	*vret = fs->Value_cp(r->v[INDEX_NAME]);
	return TRUE;
}
static int xml_elem_attr(Value *vret, Value *v, RefNode *node)
{
	Value *vp;
	int v_size;
	int i;

	if (FUNC_INT(node)) {
		RefArray *ra = Value_vp(*v);
		vp = ra->p;
		v_size = ra->size;
	} else {
		vp = v;
		v_size = 1;
	}

	if (fg->stk_top > v + 2) {
		RefNode *v_type = fs->Value_type(v[2]);
		if (v_type == fs->cls_str) {
			// 追加
			for (i = 0; i < v_size; i++) {
				Value p = vp[i];
				if (fs->Value_type(p) == cls_elem) {
					Ref *pr = Value_vp(p);
					HashValueEntry *ve = fs->refmap_add(Value_vp(pr->v[INDEX_ATTR]), v[1], TRUE, FALSE);
					if (ve == NULL) {
						return TRUE;
					}
					ve->val = fs->Value_cp(v[2]);
				}
			}
		} else if (v_type == fs->cls_null) {
			// 削除
			for (i = 0; i < v_size; i++) {
				Value p = vp[i];
				if (fs->Value_type(p) == cls_elem) {
					Ref *pr = Value_vp(p);
					if (!fs->refmap_del(vret,  Value_vp(pr->v[INDEX_ATTR]), v[1])) {
						return FALSE;
					}
				}
			}
		} else {
			fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_null, v_type, 2);
			return FALSE;
		}
	} else {
		// 取得
		HashValueEntry *he = NULL;
		int found = FALSE;

		for (i = 0; i < v_size; i++) {
			Value p = vp[i];
			if (fs->Value_type(p) == cls_elem) {
				Ref *pr = Value_vp(p);
				if (!fs->refmap_get(&he, Value_vp(pr->v[INDEX_ATTR]), v[1])) {
					return FALSE;
				}
				if (he != NULL) {
					if (found) {
						fs->throw_errorf(fs->mod_lang, "ValueError", "More than 2 attributes found");
					}
					found = TRUE;
				}
			}
		}
		if (he != NULL) {
			*vret = fs->Value_cp(he->val);
		}
	}

	return TRUE;
}
/**
 * xmlのテキスト部分を抽出
 */
static int node_text_xml(StrBuf *buf, Value v)
{
	if (fs->Value_type(v) == cls_text) {
		if (!fs->StrBuf_add_r(buf, Value_vp(v))) {
			return FALSE;
		}
	} else {
		Ref *r = Value_vp(v);
		RefArray *ra = Value_vp(r->v[INDEX_CHILDREN]);
		int i;
		for (i = 0; i < ra->size; i++) {
			if (!node_text_xml(buf, ra->p[i])) {
				return FALSE;
			}
		}
	}
	return TRUE;
}
static int xml_elem_text(Value *vret, Value *v, RefNode *node)
{
	StrBuf buf;

	fs->StrBuf_init_refstr(&buf, 0);
	if (!node_text_xml(&buf, *v)) {
		StrBuf_close(&buf);
		return FALSE;
	}
	*vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

	return TRUE;
}

/**
 * CSSセレクタ互換
 */
static int xml_elem_css(Value *vret, Value *v, RefNode *node)
{
	if (!select_css(vret, v, 1, fs->Value_str(v[1]))) {
		return FALSE;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_text_new(Value *vret, Value *v, RefNode *node)
{
	const RefStr *rs = Value_vp(v[1]);
	*vret = fs->cstr_Value(cls_text, rs->c, rs->size);
	return TRUE;
}
static int xml_text_text(Value *vret, Value *v, RefNode *node)
{
	const RefStr *rs = Value_vp(*v);
	*vret = fs->cstr_Value(fs->cls_str, rs->c, rs->size);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_node_list_tostr(Value *vret, Value *v, RefNode *node)
{
	RefArray *ra = Value_vp(*v);
	StrBuf buf;
	Str fmt;
	int html = FALSE;
	int ascii = FALSE;
	int pretty = FALSE;
	int i;

	if (fg->stk_top > v + 1) {
		fmt = fs->Value_str(v[1]);
	} else {
		const char *pf = FUNC_VP(node);
		if (pf != NULL) {
			fmt = Str_new(pf, -1);
		} else {
			fmt.p = NULL;
			fmt.size = 0;
		}
	}
	if (!xml_parse_format(&html, &ascii, &pretty, fmt)) {
		return FALSE;
	}

	fs->StrBuf_init_refstr(&buf, 0);
	for (i = 0; i < ra->size; i++) {
		if (!node_tostr_xml(&buf, ra->p[i], html, ascii, (pretty ? 0 : -1)) || !fs->StrBuf_add_c(&buf, '\n')) {
			StrBuf_close(&buf);
			return FALSE;
		}
	}
	*vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

	return TRUE;
}
static int xml_node_list_text(Value *vret, Value *v, RefNode *node)
{
	RefArray *ra = Value_vp(*v);
	StrBuf buf;
	int i;

	fs->StrBuf_init_refstr(&buf, 0);
	for (i = 0; i < ra->size; i++) {
		if (!node_text_xml(&buf, ra->p[i])) {
			StrBuf_close(&buf);
			return FALSE;
		}
	}
	*vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

	return TRUE;
}
static int xml_node_list_css(Value *vret, Value *v, RefNode *node)
{
	RefArray *ra = Value_vp(*v);

	if (!select_css(vret, ra->p, ra->size, fs->Value_str(v[1]))) {
		return FALSE;
	}
	return TRUE;
}
static int xml_node_remove_css(Value *vret, Value *v, RefNode *node)
{
	RefArray *ra = Value_vp(*v);
	RefNode *v1_type = fs->Value_type(v[1]);
	int result = FALSE;

	if (v1_type == fs->cls_str) {
		result = delete_css(vret, ra->p, ra->size, fs->Value_str(v[1]));
	} else if (v1_type == cls_nodelist) {
		result = delete_nodelist(vret, ra->p, ra->size, Value_vp(v[1]));
	} else if (v1_type == cls_elem) {
		RefArray ra2;
		Value va = v[1];
		fs->Value_inc(va);
		ra2.p = &va;
		ra2.size = 1;
		result = delete_nodelist(vret, ra->p, ra->size, &ra2);
		fs->Value_dec(va);
	} else {
		fs->throw_errorf(fs->mod_lang, "TypeError", "Str, XMLElem or XMLRefNodeList required but %n (argument #2)", v1_type);
		return FALSE;
	}
	if (!result) {
		return FALSE;
	}
	return TRUE;
}

static int xml_node_list_index_key(Value *vret, Value *v, RefNode *node)
{
	RefArray *rch = Value_vp(*v);
	RefNode *v1_type = fs->Value_type(v[1]);

	if (v1_type == fs->cls_str) {
		RefStr *key = Value_vp(v[1]);
		RefArray *ra = fs->refarray_new(0);
		int i, j;

		*vret = vp_Value(ra);
		ra->rh.type = cls_nodelist;
		for (i = 0; i < rch->size; i++) {
			Value vch2 = rch->p[i];
			RefArray *rch2 = Value_vp(Value_ref_memb(vch2, INDEX_CHILDREN));

			for (j = 0; j < rch2->size; j++) {
				Value vk = rch2->p[j];
				if (fs->Value_type(vk) == cls_elem) {
					RefStr *k = Value_vp(Value_ref_memb(vk, INDEX_NAME));
					if (str_eq(key->c, key->size, k->c, k->size)) {
						Value *ve = fs->refarray_push(ra);
						*ve = fs->Value_cp(vk);
					}
				}
			}
		}
	} else if (v1_type == fs->cls_int) {
		int64_t idx = fs->Value_int(v[1], NULL);

		if (idx < 0) {
			idx += rch->size;
		}
		if (idx < 0 || idx >= rch->size) {
			fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, &v[1], rch->size);
			return FALSE;
		}
		*vret = fs->Value_cp(rch->p[idx]);
	} else {
		fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_null, v1_type, 1);
		return FALSE;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
	RefNode *n;

	n = fs->define_identifier(m, m, "parse_xml", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, parse_xml, 1, 2, (void*) LOAD_XML, NULL, fs->cls_bool);
	n = fs->define_identifier(m, m, "parse_html", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, parse_xml, 1, 2, (void*) 0, NULL, fs->cls_charset);
	n = fs->define_identifier(m, m, "parse_xmlfile", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, parse_xml, 1, 2, (void*) (LOAD_XML|LOAD_FILE), NULL, fs->cls_bool);
	n = fs->define_identifier(m, m, "parse_htmlfile", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, parse_xml, 1, 2, (void*) LOAD_FILE, NULL, fs->cls_charset);
}
static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;
	RefStr *empty = fs->intern("empty", -1);
	RefStr *size = fs->intern("size", -1);

	cls_node = fs->define_identifier(m, m, "XMLNode", NODE_CLASS, NODEOPT_ABSTRACT);
	cls_nodelist = fs->define_identifier(m, m, "XMLNodeList", NODE_CLASS, 0);
	cls_elem = fs->define_identifier(m, m, "XMLElem", NODE_CLASS, 0);
	cls_text = fs->define_identifier(m, m, "XMLText", NODE_CLASS, NODEOPT_STRCLASS);

	// XMLRefNodeList
	cls = cls_nodelist;
	n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_dispose), 0, 0, NULL);

	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_node_list_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = fs->define_identifier(m, cls, "as_text", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_node_list_text, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "as_xml", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_node_list_tostr, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "as_html", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_node_list_tostr, 0, 0, (void*)"h");
	n = fs->define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, get_function_ptr(fs->cls_list, empty), 0, 0, NULL);
	n = fs->define_identifier_p(m, cls, size, NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, get_function_ptr(fs->cls_list, size), 0, 0, NULL);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_node_list_index_key, 1, 1, NULL, NULL);
	n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_node_list_index_key, 1, 1, NULL, NULL);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->symbol_stock[T_EQ]), 1, 1, NULL, cls_nodelist);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->symbol_stock[T_LET_B]), 2, 2, NULL, fs->cls_int, cls_node);
	n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_iterator), 0, 0, NULL);
	n = fs->define_identifier(m, cls, "children", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_iterator), 0, 0, NULL);
	n = fs->define_identifier(m, cls, "attr", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_elem_attr, 1, 2, (void*) TRUE, fs->cls_str, NULL);
	n = fs->define_identifier(m, cls, "select", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_node_list_css, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier(m, cls, "remove", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_node_remove_css, 1, 1, NULL, NULL);
	fs->extends_method(cls, fs->cls_obj);

	// XMLRefNode(abstract)
	cls = cls_node;
	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_node_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = fs->define_identifier(m, cls, "as_text", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_elem_text, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "as_xml", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_node_tostr, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "as_html", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_node_tostr, 0, 0, (void*)"h");
	n = fs->define_identifier(m, cls, "save", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_node_save, 1, 3, NULL, NULL, fs->cls_str, NULL);
	fs->extends_method(cls, fs->cls_obj);

	// XMLElem
	cls = cls_elem;
	cls->u.c.n_memb = 4;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, xml_elem_new, 1, -1, NULL, fs->cls_str);

	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_elem_index, 1, 1, NULL, NULL);
	n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_elem_index_key, 1, 1, NULL, NULL);
	n = fs->define_identifier(m, cls, "push", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_elem_push, 1, 1, (void*)FALSE, NULL);
	n = fs->define_identifier(m, cls, "unshift", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_elem_push, 1, 1, (void*)TRUE, NULL);
	n = fs->define_identifier(m, cls, "children", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_elem_iterator, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "tagname", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_elem_tagname, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "attr", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_elem_attr, 1, 2, (void*) FALSE, fs->cls_str, NULL);
	n = fs->define_identifier(m, cls, "select", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, xml_elem_css, 1, 1, NULL, fs->cls_str);
	fs->extends_method(cls, cls_node);

	// XMLText
	cls = cls_text;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, xml_text_new, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier(m, cls, "as_text", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, xml_text_text, 0, 0, NULL);
	fs->extends_method(cls, cls_node);


	cls = fs->define_identifier(m, m, "XMLError", NODE_CLASS, 0);
	cls->u.c.n_memb = 2;
	fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	RefNode *mod_unicode;

	fs = a_fs;
	fg = a_fg;
	mod_xml = m;

	mod_unicode = fs->get_module_by_name("foxtk", -1, TRUE, FALSE);
	uni = mod_unicode->u.m.ext;

	define_class(m);
	define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}
