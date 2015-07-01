#include "m_xml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



enum {
    MD_NONE,
    MD_EOS,
    MD_FATAL_ERROR,
    MD_IGNORE,        // child要素を出力

    // 1行
    MD_EMPTY_LINE,    // 空行
    MD_HEADING,       // opt=1...6 <h1>child</h1>, <h2>child</h2>
    MD_HEADING_D,     // === or ---
    MD_PARAGRAPH,     // <p>child</p>
    MD_HORIZONTAL,    // <hr>
    MD_BLOCK_PLUGIN,  // %plugin

    // インライン要素
    MD_TEXT,          // text
    MD_EM,            // <em>child</em>
    MD_STRONG,        // <strong>child</strong>
    MD_STRIKE,        // <strike>child</strike>
    MD_CODE_INLINE,   // <code>#hilight:link text</code>
    MD_LINK,          // <a href="link" title="title">child</a>
    MD_CODE,          // title=filename, <pre><code></code></pre>
    MD_INLINE_PLUGIN, // [%plugin]

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
    INDEX_MARKDOWN_MD,
    INDEX_MARKDOWN_NUM,
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

    char *cstr;
    char *href;
} MDNode;

typedef struct SimpleHash
{
    struct SimpleHash *next;
    uint32_t hash;
    const char *name;
    MDNode *node;
} SimpleHash;

typedef struct {
    int enable_semantic;
    int enable_tex;
    int quiet_error;
    int tabstop;
    int heading;

    Mem mem;
    SimpleHash *link_map[SIMPLE_HASH_MAX];
    MDNode *root;
} Markdown;

typedef struct {
    Markdown *md;
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

static RefStr *str_link_callback;
static RefStr *str_inline_plugin;
static RefStr *str_block_plugin;


static uint32_t str_hash(const char *p)
{
    uint32_t h = 0;
    for (; *p != '\0'; p++) {
        h = h * 31 + *p;
    }
    return h & (SIMPLE_HASH_MAX - 1);
}

static MDNode *SimpleHash_get_node(SimpleHash **hash, const char *name)
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

static void SimpleHash_add_node(SimpleHash **hash, Mem *mem, const char *name, MDNode *node)
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

static void MDTok_init(MDTok *tk, Markdown *md, const char *src)
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
            if (*tk->p != '\0' && *tk->p != '\n') {
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
    // 行末までのスペースを除去
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
                tk->indent = (tk->indent / ts) * ts + 1;
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
        case '\n':
            tk->p++;
            tk->type = MD_EMPTY_LINE;
            tk->head = TRUE;
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
        case '-':
        case '=':
            {
                // 1行全て '='か'-'
                const char *top = tk->p;
                const char *p = tk->p + 1;
                int ch = *tk->p;
                while (*p != '\0' && *p != '\n') {
                    if (*p != ch) {
                        break;
                    }
                    p++;
                }
                if (*p == '\0' || *p == '\n') {
                    fs->StrBuf_alloc(&tk->val, p - top);
                    memset(tk->val.p, ch, p - top);
                    if (*p == '\n') {
                        p++;
                    }
                    tk->p = p;
                    tk->type = MD_HEADING_D;
                    tk->opt = (ch == '=' ? 1 : 2);
                    tk->head = TRUE;
                    return;
                }
                if (ch == '=') {
                    break;
                }
            }
            // fall throught
        case '*':
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
                MDTok_skip_space(tk);
                tk->type = MD_UNORDERD_LIST;
                return;
            }
            break;
        case '|':
            tk->p++;
            MDTok_skip_space(tk);
            tk->type = MD_TABLE;
            return;
        case '%': {
            const char *top;

            tk->p++;
            MDTok_skip_space(tk);
            top = tk->p;
            while (*tk->p != '\0' && *tk->p != '\n') {
                tk->p++;
            }
            fs->StrBuf_add(&tk->val, top, tk->p - top);
            if (*tk->p == '\n') {
                tk->p++;
            }
            tk->type = MD_BLOCK_PLUGIN;
            tk->head = TRUE;
            tk->prev_link = FALSE;
            return;
        }
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
    case '[':
        if ((tk->p[1] & 0xFF) > ' ') {
            tk->p++;
            if (*tk->p == '%') {
                tk->p++;
                tk->type = MD_INLINE_PLUGIN;
            } else {
                tk->type = MD_LINK_BRAKET;
            }
            if (!parse_text(tk, "]", FALSE)) {
                tk->type = MD_FATAL_ERROR;
                return;
            }
            if (*tk->p == ']') {
                tk->p++;
            }
            if (tk->type != MD_INLINE_PLUGIN) {
                if (tk->prev_link) {
                    tk->type = MD_LINK_BRAKET_NEXT;
                    tk->prev_link = TRUE;
                } else {
                    tk->type = MD_LINK_BRAKET;
                    tk->prev_link = TRUE;
                }
            }
            return;
        }
        break;
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
    case '\n':
        tk->p++;
        tk->head = TRUE;
        tk->prev_link = FALSE;
        tk->type = MD_EMPTY_LINE;
        return;
    default:
        break;
    }

    tk->prev_link = FALSE;
    if (!parse_text(tk, "*_`[:", TRUE)) {
        tk->type = MD_FATAL_ERROR;
        return;
    }
    // 末尾の\nを' 'に変換
    if (tk->head) {
        fs->StrBuf_add_c(&tk->val, ' ');
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
    return *tk->p == ':';
}

const char *tktype_to_name(int type)
{
    static const char *name[] = {
        "MD_NONE",
        "MD_EOS",
        "MD_FATAL_ERROR",
        "MD_EMPTY_LINE",
        "MD_HEADING",
        "MD_HEADING_D",
        "MD_SENTENSE",
        "MD_HORIZONTAL",
        "MD_BLOCK_PLUGIN",
        "MD_TEXT",
        "MD_EM",
        "MD_STRONG",
        "MD_STRIKE",
        "MD_CODE_INLINE",
        "MD_LINK",
        "MD_CODE",
        "MD_INLINE_PLUGIN",
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
    return name[type];
}
void dump_mdtree(MDNode *node, int level)
{
    while (node != NULL) {
        int i;
        for (i = 0; i < level; i++) {
            fprintf(stderr, " ");
        }
        if (node->cstr != NULL) {
            fprintf(stderr, "%p:%s '%s'\n", node, tktype_to_name(node->type), node->cstr);
        } else {
            fprintf(stderr, "%p:%s\n", node, tktype_to_name(node->type));
        }
        dump_mdtree(node->child, level + 1);
        node = node->next;
    }
}

////////////////////////////////////////////////////////////////////////////////////

static MDNode *MDNode_new(int type, Markdown *r)
{
    MDNode *n = fs->Mem_get(&r->mem, sizeof(MDNode));
    memset(n, 0, sizeof(MDNode));
    n->type = type;
    n->indent = 0;
    n->cstr = NULL;
    n->href = NULL;
    return n;
}
static int parse_markdown_code_block(Markdown *r, MDTok *tk, MDNode **ppnode, const char *type)
{
    MDNode *node = MDNode_new(MD_TEXT, r);
    node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
    *ppnode = node;
    return TRUE;
}
static int parse_markdown_line(Markdown *r, MDTok *tk, MDNode **ppnode, int term)
{
    MDNode *node = NULL;
    int first = TRUE;
    int bq_level = tk->bq_level;

    for (;;) {
        if (tk->bq_level != bq_level) {
            goto FINALLY;
        }
        if (tk->type == term) {
            MDTok_next(tk);
            goto FINALLY;
        }
        switch (tk->type) {
        case MD_HEADING_D:
            if (!first) {
                goto FINALLY;
            }
            // fall through
        case MD_TEXT:
            node = MDNode_new(MD_TEXT, r);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_COLON:
            node = MDNode_new(MD_TEXT, r);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_LINK:
            node = MDNode_new(MD_LINK, r);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            node->href = node->cstr;
            node->opt = OPT_LINK_RESOLVED;
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_LINK_BRAKET:
            node = MDNode_new(MD_LINK, r);
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

            node = MDNode_new(type, r);
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
                node = MDNode_new(MD_CODE_INLINE, r);
                node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
                *ppnode = node;
                ppnode = &node->next;
                MDTok_next(tk);
            }
            break;
        case MD_FATAL_ERROR:
            return FALSE;
        default:
            goto FINALLY;
        }
        first = FALSE;
    }
FINALLY:
    // 末尾のスペースを除去
    if (node != NULL && node->type != MD_CODE_INLINE && node->cstr != NULL) {
        char *p = node->cstr + strlen(node->cstr) - 1;
        while (p >= node->cstr && isspace_fox(*p)) {
            *p = '\0';
            p--;
        }
    }
    return TRUE;
}

static int parse_markdown_list_block(Markdown *r, MDTok *tk, MDNode **ppnode)
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
            node = MDNode_new(tk->type, r);
            node->indent = tk->indent;
            *ppnode = node;
            ppnode = &node->next;
            if (!parse_markdown_list_block(r, tk, &node->child)) {
                return FALSE;
            }
        } else {
            if (tk->type != type) {
                return TRUE;
            }
            node = MDNode_new(MD_LIST_ITEM, r);
            node->indent = tk->indent;
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            parse_markdown_line(r, tk, &node->child, MD_NONE);
        }
    }
}
// | cell1 | cell2 |
// | ....
static int parse_markdown_list_table_block(Markdown *r, MDTok *tk, MDNode **ppnode)
{
    return TRUE;
}

static int parse_markdown_block(Markdown *r, MDTok *tk, MDNode **ppnode, int bq_level)
{
    MDNode *node;
    MDNode *prev_node = NULL;

    for (;;) {
        if (tk->bq_level > bq_level) {
            node = MDNode_new(MD_BLOCKQUOTE, r);
            node->indent = tk->indent;
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
            node = MDNode_new(MD_HEADING, r);
            node->indent = tk->indent;
            node->opt = tk->opt;
            prev_node = node;
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            parse_markdown_line(r, tk, &node->child, MD_NONE);
            break;
        case MD_HORIZONTAL:
            node = MDNode_new(MD_HORIZONTAL, r);
            prev_node = node;
            *ppnode = node;
            ppnode = &node->next;
            MDTok_next(tk);
            break;
        case MD_ORDERD_LIST:
        case MD_UNORDERD_LIST:
            node = MDNode_new(tk->type, r);
            node->indent = tk->indent;
            prev_node = node;
            *ppnode = node;
            ppnode = &node->next;
            if (!parse_markdown_list_block(r, tk, &node->child)) {
                return FALSE;
            }
            break;
        case MD_TABLE:
            node = MDNode_new(MD_TABLE, r);
            prev_node = node;
            *ppnode = node;
            ppnode = &node->next;
            // TODO
            if (!parse_markdown_list_table_block(r, tk, &node->child)) {
                return FALSE;
            }
            break;
        case MD_CODE:
            node = MDNode_new(MD_CODE, r);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            prev_node = node;
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
                MDTok_skip_space(tk);
                MDTok_next(tk);
                parse_markdown_line(r, tk, &node, MD_NONE);
                SimpleHash_add_node(r->link_map, &r->mem, name, node);
 
                prev_node = NULL;
           } else {
                node = MDNode_new(MD_PARAGRAPH, r);
                node->indent = tk->indent;
                prev_node = node;
                *ppnode = node;
                ppnode = &node->next;
                parse_markdown_line(r, tk, &node->child, MD_NONE);
            }
            break;
        case MD_HEADING_D:
            if (prev_node != NULL && prev_node->type == MD_PARAGRAPH) {
                prev_node->type = MD_HEADING;
                prev_node->opt = tk->opt;
                prev_node = NULL;
                MDTok_next(tk);
                break;
            }
            // fall through
        case MD_TEXT:
        case MD_LINK:
        case MD_EM:
        case MD_STRONG:
        case MD_STRIKE:
        case MD_CODE_INLINE:
            node = MDNode_new(MD_PARAGRAPH, r);
            node->indent = tk->indent;
            prev_node = node;
            *ppnode = node;
            ppnode = &node->next;
            parse_markdown_line(r, tk, &node->child, MD_NONE);
            break;
        case MD_COLON:
            break;
        case MD_BLOCK_PLUGIN:
            node = MDNode_new(MD_BLOCK_PLUGIN, r);
            node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
            prev_node = node;
            *ppnode = node;
            MDTok_next(tk);
            break;
        case MD_FATAL_ERROR:
            return FALSE;
        case MD_EOS:
            return TRUE;
        default:
            MDTok_next(tk);
            prev_node = NULL;
            break;
        }
    }
    return TRUE;
}

// 文字列をmarkdown中間形式に変換
static int parse_markdown(Markdown *r, const char *p)
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

static int ref_has_method(Ref *obj, RefStr *name)
{
    RefNode *type = obj->rh.type;
    return fs->Hash_get_p(&type->u.c.h, name) != NULL;
}
static int link_markdown_plugin(Ref *ref, MDNode *node, RefStr *name, int inline_p)
{
    if (ref_has_method(ref, name)) {
        RefNode *ret_type;

        fs->Value_push("rs", ref, node->cstr);
        if (!fs->call_member_func(name, 1, TRUE)) {
            return FALSE;
        }
        ret_type = fs->Value_type(fg->stk_top[-1]);
        if (ret_type != fs->cls_str) {
            fs->throw_error_select(THROW_RETURN_TYPE__NODE_NODE, fs->cls_str, ret_type);
            return FALSE;
        }
        node->type = MD_IGNORE;

        {
            MDTok tk;
            Markdown *md = Value_ptr(ref->v[INDEX_MARKDOWN_MD]);
            MDTok_init(&tk, md, Value_cstr(fg->stk_top[-1]));
            if (inline_p) {
                tk.head = FALSE;
            }
            MDTok_next(&tk);
            if (!parse_markdown_block(md, &tk, &node->child, 0)) {
                MDTok_close(&tk);
                return FALSE;
            }
            if (inline_p && node->child != NULL) {
                node->child = node->child->child;
            }
            MDTok_close(&tk);
        }

        fs->Value_pop();
    }
    return TRUE;
}
static int link_markdown_sub(Ref *ref, Markdown *r, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        if (node->type == MD_LINK && node->opt == OPT_LINK_NAME_REF) {
            MDNode *nd = SimpleHash_get_node(r->link_map, node->href);
            switch (node->href[0]) {
            case '^':  // footnote
                break;
            default:
                if (nd != NULL) {
                    if (nd->type == MD_TEXT) {
                        StrBuf buf;
                        fs->StrBuf_init(&buf, strlen(nd->cstr));
                        while (nd != NULL && nd->type == MD_TEXT) {
                            fs->StrBuf_add(&buf, nd->cstr, -1);
                            nd = nd->next;
                        }
                        node->opt = OPT_LINK_RESOLVED;
                        node->href = fs->str_dup_p(buf.p, buf.size, &r->mem);
                        StrBuf_close(&buf);
                    } else if (nd->type == MD_LINK) {
                        if (node->cstr == node->href) {   // リンク名とリンク先の参照が同一
                            nd->next = node->next;
                            *node = *nd;
                        } else {                          // リンク名が指定してあるため、そちらを優先
                            node->opt = OPT_LINK_RESOLVED;
                            node->href = nd->href;
                        }
                    }
                } else if (ref_has_method(ref, str_link_callback)) {
                    RefNode *ret_type;
                    RefStr *ret_str;

                    fs->Value_push("rs", ref, node->href);
                    if (!fs->call_member_func(str_link_callback, 1, TRUE)) {
                        return FALSE;
                    }
                    ret_type = fs->Value_type(fg->stk_top[-1]);
                    if (ret_type != fs->cls_str) {
                        fs->throw_error_select(THROW_RETURN_TYPE__NODE_NODE, fs->cls_str, ret_type);
                        return FALSE;
                    }
                    ret_str = Value_vp(fg->stk_top[-1]);
                    node->href = fs->str_dup_p(ret_str->c, ret_str->size, &r->mem);

                    fs->Value_pop();
                }
                break;
            }
        } else if (node->type == MD_BLOCK_PLUGIN) {
            if (!link_markdown_plugin(ref, node, str_block_plugin, FALSE)) {
                return FALSE;
            }
        } else if (node->type == MD_INLINE_PLUGIN) {
            if (!link_markdown_plugin(ref, node, str_inline_plugin, TRUE)) {
                return FALSE;
            }
        }
        if (node->child != NULL) {
            link_markdown_sub(ref, r, node->child);
        }
    }
    return TRUE;
}
static int link_markdown(Ref *r)
{
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (!link_markdown_sub(r, md, md->root)) {
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

static int xml_convert_line(Ref *r, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_TEXT:
            xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            break;
        case MD_LINK: {
            Ref *child = xmlelem_new("a");
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
            if (!xml_convert_line(child, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_CODE_INLINE: {
            Ref *code;
            code = xmlelem_new("code");
            xmlelem_add(r, code);
            xmlelem_add(code, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            break;
        }
        default:
            break;
        }
    }
    return TRUE;
}
static int xml_source_code(Ref *r, const char *code, const char *type)
{
    // TODO 色分け
    xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, code, -1)));
    return TRUE;
}
static int xml_convert_listitem(Ref *r, MDNode *node)
{
    Ref *prev_li = NULL;

    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_LIST_ITEM: {
            Ref *li = xmlelem_new("li");
            xmlelem_add(r, li);
            if (!xml_convert_line(li, node->child)) {
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
                if (!xml_convert_listitem(list, node->child)) {
                    return FALSE;
                }
            }
            break;
        }
    }
    return TRUE;
}
static int xml_from_markdown(Ref *root, Markdown *r, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_HEADING: {
            char tag[4];
            int head = node->opt + r->heading - 1;
            Ref *heading;
            tag[0] = 'h';
            tag[1] = (head <= 6 ? '0' + head : '6');
            tag[2] = '\0';
            heading = xmlelem_new(tag);
            xmlelem_add(root, heading);
            if (!xml_convert_line(heading, node->child)) {
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
            if (!xml_convert_listitem(list, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_PARAGRAPH: {
            Ref *para = xmlelem_new("p");
            xmlelem_add(root, para);
            if (!xml_convert_line(para, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_CODE: {
            Ref *pre = xmlelem_new("pre");
            Ref *code = xmlelem_new("code");
            xmlelem_add(root, pre);
            xmlelem_add(pre, code);
            if (!xml_source_code(code, node->child->cstr, node->cstr)) {
                return FALSE;
            }
            break;
        }
        case MD_BLOCKQUOTE: {
            Ref *blockquote = xmlelem_new("blockquote");
            xmlelem_add(root, blockquote);
            if (!xml_from_markdown(blockquote, r, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_IGNORE:
            xml_from_markdown(root, r, node->child);
            break;
        default:
            break;
        }
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int markdown_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_markdown = FUNC_VP(node);
    Ref *ref;
    Markdown *md;
    RefNode *type = fs->Value_type(*v);

    // 継承可能なクラス
    if (type == fs->cls_fn) {
        ref = fs->ref_new(cls_markdown);
        *vret = vp_Value(ref);
    } else {
        ref = Value_vp(*v);
        *vret = fs->Value_cp(*v);
    }

    md = malloc(sizeof(Markdown));
    memset(md, 0, sizeof(Markdown));
    ref->v[INDEX_MARKDOWN_MD] = ptr_Value(md);

    fs->Mem_init(&md->mem, 1024);
    md->tabstop = 4;
    md->heading = 1;

    return TRUE;
}
static int markdown_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Markdown *r = Value_ptr(ref->v[INDEX_MARKDOWN_MD]);
    fs->Mem_close(&r->mem);
    free(r);
    ref->v[INDEX_MARKDOWN_MD] = VALUE_NULL;

    return TRUE;
}
static int markdown_enable_semantic(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        md->enable_semantic = Value_bool(v[1]);
    } else {
        *vret = bool_Value(md->enable_semantic);
    }
    return TRUE;
}
static int markdown_quiet_error(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        md->quiet_error = Value_bool(v[1]);
    } else {
        *vret = bool_Value(md->quiet_error);
    }
    return TRUE;
}
static int markdown_enable_tex(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        md->enable_tex = Value_bool(v[1]);
    } else {
        *vret = bool_Value(md->enable_tex);
    }
    return TRUE;
}
static int markdown_tabstop(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        int err;
        int64_t i64 = fs->Value_int64(v[1], &err);
        if (err || i64 < 1 || i64 > 16) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Out of range (1 - 16)");
            return FALSE;
        }
        md->tabstop = (int)i64;
    } else {
        *vret = int32_Value(md->tabstop);
    }
    return TRUE;
}
static int markdown_heading(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        int err;
        int64_t i64 = fs->Value_int64(v[1], &err);
        if (err || i64 < 1 || i64 > 6) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Out of range (1 - 6)");
            return FALSE;
        }
        md->heading = (int)i64;
    } else {
        *vret = int32_Value(md->heading);
    }
    return TRUE;
}

static int markdown_compile(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);

    if (!parse_markdown(md, Value_cstr(v[1]))) {
        return FALSE;
    }

    return TRUE;
}
static int markdown_add_link(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    RefStr *key = Value_vp(v[1]);
    RefStr *href = Value_vp(v[2]);

    MDNode *nd = MDNode_new(MD_TEXT, md);
    nd->cstr = fs->str_dup_p(href->c, href->size, &md->mem);
    SimpleHash_add_node(md->link_map, &md->mem, key->c, nd);

    return TRUE;
}
static int markdown_make_xml(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    RefStr *root_name = Value_vp(v[1]);
    Ref *root;

    if (!xst->is_valid_elem_name(root_name->c, root_name->size)) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid element name");
        return FALSE;
    }

    if (!link_markdown(r)) {
        return FALSE;
    }
    root = xmlelem_new(root_name->c);
    *vret = vp_Value(root);
    if (!xml_from_markdown(root, md, md->root)) {
        return FALSE;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int md_escape(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;
    RefStr *src = Value_vp(v[1]);
    int i;

    fs->StrBuf_init(&buf, src->size);
    for (i = 0; i < src->size; i++) {
        int ch = src->c[i];
        switch (ch) {
        case '[': case ']':
        case '(': case ')':
        case '`': case '~': case '_': case '*':
        case '&':
        case '\\':
            fs->StrBuf_add_c(&buf, '\\');
            fs->StrBuf_add_c(&buf, ch);
            break;
        default:
            fs->StrBuf_add_c(&buf, ch);
            break;
        }
    }
    *vret = fs->cstr_Value(fs->cls_str, buf.p, buf.size);
    StrBuf_close(&buf);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_function(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "md_escape", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, md_escape, 1, 1, NULL, fs->cls_str);
}
static void define_class(RefNode *m)
{
    RefNode *n;
    RefNode *cls;

    cls = fs->define_identifier(m, m, "Markdown", NODE_CLASS, NODEOPT_ABSTRACT);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, markdown_new, 0, 0, cls);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_NEW_N, 0);
    fs->define_native_func_a(n, markdown_dispose, 0, 0, cls);
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
    n = fs->define_identifier(m, cls, "heading", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_heading, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "heading=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_heading, 1, 1, NULL, fs->cls_int);

    n = fs->define_identifier(m, cls, "compile", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_compile, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "add_link", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_add_link, 2, 2, NULL, fs->cls_str, fs->cls_str);
    n = fs->define_identifier(m, cls, "make_xml", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_make_xml, 1, 1, NULL, fs->cls_str);

    cls->u.c.n_memb = INDEX_MARKDOWN_NUM;
    fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_markdown = m;
    mod_xml = fs->get_module_by_name("marshal.xml", -1, TRUE, FALSE);
    xst = mod_xml->u.m.ext;

    fs->add_unresolved_ptr(m, mod_xml, "XMLElem", &cls_xmlelem);
    fs->add_unresolved_ptr(m, mod_xml, "XMLText", &cls_xmltext);
    fs->add_unresolved_ptr(m, mod_xml, "XMLNodeList", &cls_nodelist);

    str_link_callback = fs->intern("link_callback", -1);
    str_inline_plugin = fs->intern("inline_plugin", -1);
    str_block_plugin = fs->intern("block_plugin", -1);

    define_class(m);
    define_function(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\n";
}
