#ifndef M_TEXT_UTIL_H_INCLUDED
#define M_TEXT_UTIL_H_INCLUDED

#include "fox.h"
#include "compat.h"


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
    RefHeader rh;

    Mem mem;
    Value mimetype;

    Hash state; // Hash<State>
    uint8_t word_char[128 / 8];
} Hilight;

typedef struct {
    RefHeader rh;

    Value src_val;
    Value h_val;

    Hilight *h;
    const char *src_end;
    const char *ptr;
    const char *end;

    State *state;
    State *state_next1;
    State *state_next2;
    State *next_state;

    Word *word;
    Word *next_word;
} HilightIter;



#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_util;

#ifdef DEFINE_GLOBALS
#undef extern
#endif

int load_hilight(Hilight *h, RefStr *type, int *result);
int hilight_code_next(HilightIter *it);


#endif /* M_TEXT_UTIL_H_INCLUDED */
