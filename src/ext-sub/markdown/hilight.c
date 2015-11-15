#include "markdown.h"


typedef struct {
    RefStr *self;
    RefStr *after;
    int32_t len;
    char s[0];
} Word;

typedef struct {
    RefStr *name;
    int8_t is_word;
    int8_t ignore_case;
} State;

typedef struct {
    Word *before;
    State *state;
} Hilight;


static Hilight *load_hilight(const char *type)
{
    return NULL;
}

int parse_markdown_code_block(Markdown *r, MDTok *tk, MDNode **ppnode, const char *type)
{
    MDNode *node;
    Hilight *hilight = NULL;

    if (type != NULL) {
        Hilight *h = fs->Hash_get(&r->hilight, type, -1);
        if (h == NULL) {
            h = load_hilight(type);
            if (h != NULL) {
                fs->Hash_add_p(&r->hilight, &r->mem, fs->intern(type, -1), h);
            }
        }
        hilight = h;
    }

    if (hilight != NULL) {
    } else {
        node = MDNode_new(MD_TEXT, r);
        node->cstr = fs->str_dup_p(tk->val.p, tk->val.size, &r->mem);
        *ppnode = node;
    }

    return TRUE;
}
