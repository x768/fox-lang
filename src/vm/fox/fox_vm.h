#ifndef FOX_VM_H_INCLUDED
#define FOX_VM_H_INCLUDED

#include "fox_io.h"
#include "m_number.h"
#include "m_codecvt.h"
#include "compat_vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


enum {
    MAX_EXPR_STACK = 1024,
    MAX_CALLFUNC_NUM = 1024,
    MAX_STACKTRACE_NUM = 64,

    MAX_UNRSLV_NUM = 256,

    CURLY_MASK   = 0xF00,
    CURLY_NORMAL = 0x100,
    CURLY_STRING = 0x200,
    CURLY_IDENT  = 0x400,
};
enum {
    ENVSET_IMPORT,
    ENVSET_RESOURCE,
    ENVSET_TZ,
    ENVSET_LANG,
    ENVSET_ERROR,
    ENVSET_MAX_ALLOC,
    ENVSET_MAX_STACK,
    ENVSET_NUM,
};
enum {
    ITERATOR_KEY,
    ITERATOR_VAL,
    ITERATOR_BOTH,
};

// class Generator
enum {
    INDEX_GENERATOR_PC,
    INDEX_GENERATOR_NSTACK,
    INDEX_GENERATOR_FUNC,
    INDEX_GENERATOR_LOCAL,
};
// class MimeData
enum {
    INDEX_MIMEDATA_HEADER = INDEX_STREAM_NUM,
    INDEX_MIMEDATA_BUF,
    INDEX_MIMEDATA_NUM,
};

// class Locale
enum {
    INDEX_LOCALE_LOCALE,
    INDEX_LOCALE_LANGTAG,
    INDEX_LOCALE_LANGUAGE,
    INDEX_LOCALE_EXTENDED,
    INDEX_LOCALE_SCRIPT,
    INDEX_LOCALE_TERRITORY,
    INDEX_LOCALE_VARIANT,
    INDEX_LOCALE_EXTENTIONS,
    INDEX_LOCALE_NUM,
};

// class Entry
enum {
    INDEX_ENTRY_KEY,
    INDEX_ENTRY_VAL,
    INDEX_ENTRY_NUM,
};

enum {
    LOCALE_LANGUAGE,
    LOCALE_SCRIPT,
    LOCALE_TERRITORY,
    LOCALE_VALIANT,
    LOCALE_NUM,
};

enum {
    VALUE_CMP_LT,
    VALUE_CMP_EQ,
    VALUE_CMP_GT,
    VALUE_CMP_ERROR,
};

enum {
    ROUND_ERR,
    ROUND_DOWN,
    ROUND_UP,
    ROUND_FLOOR,
    ROUND_CEILING,
    ROUND_HALF_DOWN,
    ROUND_HALF_UP,
    ROUND_HALF_EVEN,
};

/////////////////////////////////////////////////////////////////////////////////////

typedef void (*FoxModuleDefine)(RefNode *m, const FoxStatic *fs, FoxGlobal *fg);
typedef const char *(*FoxModuleVersion)(const FoxStatic *fs);


typedef struct {
    Mem mem;

    RefStr *langtag;
    const char *month[13];
    const char *month_w[13];
    const char *week[7];
    const char *week_w[7];
    const char *am_pm[2];
    const char *date[4];
    const char *time[4];

    int group_n;
    const char *group;
    const char *decimal;
    char bidi;   // 'L':ltr, 'R':rtl
} LocaleData;

typedef struct
{
    int argc;
    const char **argv;
    RefStr *cur_dir;

    const char *path_info;   // CGI modeのみ
    int headers_sent;        // CGI modeのみ

    RefNode *cls_fileio;
    RefNode *cls_strio;
    RefNode *cls_generator;
    RefNode *cls_mimeheader;
    RefNode *cls_mimedata;
    RefNode *cls_entry;
    RefNode *cls_nullio;

    RefNode *func_strcat;
    RefNode *func_array_new;
    RefNode *func_map_new;
    RefNode *func_range_new;

    RefNode *refnode_sequence_hash;
    LocaleData *loc_neutral;

    uint32_t hash_seed;
    int heap_count;
    int n_callfunc;

    RefNode **integral;  // 整数型互換クラス
    int integral_num;
    int integral_max;

    RefNode *startup;
    const char *err_dst;

    PtrList *const_refs;

#ifdef WIN32
    int console_read;  // 標準入力がコンソール
    int console_write; // 標準出力がコンソール
    int console_error; // 標準エラー出力がコンソール
#endif

    UnresolvedMemb *unresolved_memb;  // Class.memb参照解決(コンパイル時に使われる)
    UnresolvedMemb *unresolved_memb_root;
    int unresolved_memb_n;

    Mem cmp_mem;     // コンパイル時のみ
    int cmp_dynamic;
} FoxVM;

/////////////////////////////////////////////////////////////////////////////////////

#define FOX_VERSION_MAJOR    0
#define FOX_VERSION_MINOR    23
#define FOX_VERSION_REVISION 0

#define int32_hash(v) (((v) * 31) & INT32_MAX)

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern FoxStatic *fs;
extern FoxGlobal *fg;
extern FoxVM *fv;
extern CodeCVTStatic *codecvt;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


// col.c
void Hash_init(Hash *hash, Mem *mem, int init_size);
void Hash_add_p(Hash *hash, Mem *mem, RefStr *key, void *ptr);
void *Hash_get(const Hash *hash, const char *key_p, int key_size);
void *Hash_get_p(const Hash *hash, RefStr *key);
HashEntry *Hash_get_add_entry(Hash *hash, Mem *mem, RefStr *key);

void g_intern_init(void);
RefStr *intern(const char *p, int size);
RefStr *get_intern(const char *p, int size);

PtrList *PtrList_add(PtrList **pp, int size, Mem *mem);
void PtrList_add_p(PtrList **pp, void *p, Mem *mem);
PtrList *PtrList_push(PtrList **pp, int size, Mem *mem);
void PtrList_close(PtrList *lst);


// util.c
int align_pow2(int sz, int min);
int char2hex(char ch);
int write_p(int fd, const char *str);
int parse_memory_size(Str s);

void Mem_init(Mem *mem, int chunk_size);
void *Mem_get(Mem *mem, int size);
void Mem_close(Mem *mem);

int Str_split(Str src, const char **ret, int n, char c);
Str Str_trim(Str src);

void StrBuf_init(StrBuf *s, int size);
void StrBuf_init_refstr(StrBuf *s, int size);
Value StrBuf_str_Value(StrBuf *s, RefNode *type);
int StrBuf_alloc(StrBuf *s, int size);
int StrBuf_add(StrBuf *s, const char *p, int size);
int StrBuf_add_c(StrBuf *s, char c);
int StrBuf_add_r(StrBuf *s, RefStr *r);
int StrBuf_vprintf(StrBuf *s, const char *fmt, va_list va);
int StrBuf_printf(StrBuf *s, const char *fmt, ...);
int StrBuf_add_v(StrBuf *s, Value v);

char *read_from_file(int *psize, const char *path, Mem *mem);


// util_str.c
int32_t parse_hex(const char **pp, const char *end, int n);
int32_t parse_int(const char *src_p, int src_size, int max);
int str_hash(const char *p, int size);
char *str_dup_p(const char *p, int size, Mem *mem);
char *str_printf(const char *fmt, ...);
char hex2lchar(int i);
char hex2uchar(int i);
int encode_b64_sub(char *dst, const char *p, int size, int url);
int decode_b64_sub(char *dst, const char *p, int size, int strict);
void add_backslashes_sub(StrBuf *buf, const char *src_p, int src_size, int mode, int quotes);
void add_backslashes_bin(StrBuf *buf, const char *src_p, int src_size);
int escape_backslashes_sub(char *dst, const char *src, int size, int bin);
void urlencode_sub(StrBuf *buf, const char *p, int size, int plus);
void urldecode_sub(StrBuf *buf, const char *p, int size);
void encode_hex_sub(char *dst, const char *src, int len, int upper);
void convert_html_entity(StrBuf *buf, const char *p, int size, int br);
int str_add_codepoint(char **pdst, int ch, const char *error_type);
int is_string_only_ascii(const char *p, int size, const char *except);


// file.c
Str base_dir_with_sep(const char *path_p, int path_size);
Str file_name_from_path(const char *path_p, int path_size);
int make_path_regularize(char *path, int size);
char *path_normalize(Str *ret, RefStr *base_dir, const char *path_p, int path_size, Mem *mem);
Str get_name_part_from_path(Str path);


// value.c
RefNode *Value_type(Value v);
int32_t Value_int32(Value v);
int64_t Value_int64(Value v, int *err);
double Value_float(Value v);
char *Value_frac_s(Value v, int max_frac);

Value int64_Value(int64_t i);
Value float_Value(RefNode *klass, double dval);
Value cstr_Value(RefNode *klass, const char *p, int size);
Value frac_s_Value(const char *str);
#ifdef WIN32
Value wstr_Value(RefNode *klass, const wchar_t *wstr, int size);
#endif

Value printf_Value(const char *fmt, ...);

void addref(Value v);
void unref(Value v);
void Value_push(const char *fmt, ...);
void Value_pop(void);

Value Value_cp(Value v);
void Value_set(Value *vp, Value v);

Ref *ref_new(RefNode *klass);
Ref *ref_new_n(RefNode *klass, int n);
void *buf_new(RefNode *klass, int size);
RefStr *refstr_new_n(RefNode *klass, int size);
RefNode *Module_new_sys(const char *name_p);

int Value_hash(Value v, int32_t *phash);
int Value_eq(Value v1, Value v2, int *ret);

int is_subclass(RefNode *klass, RefNode *super);
RefNode *get_node_member(RefNode *node, ...);
int get_loader_function(RefNode **fn, const char *type_p, int type_size, const char *prefix, Hash *hash);
int load_aliases_file(Hash *ret, const char *filename);


// vm.c
void extends_method(RefNode *klass, RefNode *base);
void init_fox_vm(int running_mode);
void init_fox_stack(void);
void fox_init_compile(int dynamic);
int fox_link(void);
void fox_error_dump(StrBuf *sb, int log_style);
FileHandle open_errorlog_file();
void fox_close(void);


// parse.c
RefNode *Node_define(RefNode *root, RefNode *module, RefStr *name, int type, RefNode *node);
void init_so_func(void);
void add_unresolved_module_p(RefNode *mod_src, const char *name, RefNode *module, RefNode *node);
void add_unresolved_args(RefNode *cur_mod, RefNode *mod, const char *name, RefNode *func, int argn);
void add_unresolved_ptr(RefNode *cur_mod, RefNode *mod, const char *name, RefNode **ptr);
void load_env_settings(int *defs);
int load_htfox(void);
RefNode *get_module_from_src(const char *src_p, int src_size);
RefNode *get_module_by_name(const char *name_p, int name_size, int syslib, int initialize);
RefNode *get_module_by_file(const char *path_p);


// exec.c
RefNode *search_member(Value v, RefNode *klass, RefStr *name);
void dispose_opcode(RefNode *func);
int fox_run(RefNode *mod);
void Value_release_ref(Value v);
int call_function(RefNode *node, int argc);
int call_function_obj(int argc);
int call_member_func(RefStr *name, int argc, int raise_exception);
int call_property(RefStr *name);
int invoke_code(RefNode *func, int pc);


// throw.c
void fatal_errorf(const char *msg, ...);
void throw_errorf(RefNode *err_m, const char *err_name, const char *fmt, ...);
void throw_error_select(int err_type, ...);
void throw_stopiter(void);


// main.c
void print_foxinfo(void);
void print_last_error(void);
int main_fox(int argc, const char **argv);


//////////////////////////////////////////////////////////////////////////////////////////////////////

// m_cgi.c
void send_headers(void);
void cgi_init_responce(void);
int param_string_init_hash(RefMap *v_map, Value keys);
int is_content_type_html(void);
void init_cgi_module_1(void);


// m_charset.c
void CodeCVTStatic_init(void);
RefCharset *get_charset_from_name(const char *name_p, int name_size);
int convert_str_to_bin_sub(StrBuf *dst_buf, const char *src_p, int src_size, RefCharset *cs, const char *alt_s);
int convert_str_to_bin(Value *dst, StrBuf *dst_buf, int arg);
int convert_bin_to_str_sub(StrBuf *dst_buf, const char *src_p, int src_size, RefCharset *cs, int alt_b);
int convert_bin_to_str(Value *dst, const Str *src_str, int arg);
void define_charset_class(RefNode *m);


// m_col.c
int value_cmp_invoke(Value v1, Value v2, Value fn);
RefArray *refarray_new(int size);
Value *refarray_push(RefArray *r);
int refarray_set_size(RefArray *ra, int size);
void refarray_push_str(RefArray *arr, const char *p, int size, RefNode *type);
int pairvalue_hash(Value *vret, Value *v, RefNode *node);
int pairvalue_eq(Value *vret, Value *v, RefNode *node);

int col_tostr(Value *vret, Value *v, RefNode *node);
int col_hash(Value *vret, Value *v, RefNode *node);
int col_eq(Value *vret, Value *v, RefNode *node);
void define_lang_col_class(RefNode *m);


// m_map.c
RefMap *refmap_new(int size);
HashValueEntry *refmap_add(RefMap *rm, Value key, int overwrite, int raise_error);
int refmap_get(HashValueEntry **ret, RefMap *rm, Value key);
HashValueEntry *refmap_get_strkey(RefMap *rm, const char *key_p, int key_size);

int refmap_del(Value *val, RefMap *rm, Value key);
int map_index(Value *vret, Value *v, RefNode *node);
int map_size(Value *vret, Value *v, RefNode *node);
int map_empty(Value *vret, Value *v, RefNode *node);
int map_dispose(Value *vret, Value *v, RefNode *node);
int map_has_key(Value *vret, Value *v, RefNode *node);
int map_iterator(Value *vret, Value *v, RefNode *node);

void define_lang_map_class(RefNode *m);


// m_file.c
void init_file_module_stubs(void);
void init_file_module_1(void);


// m_io.c
char *file_value_to_path(Str *ret, Value v, int argn);
int value_to_streamio(Value *stream, Value v, int writemode, int argn, int accept_textio);

RefBytesIO *bytesio_new_sub(const char *src, int size);
int bytesio_gets_sub(Str *result, Value v);
StrBuf *bytesio_get_strbuf(Value v);

void init_stream_ref(Ref *r, int mode);

int stream_read_data(Value r, StrBuf *sb, char *p, int *psize, int keep, int read_all);
int stream_read_uint8(Value r, uint8_t *val);
int stream_read_uint16(Value r, uint16_t *val);
int stream_read_uint32(Value r, uint32_t *val);

int stream_write_data(Value w, const char *p, int size);
int stream_write_uint8(Value w, uint8_t val);
int stream_write_uint16(Value w, uint16_t val);
int stream_write_uint32(Value w, uint32_t val);

int stream_flush_sub(Value v);
int stream_gets_sub(StrBuf *sb, Value v, int sep);
int stream_gets_sub16(StrBuf *sb, Value v, int sep_u, int sep_l);
int stream_gets_limit(Value v, char *p, int *psize);
int stream_seek(Value v, int64_t offset);
int stream_get_write_memio(Value v, Value *pmb, int *pmax);

void init_io_module_stubs(void);
void init_io_module_1(void);


// m_io_text.c
void init_io_text_module_1(void);


// m_lang.c
RefNode *define_identifier(RefNode *module, RefNode *klass, const char *name, int type, int opt);
RefNode *define_identifier_p(RefNode *module, RefNode *klass, RefStr *pn, int type, int opt);
void redefine_identifier(RefNode *module, RefNode *klass, const char *name, int type, int opt, RefNode *node);
void define_native_func_a(RefNode *node, NativeFunc func, int argc_min, int argc_max, void *vp, ...);
int native_return_null(Value *vret, Value *v, RefNode *node);
int native_return_bool(Value *vret, Value *v, RefNode *node);
int native_return_this(Value *vret, Value *v, RefNode *node);
int native_get_member(Value *vret, Value *v, RefNode *node);
int native_set_member(Value *vret, Value *v, RefNode *node);
int object_eq(Value *vret, Value *v, RefNode *node);
void stacktrace_to_str(StrBuf *buf, Value v, char sep);
void add_stack_trace(RefNode *module, RefNode *func, int line);
void define_error_class(RefNode *cls, RefNode *base, RefNode *m);
void init_lang_module_stubs(void);
void init_lang_module_1(void);


// m_locale.c
LocaleData *Value_locale_data(Value v);
void set_neutral_locale(void);
char *resource_to_path(Str name_s, const char *ext_p);
RefStr **get_best_locale_list(void);
void init_locale_module_1(void);


// m_marshal.c
RefNode *init_marshal_module_stubs(void);
void init_marshal_module_1(RefNode *m);


// m_mime.c
int parse_header_sub(Str *s_ret, const char *subkey_p, int subkey_size, const char *src_p, int src_size);
int mimerandomreader_sub(RefMap *rm, Value reader, const char *boundary_p, int boundary_size, RefCharset *cs, const char *key_p, int key_size, const char *subkey_p, int subkey_size, int keys);
int mimedata_get_header_sub(int *p_found, Str *s_ret, RefMap *map, const char *key_p, int key_size, const char *subkey_p, int subkey_size);
int mimereader_next_sub(Value *vret, Value v, const char *boundary_p, int boundary_size, RefCharset *cs);
int mimetype_new_sub(Value *v, RefStr *src);
RefStr *mimetype_from_name_refstr(RefStr *name);
RefStr *mimetype_from_suffix(const char *name_p, int name_size);
RefStr *mimetype_from_magic(const char *p, int size);
RefStr *mimetype_get_parent(RefStr *name);
RefStr *resolve_mimetype_alias(RefStr *name);
void init_mime_module_stubs(void);
void init_mime_module_1(void);


// m_number.c
void fix_bigint(Value *v, BigInt *bi);
int float_hash(Value *vret, Value *v, RefNode *node);
int float_eq(Value *vret, Value *v, RefNode *node);
int float_cmp(Value *vret, Value *v, RefNode *node);
int float_marshal_read(Value *vret, Value *v, RefNode *node);
int float_marshal_write(Value *vret, Value *v, RefNode *node);
char *frac_tostr_sub(int sign, BigInt *mi, BigInt *rem, int width_f);
void define_lang_number_func(RefNode *m);
void define_lang_number_class(RefNode *m);

// m_number_str.c
int parse_round_mode(RefStr *rs);
int integer_tostr(Value *vret, Value *v, RefNode *node);
int frac_tostr(Value *vret, Value *v, RefNode *node);
int float_tostr(Value *vret, Value *v, RefNode *node);
int char_tostr(Value *vret, Value *v, RefNode *node);
int base58encode(Value *vret, Value *v, RefNode *node);
int base58decode(Value *vret, Value *v, RefNode *node);

// m_str.c
int string_format_sub(Value *v, Str src, const char *fmt_p, int fmt_size, int utf8);
int sequence_eq(Value *vret, Value *v, RefNode *node);
int sequence_cmp(Value *vret, Value *v, RefNode *node);
int sequence_hash(Value *vret, Value *v, RefNode *node);
int sequence_marshal_read(Value *vret, Value *v, RefNode *node);
int sequence_marshal_write(Value *vret, Value *v, RefNode *node);
void string_substr_position(int *pbegin, int *pend, const char *src_p, int src_size, Value *v);
void calc_splice_position(int *pstart, int *plen, int size, Value *v);
int strclass_tostr(Value *vret, Value *v, RefNode *node);
void define_lang_str_func(RefNode *m);
void define_lang_str_class(RefNode *m);


// m_test.c
void define_test_driver(RefNode *m);


// m_time.c
int64_t Value_timestamp(Value v, RefTimeZone *tz);
Value time_Value(int64_t ts, RefTimeZone *tz);
RefTimeZone *get_local_tz(void);
void adjust_timezone(RefDateTime *dt);
void adjust_datetime(RefDateTime *dt);
int timedelta_parse_string(double *ret, const char *src_p, int src_size);
void timestamp_to_RFC2822_UTC(int64_t ts, char *dst);
void timestamp_to_cookie_date(int64_t ts, char *dst);
RefNode *init_time_module_stubs(void);
void init_time_module_1(RefNode *m);


// m_unicode.c
int invalid_utf8_pos(const char *p, int size);
Str fix_utf8_part(Str src);
void alt_utf8_string(StrBuf *dst, const char *p, int size);
int strlen_utf8(const char *ptr, int len);
void utf8_next(const char **pp, const char *end);
int utf8_position(const char *p, int size, int idx);
int utf8_codepoint_at(const char *p);


#ifdef DEBUGGER
void fox_debugger(RefNode *module);
void fox_intaractive(RefNode *m);
#endif


#endif /* FOX_VM_H_INCLUDED */
