#ifndef _M_TOKEN_H_
#define _M_TOKEN_H_

#include "fox.h"

enum {
    T_RP = T_INDEX_NUM, // )
    T_LC,    // {
    T_RC,    // }
    T_RB,    // ]
    T_LP_C,  // f (
    T_LB_C,  // a [
    T_COMMA,
    T_SEMICL,
    T_MEMB,
    T_LAMBDA,// (a)=>

    TL_PRAGMA,
    TL_INT,
    TL_BIGINT,
    TL_FLOAT,
    TL_FRAC,
    TL_STR,
    TL_BYTES,
    TL_REGEX,
    TL_CLASS,
    TL_VAR,
    TL_CONST,

    TL_STRCAT_BEGIN,
    TL_STRCAT_MID,
    TL_STRCAT_END,
    TL_FORMAT,

    TT_ABSTRACT,
    TT_BREAK,
    TT_CASE,
    TT_CATCH,
    TT_CLASS,
    TT_CONTINUE,
    TT_DEF,
    TT_DEFAULT,
    TT_ELSE,
    TT_ELIF,
    TT_FALSE,
    TT_FOR,
    TT_IF,
    TT_IMPORT,
    TT_IN,
    TT_LET,
    TT_NULL,
    TT_RETURN,
    TT_SUPER,
    TT_SWITCH,
    TT_THIS,
    TT_THROW,
    TT_TRUE,
    TT_TRY,
    TT_VAR,
    TT_WHILE,
    TT_YIELD,
};

enum {
    PREFETCH_MAX = 4, // 高々4の先読みで解析できる
};

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

struct Tok
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
};


#endif /* _M_TOKEN_H_ */
