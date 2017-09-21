#include "m_text_util.h"
#include "m_token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static char *read_hilight_file(RefStr *type)
{
    PtrList *res_path;

    // TODO:見つからない場合はMimeType.parentをたどる
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
static void add_hilight_state(Hilight *h, RefStr *name, int word, int icase)
{
    State *state = fs->Mem_get(&h->mem, sizeof(State));
    memset(state->left, 0, sizeof(state->left));
    state->name = name;
    state->word = word;
    state->icase = icase;
    fs->Hash_add_p(&h->state, &h->mem, name, state);
}
static int is_word_char(Hilight *h, int ch)
{
    if ((ch & ~0x7F) != 0) {
        return FALSE;
    }
    return h->word_char[ch / 8] & (1 << (ch % 8));
}
static void set_word_char(Hilight *h, int ch)
{
    if ((ch & ~0x7F) != 0) {
        return;
    }
    h->word_char[ch / 8] |= 1 << (ch % 8);
}
static void set_word_char_range(Hilight *h, int c1, int c2)
{
    int ch;

    if ((c1 & ~0x7F) != 0 || (c2 & ~0x7F) != 0) {
        return;
    }
    if (c1 > c2) {
        int tmp = c1;
        c1 = c2;
        c2 = tmp;
    }
    
    for (ch = c1; ch <= c2; ch++) {
        h->word_char[ch / 8] |= 1 << (ch % 8);
    }
}
static int streq_n(const char *p1, const char *p2, int len, int icase)
{
    int i;

    if (icase) {
        for (i = 0; i < len; i++) {
            if (tolower_fox(*p1) != *p2) {
                return FALSE;
            }
            p1++;
            p2++;
        }
    } else {
        for (i = 0; i < len; i++) {
            if (*p1 != *p2) {
                return FALSE;
            }
            p1++;
            p2++;
        }
    }
    return TRUE;
}

static void parse_word_chars(Hilight *h, const char *p, const char *end)
{
    memset(h->word_char, 0, sizeof(h->word_char));

    while (p < end) {
        if (*p == '\\') {
            p++;
            if (p < end) {
                set_word_char(h, *p);
                p++;
            }
        } else {
            int ch = *p;
            p++;

            if (p < end && *p == '-') {
                p++;
                if (p < end) {
                    set_word_char_range(h, ch, *p);
                    p++;
                }
            } else {
                set_word_char(h, ch);
            }
        }
    }
}
static int load_hilight_sub(Hilight *h, char *buf, RefStr *type)
{
    Tok tk;

    fs->Tok_simple_init(&tk, buf);
    fs->Tok_simple_next(&tk);
    
    if (tk.v.type == TL_VAR && str_eq(tk.str_val.p, tk.str_val.size, "import", 6)) {
        char *buf2;
        RefStr *type2;
        int result;

        fs->Tok_simple_next(&tk);
        if (tk.v.type != TL_STR) {
            return FALSE;
        }

        type2 = fs->intern(tk.str_val.p, tk.str_val.size);
        buf2 = read_hilight_file(type2);
        if (buf2 == NULL) {
            fs->throw_errorf(fs->mod_lang, "InternalError", "'%S' not found at line %d (%r)", tk.str_val, tk.v.line, type);
            return FALSE;
        }
        result = load_hilight_sub(h, buf2, type2);
        free(buf2);
        if (!result) {
            return FALSE;
        }

        fs->Tok_simple_next(&tk);
    } else {
        add_hilight_state(h, fs->intern("_", 1), FALSE, FALSE);
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
                    return FALSE;
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
                add_hilight_state(h, name, word, icase);
            } else if (str_eq(tk.str_val.p, tk.str_val.size, "word", 4)) {
                fs->Tok_simple_next(&tk);
                if (tk.v.type != TL_STR) {
                    return FALSE;
                }
                parse_word_chars(h, tk.str_val.p, tk.str_val.p + tk.str_val.size);

                fs->Tok_simple_next(&tk);
            } else if (str_eq(tk.str_val.p, tk.str_val.size, "import", 6)) {
                fs->throw_errorf(fs->mod_lang, "InternalError", "'import' cannot place at line %d (%r)", tk.v.line, type);
                return FALSE;
            }
            break;
        case TL_STR:
            if (tk.str_val.size > 0) {
                Word *word = fs->Mem_get(&h->mem, sizeof(Word) + tk.str_val.size);
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
                        fs->throw_errorf(fs->mod_lang, "InternalError", "Cannot find state '%S' at line %d (%r)", tk.str_val, tk.v.line, type);
                        return FALSE;
                    }
                }

                word->self = state[1];
                word->right = state[2];
                {
                    // 後から追加したほうが優先
                    int hash = word->c[0] & (STATE_WORD_SIZE - 1);
                    word->next = state[0]->left[hash];
                    state[0]->left[hash] = word;
                }
                if (word->self->icase) {
                    for (i = 0; i < word->len; i++) {
                        word->c[i] = tolower_fox(word->c[i]);
                    }
                }
                fs->Tok_simple_next(&tk);
            } else {
                goto SYNTAX_ERROR;
            }
            break;
        case T_NL:
            break;
        case T_EOF:
            return TRUE;
        case T_ERR:
            goto SYNTAX_ERROR;
        }
        if (tk.v.type == T_NL) {
            fs->Tok_simple_next(&tk);
        } else if (tk.v.type != T_EOF) {
            goto SYNTAX_ERROR;
        }
    }
SYNTAX_ERROR:
    fs->throw_errorf(fs->mod_lang, "InternalError", "Syntax error at line %d (%r)", tk.v.line, type);
    return FALSE;
}
int load_hilight(Hilight *h, RefStr *type, int *result)
{
    char *buf = read_hilight_file(type);
    if (buf == NULL) {
        *result = FALSE;
        return TRUE;
    }
    fs->Hash_init(&h->state, &h->mem, 16);

    // 0-9A-Za-z_
    memcpy(h->word_char, "\0\0\0\0\0\0\xff\x03\xfe\xff\xff\x87\xfe\xff\xff\x07", 128 / 8);

    *result = load_hilight_sub(h, buf, type);
    free(buf);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

int hilight_code_next(HilightIter *it)
{
    Hilight *h = it->h;
    const char *ptr = it->end;
    it->ptr = it->end;

NEXT:
    if (it->ptr >= it->src_end) {
        return FALSE;
    }

    if (it->next_word != NULL) {
        it->state = it->state_next1;
        it->state_next1 = it->state_next2;
        it->word = it->next_word;
        it->next_word = NULL;

        if (it->word->len > 0) {
            it->end = it->ptr + it->word->len;
            return TRUE;
        }
    }

    while (ptr < it->src_end) {
        Word *w;
        for (w = it->next_state->left[*ptr & (STATE_WORD_SIZE - 1)]; w != NULL; w = w->next) {
            if (ptr + w->len > it->src_end) {
                continue;
            }
            if (w->self->word) {
                // 単語単位で検索
                if (ptr > it->ptr && is_word_char(h, ptr[-1])) {
                    continue;
                }
                if (ptr + w->len < it->src_end && is_word_char(h, ptr[w->len])) {
                    continue;
                }
            }
            if (!streq_n(ptr, w->c, w->len, it->next_state->icase)) {
                continue;
            }
            it->state = it->state_next1;
            it->state_next1 = w->self;
            it->state_next2 = w->right;
            
            it->word = it->next_word;
            it->next_word = w;
            it->next_state = w->right;
            goto BREAK;
        }
        ptr++;
    }

    it->state = it->state_next1;
    it->word = it->next_word;
    it->next_word = NULL;

BREAK:
    if (ptr > it->src_end) {
        ptr = it->src_end;
    }
    it->end = ptr;

    if (it->ptr < it->end) {
        return TRUE;
    } else {
        goto NEXT;
    }
}
