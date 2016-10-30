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

enum {
    OP_NONE,        // EOS
    OP_LET_UNRES,   // 未解決の変数代入(Str*)

    OP_SIZE_1,

    OP_RETURN,      // nullを返す
    OP_RETURN_VAL,  // 現在のstktopの値を戻り値として返す
    OP_SUSPEND,     // 実行を中断(evalを継続する場合)
    OP_YIELD_VAL,   // 現在のstktopの値をyieldとして返す
    OP_INIT_GENR,   // ジェネレーター初期化
    OP_RETURN_THRU, // thisをそのまま返す
    OP_END,         // catch,finallyの終端から復帰
    OP_DUP,         // スタックトップからi個の値をpush
    OP_GET_LOCAL,   // ローカル変数をstk_topにpush(let)
    OP_GET_LOCAL_V, // ローカル変数をstk_topにpush(var)
    OP_SET_LOCAL,   // スタック変数代入(stack index)
    OP_NOT,         // stktopの値がnull,falseならtrue else false

    OP_SIZE_2,

    OP_JMP,         // jmp
    OP_POP_IF_J,    // pop!=null,false then jmp
    OP_POP_IFN_J,   // pop==null,false then jmp
    OP_IF_J_POP,    // !=null,false then jmp else pop
    OP_IF_POP_J,    // !=null,false then pop else jmp
    OP_IFN_POPJ,    // ==null,false then pop,jmp
    OP_IFNULL_J,    // ==null then jmp
    OP_GET_FIELD,   // フィールド取得
    OP_SET_FIELD,   // フィールド設定
    OP_THROW,       // stk_topをthrow
    OP_LITERAL,     // 即値(リテラル)
    OP_FUNC,        // 関数
    OP_CLASS,       // クラス
    OP_MODULE,      // モジュール
    OP_NEW_REF,     // コンストラクタの最初でrefを作成
    OP_NEW_FN,      // 関数オブジェクトを作成
    OP_LOCAL_FN,    // ローカル変数をクロージャの内部にコピー
    OP_RANGE_NEW,   // 0..10
    OP_CALL_ITER,   // iter()があれば呼び出し、無ければ何もしない
    OP_CALL_IN,     // in演算子
    OP_CALL,        // stk_topの関数呼び出し
    OP_CALL_POP,    // stk_topの関数呼び出し、戻り値を捨てる
    OP_EQUAL,       // 型比較とop_cmpの呼び出し(型不一致はTypeError)
    OP_EQUAL2,      // 型比較とop_cmpの呼び出し(型不一致はfalse)
    OP_CMP,         // stktopの値の大小をboolに変換(型不一致はTypeError)
    OP_POP,         // スタックを下げてrefcount--

    OP_SIZE_3,

    OP_LITERAL_P,   // 即値(定数シンボル)
    OP_PUSH_CATCH,  // スタックにcatchハンドラを積む
    OP_GET_PROP,    // プロパティ取得(RefStr*)
    OP_CALL_M,      // メソッド呼び出し(RefStr*)
    OP_CALL_M_POP,  // メソッド呼び出し(RefStr*)、戻り値を捨てる
    OP_CALL_INIT,   // super constructor呼び出し
    OP_CALL_NEXT,   // next()を呼び出し、StopIterationならjmp
    OP_CATCH_JMP,   // catch節の型を比較し、違う場合ジャンプ
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
