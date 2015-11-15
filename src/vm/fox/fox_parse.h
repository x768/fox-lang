#ifndef _FOX_PARSE_H_
#define _FOX_PARSE_H_

#include "fox_vm.h"
#include "m_token.h"

enum {
    UNRESOLVED_IDENT,
    UNRESOLVED_ARGUMENT,
    UNRESOLVED_POINTER,
    UNRESOLVED_EXTENDS,
    UNRESOLVED_CATCH,
};

#ifdef DEBUGGER

enum {
    T_WANTS_NEXT = TT_YIELD + 1,
};

#endif

/////////////////////////////////////////////////////////////////////////////////////

struct OpCode
{
    int32_t type;
    int32_t s;
    Value op[0];
};

typedef struct Unresolved
{
    struct Unresolved *next;

    int type;
    union {
        RefNode *node;   // 関数またはクラス
        RefNode **pnode; // UNRESOLVED_POINTERで使用
    } u;
    int pc;      // 関数のpcまたは引数の位置(0 origin)
} Unresolved;

struct UnresolvedID
{
    Unresolved *ur;

    RefNode *ref_module;
    int ref_line;
};
typedef struct UnrslvMemb
{
    RefStr *memb;
    RefNode *fn;
    int line;
    int pc;
} UnrslvMemb;

struct UnresolvedMemb
{
    struct UnresolvedMemb *next;
    UnrslvMemb rslv[MAX_UNRSLV_NUM];
};

typedef struct Block
{
    struct Block *parent;
    int offset;
    Hash h;          // ローカル変数
    RefNode *func;
    Hash *import;
    int break_p;     // breakでのジャンプ先(後で更新する)
    int continue_p;  // continueでのジャンプ先
} Block;

///////////////////////////////////////////////////////////////////////////////////////////

#define VALUE_ISCONST_U(v)  ((v) != 0ULL && ((v) & 3ULL) == 3ULL)
#define VALUE_ISCATCH_S(v)  ((v) != 0ULL && ((v) & 3ULL) == 3ULL)

// lex.c
void Tok_init(Tok *tk, RefNode *module, char *buf);
void Tok_close(Tok *tk);
void Tok_next(Tok *tk);
void Tok_next_skip(Tok *tk);
void Tok_skip(Tok *tk);
int Tok_peek(Tok *tk, int n);
RefStr *Tok_intern(Tok *tk);
#ifdef DEBUGGER
void Tok_init_continue(Tok *tk, char *buf);
int Tok_next_test(Tok *tk);
int set_module_eval(RefNode *module, char *src_p, RefNode *func, Block *block);
#endif

void Tok_simple_init(Tok *tk, char *buf);
void Tok_simple_next(Tok *tk);

// parse.c
Block *Block_new(Block *parent, RefNode *func);
#ifdef DEBUGGER
RefNode *init_toplevel(RefNode *module);
#endif


// throw.c
void throw_error_vprintf(RefNode *err_m, const char *err_name, const char *fmt, va_list va);



#endif /* _FOX_PARSE_H_ */
