#include "fox_vm.h"
#include "bigint.h"
#include "m_codecvt.h"
#include <stdio.h>
#include <string.h>


enum {
    TEXTIO_M_GETS,
    TEXTIO_M_GETLN,
    TEXTIO_M_NEXT,
};
enum {
    TEXTIO_S_STREAM,
    TEXTIO_S_STDIO,
    TEXTIO_S_STR,
};

static Value s_std_textio;

//////////////////////////////////////////////////////////////////////////////////////////

static int utf8io_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = ref_new(fs->cls_utf8io);
    *vret = vp_Value(r);

    r->v[INDEX_TEXTIO_STREAM] = Value_cp(v[1]);
    if (v + 2 < fg->stk_top && Value_bool(v[2])) {
        r->v[INDEX_UTF8IO_TRANS] = VALUE_TRUE;
    } else {
        r->v[INDEX_UTF8IO_TRANS] = VALUE_FALSE;
    }
    return TRUE;
}
static int utf8io_gets(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Value stream = ref->v[INDEX_TEXTIO_STREAM];
    StrBuf buf;
    int ret = TRUE;
    int gets_type = FUNC_INT(node);

    StrBuf_init(&buf, 0);
    if (!stream_gets_sub(&buf, stream, '\n')) {
        StrBuf_close(&buf);
        return FALSE;
    }

    if (buf.size > 0) {
        if (gets_type != TEXTIO_M_GETS) {
            // 末尾の改行を取り除く
            if (buf.p[buf.size - 1] == '\n') {
                buf.size--;
                if (buf.size > 0 && buf.p[buf.size - 1] == '\r') {
                    buf.size--;
                }
            }
        }
        if (Value_bool(ref->v[INDEX_UTF8IO_TRANS])) {
            // 不正な文字をU+FFFDに置き換える
            *vret = cstr_Value(NULL, buf.p, buf.size);
        } else {
            if (invalid_utf8_pos(buf.p, buf.size) >= 0) {
                throw_error_select(THROW_INVALID_UTF8);
                ret = FALSE;
                goto FINALLY;
            }
            *vret = cstr_Value(fs->cls_str, buf.p, buf.size);
        }
    } else {
        if (gets_type == TEXTIO_M_NEXT) {
            throw_stopiter();
            ret = FALSE;
        }
    }

FINALLY:
    StrBuf_close(&buf);
    return ret;
}
static int utf8io_write(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Value stream = ref->v[INDEX_TEXTIO_STREAM];
    Value *vp;

    for (vp = v + 1; vp < fg->stk_top; vp++) {
        if (Value_type(*vp) == fs->cls_str) {
            RefStr *rs = Value_vp(*vp);
            if (!stream_write_data(stream, rs->c, rs->size)) {
                return FALSE;
            }
        } else {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, Value_type(*vp), (int)(vp - v));
            return FALSE;
        }
    }
    return TRUE;
}
static int utf8io_translit(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    *vret = bool_Value(ref->v[INDEX_UTF8IO_TRANS]);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

static int strio_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *src;
    Ref *r = ref_new(fv->cls_strio);

    *vret = vp_Value(r);

    if (fg->stk_top > v + 1) {
        src = Value_vp(v[1]);
    } else {
        src = fs->str_0;
    }
    r->v[INDEX_TEXTIO_STREAM] = vp_Value(bytesio_new_sub(src->c, src->size));

    return TRUE;
}

static int strio_size(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefBytesIO *mb = Value_vp(ref->v[INDEX_TEXTIO_STREAM]);
    *vret = int32_Value(strlen_utf8(mb->buf.p, mb->buf.size));
    return TRUE;
}
static int strio_empty(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefBytesIO *mb = Value_vp(ref->v[INDEX_TEXTIO_STREAM]);
    *vret = bool_Value(mb->buf.size == 0);
    return TRUE;
}
static int strio_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefBytesIO *mb = Value_vp(ref->v[INDEX_TEXTIO_STREAM]);

    if (fg->stk_top > v + 1) {
        Str s = Str_new(mb->buf.p, mb->buf.size);
        RefStr *fmt = Value_vp(v[1]);
        if (!string_format_sub(vret, s, fmt->c, fmt->size, FALSE)) {
            return FALSE;
        }
    } else {
        *vret = cstr_Value(fs->cls_str, mb->buf.p, mb->buf.size);
    }

    return TRUE;
}
static int strio_data(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefBytesIO *mb = Value_vp(ref->v[INDEX_TEXTIO_STREAM]);
    int begin, end;

    string_substr_position(&begin, &end, mb->buf.p, mb->buf.size, v);
    *vret = cstr_Value(fs->cls_str, mb->buf.p + begin, end - begin);

    return TRUE;
}
static int strio_dup(Value *vret, Value *v, RefNode *node)
{
    Ref *src = Value_ref(*v);
    RefBytesIO *mb = Value_vp(src->v[INDEX_TEXTIO_STREAM]);

    Ref *dst = ref_new(fv->cls_strio);
    *vret = vp_Value(dst);
    dst->v[INDEX_TEXTIO_STREAM] = vp_Value(bytesio_new_sub(mb->buf.p, mb->buf.size));

    return TRUE;
}
static int strio_index(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefBytesIO *mb = Value_vp(ref->v[INDEX_TEXTIO_STREAM]);
    const char *src_p = mb->buf.p;
    int src_size = mb->buf.size;
    int idx = utf8_position(src_p, src_size, Value_int64(v[1], NULL));

    if (idx >= 0 && idx < src_size) {
        int code = utf8_codepoint_at(&src_p[idx]);
        *vret = int32_Value(code);
    } else {
        throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], src_size);
        return FALSE;
    }

    return TRUE;
}
static int strio_gets(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefBytesIO *mb = Value_vp(ref->v[INDEX_TEXTIO_STREAM]);
    int gets_type = FUNC_INT(node);
    const char *mbuf = mb->buf.p + mb->cur;
    int sz = mb->buf.size - mb->cur;
    int i;

    // sepの次まで読む
    for (i = 0; i < sz; i++) {
        if (mbuf[i] == '\n') {
            i++;
            break;
        }
    }
    if (i > 0) {
        if (gets_type == TEXTIO_M_GETS) {
            *vret = cstr_Value(fs->cls_str, mbuf, i);
        } else {
            // 末尾の改行を取り除く
            if (mbuf[i - 1] == '\n') {
                *vret = cstr_Value(fs->cls_str, mbuf, i - 1);
            } else {
                *vret = cstr_Value(fs->cls_str, mbuf, i);
            }
        }
    } else {
        if (gets_type == TEXTIO_M_NEXT) {
            throw_stopiter();
            return FALSE;
        }
    }
    mb->cur += i;
    return TRUE;
}
static int strio_write(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefBytesIO *mb = Value_vp(ref->v[INDEX_TEXTIO_STREAM]);
    Value *vp;

    for (vp = v + 1; vp < fg->stk_top; vp++) {
        if (Value_type(*vp) == fs->cls_str) {
            RefStr *rs = Value_vp(*vp);
            if (!StrBuf_add_r(&mb->buf, rs)) {
                return FALSE;
            }
        } else {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, Value_type(*vp), (int)(vp - v));
            return FALSE;
        }
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int textio_print(Value *vret, Value *v, RefNode *node)
{
    int new_line = FUNC_INT(node);
    Value *base = fg->stk_top;
    Value *vp;

    *fg->stk_top++ = Value_cp(*v);
    for (vp = v + 1; vp < base; vp++) {
        *fg->stk_top++ = Value_cp(*vp);
        if (Value_type(*vp) != fs->cls_str) {
            // to_strを呼び出してStrに変換
            if (!call_member_func(fs->str_tostr, 0, TRUE)) {
                return FALSE;
            }
        }
    }
    if (new_line) {
        *fg->stk_top++ = cstr_Value(fs->cls_str, "\n", 1);
    }
    if (!call_member_func(intern("_write", -1), fg->stk_top - base - 1, TRUE)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * printf(format, v1, v2, ...)
 * printf(format, [v1, v2, ...])
 * printf(format, {k1:v1, k2:v2, ...})
 *
 * start_argは1から始まる
 */
static int textio_printf_sub(Value dst, RefStr *fmt_src, int start_arg, Ref *r_loc)
{
    const char *p = fmt_src->c;
    const char *end = p + fmt_src->size;

    Value *varg = fg->stk_base + start_arg;
    int vargc = (fg->stk_top - fg->stk_base) - start_arg;
    HashValueEntry **vhash = NULL;

    // フォーマットの後の引数が1個の場合はArray,Mapを解析する
    if (vargc == 1) {
        RefNode *va_type = Value_type(*varg);
        if (va_type == fs->cls_map) {
            RefMap *r2 = Value_vp(*varg);
            vhash = r2->entry;
            vargc = r2->entry_num;       // hashの場合はhash_entryサイズとして使う
        } else if (va_type == fs->cls_list) {
            RefArray *r2 = Value_vp(*varg);
            varg = r2->p;
            vargc = r2->size;
        }
    }
    // 最後の_write呼び出しのため
    *fg->stk_top++ = Value_cp(dst);

    while (p < end) {
        if (*p == '%') {
            p++;
            if (p >= end) {
                break;
            }
            if (*p == '{' || isalnum(*p) || *p == '_') {
                Str key;
                Str fmt;
                Value *val = NULL;

                if (*p == '{') {
                    // %{key:format}
                    p++;
                    key.p = p;
                    while (p < end && *p != '}' && *p != ':') {
                        p++;
                    }
                    key.size = p - key.p;

                    if (p < end && *p == ':') {
                        p++;
                        fmt.p = p;
                        while (p < end && *p != '}') {
                            p++;
                        }
                        fmt.size = p - fmt.p;
                    } else {
                        fmt.p = NULL;
                        fmt.size = 0;
                    }
                    if (p == end) {
                        throw_errorf(fs->mod_lang, "FormatError", "Missing '}'");
                        return FALSE;
                    }
                    p++;
                } else {
                    // $key
                    key.p = p;
                    while (p < end && (isalnum(*p) || *p == '_')) {
                        p++;
                    }
                    key.size = p - key.p;

                    fmt.p = NULL;
                    fmt.size = 0;
                }

                if (vhash != NULL) {
                    // keyに対応する値をhashから取り出す
                    uint32_t hash = str_hash(key.p, key.size);
                    HashValueEntry *ep = vhash[hash & (vargc - 1)];
                    for (; ep != NULL; ep = ep->next) {
                        if (hash == ep->hash) {
                            RefNode *key_type = Value_type(ep->key);
                            if (key_type == fs->cls_str) {
                                RefStr *rkey = Value_vp(ep->key);
                                if (key.size == rkey->size && memcmp(key.p, rkey->c, key.size) == 0) {
                                    val = &ep->val;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    // keyを整数に変換して引数のn番目から取り出す
                    int n = parse_int(key.p, key.size, 65536);
                    if (n >= 0 && n < vargc) {
                        val = &varg[n];
                    }
                }

                if (val != NULL) {
                    if (fmt.size > 0) {
                        // to_strを実行
                        if (r_loc != NULL) {
                            Value_push("vSr", *val, fmt, r_loc);
                            if (!call_member_func(fs->str_tostr, 2, TRUE)) {
                                return FALSE;
                            }
                        } else {
                            Value_push("vS", *val, fmt);
                            if (!call_member_func(fs->str_tostr, 1, TRUE)) {
                                return FALSE;
                            }
                        }
                        {
                            Value *vret = fg->stk_top - 1;
                            RefNode *ret_type = Value_type(*vret);
                            if (ret_type != fs->cls_str) {
                                throw_error_select(THROW_RETURN_TYPE__NODE_NODE, fs->cls_str, ret_type);
                                return FALSE;
                            }
                        }
                    } else {
                        if (Value_type(*val) == fs->cls_str) {
                            *fg->stk_top++ = Value_cp(*val);
                        } else {
                            Value_push("v", *val);
                            if (!call_member_func(fs->str_tostr, 0, TRUE)) {
                                return FALSE;
                            }
                        }
                    }
                } else {
                    throw_errorf(fs->mod_lang, "IndexError", "Index %Q not found", key);
                    return FALSE;
                }
            } else if (*p == '%') {
                // %%は%にして出力
                *fg->stk_top++ = cstr_Value(fs->cls_str, "%", 1);
                p++;
            } else {
                throw_errorf(fs->mod_lang, "FormatError", "Invalid character after '%%'");
                return FALSE;
            }
        } else {
            // %が出るまでそのまま出力
            const char *top = p;
            while (p < end && *p != '%') {
                p++;
            }
            if (p > top) {
                *fg->stk_top++ = cstr_Value(fs->cls_str, top, p - top);
            }
        }
    }
    return TRUE;
}

/**
 * printf("format", ...)
 * printf(locale, "format", ...)
 */
static int textio_printf(Value *vret, Value *v, RefNode *node)
{
    int str = FUNC_INT(node);
    RefNode *v1_type = Value_type(v[1]);
    RefStr *s_fmt;
    Ref *r_loc = NULL;
    int start_arg;

    if (v1_type == fs->cls_str) {
        s_fmt = Value_vp(v[1]);
        start_arg = 2;
    } else if (v1_type == fs->cls_locale) {
        if (fg->stk_top > v + 2) {
            RefNode *v2_type = Value_type(v[2]);
            if (v2_type != fs->cls_str) {
                throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, v2_type, 2);
                return FALSE;
            }
        } else {
            throw_errorf(fs->mod_lang, "ArgumentError", "2 or more arguments excepted (1 given)");
            return FALSE;
        }
        r_loc = Value_ref(v[1]);
        s_fmt = Value_vp(v[2]);
        start_arg = 3;
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_locale, v1_type, 1);
        return FALSE;
    }

    {
        Value *stk_top = fg->stk_top;
        if (!textio_printf_sub(*v, s_fmt, start_arg, r_loc)) {
            return FALSE;
        }
        if (str) {
            // sprintf
            Value *vp;
            StrBuf sb;
            StrBuf_init_refstr(&sb, 0);
            for (vp = stk_top + 1; vp < fg->stk_top; vp++) {
                if (!StrBuf_add_r(&sb, Value_vp(*vp))) {
                    StrBuf_close(&sb);
                    return FALSE;
                }
            }
            *vret = StrBuf_str_Value(&sb, fs->cls_str);
        } else {
            if (!call_member_func(intern("_write", -1), fg->stk_top - stk_top - 1, TRUE)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}
int textio_flush(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Value stream = ref->v[INDEX_TEXTIO_STREAM];

    Value_push("v", stream);
    if (!call_member_func(intern("flush", -1), 0, TRUE)) {
        return FALSE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

static void std_textio_init(void)
{
    Ref *ref = ref_new(fs->cls_utf8io);
    ref->v[INDEX_TEXTIO_STREAM] = Value_cp(fg->v_cio);
    s_std_textio = vp_Value(ref);
}
static int stdout_gets(Value *vret, Value *v, RefNode *node)
{
    if (s_std_textio == VALUE_NULL) {
        std_textio_init();
    }
    *v = Value_cp(s_std_textio);
    return utf8io_gets(vret, v, node);
}
static int stdout_print(Value *vret, Value *v, RefNode *node)
{
    if (s_std_textio == VALUE_NULL) {
        std_textio_init();
    }
    *v = Value_cp(s_std_textio);
    return textio_print(vret, v, node);
}

static int stdout_printf(Value *vret, Value *v, RefNode *node)
{
    if (s_std_textio == VALUE_NULL) {
        std_textio_init();
    }
    *v = Value_cp(s_std_textio);

    return textio_printf(vret, v, node);
}

////////////////////////////////////////////////////////////////////////////////

static void define_io_text_func(RefNode *m)
{
    RefNode *n;

    // 標準出力系
    n = define_identifier(m, m, "gets", NODE_FUNC_N, 0);
    define_native_func_a(n, stdout_gets, 0, 0, (void*)FALSE, NULL);

    n = define_identifier(m, m, "getln", NODE_FUNC_N, 0);
    define_native_func_a(n, stdout_gets, 0, 0, (void*)TRUE, NULL);

    n = define_identifier(m, m, "print", NODE_FUNC_N, 0);
    define_native_func_a(n, stdout_print, 0, -1, (void*)FALSE, NULL);

    n = define_identifier(m, m, "puts", NODE_FUNC_N, 0);
    define_native_func_a(n, stdout_print, 0, -1, (void*)TRUE, NULL);

    n = define_identifier(m, m, "printf", NODE_FUNC_N, 0);
    define_native_func_a(n, stdout_printf, 1, -1, (void*)FALSE, NULL);

    n = define_identifier(m, m, "sprintf", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_printf, 1, -1, (void*)TRUE, NULL);
}
static void define_io_text_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;


    // TextIO
    cls = fs->cls_textio;
    n = define_identifier(m, cls, "print", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_print, 0, -1, (void*) FALSE);
    n = define_identifier(m, cls, "puts", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_print, 0, -1, (void*) TRUE);
    n = define_identifier(m, cls, "printf", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_printf, 1, -1, (void*)FALSE, NULL);
    n = define_identifier(m, cls, "flush", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_flush, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);


    // Utf8IO
    // 文字入出力、文字コード変換
    cls = fs->cls_utf8io;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, utf8io_new, 1, 2, NULL, fs->cls_streamio, fs->cls_bool);

    n = define_identifier(m, cls, "gets", NODE_FUNC_N, 0);
    define_native_func_a(n, utf8io_gets, 0, 0, (void*)TEXTIO_M_GETS);
    n = define_identifier(m, cls, "getln", NODE_FUNC_N, 0);
    define_native_func_a(n, utf8io_gets, 0, 0, (void*)TEXTIO_M_GETLN);
    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, utf8io_gets, 0, 0, (void*)TEXTIO_M_NEXT);
    n = define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    define_native_func_a(n, utf8io_write, 0, -1, NULL);
    n = define_identifier(m, cls, "translit", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, utf8io_translit, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_TEXTIO_NUM;
    extends_method(cls, fs->cls_textio);


    // StrIO
    cls = fv->cls_strio;
    cls->u.c.n_memb = INDEX_TEXTIO_NUM;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, strio_new, 0, 1, NULL, fs->cls_str);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, strio_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, strio_empty, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, strio_index, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, strio_size, 0, 0, NULL);
    n = define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    define_native_func_a(n, strio_dup, 0, 0, NULL);
    n = define_identifier(m, cls, "data", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, strio_data, 0, 0, NULL);
    n = define_identifier(m, cls, "sub", NODE_FUNC_N, 0);
    define_native_func_a(n, strio_data, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "gets", NODE_FUNC_N, 0);
    define_native_func_a(n, strio_gets, 0, 0, (void*)TEXTIO_M_GETS);
    n = define_identifier(m, cls, "getln", NODE_FUNC_N, 0);
    define_native_func_a(n, strio_gets, 0, 0, (void*)TEXTIO_M_GETLN);
    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, strio_gets, 0, 0, (void*)TEXTIO_M_NEXT);
    n = define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    define_native_func_a(n, strio_write, 0, -1, NULL);
    n = define_identifier(m, cls, "flush", NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_null, 0, 0, NULL); // 何もしない
    extends_method(cls, fs->cls_textio);
}

void init_io_text_module_1()
{
    RefNode *m = fs->mod_io;

    define_io_text_class(m);
    define_io_text_func(m);

    m->u.m.loaded = TRUE;
}
