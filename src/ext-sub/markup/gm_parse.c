#include "genml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


enum {
    TK_NONE,
    TK_ERROR,
    TK_EOF,

    TK_NL,
    TK_TEXT,
    TK_RAWTEXT,
    TK_CMD,         // \cmd
    TK_TOPLEVEL,    // \BEGIN
    TK_LC,          // {
    TK_RC,          // }

    TK_RANGE,
    TK_RANGE_OPEN,
    TK_RANGE_CLOSE,
    TK_RAW_CMD,
    TK_HEADING,
    TK_LIST,
    TK_TABLE,
    TK_TABLE_SEP,
};

typedef struct {
    GenML *gm;
    const char *p;

    int line;
    int type;
    int is_head;
    int charval;
    int32_t intval;
    StrBuf strval;
    char *cstrval;

    StrBuf prevlist;
    int range_close;
} GMTok;


static int is_public_member_name(RefStr *rs)
{
    int i;

    if (rs->size == 0) {
        return FALSE;
    }
    if (!isalpha(rs->c[0] & 0xFF)) {
        return FALSE;
    }
    for (i = 1; i < rs->size; i++) {
        if (!isalnumu_fox(rs->c[i])) {
            return FALSE;
        }
    }
    return TRUE;
}
static char *Mem_strdup(Mem *mem, const char *src, int len)
{
    char *p;
    if (len < 0) {
        len = strlen(src);
    }
    p = fs->Mem_get(mem, len + 1);
    memcpy(p, src, len);
    p[len] = '\0';
    return p;
}

static PtrList *PtrList_push(PtrList **pp, Mem *mem)
{
    PtrList *pos = *pp;

    while (pos != NULL) {
        pp = &pos->next;
        pos = *pp;
    }
    pos = fs->Mem_get(mem, sizeof(PtrList));
    pos->next = NULL;
    *pp = pos;
    return pos;
}

/////////////////////////////////////////////////////////////////////////

static GenMLPunct *GenML_add_punct(GenML *gm, int ch, int is_heading)
{
    GenMLPunct *pc;

    // \{}を除く記号
    if ((ch < ' ' || ch > '/') &&
        (ch < ':' || ch > '@') &&
        (ch != '[') &&
        (ch < ']' || ch > '`') &&
        (ch != '|') && (ch != '~'))
    {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal character %c", ch);
        return NULL;
    }
    if (!is_heading && ch == ' ') {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal character %c", ch);
        return NULL;
    }
    pc = gm->punct[ch];
    if (pc == NULL) {
        pc = fs->Mem_get(&gm->mem, sizeof(GenMLPunct));
        memset(pc, 0, sizeof(GenMLPunct));
        gm->punct[ch] = pc;
    }
    return pc;
}
static int GenML_parse_grammer(GenML *gm, RefStr *name, RefStr *value)
{
    int ch = value->c[0];

    // rawtext
    if (value->size == 3) {
        if (value->c[1] == ch && value->c[2] == ch) {
            switch (ch) {
            case ')': case ']': case '>':
                fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal character %c", ch);
                return FALSE;
            }
            {
                GenMLPunct *pc = GenML_add_punct(gm, ch, FALSE);
                if (pc == NULL) {
                    return FALSE;
                }
                if ((pc->flags & PUNCT_RAW) != 0) {
                    fs->throw_errorf(fs->mod_lang, "ValueError", "Duplicate definition (%r)", value);
                    return FALSE;
                }
                pc->flags |= PUNCT_RAW;
                pc->raw = name;
            }
            return TRUE;
        }
    }

    // list, table, heading
    if (value->size >= 5) {
        int i = 1;
        while (value->c[i] == ' ' || value->c[i] == '\t') {
            i++;
        }
        if (str_eq(value->c + i, value->size - i, "list", 4)) {
            GenMLPunct *pc = GenML_add_punct(gm, ch, FALSE);
            if (pc == NULL) {
                return FALSE;
            }
            if ((pc->flags & (PUNCT_HEADING | PUNCT_LIST | PUNCT_TABLE)) != 0) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Duplicate definition (%r)", value);
                return FALSE;
            }
            pc->flags |= PUNCT_LIST;
            pc->heading_list_table = fs->intern(name->c, name->size);
            return TRUE;
        }
        if (str_eq(value->c + i, value->size - i, "table", 5)) {
            GenMLPunct *pc = GenML_add_punct(gm, ch, FALSE);
            if (pc == NULL) {
                return FALSE;
            }
            if ((pc->flags & (PUNCT_HEADING | PUNCT_LIST | PUNCT_TABLE)) != 0) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Duplicate definition (%r)", value);
                return FALSE;
            }
            pc->flags |= PUNCT_TABLE;
            pc->heading_list_table = name;
            return TRUE;
        }
        if (str_eq(value->c + i, value->size - i, "heading", 7)) {
            GenMLPunct *pc = GenML_add_punct(gm, ch, TRUE);
            if (pc == NULL) {
                return FALSE;
            }
            if ((pc->flags & (PUNCT_HEADING | PUNCT_LIST | PUNCT_TABLE)) != 0) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Duplicate definition (%r)", value);
                return FALSE;
            }
            pc->flags |= PUNCT_HEADING;
            pc->heading_list_table = name;
            return TRUE;
        }
    }
    // inline
    if (value->size >= 8) {
        int i = 1;
        while (value->c[i] == ' ' || value->c[i] == '\t') {
            i++;
        }
        if (str_eq(value->c + i, 6, "inline", 6)) {
            int ch2;
            int pair;

            i += 6;
            while (value->c[i] == ' ' || value->c[i] == '\t') {
                i++;
            }
            if (value->size > i + 1) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal GenML grammer definition");
                return FALSE;
            }
            ch2 = value->c[i];
            switch (ch) {
            case '(':
                pair = ')';
                break;
            case '[':
                pair = ']';
                break;
            case '<':
                pair = '>';
                break;
            case ')': case ']': case '>':
                fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal GenML grammer definition");
                return FALSE;
            default:
                pair = ch;
                break;
            }
            if (ch2 != pair) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Mismatch open-close");
                return FALSE;
            }
            {
                GenMLPunct *pc_open = GenML_add_punct(gm, ch, FALSE);
                GenMLPunct *pc_close = (pc_open != NULL ? GenML_add_punct(gm, ch2, FALSE) : NULL);
                if (pc_open == NULL || pc_close == NULL) {
                    return FALSE;
                }
                if ((pc_open->flags & (PUNCT_RANGE_OPEN | PUNCT_RANGE_CLOSE)) != 0) {
                    fs->throw_errorf(fs->mod_lang, "ValueError", "Duplicate definition (%r)", value);
                    return FALSE;
                }
                if ((pc_close->flags & (PUNCT_RANGE_OPEN | PUNCT_RANGE_CLOSE)) != 0) {
                    fs->throw_errorf(fs->mod_lang, "ValueError", "Duplicate definition (%r)", value);
                    return FALSE;
                }
                pc_open->flags |= PUNCT_RANGE_OPEN;
                pc_close->flags |= PUNCT_RANGE_CLOSE;
                pc_open->range = name;
                pc_close->range = name;
            }
            return TRUE;
        }
    }

    fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal GenML grammer definition (%r)", value);
    return FALSE;
}

GenML *GenML_new(RefMap *rm)
{
    GenML *gm = fs->buf_new(cls_genml, sizeof(GenML));
    fs->Mem_init(&gm->mem, 1024);

    if (rm != NULL) {
        int i;
        for (i = 0; i < rm->entry_num; i++) {
            HashValueEntry *e;
            for (e = rm->entry[i]; e != NULL; e = e->next) {
                RefStr *name;
                RefStr *value;
                GenMLPunct *pc;
                if (fs->Value_type(e->key) != fs->cls_str || fs->Value_type(e->val) != fs->cls_str) {
                    fs->throw_errorf(fs->mod_lang, "TypeError", "{Str:Str, ...} required");
                    goto FAILED;
                }
                name = Value_vp(e->key);
                value = Value_vp(e->val);
                if (value->size == 0) {
                    fs->throw_errorf(fs->mod_lang, "ValueError", "Cannot define empty value");
                    return NULL;
                }

                if (str_eq(name->c, name->size, "_comment", 8)) {
                    pc = GenML_add_punct(gm, value->c[0], FALSE);
                    if (pc == NULL) {
                        goto FAILED;
                    }
                    pc->comment = value;
                } else if (is_public_member_name(name)) {
                    if (!GenML_parse_grammer(gm, name, value)) {
                        goto FAILED;
                    }
                } else {
                    fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal GenML grammer definition (%r)", value);
                    goto FAILED;
                }
            }
        }
    }

    return gm;

FAILED:
    fs->Mem_close(&gm->mem);
    fs->unref(vp_Value(gm));
    return NULL;
}

static int GenML_find_sub_name(GenML *gm, const char *name)
{
    GenMLNode *node;

    for (node = gm->sub; node != NULL; node = node->next) {
        if (strcmp(node->name, name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////

static GenMLNode *GenMLNode_new(GMTok *tk, Mem *mem, int type)
{
    GenMLNode *node = fs->Mem_get(mem, sizeof(GenMLNode));
    memset(node, 0, sizeof(GenMLNode));

    if (type > GMLTYPE_ARG) {
        node->name = Mem_strdup(mem, tk->strval.p, tk->strval.size);
    }
    node->type = type;
    return node;
}
static const char *GenML_get_range_name(GMTok *tk, int ch)
{
    GenMLPunct *pc = (ch & 0x80) == 0 ? tk->gm->punct[ch] : NULL;
    if (pc != NULL && pc->range != NULL) {
        return Mem_strdup(&tk->gm->mem, pc->range->c, pc->range->size);
    } else {
        return NULL;
    }
}
static const char *GenML_get_heading_list_table_name(GMTok *tk, int ch)
{
    GenMLPunct *pc = (ch & 0x80) == 0 ? tk->gm->punct[ch] : NULL;
    if (pc != NULL && pc->heading_list_table != NULL) {
        return Mem_strdup(&tk->gm->mem, pc->heading_list_table->c, pc->heading_list_table->size);
    } else {
        return NULL;
    }
}
static const char *GenML_get_raw_name(GMTok *tk, int ch)
{
    GenMLPunct *pc = (ch & 0x80) == 0 ? tk->gm->punct[ch] : NULL;
    if (pc != NULL && pc->raw != NULL) {
        return Mem_strdup(&tk->gm->mem, pc->raw->c, pc->raw->size);
    } else {
        return NULL;
    }
}

static void GMTok_init(GMTok *tk, GenML *gm, const char *src)
{
    tk->gm = gm;
    tk->type = TK_NONE;
    tk->p = src;
    tk->line = 1;
    tk->is_head = TRUE;
    fs->StrBuf_init(&tk->strval, 64);
    fs->StrBuf_init(&tk->prevlist, 64);
    tk->cstrval = NULL;
    tk->range_close = '\0';
}
static void GMTok_close(GMTok *tk)
{
    StrBuf_close(&tk->strval);
    StrBuf_close(&tk->prevlist);
}

static int count_rawtext_begin(GMTok *tk)
{
    const char *p = tk->p;
    while (*p == '{') {
        p++;
    }
    if (*p == '\n') {
        return p - tk->p;
    } else {
        return 0;
    }
}
static int count_rawtext_begin_ch(GMTok *tk, int ch)
{
    const char *p = tk->p;
    while (*p == ch) {
        p++;
    }
    return p - tk->p;
}
static GenMLPunct *get_punct(GenMLPunct **punct, int ch)
{
    if ((ch & 0x80) != 0) {
        return NULL;
    }
    return punct[ch & 0x7F];
}
static void GMTok_next_text(GMTok *tk, int type)
{
    GenMLPunct **punct = tk->gm->punct;
    tk->strval.size = 0;

    if (type == TK_CMD) {
        if (isupper_fox(*tk->p)) {
            type = TK_TOPLEVEL;
        } else {
            fs->StrBuf_add(&tk->strval, "tag_", 4);
        }
    }

    for (;;) {
        int ch = *tk->p & 0xFF;
        int c2;
        GenMLPunct *pc = get_punct(punct, ch);

        if (pc != NULL && (pc->flags & (PUNCT_RANGE_OPEN | PUNCT_RANGE_CLOSE)) != 0) {
            tk->type = type;
            return;
        }
        switch (ch) {
        case '\0':
        case '\n':
        case '{':
        case '}':
            tk->type = type;
            return;
        case '\\':
            c2 = tk->p[1] & 0xFF;
            if (c2 == '\0') {
                tk->type = TK_ERROR;
                return;
            } else if (isalnum(c2)) {
                tk->type = type;
                return;
            } else if (c2 == '\n') {
                tk->p++;
            } else {
                fs->StrBuf_add_c(&tk->strval, c2);
                tk->p++;
            }
            break;
        default:
            fs->StrBuf_add_c(&tk->strval, ch);
            break;
        }
        tk->p++;
    }
}
// ```ruby
// puts "hello"
// ```
// -> ruby
static int GMTok_next_rawtext_option(GMTok *tk)
{
    const char *p = tk->p;
    while (*tk->p != '\0' && *tk->p != '\n') {
        tk->p++;
    }
    if (*tk->p == '\0') {
        return FALSE;
    }
    if (tk->p > p) {
        tk->cstrval = Mem_strdup(&tk->gm->mem, p, tk->p - p);
    } else {
        tk->cstrval = NULL;
    }
    tk->p++;
    return TRUE;
}
static void GMTok_next_rawtext(GMTok *tk, int n, int ch, int type)
{
    tk->strval.size = 0;

    while (*tk->p != '\0') {
        if (*tk->p == '\n') {
            int i;
            const char *p = tk->p + 1;
            tk->line++;

            for (i = 0; i < n; i++) {
                if (p[i] != ch) {
                    break;
                }
            }
            if (i == n) {
                tk->p = p + i;
                tk->charval = ch;
                tk->type = type;
                return;
            }
        }
        fs->StrBuf_add_c(&tk->strval, *tk->p);
        tk->p++;
    }
    tk->type = TK_ERROR;
}

static void GMTok_next(GMTok *tk)
{
START:
    switch (*tk->p) {
    case '\0':
        tk->type = TK_EOF;
        break;
    case '\n':
        tk->p++;
        tk->line++;
        tk->type = TK_NL;
        tk->is_head = TRUE;
        break;
    case '{':
        {
            int count = count_rawtext_begin(tk);
            if (count >= 3) {
                tk->p += count + 1;
                GMTok_next_rawtext(tk, count, '}', TK_RAWTEXT);
            } else {
                tk->p++;
                tk->type = TK_LC;
                tk->is_head = FALSE;
            }
        }
        break;
    case '}':
        tk->p++;
        tk->type = TK_RC;
        tk->is_head = FALSE;
        break;
    case '\\':
        if (isalnumu_fox(tk->p[1])) {
            tk->p++;
            GMTok_next_text(tk, TK_CMD);
        } else {
            GMTok_next_text(tk, TK_TEXT);
        }
        tk->is_head = FALSE;
        break;

    default:
        {
            GenMLPunct *pc = get_punct(tk->gm->punct, *tk->p);

            if (pc != NULL) {
                // comment
                RefStr *cmt = pc->comment;
                if (cmt != NULL && str_eq(tk->p, cmt->size, cmt->c, cmt->size)) {
                    tk->p += cmt->size;
                    while (*tk->p != '\n' && *tk->p != '\0') {
                        tk->p++;
                    }
                    if (*tk->p == '\n') {
                        tk->p++;
                        tk->line++;
                    }
                    goto START;
                }
                // rawtext
                if ((pc->flags & PUNCT_RAW) != 0) {
                    int ch = *tk->p;
                    int count = count_rawtext_begin_ch(tk, ch);
                    if (count >= 3) {
                        int pair = ch;
                        switch (ch) {
                        case '(':
                            pair = ')';
                            break;
                        case '[':
                            pair = ']';
                            break;
                        case '<':
                            pair = '>';
                            break;
                        }
                        tk->p += count;
                        if (!GMTok_next_rawtext_option(tk)) {
                            tk->type = TK_ERROR;
                            break;
                        }
                        GMTok_next_rawtext(tk, count, pair, TK_RAW_CMD);
                        break;
                    }
                }
                // -list or |table
                if (tk->is_head && (pc->flags & (PUNCT_LIST | PUNCT_TABLE)) != 0) {
                    GenMLPunct **punct = tk->gm->punct;
                    tk->strval.size = 0;
                    tk->type = TK_LIST;
                    while ((pc = get_punct(punct, *tk->p)) != NULL) {
                        if ((pc->flags & (PUNCT_LIST | PUNCT_TABLE)) == 0) {
                            break;
                        }
                        fs->StrBuf_add_c(&tk->strval, *tk->p);
                        tk->p++;
                        if ((pc->flags & PUNCT_TABLE) != 0) {
                            tk->type = TK_TABLE;
                            break;
                        }
                    }
                    while (*tk->p == ' ' || *tk->p == '\t') {
                        tk->p++;
                    }
                    break;
                }
                // # heading
                if (tk->is_head && (pc->flags & PUNCT_HEADING) != 0) {
                    tk->type = TK_HEADING;
                    tk->charval = *tk->p;
                    tk->intval = 0;
                    while (*tk->p == tk->charval) {
                        tk->intval++;
                        tk->p++;
                    }
                    while (*tk->p == ' ' || *tk->p == '\t') {
                        tk->p++;
                    }
                    break;
                }
                // |table|
                if ((pc->flags & PUNCT_TABLE) != 0) {
                    tk->type = TK_TABLE_SEP;
                    tk->p++;
                    break;
                }
                // [inline]
                if ((pc->flags & (PUNCT_RANGE_OPEN | PUNCT_RANGE_CLOSE)) != 0) {
                    tk->charval = *tk->p;
                    switch (tk->charval) {
                    case '(': case '[': case '<':
                        tk->type = TK_RANGE_OPEN;
                        break;
                    case ')': case ']': case '>':
                        tk->type = TK_RANGE_CLOSE;
                        break;
                    default:
                        tk->type = TK_RANGE;
                        break;
                    }
                    tk->p++;
                    break;
                }
            }
        }
        GMTok_next_text(tk, TK_TEXT);
        tk->is_head = FALSE;
        break;
    }
}

static void syntax_error(GMTok *tk)
{
    fs->throw_errorf(mod_markup, "GenMLError", "Syntax error (%d)", tk->line);
}

static int get_match_length(StrBuf *s1, StrBuf *s2)
{
    int len = (s1->size < s2->size) ? s1->size : s2->size;
    int i;

    for (i = 0; i < len; i++) {
        if (s1->p[i] != s2->p[i]) {
            return i;
        }
    }
    return len;
}

////////////////////////////////////////////////////////////////////////////////

static int parse_genml_sub(GenML *gm, GMTok *tk, GenMLNode **pp);
static int parse_genml_line(GenML *gm, GMTok *tk, GenMLNode **pp, int parse_table);


static int parse_genml_cmd(GenML *gm, GMTok *tk, GenMLNode **pp)
{
    GenMLNode *node = GenMLNode_new(tk, &gm->mem, GMLTYPE_COMMAND);
    *pp = node;
    pp = &node->child;
    GMTok_next(tk);

    if (tk->type == TK_LC && *tk->p == '\n') {
        GMTok_next(tk);
        GMTok_next(tk);
        for (;;) {
            if (tk->type == TK_RC) {
                GMTok_next(tk);
                break;
            }
            if (tk->type == TK_LC) {
                // \cmd{\n{arg11}{arg12}\n{arg21}{arg22}\n}
                node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                *pp = node;
                if (!parse_genml_line(gm, tk, &node->child, FALSE)) {
                    return FALSE;
                }
            } else {
                // \cmd{\narg1\narg2\n}
                node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                *pp = node;
                if (!parse_genml_sub(gm, tk, &node->child)) {
                    return FALSE;
                }
            }
            pp = &node->next;
            if (tk->type != TK_NL) {
                syntax_error(tk);
                return FALSE;
            }
            GMTok_next(tk);
        }
    } else {
        while (tk->type == TK_LC || tk->type == TK_RAWTEXT) {
            // \cmd{arg1}{arg2}
            if (tk->type == TK_LC) {
                GMTok_next(tk);
                node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                *pp = node;
                if (!parse_genml_sub(gm, tk, &node->child)) {
                    return FALSE;
                }
                node = *pp;
                pp = &node->next;
                if (tk->type != TK_RC) {
                    fs->throw_errorf(mod_markup, "GenMLError", "'}' required (%d)", tk->line);
                    return FALSE;
                }
            } else {
                node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                node->child = GenMLNode_new(tk, &gm->mem, GMLTYPE_TEXT);
                *pp = node;
                pp = &node->next;
                GMTok_next(tk);
            }
            GMTok_next(tk);
        }
    }
    return TRUE;
}
static int parse_genml_sub(GenML *gm, GMTok *tk, GenMLNode **pp)
{
    GenMLNode *node = NULL;

    for (;;) {
        switch (tk->type) {
        case TK_CMD:
            if (!parse_genml_cmd(gm, tk, pp)) {
                return FALSE;
            }
            node = *pp;
            pp = &node->next;
            break;
        case TK_TEXT:
            node = GenMLNode_new(tk, &gm->mem, GMLTYPE_TEXT);
            GMTok_next(tk);
            *pp = node;
            pp = &node->next;
            break;
        case TK_RANGE:
        case TK_RANGE_OPEN:
            if (tk->range_close == tk->charval) {
                if (node == NULL) {
                    node = GenMLNode_new(tk, &gm->mem, GMLTYPE_NONE);
                    *pp = node;
                }
                return TRUE;
            } else {
                int cl = tk->charval;
                int range_close = tk->range_close;
                switch (cl) {
                case '(':
                    cl = ')';
                    break;
                case '[':
                    cl = ']';
                    break;
                case '<':
                    cl = '>';
                    break;
                }
                tk->range_close = cl;
                node = GenMLNode_new(tk, &gm->mem, GMLTYPE_COMMAND);
                node->name = GenML_get_range_name(tk, tk->charval);
                node->child = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                GMTok_next(tk);
                if (!parse_genml_sub(gm, tk, &node->child->child)) {
                    return FALSE;
                }
                tk->range_close = range_close;
                if ((tk->type != TK_RANGE && tk->type != TK_RANGE_CLOSE) || tk->charval != cl) {
                    syntax_error(tk);
                    return FALSE;
                }
                GMTok_next(tk);
                *pp = node;
                pp = &node->next;
            }
            break;
        case TK_RAW_CMD:
            node = GenMLNode_new(tk, &gm->mem, GMLTYPE_NONE);
            node->type = GMLTYPE_COMMAND;
            node->name = GenML_get_raw_name(tk, tk->charval);
            {
                GenMLNode *arg = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                node->child = arg;
                if (tk->cstrval != NULL) {
                    GenMLNode *text = GenMLNode_new(tk, &gm->mem, GMLTYPE_NONE);
                    text->type = GMLTYPE_TEXT;
                    text->name = tk->cstrval;
                    arg->child = text;

                    arg->next = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                    arg = arg->next;
                }
                arg->child = GenMLNode_new(tk, &gm->mem, GMLTYPE_TEXT);
            }
            GMTok_next(tk);
            *pp = node;
            pp = &node->next;
            break;
        default:
            if (node == NULL) {
                node = GenMLNode_new(tk, &gm->mem, GMLTYPE_NONE);
                *pp = node;
            }
            return TRUE;
        }
    }
}
static int parse_genml_line(GenML *gm, GMTok *tk, GenMLNode **pp, int parse_table)
{
    GenMLNode *node = NULL;

    for (;;) {
        switch (tk->type) {
        case TK_LC:
            node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
            *pp = node;
            pp = &node->next;
            GMTok_next(tk);
            if (!parse_genml_sub(gm, tk, &node->child)) {
                return FALSE;
            }
            if (tk->type != TK_RC) {
                syntax_error(tk);
                return FALSE;
            }
            GMTok_next(tk);
            break;
        case TK_TABLE:
            if (parse_table) {
                node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                *pp = node;
                pp = &node->next;
                GMTok_next(tk);
            }
            break;
        case TK_RAWTEXT:
            node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
            node->child = GenMLNode_new(tk, &gm->mem, GMLTYPE_TEXT);
            *pp = node;
            pp = &node->next;
            GMTok_next(tk);
            break;
        default:
            return TRUE;
        }
    }
}

static int parse_genml_cmds(GenML *gm, GMTok *tk, GenMLNode **pp, GenMLNode **pp_sub)
{
    for (;;) {
        switch (tk->type) {
        case TK_CMD:
            if (tk->prevlist.size > 0) {
                return TRUE;
            }
            {
                GenMLNode *p;
                if (!parse_genml_cmd(gm, tk, &p)) {
                    return FALSE;
                }
                *pp = p;
                pp = &p->next;
            }
            break;
        case TK_TOPLEVEL:
            if (tk->prevlist.size > 0) {
                return TRUE;
            }
            {
                GenMLNode *p;
                if (!parse_genml_cmd(gm, tk, &p)) {
                    return FALSE;
                }
                if (pp_sub == NULL) {
                    fs->throw_errorf(mod_markup, "GenMLError", "%s is only for toplevel (%d)", p->name, tk->line);
                    return FALSE;
                }
                if (GenML_find_sub_name(gm, p->name)) {
                    fs->throw_errorf(mod_markup, "GenMLError", "Already defined name %s (%d)", p->name, tk->line);
                    return FALSE;
                }
                *pp_sub = p;
                pp_sub = &p->next;
            }
            break;
        case TK_RAW_CMD:
            if (tk->prevlist.size > 0) {
                return TRUE;
            }
            {
                GenMLNode *p;
                if (!parse_genml_sub(gm, tk, &p)) {
                    return FALSE;
                }
                *pp = p;
                pp = &p->next;
            }
            break;
        case TK_HEADING:
            if (tk->prevlist.size > 0) {
                return TRUE;
            }
            {
                GenMLNode *node = GenMLNode_new(tk, &gm->mem, GMLTYPE_COMMAND_OPT);
                node->name = GenML_get_heading_list_table_name(tk, tk->charval);
                node->opt = tk->intval;

                *pp = node;
                pp = &node->next;
                PtrList_push(&gm->headings, &gm->mem)->u.p = node;
                GMTok_next(tk);

                // # heading
                if (!parse_genml_sub(gm, tk, &node->child)) {
                    return FALSE;
                }
                if (tk->type == TK_NL) {
                    GMTok_next(tk);
                } else if (tk->type != TK_EOF) {
                    syntax_error(tk);
                    return FALSE;
                }
            }
            break;
        case TK_LIST:
        case TK_TABLE:
            {
                int match = get_match_length(&tk->strval, &tk->prevlist);
                if (match < tk->prevlist.size) {
                    return TRUE;
                } else if (match < tk->strval.size) {
                    int ch = tk->strval.p[match];
                    GenMLNode *node = GenMLNode_new(tk, &gm->mem, GMLTYPE_COMMAND);
                    node->name = GenML_get_heading_list_table_name(tk, ch);
                    fs->StrBuf_add_c(&tk->prevlist, ch);
                    if (!parse_genml_cmds(gm, tk, &node->child, NULL)) {
                        return FALSE;
                    }
                    *pp = node;
                    pp = &node->next;
                    tk->prevlist.size--;
                } else if (tk->type == TK_LIST) {
                    GenMLNode *node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                    *pp = node;
                    pp = &node->next;
                    GMTok_next(tk);
                    // - list
                    if (!parse_genml_sub(gm, tk, &node->child)) {
                        return FALSE;
                    }
                    if (tk->type == TK_NL) {
                        GMTok_next(tk);
                    } else if (tk->type != TK_EOF) {
                        syntax_error(tk);
                        return FALSE;
                    }
                } else {
                    GenMLNode *node = GenMLNode_new(tk, &gm->mem, GMLTYPE_ARG);
                    *pp = node;
                    pp = &node->next;
                    GMTok_next(tk);
                    // | table | table
                    if (!parse_genml_line(gm, tk, &node->child, TRUE)) {
                        return FALSE;
                    }
                    if (tk->type == TK_NL) {
                        GMTok_next(tk);
                    } else if (tk->type != TK_EOF) {
                        syntax_error(tk);
                        return FALSE;
                    }
                }
            }
            break;
        case TK_NL:
            if (tk->prevlist.size > 0) {
                return TRUE;
            }
            GMTok_next(tk);
            break;
        default:
            return TRUE;
        }
    }
}

#if 0

static void dump_genml(GenMLNode *node, int lv)
{
    if (node != NULL) {
        const char *name[] = {
            "NONE",
            "ARGS",
            "CATS",
            "ARGN",
            "TEXT",
            "COMD",
        };
        fprintf(stderr, "%*s%s:%s\n", lv, "", name[node->type], node->name);
        dump_genml(node->child, lv + 1);
        dump_genml(node->next, lv);
    }
}
#else

#define dump_genml(node, lv)

#endif

int parse_genml(GenML *gm, const char *src)
{
    GMTok tk;
    int result;

    GMTok_init(&tk, gm, src);
    GMTok_next(&tk);

    result = parse_genml_cmds(gm, &tk, &gm->root, &gm->sub);
    if (result) {
        if (tk.type != TK_EOF) {
            syntax_error(&tk);
            result = FALSE;
        }
    }

    GMTok_close(&tk);
    dump_genml(gm->root, 0);

    return result;
}
