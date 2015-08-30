#ifndef _FOX_PARSE_H_
#define _FOX_PARSE_H_

#include "fox_vm.h"

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

    OP_JMP,         // 無条件ジャンプ
    OP_JIF,         // pop ならジャンプ
    OP_JIF_N,       // !pop ならジャンプ
    OP_IF_P,        // !=null ならジャンプ else pop
    OP_IF_NP,       // ==null ならジャンプ else pop
    OP_IFP,         // !=null ならpop,jmp
    OP_IFP_N,       // ==null ならpop,jmp
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
    OP_GET_PROP,    // プロパティ取得(Str*)
    OP_CALL_M,      // メソッド呼び出し(Str*)
    OP_CALL_M_POP,  // メソッド呼び出し(Str*)、戻り値を捨てる
    OP_CALL_INIT,   // super constructor呼び出し
    OP_CALL_NEXT,   // next()を呼び出し、StopIterationならjmp
    OP_CATCH_JMP,   // catch節の型を比較し、違った場合ジャンプ
};

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

typedef struct TokValue
{
    uint16_t type;
    uint16_t base;   // 数値の場合、基数
    int line;       // トークンのある行(複数行にまたがる場合は先頭の行)
    char *p;        // 文字列・数値・識別子などの場合、内容の先頭、正規表現の場合、内容のc strのポインタ
    union {
        char *end;  // 文字列・数値・識別子などの場合、内容の末尾
        int flag;   // 正規表現の場合、オプション
    } u;
} TokValue;

typedef struct Tok
{
    char *top;
    char *p;
    int prev_id;

    TokValue v;
    TokValue fetch[PREFETCH_MAX];  // 先読みトークンを保存する
    int head_line;         // 現在のポインタの位置の行
    int prev_n, prev_top;  // リングバッファ

    int32_t int_val;
    double real_val;
    Str str_val;
    BigInt bi_val[2];   // intの場合は0のみ使う、rationalの場合は0,1を使う

    int simple_mode;   // 設定ファイル等の解析で使用する
    RefNode *module;
    int mode_pragma;   // modeプラグマが出現済み
    int ignore_nl;     // ()内の改行を無視

    int wait_colon;    // ?:演算子の:を待っている状態
    int p_nest_n;      // リテラル内の{}のネストレベル
    int *p_nest;       // リテラル内の{}

    RefNode *parse_cls;// クラス解析中
    int parse_genr;    // ジェネレータ解析中
    Hash m_var;        // メンバ変数
    RefNode *arg_type; // 引数の型を覚えておく
    int arg_type_max;

    Mem *st_mem;
    Mem *cmp_mem;
#ifdef DEBUGGER
    int is_test;
#endif
} Tok;

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
