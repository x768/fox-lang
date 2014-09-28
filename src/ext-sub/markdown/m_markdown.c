#include "m_xml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



enum {
	MD_EOS,
	MD_FATAL_ERROR,

	// 1行
	MD_EMPTY_LINE,    // 空行
	MD_HEADING,       // opt=1...6 <h1>child</h1>, <h2>child</h2>
	MD_SENTENSE,      // <p>child</p>

	// インライン要素
	MD_EMPTY,         // 空文字列
	MD_TEXT,          // text
	MD_STRONG,        // opt=1, 2 <em>child</em>, <strong>child</strong>
	MD_CODE_INLINE,   // <code>#hilight:link text</code>
	MD_LINK,          // <a href="link" title="title">child</a>
	MD_CODE,          // title=filename, <pre><code></code></pre>

	MD_LINK_BRAKET,   // [hogehoge]
	MD_LINK_PAREN,    // (hogehoge)  # [...]の後ろに限る

	// 単一要素
	MD_HORIZONTAL,    // <hr>
	MD_IMAGE,         // <img src="link" alt="title">
	MD_TEX_FORMULA,   // optional: $$ {e} ^ {i\pi} + 1 = 0 $$
	MD_PLUGIN,

	// 複数行
	MD_UNORDERD_LIST, // <ul>child</ul>
	MD_ORDERD_LIST,   // <ol>child</ol>
	MD_DEFINE_LIST,   // <dl>child</dl>
	MD_LIST_ITEM,     // <li>child</li>
	MD_DEFINE_DT,     // <dt>child</dt>
	MD_DEFINE_DD,     // <dd>child</dd>
	MD_BLOCKQUOTE,    // title=, link=, <blockquote></blockquote>

	// テーブル
	MD_TABLE,         // <table></table>
	MD_TABLE_ROW,     // <tr></tr>
	MD_TABLE_HEADER,  // <th></th>
	MD_TABLE_CELL,    // <td></td>
};



typedef struct MDNode {
	struct MDNode *child;
	struct MDNode *next;

	uint16_t type;
	uint16_t opt;
	uint16_t level;

	const char *cstr;
} MDNode;

typedef struct {
	RefHeader rh;

	int enable_semantic;
	int enable_tex;
	int quiet_error;
	int tabstop;

	Mem mem;
	Hash link_map;
	MDNode *root;

	MDNode *cur_node;
	MDNode *parent_node;
	MDNode **prev_nodep;

	const char *code_block;
	int prev_level;

	Value plugin_callback;
	Value error;
} RefMarkdown;

typedef struct {
	RefMarkdown *md;
	const char *p;
	int head;
	int prev_link;

	uint16_t type;
	uint16_t opt;
	uint16_t indent;

	StrBuf val;
} MDTok;


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_markdown;
static RefNode *mod_xml;
static XMLStatic *xst;


// markdown字句解析

static void MDTok_init(MDTok *tk, RefMarkdown *md, const char *src)
{
	fs->StrBuf_init(&tk->val, 256);
	tk->md = md;
	tk->p = src;
	tk->type = MD_EMPTY;
	tk->head = TRUE;
	tk->prev_link = FALSE;
}
static void MDTok_close(MDTok *tk)
{
	StrBuf_close(&tk->val);
}

static int parse_text(MDTok *tk, const char *term)
{
	tk->val.size = 0;

	while (*tk->p != '\0') {
		int ch = *tk->p++;
		switch (ch) {
		case '&': {
			const char *top = tk->p;
			const char *p = top;
			char cbuf[6];
			char *dst = cbuf;

			if (*p == '#') {
				if (*p == 'x' || *p == 'X') {
					// 16進数
					p++;
					ch = 0;
					while (isxdigit_fox(*p)) {
						ch = ch * 16;
						if (*p <= '9') {
							ch += (*p - '0');
						} else if (*p <= 'F') {
							ch += (*p - 'A') + 10;
						} else if (*p <= 'f') {
							ch += (*p - 'a') + 10;
						}
						p++;
					}
				} else if (isdigit_fox(*p)) {
					// 10進数
					ch = 0;
					while (isdigit_fox(*p)) {
						ch = ch * 10 + (*p - '0');
						p++;
					}
				} else {
					ch = -1;
				}
			} else if (isalphau_fox(*p)) {
				// 名前
				while (isalnumu_fox(*p)) {
					p++;
				}
				ch = xst->resolve_entity(top, p);
			}
			if (*p == ';') {
				if (ch < 0 || ch >= 0x110000 || (ch >= 0xD800 && ch < 0xE000)) {
					ch = 0xFFFD;
				}
				tk->p = p + 1;
			} else {
				if (*tk->p != '\0') {
					tk->p++;
				}
				ch = '&';
			}
			// utf-8
			if (ch < 0x80) {
				*dst++ = ch;
			} else if (ch < 0x800) {
				*dst++ = 0xC0 | (ch >> 6);
				*dst++ = 0x80 | (ch & 0x3F);
			} else if (ch < 0xD800) {
				*dst++ = 0xE0 | (ch >> 12);
				*dst++ = 0x80 | ((ch >> 6) & 0x3F);
				*dst++ = 0x80 | (ch & 0x3F);
			} else if (ch < 0x10000) {
				*dst++ = 0xE0 | (ch >> 12);
				*dst++ = 0x80 | ((ch >> 6) & 0x3F);
				*dst++ = 0x80 | (ch & 0x3F);
			} else {
				*dst++ = 0xF0 | (ch >> 18);
				*dst++ = 0x80 | ((ch >> 12) & 0x3F);
				*dst++ = 0x80 | ((ch >> 6) & 0x3F);
				*dst++ = 0x80 | (ch & 0x3F);
			}
			if (!fs->StrBuf_add(&tk->val, cbuf, dst - cbuf)) {
				return FALSE;
			}
			break;
		}
		case '\\':
			if (*tk->p != '\0') {
				ch = *tk->p;
				tk->p++;
			}
			if (!fs->StrBuf_add_c(&tk->val, ch)) {
				return FALSE;
			}
			break;
		case '\n':
			return TRUE;
		default: {
			const char *t = term;
			while (*t != '\0') {
				if (ch == *t) {
					return TRUE;
				}
				t++;
			}
			if (!fs->StrBuf_add_c(&tk->val, ch)) {
				return FALSE;
			}
			break;
		}
		}
	}
	return TRUE;
}
static void MDTok_next(MDTok *tk)
{
	tk->val.size = 0;
	tk->opt = 0;

	if (!tk->head) {
		while (*tk->p != '\0') {
			if ((*tk->p & 0xFF) <= ' ' && *tk->p != '\n') {
				tk->p++;
			} else {
				break;
			}
		}
		if (*tk->p == '\n') {
			tk->p++;
			tk->head = TRUE;
			tk->prev_link = FALSE;
		}
	}

	if (tk->head) {
		tk->indent = 0;
		tk->head = FALSE;
		tk->prev_link = FALSE;

		while (*tk->p != '\0') {
			if (*tk->p == '\t') {
				tk->p++;
				tk->indent += tk->md->tabstop;
			} else if ((*tk->p & 0xFF) <= ' ' && *tk->p != '\n' && *tk->p != '\r') {
				tk->p++;
				tk->indent++;
			} else {
				break;
			}
		}

		switch (*tk->p) {
		case '\0':
			tk->type = MD_EOS;
			return;
		case '#':
			tk->type = MD_HEADING;
			tk->opt = 1;
			tk->p++;
			while (*tk->p == '#') {
				tk->opt++;
				tk->p++;
			}
			return;
		case '*': case '-':
			if (tk->p[1] == ' ') {
				tk->p += 2;
				tk->type = MD_UNORDERD_LIST;
				return;
			}
			{
				int i;
				int n = 0;
				for (i = 0; ; i++) {
					int c = tk->p[i] & 0xFF;
					if (c == '-') {
						n++;
					} else if (c == '\0' || c > ' ') {
						break;
					}
				}
				if (n >= 3) {
					tk->p += i;
					tk->type = MD_HORIZONTAL;
					return;
				}
			}
			break;
		case '>':
			tk->type = MD_BLOCKQUOTE;
			tk->opt = 1;
			tk->p++;
			while (*tk->p == '>') {
				tk->opt++;
				tk->p++;
			}
			return;
		case '`':
			if (tk->p[1] == '`' && tk->p[2] == '`') {
				tk->p += 3;
				tk->type = MD_CODE;
				return;
			}
			// fall through
		default:
			if (isdigit_fox(*tk->p)) {
				// \d+\.
				const char *p = tk->p + 1;
				while (isdigit_fox(*p)) {
					p++;
				}
				if (p[0] == '.' && p[1] == ' ') {
					tk->p = p + 2;
					tk->type = MD_ORDERD_LIST;
					return;
				}
			} else if (*tk->p == '\n') {
				tk->p++;
				tk->type = MD_EMPTY_LINE;
				tk->head = TRUE;
				return;
			}
			break;
		}
	}

	tk->indent = 0;

	switch (*tk->p) {
	case '\0':
		tk->type = MD_EOS;
		tk->prev_link = FALSE;
		return;
	case '_':
		tk->p++;
		tk->type = MD_STRONG;
		if (*tk->p == '_') {
			tk->p++;
			tk->opt = 2;
		} else {
			tk->opt = 1;
		}
		tk->prev_link = FALSE;
		return;
	case '*':
		tk->p++;
		tk->type = MD_STRONG;
		if (*tk->p == '*') {
			tk->p++;
			tk->opt = 2;
		} else {
			tk->opt = 1;
		}
		tk->prev_link = FALSE;
		return;
	case '`':
		tk->p++;
		tk->type = MD_CODE_INLINE;
		tk->prev_link = FALSE;
		return;
	case '<': {
		const char *top = tk->p + 1;
		const char *p = top;

		if (isalphau_fox(*p)) {
			tk->p++;
			if (!parse_text(tk, ">")) {
				tk->type = MD_FATAL_ERROR;
				return;
			}
			if (*tk->p == '>') {
				tk->p++;
			}
			tk->type = MD_LINK;
			tk->prev_link = FALSE;
			return;
		}
		break;
	}
	case '[': {
		const char *top = tk->p + 1;
		const char *p = top;

		if ((*p & 0xFF) > ' ') {
			tk->p++;
			if (!parse_text(tk, "]")) {
				tk->type = MD_FATAL_ERROR;
				return;
			}
			if (*tk->p == ']') {
				tk->p++;
			}
			tk->type = MD_LINK_BRAKET;
			tk->prev_link = TRUE;
			return;
		}
		break;
	}
	case '(':
		if (tk->prev_link) {
			tk->prev_link = FALSE;
			if (isalnumu_fox(*tk->p)) {
				tk->p++;
				if (!parse_text(tk, ")")) {
					tk->type = MD_FATAL_ERROR;
					return;
				}
				if (*tk->p == ')') {
					tk->p++;
				}
				tk->type = MD_LINK_PAREN;
				return;
			}
		}
		break;
	default:
		break;
	}

	tk->prev_link = FALSE;
	if (!parse_text(tk, " *_`[")) {
		tk->type = MD_FATAL_ERROR;
		return;
	}
	if (tk->val.size > 0) {
		tk->type = MD_TEXT;
	} else {
		tk->type = MD_EMPTY;
	}
}

////////////////////////////////////////////////////////////////////////////////////

static int parse_markdown_line(RefMarkdown *r, const char *p)
{
	MDTok tk;
	MDTok_init(&tk, r, p);

#if 0
	for (;;) {
		MDTok_next(&tk);
		if (tk.type == MD_FATAL_ERROR) {
			return FALSE;
		}
		if (tk.type == MD_EOS) {
			break;
		}
		fprintf(stderr, "%d : %.*s\n", tk.type, tk.val.size, tk.val.p);
	}
#endif
	for (;;) {
		MDTok_next(&tk);
		if (tk.type == MD_FATAL_ERROR) {
			return FALSE;
		}
		if (tk.type == MD_EOS) {
			break;
		}
	}

	MDTok_close(&tk);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int markdown_new(Value *vret, Value *v, RefNode *node)
{
	RefNode *cls_markdown = FUNC_VP(node);
	RefMarkdown *r = fs->new_buf(cls_markdown, sizeof(RefMarkdown));
	*vret = vp_Value(r);

	fs->Mem_init(&r->mem, 1024);
	r->tabstop = 4;
	r->prev_nodep = &r->root;

	return TRUE;
}
static int markdown_dispose(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	fs->Mem_close(&r->mem);
	fs->Value_dec(r->plugin_callback);
	fs->Value_dec(r->error);
	return TRUE;
}
static int markdown_plugin_callback(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	if (fg->stk_top > v + 1) {
		r->plugin_callback = fs->Value_cp(v[1]);
	} else {
		*vret = fs->Value_cp(r->plugin_callback);
	}
	return TRUE;
}
static int markdown_enable_semantic(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	if (fg->stk_top > v + 1) {
		r->enable_semantic = Value_bool(v[1]);
	} else {
		*vret = bool_Value(r->enable_semantic);
	}
	return TRUE;
}
static int markdown_quiet_error(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	if (fg->stk_top > v + 1) {
		r->quiet_error = Value_bool(v[1]);
	} else {
		*vret = bool_Value(r->quiet_error);
	}
	return TRUE;
}
static int markdown_enable_tex(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	if (fg->stk_top > v + 1) {
		r->enable_tex = Value_bool(v[1]);
	} else {
		*vret = bool_Value(r->enable_tex);
	}
	return TRUE;
}
static int markdown_tabstop(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);
	if (fg->stk_top > v + 1) {
		int err;
		int64_t i64 = fs->Value_int(v[1], &err);
		if (err || i64 < 1 || i64 > 16) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Out of range (1 - 16)");
			return FALSE;
		}
		r->tabstop = (int)i64;
	} else {
		*vret = int32_Value(r->tabstop);
	}
	return TRUE;
}

static int markdown_compile(Value *vret, Value *v, RefNode *node)
{
	RefMarkdown *r = Value_vp(*v);

	if (!parse_markdown_line(r, Value_cstr(v[1]))) {
		return FALSE;
	}

	return TRUE;
}
static int markdown_make_xml(Value *vret, Value *v, RefNode *node)
{
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
	RefNode *n;
	RefNode *cls;

	cls = fs->define_identifier(m, m, "Markdown", NODE_CLASS, 0);
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, markdown_new, 0, 0, cls);

	n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_NEW_N, 0);
	fs->define_native_func_a(n, markdown_dispose, 0, 0, cls);
	n = fs->define_identifier(m, cls, "plugin_callback", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, markdown_plugin_callback, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "plugin_callback=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_plugin_callback, 1, 1, NULL, fs->cls_fn);
	n = fs->define_identifier(m, cls, "enable_semantic", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, markdown_enable_semantic, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "enable_semantic=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_enable_semantic, 1, 1, NULL, fs->cls_bool);
	n = fs->define_identifier(m, cls, "quiet_error", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, markdown_quiet_error, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "quiet_error=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_quiet_error, 1, 1, NULL, fs->cls_bool);
	n = fs->define_identifier(m, cls, "enable_tex", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, markdown_enable_tex, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "enable_tex=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_enable_tex, 1, 1, NULL, fs->cls_bool);
	n = fs->define_identifier(m, cls, "tabstop", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, markdown_tabstop, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "tabstop=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_tabstop, 1, 1, NULL, fs->cls_int);

	n = fs->define_identifier(m, cls, "compile", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_compile, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier(m, cls, "make_xml", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, markdown_make_xml, 0, 0, NULL);
	fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_markdown = m;
	mod_xml = fs->get_module_by_name("datafmt.xml", -1, TRUE, FALSE);
	xst = mod_xml->u.m.ext;

	define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\n";
}
