#include "m_xml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



enum {
    MD_NONE,
    MD_EOS,
    MD_FATAL_ERROR,

    // 1行
    MD_EMPTY_LINE,    // 空行
    MD_HEADING,       // opt=1...6 <h1>child</h1>, <h2>child</h2>
    MD_PARAGRAPH,     // <p>child</p>
    MD_HORIZONTAL,    // <hr>

    // インライン要素
    MD_TEXT,          // text
    MD_EM,            // <em>child</em>
    MD_STRONG,        // <strong>child</strong>
    MD_STRIKE,        // <strike>child</strike>
    MD_CODE_INLINE,   // <code>#hilight:link text</code>
    MD_LINK,          // <a href="link" title="title">child</a>
    MD_CODE,          // title=filename, <pre><code></code></pre>

    MD_LINK_BRAKET,   // [hogehoge]
    MD_LINK_BRAKET_NEXT,// [hoge]    # [...]の後ろに限る
    MD_LINK_PAREN,    // (hogehoge)  # [...]の後ろに限る

    // 単一要素
    MD_IMAGE,         // <img src="link" alt="title">
    MD_TEX_FORMULA,   // optional: $$ {e} ^ {i\pi} + 1 = 0 $$
    MD_COLON,         // :
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
    MD_TABLE,         // "|" <table></table>
    MD_TABLE_ROW,     // <tr></tr>
    MD_TABLE_HEADER,  // <th></th>
    MD_TABLE_CELL,    // <td></td>
};

enum {
    OPT_LINK_RESOLVED,
    OPT_LINK_NAME_REF,
};

enum {
    SIMPLE_HASH_MAX = 32,
};

typedef struct MDNode {
    struct MDNode *child;
    struct MDNode *next;
    struct MDNode *parent;

    uint16_t type;
    uint16_t opt;
    uint16_t indent;

    const char *cstr;
    const char *href;
} MDNode;

typedef struct SimpleHash
{
    struct SimpleHash *next;
    uint32_t hash;
    const char *name;
    MDNode *node;
} SimpleHash;

typedef struct {
    RefHeader rh;

    int enable_semantic;
    int enable_tex;
    int quiet_error;
    int tabstop;

    Mem mem;
    SimpleHash *link_map[SIMPLE_HASH_MAX];
    MDNode *root;

    const char *code_block;
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
    uint16_t bq_level;
    uint16_t indent;

    StrBuf val;
} MDTok;



static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_markdown;
static RefNode *mod_xml;
static const XMLStatic *xst;

static RefNode *cls_xmlelem;
static RefNode *cls_xmltext;
static RefNode *cls_nodelist;



static uint32_t str_hash(const char *p)
{
    uint32_t h = 0;
    for (; *p != '\0'; p++) {
        h = h * 31 + *p;
    }
    return h & (SIMPLE_HASH_MAX - 1);
}

static MDNode *SimpleHash_get(SimpleHash **hash, const char *name)
{
    uint32_t ihash = str_hash(name);
    SimpleHash *h = hash[ihash];
    for (; h != NULL; h = h->next) {
        if (ihash == h->hash && strcmp(name, h->name) == 0) {
            return h->node;
        }
    }
    return NULL;
}

static void SimpleHash_add(SimpleHash **hash, Mem *mem, const char *name, MDNode *node)
{
    uint32_t ihash = str_hash(name);
    SimpleHash **pp = &hash[ihash];
    SimpleHash *h = *pp;

    while (h != NULL) {
        if (ihash == h->hash && strcmp(name, h->name) == 0) {
            return;
        }
        pp = &h->next;
        h = *pp;
    }
    h = fs->Mem_get(mem, sizeof(SimpleHash));
    h->hash = ihash;
    h->name = fs->str_dup_p(name, -1, mem);
    h->node = node;
    *pp = h;
}

///////////////////////////////////////////////////////////////////////////////////////

// markdown字句解析

static void MDTok_init(MDTok *tk, RefMarkdown *md, const char *src)
{
    fs->StrBuf_init(&tk->val, 256);
    tk->md = md;
    tk->p = src;
    tk->head = TRUE;
    tk->prev_link = FALSE;

    tk->type = MD_NONE;
    tk->opt = 0;
    tk->bq_level = 0;
    tk->indent = 0;
}
static void MDTok_close(MDTok *tk)
{
    StrBuf_close(&tk->val);
}

static int parse_text(MDTok *tk, const char *term, int term_strike)
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
                if (ch < 0 || ch >= CODEPOINT_END || (ch >= SURROGATE_U_BEGIN && ch < SURROGATE_END)) {
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
            tk->head = TRUE;
            tk->prev_link = FALSE;
            return TRUE;
        case '~':
            if (term_strike && *tk->p == '~') {
                tk->p--;
                return TRUE;
            }
            if (!fs->StrBuf_add_c(&tk->val, ch)) {
                return FALSE;
            }
            break;
        default: {
            const char *t = term;
            while (*t != '\0') {
                if (ch == *t) {
                    tk->p--;
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
static void MDTok_skip_space(MDTok *tk)
{
    while (*tk->p != '\0') {
        if ((*tk->p & 0xFF) <= ' ' && *tk->p != '\n') {
            tk->p++;
        } else {
            break;
        }
    }
}
static void MDTok_next(MDTok *tk)
{
    tk->val.size = 0;
    tk->opt = 0;

    if (!tk->head) {
        const char *top = tk->p;
        MDTok_skip_space(tk);
        if (*tk->p == '\n') {
            tk->p++;
            tk->head = TRUE;
        }
        if (tk->p != top) {
            tk->prev_link = FALSE;
            tk->val.p[0] = ' ';
            tk->val.size = 1;
            tk->type = MD_TEXT;
            return;
        }
    }

    if (tk->head) {
        tk->indent = 0;
        tk->bq_level = 0;
        tk->head = FALSE;
        tk->prev_link = FALSE;

        while (*tk->p == '>') {
            tk->bq_level++;
            tk->p++;
        }
        while (*tk->p != '\0') {
            if (*tk->p == '\t') {
                int ts = tk->md->tabstop;
                tk->indent = tk->indent / ts * ts + 1;
                tk->p++;
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
            MDTok_skip_space(tk);
            return;
        case '*': case '-':
            {
                int i;
                int c = 0;
                int n = 0;
                for (i = 0; ; i++) {
                    c = tk->p[i] & 0xFF;
                    if (c == '-') {
                        n++;
                    } else if (c == '\n') {
                        i++;
                        break;
                    } else if (c == '\0' || c > ' ') {
                        break;
                    }
                }
                if (n >= 3 && (c == '\0' || c == '\n')) {
                    tk->p += i;
                    tk->type = MD_HORIZONTAL;
                    tk->head = TRUE;
                    return;
                }
            }
            if (tk->p[1] == ' ') {
                tk->p += 2;
                tk->type = MD_UNORDERD_LIST;
                return;
            }
            break;
        case '|':
            tk->p++;
            tk->type = MD_TABLE;
            return;
        case '`':
            if (tk->p[1] == '`' && tk->p[2] == '`') {
                const char *top;
                tk->p += 3;
                top = tk->p;
                while (*tk->p != '\0' && *tk->p != '\n') {
                    tk->p++;
                }
                tk->val.size = 0;
                fs->StrBuf_add(&tk->val, top, tk->p - top);
                if (*tk->p == '\n') {
                    tk->p++;
                }
                tk->head = TRUE;
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
                if (p[0] == '.' && isspace_fox(p[1])) {
                    tk->p = p + 2;
                    MDTok_skip_space(tk);
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

RETRY:
    switch (*tk->p) {
    case '\0':
        tk->type = MD_EOS;
        tk->prev_link = FALSE;
        return;
    case '_':
        tk->p++;
        if (*tk->p == '_') {
            tk->p++;
            tk->type = MD_STRONG;
        } else {
            tk->type = MD_EM;
        }
        tk->prev_link = FALSE;
        return;
    case '*':
        tk->p++;
        if (*tk->p == '*') {
            tk->p++;
            tk->type = MD_STRONG;
        } else {
            tk->type = MD_EM;
        }
        tk->prev_link = FALSE;
        return;
    case '~':
        tk->p++;
        if (*tk->p == '~') {
            tk->p++;
            tk->type = MD_STRIKE;
            tk->prev_link = FALSE;
            return;
        }
        break;
    case '`':
        tk->p++;
        tk->type = MD_CODE_INLINE;
        tk->prev_link = FALSE;
        return;
    case ':':
        tk->p++;
        tk->type = MD_COLON;
        tk->prev_link = FALSE;
        return;
    case '<': {
        const char *top = tk->p + 1;
        const char *p = top;

        if (isalphau_fox(*p)) {
            tk->p++;
            if (!parse_text(tk, ">", FALSE)) {
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
            if (!parse_text(tk, "]", FALSE)) {
                tk->type = MD_FATAL_ERROR;
                return;
            }
            if (*tk->p == ']') {
                tk->p++;
            }
            if (tk->prev_link) {
                tk->type = MD_LINK_BRAKET_NEXT;
                tk->prev_link = TRUE;
            } else {
                tk->type = MD_LINK_BRAKET;
                tk->prev_link = TRUE;
            }
            return;
        }
        break;
    }
    case '(':
        if (tk->prev_link) {
            tk->prev_link = FALSE;
            if (isalnumu_fox(tk->p[1])) {
                tk->p++;
                if (!parse_text(tk, ")", FALSE)) {
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
    if (!parse_text(tk, " *_`[:", TRUE)) {
        tk->type = MD_FATAL_ERROR;
        return;
    }
    if (tk->val.size > 0) {
        tk->type = MD_TEXT;
    } else {
        goto RETRY;
    }
}
static void MDTok_next_code(MDTok *tk)
{
    tk->val.size = 0;

    while (*tk->p != '\0') {
        const char *top = tk->p;
        while (*tk->p != '\0' && *tk->p != '\n' && *tk->p != '\r') {
            tk->p++;
        }
        if (tk->p - top == 3 && memcmp(top, "```", 3) == 0) {
            if (*tk->p == '\r') {
                tk->p++;
            }
            if (*tk->p == '\n') {
                tk->p++;
            }
            return;
        }
        if (*tk->p == '\r') {
            tk->p++;
        }
        if (*tk->p == '\n') {
            tk->p++;
        }
        fs->StrBuf_add(&tk->val, top, tk->p - top);
    }
}
static void MDTok_next_code_inline(MDTok *tk)
{
    const char *top = tk->p;

    if (*tk->p == '\0') {
        tk->type = MD_EOS;
        tk->prev_link = FALSE;
        return;
    }
    while (*tk->p != '\0' && *tk->p != '`') {
        tk->p++;
    }
    tk->val.size = 0;
    fs->StrBuf_add(&tk->val, top, tk->p - top);
    if (*tk->p == '`') {
        tk->p++;
    }
}
static int MDTok_is_next_colon(MDTok *tk)
{
    const uint8_t *p = (const uint8_t*)tk->p;
    while (*p != '\0') {
        if (*p <= ' ' && *p != '\n') {
            p++;
        } else {
            break;
        }
    }
    return *p == ':';
}

void dump_mdtree(MDNode *node, int level)
{
    static const char *name[] = {
        "MD_NONE",
        "MD_EOS",
        "MD_FATAL_ERROR",
        "MD_EMPTY_LINE",
        "MD_HEADING",
        "MD_SENTENSE",
        "MD_HORIZONTAL",
        "MD_TEXT",
        "MD_EM",
        "MD_STRONG",
        "MD_STRIKE",
        "MD_CODE_INLINE",
        "MD_LINK",
        "MD_CODE",
        "MD_LINK_BRAKET",
        "MD_LINK_PAREN",
        "MD_IMAGE",
        "MD_TEX_FORMULA",
        "MD_COLON",
        "MD_PLUGIN",
        "MD_UNORDERD_LIST",
        "MD_ORDERD_LIST",
        "MD_DEFINE_LIST",
        "MD_LIST_ITEM",
        "MD_DEFINE_DT",
        "MD_DEFINE_DD",
        "MD_BLOCKQUOTE",
        "MD_TABLE",
        "MD_TABLE_ROW",
        "MD_TABLE_HEADER",
        "MD_TABLE_CELL",
    };
    while (node != NULL) {
        int i;
        for (i = 0; i < level; i++) {
            fprintf(stderr, " ");
        }
        if (node->cstr != NULL) {
            fprintf(stderr, "%p:%s '%s'\n", node, name[node->type], node->cstr);
        } else {
            fprintf(stderr, "%p:%s\n", node, name[node->type]);
        }
        dump_mdtree(node->child, level + 1);
        node = node->next;
    }
}

////////////////////////////////////////////////////////////////////////////////////

static MDNode *MDNode_new(int type, RefMarkdown *r, MDTok *tk)
{
    MDNode *n = fs->Mem_get(&r->mem, sizeof(MDNode));
    memset(n, 0, sizeof(MDNode));
    n->type = type;
    n->indent = tk->indent;
    n->cstr = NULL;
    n->href = NULL;
    return n;
}
static int parse_markdown_code_block(RefMarkdown *r, MDTok *tk, MDNode **ppnode, const char *type)
{
    MDNode *node = MDNode_new(MD_TEXT, r, tk);
    node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
    *ppnode = node;
    return TRUE;
}
static int parse_markdown_line(RefMarkdown *r, MDTok *tk, MDNode **ppnode, int term)
{
    MDNode *node;
    int bq_level = tk->bq_level;

    for (;;) {
        if (tk->bq_level != bq_level) {
            return TRUE;
        }
        if (tk->type == term) {
            MDTok_next(tk);
            return TRUE;
        }
        switch (tk->type) {
        case MD_TEXT:
            node = MDNode_new(MD_TEXT, r, tk);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_COLON:
            node = MDNode_new(MD_TEXT, r, tk);
            node->cstr = fs->str_dup_p(":", 1, &r->mem);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_LINK:
            node = MDNode_new(MD_LINK, r, tk);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            node->href = node->cstr;
            node->opt = OPT_LINK_RESOLVED;
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_LINK_BRAKET:
            node = MDNode_new(MD_LINK, r, tk);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);

            if (tk->type == MD_LINK_BRAKET_NEXT) {
                node->href = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
                node->opt = OPT_LINK_NAME_REF;
                MDTok_next(tk);
            } else if (tk->type == MD_LINK_PAREN) {
                node->href = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
                node->opt = OPT_LINK_RESOLVED;
                MDTok_next(tk);
            } else {
                node->href = node->cstr;
                node->opt = OPT_LINK_NAME_REF;
            }
            break;
        case MD_EM:
        case MD_STRONG:
        case MD_STRIKE: {
            int type = tk->type;

            node = MDNode_new(type, r, tk);
            node->opt = tk->opt;
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);

            if (!parse_markdown_line(r, tk, &node->child, type)) {
                return FALSE;
            }
            break;
        }
        case MD_CODE_INLINE:
            MDTok_next_code_inline(tk);
            if (tk->type == MD_CODE_INLINE) {
                node = MDNode_new(MD_CODE_INLINE, r, tk);
                node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
                *ppnode = node;
                ppnode = &node->next;
                MDTok_next(tk);
            }
            break;
        case MD_FATAL_ERROR:
            return FALSE;
        default:
            return TRUE;
        }
    }
}

static int parse_markdown_list_block(RefMarkdown *r, MDTok *tk, MDNode **ppnode)
{
    MDNode *node;
    int bq_level = tk->bq_level;
    int indent = tk->indent;
    int type = tk->type;

    for (;;) {
        if (tk->bq_level != bq_level) {
            return TRUE;
        }
        if (tk->indent < indent) {
            return TRUE;
        }
        if (tk->type != MD_ORDERD_LIST && tk->type != MD_UNORDERD_LIST) {
            return TRUE;
        }

        if (tk->indent > indent) {
            node = MDNode_new(tk->type, r, tk);
            *ppnode = node;
            ppnode = &node->next;
            if (!parse_markdown_list_block(r, tk, &node->child)) {
                return FALSE;
            }
        } else {
            if (tk->type != type) {
                return TRUE;
            }
            node = MDNode_new(MD_LIST_ITEM, r, tk);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            parse_markdown_line(r, tk, &node->child, MD_NONE);
        }
    }
}

static int parse_markdown_block(RefMarkdown *r, MDTok *tk, MDNode **ppnode, int bq_level)
{
    MDNode *node;

    for (;;) {
        if (tk->bq_level > bq_level) {
            node = MDNode_new(MD_BLOCKQUOTE, r, tk);
            *ppnode = node;
            ppnode = &node->next;

            if (!parse_markdown_block(r, tk, &node->child, tk->bq_level)) {
                return FALSE;
            }
        } else if (tk->bq_level < bq_level) {
            return TRUE;
        }
        switch (tk->type) {
        case MD_HEADING:
            node = MDNode_new(MD_HEADING, r, tk);
            node->opt = tk->opt;
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            parse_markdown_line(r, tk, &node->child, MD_NONE);
            break;
        case MD_HORIZONTAL:
            node = MDNode_new(MD_HORIZONTAL, r, tk);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_ORDERD_LIST:
        case MD_UNORDERD_LIST:
            node = MDNode_new(tk->type, r, tk);
            *ppnode = node;
            ppnode = &node->next;
            if (!parse_markdown_list_block(r, tk, &node->child)) {
                return FALSE;
            }
            break;
        case MD_CODE:
            node = MDNode_new(MD_CODE, r, tk);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next_code(tk);

            parse_markdown_code_block(r, tk, &node->child, node->cstr);
            MDTok_next(tk);
            break;
        case MD_LINK_BRAKET:
            if (MDTok_is_next_colon(tk)) {
                char *name = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
                node = NULL;
                MDTok_next(tk);  // colon
                MDTok_next(tk);
                parse_markdown_line(r, tk, &node, MD_NONE);
                SimpleHash_add(r->link_map, &r->mem, name, node);
            } else {
                node = MDNode_new(MD_PARAGRAPH, r, tk);
                *ppnode = node;
                ppnode = &node->next;
                parse_markdown_line(r, tk, &node->child, MD_NONE);
            }
            break;
        case MD_LINK:
        case MD_TEXT:
        case MD_EM:
        case MD_STRONG:
        case MD_STRIKE:
        case MD_CODE_INLINE:
            node = MDNode_new(MD_PARAGRAPH, r, tk);
            *ppnode = node;
            ppnode = &node->next;
            parse_markdown_line(r, tk, &node->child, MD_NONE);
            break;
        case MD_COLON:
            break;
        case MD_FATAL_ERROR:
            return FALSE;
        case MD_EOS:
            return TRUE;
        default:
            MDTok_next(tk);
            break;
        }
    }
    return TRUE;
}

// 文字列をmarkdown中間形式に変換
static int parse_markdown(RefMarkdown *r, const char *p)
{
    MDTok tk;

    MDTok_init(&tk, r, p);
    MDTok_next(&tk);

    if (!parse_markdown_block(r, &tk, &r->root, 0)) {
        MDTok_close(&tk);
        return FALSE;
    }

    MDTok_close(&tk);
    //dump_mdtree(r->root, 0);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int link_markdown_sub(RefMarkdown *r, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        if (node->type == MD_LINK && node->opt == OPT_LINK_NAME_REF) {
            MDNode *nd = SimpleHash_get(r->link_map, node->href);
            if (nd != NULL) {
                switch (node->href[0]) {
                case '^':  // footnote
                    break;
                default:
                    break;
                }
            }
        }
        if (node->child != NULL) {
            link_markdown_sub(r, node->child);
        }
    }
    return TRUE;
}
static int link_markdown(RefMarkdown *r)
{
    if (!link_markdown_sub(r, r->root)) {
        return FALSE;
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static Ref *xmlelem_new(const char *name)
{
    RefArray *ra;
    Ref *r = fs->ref_new(cls_xmlelem);
    r->v[INDEX_ELEM_NAME] = vp_Value(fs->intern(name, -1));

    ra = fs->refarray_new(0);
    ra->rh.type = cls_nodelist;
    r->v[INDEX_ELEM_CHILDREN] = vp_Value(ra);
    
    return r;
}
static void xmlelem_add(Ref *r, Ref *elem)
{
    RefArray *ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
    Value *v = fs->refarray_push(ra);
    *v = fs->Value_cp(vp_Value(elem));
}

static void xmlelem_add_attr(Ref *r, const char *ckey, const char *cval)
{
    RefMap *rm;
    Value key;
    HashValueEntry *ve;

    if (Value_isref(r->v[INDEX_ELEM_ATTR])) {
        rm = Value_vp(r->v[INDEX_ELEM_ATTR]);
    } else {
        rm = fs->refmap_new(0);
        r->v[INDEX_ELEM_ATTR] = vp_Value(rm);
    }
    key = fs->cstr_Value(fs->cls_str, ckey, -1);
    ve = fs->refmap_add(rm, key, TRUE, FALSE);
    fs->Value_dec(key);
    fs->Value_dec(ve->val);
    ve->val = fs->cstr_Value(fs->cls_str, cval, -1);
}

static int xml_convert_line(Ref *r, MDNode *node, StrBuf *buf)
{
    int prev_type = MD_EMPTY_LINE;

    buf->size = 0;

    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_TEXT:
            if (prev_type != MD_TEXT) {
                buf->size = 0;
            }
            fs->StrBuf_add(buf, node->cstr, -1);
            break;
        case MD_LINK: {
            Ref *child;
            if (prev_type == MD_TEXT) {
                xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, buf->p, buf->size)));
            }
            child = xmlelem_new("a");
            xmlelem_add(r, child);
            if (node->href != NULL) {
                xmlelem_add_attr(child, "href", node->href);
            }
            xmlelem_add(child, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            break;
        }
        case MD_EM:
        case MD_STRONG:
        case MD_STRIKE: {
            Ref *child;
            if (prev_type == MD_TEXT) {
                xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, buf->p, buf->size)));
            }
            switch (node->type) {
            case MD_EM:
                child = xmlelem_new("em");
                break;
            case MD_STRONG:
                child = xmlelem_new("strong");
                break;
            case MD_STRIKE:
                child = xmlelem_new("strike");
                break;
            default:
                return FALSE;
            }
            xmlelem_add(r, child);
            if (!xml_convert_line(child, node->child, buf)) {
                return FALSE;
            }
            break;
        }
        case MD_CODE_INLINE: {
            Ref *code;
            if (prev_type == MD_TEXT) {
                xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, buf->p, buf->size)));
            }
            code = xmlelem_new("code");
            xmlelem_add(r, code);
            xmlelem_add(code, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            break;
        }
        default:
            if (prev_type == MD_TEXT) {
                xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            }
            break;
        }
        prev_type = node->type;
    }
    if (prev_type == MD_TEXT) {
        xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, buf->p, buf->size)));
    }
    return TRUE;
}
static int xml_source_code(Ref *r, const char *code, const char *type, StrBuf *buf)
{
    // TODO 色分け
    xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, code, -1)));
    return TRUE;
}
static int xml_convert_listitem(Ref *r, MDNode *node, StrBuf *buf)
{
    Ref *prev_li = NULL;

    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_LIST_ITEM: {
            Ref *li = xmlelem_new("li");
            xmlelem_add(r, li);
            if (!xml_convert_line(li, node->child, buf)) {
                return FALSE;
            }
            prev_li = li;
            break;
        }
        case MD_UNORDERD_LIST:
        case MD_ORDERD_LIST:
            if (prev_li != NULL) {
                Ref *list = xmlelem_new(node->type == MD_UNORDERD_LIST ? "ul" : "ol");
                xmlelem_add(prev_li, list);
                if (!xml_convert_listitem(list, node->child, buf)) {
                    return FALSE;
                }
            }
            break;
        }
    }
    return TRUE;
}
static int xml_from_markdown(Ref *root, RefMarkdown *r, MDNode *node)
{
    StrBuf buf;
    fs->StrBuf_init(&buf, 64);

    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_HEADING: {
            char tag[4];
            Ref *heading;
            tag[0] = 'h';
            tag[1] = (node->opt <= 6 ? '0' + node->opt : '6');
            tag[2] = '\0';
            heading = xmlelem_new(tag);
            xmlelem_add(root, heading);
            if (!xml_convert_line(heading, node->child, &buf)) {
                StrBuf_close(&buf);
                return FALSE;
            }
            break;
        }
        case MD_HORIZONTAL: {
            Ref *hr = xmlelem_new("hr");
            xmlelem_add(root, hr);
            break;
        }
        case MD_UNORDERD_LIST:
        case MD_ORDERD_LIST: {
            Ref *list = xmlelem_new(node->type == MD_UNORDERD_LIST ? "ul" : "ol");
            xmlelem_add(root, list);
            if (!xml_convert_listitem(list, node->child, &buf)) {
                StrBuf_close(&buf);
                return FALSE;
            }
            break;
        }
        case MD_PARAGRAPH: {
            Ref *para = xmlelem_new("p");
            xmlelem_add(root, para);
            if (!xml_convert_line(para, node->child, &buf)) {
                StrBuf_close(&buf);
                return FALSE;
            }
            break;
        }
        case MD_CODE: {
            Ref *pre = xmlelem_new("pre");
            Ref *code = xmlelem_new("code");
            xmlelem_add(root, pre);
            xmlelem_add(pre, code);
            if (!xml_source_code(code, node->child->cstr, node->cstr, &buf)) {
                StrBuf_close(&buf);
                return FALSE;
            }
            break;
        }
        case MD_BLOCKQUOTE: {
            Ref *blockquote = xmlelem_new("blockquote");
            xmlelem_add(root, blockquote);
            if (!xml_from_markdown(blockquote, r, node->child)) {
                StrBuf_close(&buf);
                return FALSE;
            }
            break;
        }
        default:
            break;
        }
    }
    StrBuf_close(&buf);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int markdown_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_markdown = FUNC_VP(node);
    RefMarkdown *r = fs->buf_new(cls_markdown, sizeof(RefMarkdown));
    *vret = vp_Value(r);

    fs->Mem_init(&r->mem, 1024);
    r->tabstop = 4;

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
        int64_t i64 = fs->Value_int64(v[1], &err);
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

    if (!parse_markdown(r, Value_cstr(v[1]))) {
        return FALSE;
    }

    return TRUE;
}
static int markdown_make_xml(Value *vret, Value *v, RefNode *node)
{
    RefMarkdown *r = Value_vp(*v);
    RefStr *root_name = Value_vp(v[1]);
    Ref *root;

    if (!xst->is_valid_elem_name(root_name->c, root_name->size)) {
        return FALSE;
    }

    if (!link_markdown(r)) {
        return FALSE;
    }
    root = xmlelem_new(root_name->c);
    *vret = vp_Value(root);
    if (!xml_from_markdown(root, r, r->root)) {
        return FALSE;
    }

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
    fs->define_native_func_a(n, markdown_make_xml, 1, 1, NULL, fs->cls_str);
    fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_markdown = m;
    mod_xml = fs->get_module_by_name("datafmt.xml", -1, TRUE, FALSE);
    xst = mod_xml->u.m.ext;

    fs->add_unresolved_ptr(m, mod_xml, "XMLElem", &cls_xmlelem);
    fs->add_unresolved_ptr(m, mod_xml, "XMLText", &cls_xmltext);
    fs->add_unresolved_ptr(m, mod_xml, "XMLNodeList", &cls_nodelist);

    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\n";
}
