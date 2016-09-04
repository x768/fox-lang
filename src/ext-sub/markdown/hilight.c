#include "markdown.h"
#include "m_token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct Word Word;
typedef struct State State;

enum {
    STATE_WORD_SIZE = 32,
};

struct Word {
    Word *next;

    State *self;
    State *right;

    int32_t len;
    char c[0];
};

struct State {
    Word *left[STATE_WORD_SIZE];
    RefStr *name;

    int8_t word;
    int8_t icase;
};

typedef struct {
    Hash state;
    uint8_t word_char[128 / 8];
} Hilight;


static char *read_hilight_file(RefStr *type)
{
    PtrList *res_path;

    for (res_path = fs->resource_path; res_path != NULL; res_path = res_path->next) {
        char *fname = fs->str_printf("%s" SEP_S "hilight" SEP_S "%s.txt", res_path->u.c, type->c);
        char *buf = fs->read_from_file(NULL, fname, NULL);
        free(fname);
        if (buf != NULL) {
            return buf;
        }
    }
    return NULL;
}
static void copy_hilight(Markdown *r, Hilight *dst, Hilight *src)
{
    int i;

    memcpy(dst->word_char, src->word_char, sizeof(dst->word_char));

    for (i = 0; i < src->state.entry_num; i++) {
        HashEntry *he;
        for (he = src->state.entry[i]; he != NULL; he = he->next) {
        }
    }
}
static void add_hilight_state(Markdown *r, Hilight *h, RefStr *name, int word, int icase)
{
    State *state = fs->Mem_get(&r->mem, sizeof(State));
    state->name = name;
    state->word = word;
    state->icase = icase;
    fs->Hash_add_p(&h->state, &r->mem, name, state);
}
// return FALSE: 読み込みエラー
// ph == NULL:   ファイルが存在しない
static int load_hilight(Hilight **ph, Markdown *r, RefStr *type)
{
    Tok tk;
    char *buf;

    Hilight *h = fs->Hash_get_p(&r->hilight, type);
    if (h != NULL) {
        *ph = h;
        return TRUE;
    }

    *ph = NULL;

    buf = read_hilight_file(type);
    if (buf == NULL) {
        return TRUE;
    }

    h = fs->Mem_get(&r->mem, sizeof(Hilight));
    fs->Hash_init(&h->state, &r->mem, 16);

    // 0-9A-Za-z_
    memcpy(h->word_char, "\0\0\0\0\0\0\xff\x03\xfe\xff\xff\x87\xfe\xff\xff\x07", 128 / 8);

    fs->Tok_simple_init(&tk, buf);
    fs->Tok_simple_next(&tk);
    
    if (tk.v.type == TL_VAR && str_eq(tk.str_val.p, tk.str_val.size, "import", 6)) {
        Hilight *h2;
        fs->Tok_simple_next(&tk);
        if (tk.v.type != TL_STR) {
            goto ERROR_END;
        }

        if (!load_hilight(&h2, r, fs->intern(tk.str_val.p, tk.str_val.size))) {
            return FALSE;
        }
        if (h2 == NULL) {
            fs->throw_errorf(fs->mod_lang, "InternalError", "Cannot find hilight file for '%S'", tk.str_val);
            return FALSE;
        }
        copy_hilight(r, h, h2);
        fs->Tok_simple_next(&tk);
    } else {
        add_hilight_state(r, h, fs->intern("_", 1), FALSE, FALSE);
    }
    
    for (;;) {
        switch (tk.v.type) {
        case TL_VAR:
            if (str_eq(tk.str_val.p, tk.str_val.size, "define", 6)) {
                int word = FALSE;
                int icase = FALSE;
                RefStr *name;

                fs->Tok_simple_next(&tk);
                if (tk.v.type != TL_VAR) {
                    goto ERROR_END;
                }
                name = fs->intern(tk.str_val.p, tk.str_val.size);
                fs->Tok_simple_next(&tk);
                while (tk.v.type == TL_VAR) {
                    if (str_eq(tk.str_val.p, tk.str_val.size, "word", 4)) {
                        word = TRUE;
                    } else if (str_eq(tk.str_val.p, tk.str_val.size, "icase", 4)) {
                        icase = TRUE;
                    }
                    fs->Tok_simple_next(&tk);
                }
                add_hilight_state(r, h, name, word, icase);
            } else if (str_eq(tk.str_val.p, tk.str_val.size, "word", 4)) {
                fs->Tok_simple_next(&tk);
                if (tk.v.type != TL_STR) {
                    goto ERROR_END;
                }
                // todo:文字クラス解釈
                fs->Tok_simple_next(&tk);
            } else if (str_eq(tk.str_val.p, tk.str_val.size, "import", 6)) {
                fs->throw_errorf(fs->mod_lang, "InternalError", "'import' cannot place here");
                goto ERROR_END;
            }
            break;
        case TL_STR:
            if (tk.str_val.size > 0) {
                Word *word = fs->Mem_get(&r->mem, sizeof(Word) + tk.str_val.size);
                State *state[3];
                int i;

                word->len = tk.str_val.size;
                memcpy(word->c, tk.str_val.p, tk.str_val.size);
                fs->Tok_simple_next(&tk);
                if (tk.v.type != T_LET) {
                    goto SYNTAX_ERROR;
                }
                for (i = 0; i < 3; i++) {
                    fs->Tok_simple_next(&tk);
                    if (tk.v.type != TL_VAR) {
                        goto SYNTAX_ERROR;
                    }
                    state[i] = fs->Hash_get(&h->state, tk.str_val.p, tk.str_val.size);
                    if (state[i] == NULL) {
                        fs->throw_errorf(fs->mod_lang, "InternalError", "Cannot find state '%S'", tk.str_val);
                        goto ERROR_END;
                    }
                }

                word->self = state[1];
                word->right = state[2];
                {
                    int hash = word->c[0] & (STATE_WORD_SIZE - 1);
                    word->next = state[0]->left[hash];
                    state[0]->left[hash] = word;
                }
                fs->Tok_simple_next(&tk);
            } else {
                goto SYNTAX_ERROR;
            }
            break;
        case T_NL:
            break;
        case T_EOF:
            free(buf);
            fs->Hash_add_p(&r->hilight, &r->mem, type, h);
            *ph = h;
            return TRUE;
        case T_ERR:
            free(buf);
            return FALSE;
        }
        if (tk.v.type == T_NL) {
            fs->Tok_simple_next(&tk);
        } else if (tk.v.type != T_EOF) {
            goto SYNTAX_ERROR;
        }
    }
SYNTAX_ERROR:
    fs->throw_errorf(fs->mod_lang, "InternalError", "Syntax error at line %d", tk.v.line);
    
ERROR_END:
    free(buf);
    return FALSE;
}

// type: e.g. "text/css"
int parse_markdown_code_block(Markdown *r, MDTok *tk, MDNode **ppnode, RefStr *type)
{
    MDNode *node;
    Hilight *hilight;

    if (!load_hilight(&hilight, r, type)) {
        return FALSE;
    }
    if (hilight != NULL) {
        // TODO:色分け
        node = MDNode_new(MD_TEXT, r);
        node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
        *ppnode = node;
    } else {
        // 色分けなし
        node = MDNode_new(MD_TEXT, r);
        node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
        *ppnode = node;
    }

    return TRUE;
}
