#include "markdown.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct {
    Markdown *md;
    const char *p;
    int head;
    int prev_link;
    int table_row;

    uint16_t type;
    uint16_t opt;
    uint16_t bq_level;
    uint16_t indent;

    StrBuf val;
} MDTok;



#ifndef NO_DEBUG

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

#endif

static int all_chars_equal(StrBuf *sb, int ch)
{
    const char *p = sb->p;
    const char *end = p + sb->size;

    while (p < end) {
        if (*p != ch) {
            return FALSE;
        }
        p++;
    }
    return TRUE;
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
    tk->table_row = FALSE;

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
    tk->opt = OPT_TEXT_NO_BACKSLASHES;

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
            tk->opt = OPT_TEXT_BACKSLASHES;
            break;
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
        tk->table_row = FALSE;

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
            tk->type = MD_NEWLINE;
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
            tk->table_row = TRUE;
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
            if (!parse_text(tk, "\n>", FALSE)) {
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
            if (!parse_text(tk, "\n]", FALSE)) {
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
                if (!parse_text(tk, "\n)", FALSE)) {
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
        tk->type = MD_NEWLINE;
        return;
    case '|':
        if (tk->table_row) {
            tk->p++;
            tk->opt = 1;
            while (*tk->p == '|') {
                tk->p++;
                tk->opt++;
            }
            tk->type = MD_TABLE;
            return;
        }
        break;
    default:
        break;
    }

    tk->prev_link = FALSE;
    if (!parse_text(tk, tk->table_row ? "\n*_`[:|" : "\n*_`[:", TRUE)) {
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

////////////////////////////////////////////////////////////////////////////////////

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
            node->cstr = fs->str_dup_p(":", 1, &r->mem);
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
            if (tk->type == type) {
                MDTok_next(tk);
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
        case MD_NEWLINE:
            MDTok_next(tk);
            if (tk->type == MD_NEWLINE) {
                MDTok_next(tk);
                goto FINALLY;
            } else {
                node = MDNode_new(MD_TEXT, r);
                node->cstr = fs->str_dup_p(" ", 1, &r->mem);
                *ppnode = node;
                ppnode = &node->next;
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
    int head = TRUE;
    MDNode **pproot = ppnode;
    MDNode *node;

    for (;;) {
        if (tk->type != MD_TABLE) {
            break;
        }
        MDTok_next(tk);
        if (tk->type == MD_TEXT && tk->opt == OPT_TEXT_NO_BACKSLASHES && all_chars_equal(&tk->val, '-')) {
            if (head) {
                MDNode *p;
                for (p = *pproot; p != NULL; p = p->next) {
                    MDNode *q;
                    for (q = p->child; q != NULL; q = q->next) {
                        q->type = MD_TABLE_HEADER;
                    }
                }
                head = FALSE;
            }
        } else {
            MDNode **ppcell;
            node = MDNode_new(MD_TABLE_ROW, r);
            *ppnode = node;
            ppnode = &node->next;
            ppcell = &node->child;
            for (;;) {
                MDNode *cell = MDNode_new(MD_TABLE_CELL, r);
                *ppcell = cell;
                ppcell = &cell->next;
                if (!parse_markdown_line(r, tk, &cell->child, MD_TABLE)) {
                    return FALSE;
                }
                if (tk->type != MD_TABLE || tk->head) {
                    cell->opt = 1;
                    break;
                }
                cell->opt = tk->opt;  // colspan
                MDTok_next(tk);
                if (tk->type == MD_NEWLINE || tk->type == MD_EOS) {
                    break;
                }
            }
        }
        while (tk->type != MD_NEWLINE && tk->type != MD_EOS && tk->type != MD_FATAL_ERROR) {
            MDTok_next(tk);
        }
        if (tk->type == MD_NEWLINE) {
            MDTok_next(tk);
        }
    }
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
            if (!parse_markdown_line(r, tk, &node->child, MD_NONE)) {
                return FALSE;
            }
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
int parse_markdown(Markdown *r, const char *p)
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
    return TRUE;
}
static int link_callback_plugin(Ref *ref, Markdown *r, MDNode *node)
{
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
    return TRUE;
}
static int link_markdown_sub(Ref *ref, Markdown *r, MDNode *node)
{
    int has_callback = ref_has_method(ref, str_link_callback);
    int has_block_plugin = ref_has_method(ref, str_block_plugin);
    int has_inline_plugin = ref_has_method(ref, str_inline_plugin);

    for (; node != NULL; node = node->next) {
        if (node->type == MD_LINK) {
            if (node->opt == OPT_LINK_NAME_REF) {
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
                    } else if (has_callback) {
                        if (!link_callback_plugin(ref, r, node)) {
                            return FALSE;
                        }
                    }
                    break;
                }
            } else {
                if (has_callback) {
                    if (!link_callback_plugin(ref, r, node)) {
                        return FALSE;
                    }
                }
            }
        } else if (node->type == MD_BLOCK_PLUGIN) {
            if (has_block_plugin) {
                if (!link_markdown_plugin(ref, node, str_block_plugin, FALSE)) {
                    return FALSE;
                }
            }
        } else if (node->type == MD_INLINE_PLUGIN) {
            if (has_inline_plugin) {
                if (!link_markdown_plugin(ref, node, str_inline_plugin, TRUE)) {
                    return FALSE;
                }
            }
        }
        if (node->child != NULL) {
            link_markdown_sub(ref, r, node->child);
        }
    }
    return TRUE;
}
int link_markdown(Ref *r)
{
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (!link_markdown_sub(r, md, md->root)) {
        return FALSE;
    }
    return TRUE;
}
