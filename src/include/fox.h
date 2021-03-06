#ifndef FOX_H_INCLUDED
#define FOX_H_INCLUDED

#include "compat.h"
#include <stdint.h>
#include <ctype.h>

#ifndef DEFINE_NO_BOOL
enum {
    FALSE,
    TRUE,
};
#endif

#define lengthof(a) (sizeof(a) / sizeof(a[0]))

enum {
    BUFFER_SIZE = 4096,
    MAGIC_MAX = 128,
    DEFAULT_PERMISSION = 0644,
    DEFAULT_PERMISSION_DIR = 0755,
};
enum {
    NODE_UNRESOLVED = 0x00,

    NODE_MODULE     = 0x01,

    NODE_CLASS      = 0x02,
    NODE_CLASS_U    = 0x03, // クラス(継承元未解決)
    NODE_CLASS_P    = 0x04, // クラス(継承元解決中)

    NODE_CONST      = 0x25, // 定数(計算済み)
    NODE_CONST_U    = 0x66, // 定数(未計算)
    NODE_CONST_U_N  = 0xA7, // 定数(未計算)・ネイティブ

    NODE_VAR        = 0x18, // フィールド
    NODE_FUNC       = 0x59, // 関数
    NODE_FUNC_N     = 0x9A, // ネイティブ関数

    NODE_NEW        = 0x6B, // コンストラクタ
    NODE_NEW_N      = 0xAC, // ネイティブコンストラクタ
};
enum {
    NODEMASK_MEMBER   = 0x10,  // クラスインスタンスに付属するメンバ
    NODEMASK_MEMBER_S = 0x20,  // クラスに静的に付属するメンバ
    NODEMASK_FUNC     = 0x40,  // 関数
    NODEMASK_FUNC_N   = 0x80,  // ネイティブ関数
};
enum {
    NODEOPT_ABSTRACT   = 0x01, // 継承元になりうるクラス
    NODEOPT_VIRTUAL    = 0x02,
    NODEOPT_PROPERTY   = 0x04,
    NODEOPT_STRCLASS   = 0x10, // 文字列互換クラス
    NODEOPT_INTEGRAL   = 0x20, // 整数(int32)互換クラス
    NODEOPT_READONLY   = 0x40, // ローカル変数、ネイティブクラスのメンバ
};
enum {
    ADD_BACKSLASH_UCS2,
    ADD_BACKSLASH_U_UCS2,
    ADD_BACKSLASH_UCS4,
    ADD_BACKSLASH_U_UCS4,
};
enum {
    THROW_NOT_DEFINED__REFSTR,
    THROW_MAX_ALLOC_OVER__INT,
    THROW_ARGMENT_TYPE__NODE_NODE_INT,
    THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT,
    THROW_RETURN_TYPE__NODE_NODE,
    THROW_INVALID_UTF8,
    THROW_LOOP_REFERENCE,
    THROW_CANNOT_OPEN_FILE__STR,
    THROW_NOT_OPENED_FOR_READ,
    THROW_NOT_OPENED_FOR_WRITE,
    THROW_FLOAT_DOMAIN_ERROR,
    THROW_NO_MEMBER_EXISTS__NODE_REFSTR,
    THROW_CANNOT_MODIFY_ON_ITERATION,
    THROW_INVALID_INDEX__VAL_INT,
};

enum {
    T_EOF,
    T_ERR,
    T_NL,

    T_LP,     // (   // op_call
    T_LB,     // [   // op_index
    T_LET_B,  // ]=  // op_index=

    T_EQ,     // ==
    T_NEQ,    // !=
    T_CMP,    // <=>
    T_LT,     // <
    T_LE,     // <=
    T_GT,     // >
    T_GE,     // >=
    T_COLON,  // :
    T_QUEST,  // ?

    T_ADD,    // +
    T_SUB,    // -
    T_MUL,    // *
    T_DIV,    // /
    T_MOD,    // %
    T_LSH,    // <<
    T_RSH,    // >>
    T_AND,    // &
    T_OR,     // |
    T_XOR,    // ^
    T_LAND,   // &&
    T_LOR,    // ||
    T_DOT2,   // ..
    T_DOT3,   // ...
    T_ARROW,  // =>

    T_INV,    // ~
    T_NOT,    // !
    T_PLUS,   // +
    T_MINUS,  // -

    T_INC,    // ++
    T_DEC,    // --
    T_IN,     // in

    T_INDEX_NUM,
};

enum {
    SURROGATE_U_BEGIN   = 0xD800,
    SURROGATE_L_BEGIN   = 0xDC00,
    SURROGATE_END       = 0xE000,
    CODEPOINT_END       = 0x110000,
};
enum {
	BIGINT_DIGIT_BITS = 16,
    BIGINT_MASK_BITS = 0xFFFF,
};
enum {
    RUNNING_MODE_CL,
    RUNNING_MODE_CGI,
    RUNNING_MODE_GUI,
};

////////////////////////////////////////////////////////////////////////////////

// class Regex
enum {
    INDEX_REGEX_PTR,
    INDEX_REGEX_SRC,
    INDEX_REGEX_FLAGS,
    INDEX_REGEX_NUM,
};

// class MarshalDump
enum {
    INDEX_MARSHALDUMPER_SRC,
    INDEX_MARSHALDUMPER_CYCLREF,
    INDEX_MARSHALDUMPER_NUM,
};

// class Function
enum {
    INDEX_FUNC_FN,
    INDEX_FUNC_THIS,
    INDEX_FUNC_N_LOCAL,
    INDEX_FUNC_LOCAL,
};

////////////////////////////////////////////////////////////////////////////////

typedef struct MemChunk MemChunk;

typedef struct {
    MemChunk *p;
    int max, pos;
} Mem;

typedef struct {
    const char *p;
    int32_t size;
} Str;

typedef struct {
    char *p;
    int32_t size;
    int32_t alloc_size;
} StrBuf;

typedef struct {
    int8_t sign;
    int32_t size;
    int32_t alloc_size;
    uint16_t *d;
} BigInt;

////////////////////////////////////////////////////////////////////////////////

typedef struct HashValueEntry HashValueEntry;
typedef struct Ref Ref;
typedef struct RefNode RefNode;

typedef struct OpCode OpCode;

typedef struct UnresolvedID UnresolvedID;
typedef struct UnresolvedMemb UnresolvedMemb;

typedef struct FoxStatic FoxStatic;
typedef struct FoxGlobal FoxGlobal;

typedef struct RefTimeZone RefTimeZone;
typedef struct TimeOffset TimeOffset;

typedef uint64_t Value;
typedef struct Tok Tok;

typedef struct FCharset FCharset;

typedef int (*NativeFunc)(Value *vret, Value *v, RefNode *node);


typedef struct {
    int32_t year;
    int32_t month;
    int32_t day_of_year;
    int32_t day_of_month;
    int32_t day_of_week;

    int32_t isoweek_year;
    int32_t isoweek;
} Date;

typedef struct {
    int32_t hour;
    int32_t minute;
    int32_t second;
    int32_t millisec;
} Time;


// 16bytes (ILP32)
// 24bytes (LP64)
typedef struct RefHeader {
    int32_t nref;
    int32_t n_memb;
    RefNode *type;
    Ref *weak_ref;
} RefHeader;

struct Ref {
    RefHeader rh;
    Value v[0];
};

typedef struct {
    RefHeader rh;

    int32_t size;
    char c[0];
} RefStr;

typedef struct {
    RefHeader rh;

    Date d;          // TimeZone adjusted
    Time t;
    int64_t ts;      // *Always* UTC
    RefTimeZone *tz;
    TimeOffset *off;
} RefDateTime;

typedef struct RefCharset {
    RefHeader rh;
    struct RefCharset *next;

    RefStr *name;    // 文字コード名
    RefStr *iana;    // IANA登録名
    FCharset *cs;
    int type;
    const char **files;
} RefCharset;

typedef struct {
    RefHeader rh;

    int32_t lock_count;
    int32_t size, alloc_size;
    Value *p;
} RefArray;

typedef struct {
    RefHeader rh;

    int32_t lock_count;
    int32_t count, entry_num;
    HashValueEntry **entry;
} RefMap;

//////////////////////////////////////////////////////////////////////////

typedef struct HashEntry
{
    struct HashEntry *next;
    RefStr *key;
    void *p;
} HashEntry;

typedef struct Hash
{
    HashEntry **entry;
    int32_t entry_num;
    int32_t count;
} Hash;

typedef struct PtrList
{
    struct PtrList *next;
    union {
        void *p;
        char c[1];
    } u;
} PtrList;

/**
 * Module
 * Class
 * Function
 */
struct RefNode {
    RefHeader rh;

    uint8_t type;
    uint8_t opt;
    int32_t defined_line;
    RefStr *name;
    RefNode *defined_module;
    UnresolvedID *uid;

    union {
        struct {     // Class
            Hash h;
            RefNode *super;
            int n_integral;
            int n_offset;
            int n_memb;
            void *ext;
        } c;
        struct {     // Module
            Hash h;
            const char *src_path;
            int loaded;
            Hash import;         // import hogehoge => hoge : hogeが追加される
            PtrList *usng;       // import hogehoge : Module(hogehoge)が追加される
            Hash unresolved;     // key:Str, val:UnresolvedID
            void *ext;
        } m;
        struct {     // Function, const (unresolved)
            int8_t arg_min, arg_max;
            int16_t max_stack;
            RefNode **arg_type;
            RefNode *klass;
            void *vp;
            union {
                OpCode *op;
                NativeFunc fn;
            } u;
        } f;
        struct {     // const (resolved)
            Value val;
        } k;
        struct {     // local var
            int offset;
            int var;  // false:let, true:var
        } v;
    } u;
};

struct HashValueEntry {
    HashValueEntry *next;
    uint32_t hash;
    Value key;
    Value val;
};

// 外部モジュールに公開(定数)
struct FoxStatic
{
    int revision;

    RefStr *fox_home;
    int running_mode;
    Hash envs;
    int max_stack;
    int max_alloc;

    PtrList *import_path;    // FOX_IMPORTを分解したもの
    PtrList *resource_path;  // FOX_RESOURCEを分解したもの

    Str Str_EMPTY;

    RefNode *mod_lang;
    RefNode *mod_io;
    RefNode *mod_file;
    RefNode *mod_mime;
    RefNode *mod_locale;
    RefCharset *cs_utf8;
    RefCharset *cs_enumerate;
    RefTimeZone *tz_utc;

    RefStr *symbol_stock[T_INDEX_NUM];
    RefStr *str_0;      // ""
    RefStr *str_new;
    RefStr *str_dtor;   // ~this
    RefStr *str_tostr;
    RefStr *str_hash;
    RefStr *str_iterator;
    RefStr *str_next;
    RefStr *str_read;
    RefStr *str_write;
    RefStr *str_missing;
    RefStr *str_missing_set;
    RefStr *str_method_missing;
    RefStr *str_marshal_read;
    RefStr *str_marshal_write;
    RefStr *str_anonymous;
    RefStr *str_toplevel;

    RefNode *cls_obj;
    RefNode *cls_null;
    RefNode *cls_bool;
    RefNode *cls_number;
    RefNode *cls_int;
    RefNode *cls_frac;
    RefNode *cls_float;
    RefNode *cls_sequence;
    RefNode *cls_str;
    RefNode *cls_bytes;
    RefNode *cls_regex;
    RefNode *cls_char;
    RefNode *cls_range;
    RefNode *cls_list;
    RefNode *cls_map;
    RefNode *cls_set;
    RefNode *cls_mimetype;

    RefNode *cls_class;
    RefNode *cls_module;
    RefNode *cls_fn;
    RefNode *cls_error;
    RefNode *cls_switcherror;
    RefNode *cls_stopiter;
    RefNode *cls_uncaught;
    RefNode *cls_iterable;
    RefNode *cls_iterator;
    RefNode *cls_marshaldumper;

    RefNode *cls_datetime;
    RefNode *cls_timestamp;
    RefNode *cls_timezone;
    RefNode *cls_file;
    RefNode *cls_uri;
    RefNode *cls_streamio;
    RefNode *cls_bytesio;
    RefNode *cls_textio;
    RefNode *cls_utf8io;

    RefNode *cls_locale;
    RefNode *cls_resource;
    RefNode *cls_charset;

    void (*StrBuf_init)(StrBuf *s, int size);
    void (*StrBuf_init_refstr)(StrBuf *s, int size);
    Value (*StrBuf_str_Value)(StrBuf *s, RefNode *type);
    int (*StrBuf_add)(StrBuf *s, const char *p, int size);
    int (*StrBuf_add_c)(StrBuf *s, char c);
    int (*StrBuf_add_r)(StrBuf *s, RefStr *r);
    int (*StrBuf_alloc)(StrBuf *s, int size);
    int (*StrBuf_printf)(StrBuf *s, const char *fmt, ...);
    RefStr *(*intern)(const char *p, int size);
    RefStr *(*get_intern)(const char *p, int size);
    void (*Mem_init)(Mem *mem, int max);
    void *(*Mem_get)(Mem *mem, int size);
    void (*Mem_close)(Mem *mem);
    void (*Hash_init)(Hash *hash, Mem *mem, int init_size);
    void (*Hash_add_p)(Hash *hash, Mem *mem, RefStr *key, void *ptr);
    void *(*Hash_get)(const Hash *hash, const char *key_p, int key_size);
    void *(*Hash_get_p)(const Hash *hash, RefStr *key);
    HashEntry *(*Hash_get_add_entry)(Hash *hash, Mem *mem, RefStr *key);

    char *(*str_dup_p)(const char *p, int size, Mem *mem);
    char *(*str_printf)(const char *fmt, ...);
    void (*add_backslashes_sub)(StrBuf *buf, const char *src_p, int src_size, int mode, int quotes);
    int32_t (*parse_hex)(const char **pp, const char *end, int n);

    RefNode *(*Value_type)(Value v);
    int32_t (*Value_int32)(Value v);
    int64_t (*Value_int64)(Value v, int *err);
    double (*Value_float)(Value v);
    int64_t (*Value_timestamp)(Value v, RefTimeZone *tz);

    Value (*Value_cp)(Value v);
    Value (*int64_Value)(int64_t i);
    Value (*float_Value)(RefNode *klass, double dval);
    Value (*cstr_Value)(RefNode *klass, const char *p, int size);
    Value (*time_Value)(int64_t ts, RefTimeZone *tz);
#ifdef WIN32
    Value (*wstr_Value)(RefNode *klass, const wchar_t *p, int size);
#endif

    Value (*printf_Value)(const char *fmt, ...);

    Ref *(*ref_new)(RefNode *klass);
    void *(*buf_new)(RefNode *klass, int size);
    RefStr *(*refstr_new_n)(RefNode *klass, int size);

    void (*addref)(Value v);
    void (*unref)(Value v);
    void (*Value_push)(const char *fmt, ...);
    void (*Value_pop)(void);
    RefNode *(*get_node_member)(RefNode *node, ...);

    void (*BigInt_init)(BigInt *bi);
    void (*BigInt_close)(BigInt *bi);
    void (*BigInt_copy)(BigInt *dst, const BigInt *src);
    void (*int64_BigInt)(BigInt *bi, int64_t value);
    int (*cstr_BigInt)(BigInt *bi, int radix, const char *str, int size);
    int (*double_BigRat)(BigInt *rat, double d);
    int (*cstr_BigRat)(BigInt *rat, const char *src, const char **end);
    void (*BigRat_fix)(BigInt *rat);
    int32_t (*BigInt_int32)(const BigInt *bi);
    int (*BigInt_int64)(const BigInt *bi, int64_t *value);
    double (*BigInt_double)(const BigInt *bi);
    int (*BigInt_str_bufsize)(const BigInt *bi, int radix);
    int (*BigInt_str)(const BigInt *bi, int radix, char *str, int upper);
    char *(*BigRat_str)(BigInt *rat, int max_frac);
    int (*BigInt_add)(BigInt *a, const BigInt *b);
    int (*BigInt_add_d)(BigInt *a, int b);
    int (*BigInt_sub)(BigInt *a, const BigInt *b);
    int (*BigInt_mul)(BigInt *ret, const BigInt *a, const BigInt *b);
    int (*BigInt_divmod)(BigInt *ret, BigInt *mod, const BigInt *a, const BigInt *b);
    int (*BigInt_pow)(BigInt *a, uint32_t n);
    int (*BigInt_lsh)(BigInt *bi, uint32_t sh);
    void (*BigInt_rsh)(BigInt *bi, uint32_t sh);
    int (*BigInt_gcd)(BigInt *ret, const BigInt *a, const BigInt *b);
    int (*BigInt_cmp)(const BigInt *a, const BigInt *b);

    RefTimeZone *(*get_local_tz)(void);
    void (*adjust_timezone)(RefDateTime *dt);
    void (*adjust_datetime)(RefDateTime *dt);
    int64_t (*DateTime_to_Timestamp)(const Date *dt, const Time *tm);
    int (*timedelta_parse_string)(double *ret, const char *src_p, int src_size);

    int (*is_subclass)(RefNode *klass, RefNode *super);
    RefNode *(*get_module_by_name)(const char *name_p, int name_size, int syslib, int initialize);
    char *(*resource_to_path)(Str name_s, const char *ext_p);
    RefNode *(*define_identifier)(RefNode *module, RefNode *klass, const char *name, int type, int opt);
    RefNode *(*define_identifier_p)(RefNode *module, RefNode *klass, RefStr *pn, int type, int opt);
    void (*define_native_func_a)(RefNode *node, NativeFunc func, int argc_min, int argc_max, void *vp, ...);
    void (*extends_method)(RefNode *klass, RefNode *base);
    void (*add_unresolved_module_p)(RefNode *mod_src, const char *name, RefNode *module, RefNode *node);
    void (*add_unresolved_args)(RefNode *cur_mod, RefNode *mod, const char *name, RefNode *func, int argn);
    void (*add_unresolved_ptr)(RefNode *cur_mod, RefNode *mod, const char *name, RefNode **ptr);

    void (*throw_errorf)(RefNode *err_m, const char *err_name, const char *fmt, ...);
    void (*throw_error_select)(int err_type, ...);
    void (*fatal_errorf)(const char *msg, ...);
    void (*throw_stopiter)(void);
    char *(*file_value_to_path)(Str *ret, Value v, int argn);
    int (*value_to_streamio)(Value *stream, Value v, int writemode, int argn, int accept_textio);
    void (*get_random)(void *buf, int len);
    void (*fix_bigint)(Value *v, BigInt *bi);

    int (*call_function)(RefNode *node, int argc);
    int (*call_function_obj)(int argc);
    int (*call_member_func)(RefStr *name, int argc, int raise_exception);
    int (*call_property)(RefStr *name);

    void (*init_stream_ref)(Ref *r, int mode);
    StrBuf *(*bytesio_get_strbuf)(Value v);
    int (*stream_read_data)(Value v, StrBuf *sb, char *p, int *psize, int keep, int read_all);
    int (*stream_read_uint8)(Value r, uint8_t *val);
    int (*stream_read_uint16)(Value r, uint16_t *val);
    int (*stream_read_uint32)(Value r, uint32_t *val);
    int (*stream_write_data)(Value v, const char *p, int size);
    int (*stream_write_uint8)(Value w, uint8_t val);
    int (*stream_write_uint16)(Value w, uint16_t val);
    int (*stream_write_uint32)(Value w, uint32_t val);
    int (*stream_seek_sub)(Value v, int64_t pos);
    int (*stream_flush_sub)(Value v);
    int (*stream_gets_sub)(StrBuf *sb, Value v, int sep);
    int (*stream_gets_sub16)(StrBuf *sb, Value v, int sep_u, int sep_l);
    int (*stream_get_write_memio)(Value v, Value *pmb, int *pmax);

    int (*get_loader_function)(RefNode **fn, const char *type_p, int type_size, const char *prefix, Hash *hash);
    int (*load_aliases_file)(Hash *ret, const char *filename);
    RefStr *(*mimetype_from_name_refstr)(RefStr *name);
    RefStr *(*mimetype_from_suffix)(const char *name_p, int name_size);
    RefStr *(*mimetype_from_magic)(const char *p, int size);
    void (*utf8_next)(const char **pp, const char *end);
    int (*utf8_codepoint_at)(const char *p);
    RefCharset *(*get_charset_from_name)(const char *name_p, int name_size);

    RefArray *(*refarray_new)(int size);
    Value *(*refarray_push)(RefArray *r);
    int (*refarray_set_size)(RefArray *ra, int size);

    RefMap *(*refmap_new)(int size);
    HashValueEntry *(*refmap_add)(RefMap *rm, Value key, int overwrite, int raise_error);
    int (*refmap_get)(HashValueEntry **ret, RefMap *rm, Value key);
    HashValueEntry *(*refmap_get_strkey)(RefMap *rm, const char *key_p, int key_size);
    int (*refmap_del)(Value *val, RefMap *rm, Value key);

    char *(*read_from_file)(int *psize, const char *path, Mem *mem);

    void (*Tok_simple_init)(Tok *tk, char *buf);
    void (*Tok_simple_next)(Tok *tk);
};

// 外部モジュールに公開
struct FoxGlobal
{
    Value *stk;
    Value *stk_max;
    Value *stk_base;
    Value *stk_top;

    Value v_cio;
    Value v_utf8_cio;

    Hash mod_root;
    Mem st_mem;   // 永続的
    Value error;
};

////////////////////////////////////////////////////////////////////////////////

#define FOX_INTERFACE_REVISION 2


extern const char *fox_ctype_flags;

// alphabet | '_'
#define isalphau_fox(c)   (fox_ctype_flags[(c) & 0xFF] & 0x01)
// number | '_'
#define isalnumu_fox(c)   (fox_ctype_flags[(c) & 0xFF] & 0x02)

#define isupper_fox(c)    (fox_ctype_flags[(c) & 0xFF] & 0x04)
#define islower_fox(c)    (fox_ctype_flags[(c) & 0xFF] & 0x08)
#define isalpha_fox(c)    (fox_ctype_flags[(c) & 0xFF] & 0x0C)
#define isdigit_fox(c)    (fox_ctype_flags[(c) & 0xFF] & 0x10)
#define isxdigit_fox(c)   (fox_ctype_flags[(c) & 0xFF] & 0x20)

#define isspace_fox(c)    (fox_ctype_flags[(c) & 0xFF] & 0x40)
#define islexspace_fox(c) (fox_ctype_flags[(c) & 0xFF] & 0x80)

#define tolower_fox(c)    (isupper_fox(c) ? (c) | ('A' ^ 'a') : (c))

#define VALUE_NULL    ((Value)0ULL)
#define VALUE_FALSE   ((Value)0x0000000000000001ULL)
#define VALUE_TRUE    ((Value)0x0000000100000001ULL)
#define VALUE_INVALID ((Value)0xFFFFFFFFFFFFFFFDULL)

#define Value_isref(v)       ((v) != 0ULL && ((v) & 3ULL) == 0ULL)
#define Value_isintegral(v)  (((v) & 3ULL) == 1ULL)
#define Value_isint(v)       (((v) & 0xFFFFFFFFULL) == 5ULL)
#define Value_isptr(v)       (((v) & 3ULL) == 2ULL)

#define Value_vp(v)          ((void*)(uintptr_t)((v) | 0))
#define Value_ptr(v)         ((void*)((uintptr_t)((v) & ~3)))
#define Value_bool(v)        ((v) != VALUE_NULL && (v) != VALUE_FALSE)
#define Value_integral(v)    ((int32_t)((v) >> 32))
#define Value_uint62(v)      ((v) >> 2)
#define Value_ref_header(v)  ((RefHeader*)Value_vp(v))
#define Value_ref(v)         ((Ref*)Value_vp(v))
#define Value_cstr(v)        (((RefStr*)Value_vp(v))->c)

#define vp_Value(ptr)        ((uint64_t)(uintptr_t)(ptr))
#define ptr_Value(ptr)       (((uint64_t)(uintptr_t)(ptr)) | 2ULL)
#define bool_Value(b)        ((b) ? VALUE_TRUE : VALUE_FALSE)
#define int32_Value(val)     ((((uint64_t)(val)) << 32) | 5ULL)
#define uint62_Value(val)    (((val) << 2) | 2ULL)
#define integral_Value(k, i) (((uint64_t)(i) << 32) | (((uint64_t)(k)->u.c.n_integral) << 2) | 1ULL)

#define StrBuf_close(s) (free((s)->p))
#define FUNC_VP(node) ((node)->u.f.vp)
#define FUNC_INT(node) ((int)(intptr_t)((node)->u.f.vp))

#define UTF8_ALTER_CHAR "\xEF\xBF\xBD"
#define UTF8_ALTER_CHAR_LEN 3


// strutil.c
Str Str_new(const char *p, int len);
int refstr_eq(RefStr *r1, RefStr *r2);
int refstr_cmp(RefStr *r1, RefStr *r2);
int str_eq(const char *s_p, int s_size, const char *t_p, int t_size);
int str_eqi(const char *s_p, int s_size, const char *t_p, int t_size);
int str_has0(const char *p, int size);

uint16_t ptr_read_uint16(const char *p);
uint32_t ptr_read_uint32(const char *p);
void ptr_write_uint16(char *p, uint32_t val);
void ptr_write_uint32(char *p, uint32_t val);

#endif /* FOX_H_INCLUDED */
