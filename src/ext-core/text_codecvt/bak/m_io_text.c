#include "fox_vm.h"
#include <stdio.h>
#include <string.h>


typedef struct {
    RefHeader rh;

    RefCharset *cs;
    int trans;

    IconvIO in;
    IconvIO out;
} RefTextIO;


/**
 * 文字コードを変換しながらStreamに出力する
 * v : stream (optional,sb==NULLの場合必須)
 * sb : StrBuf : (optional)
 */
static int stream_write_sub_s(Value v, StrBuf *sb, const char *s_p, int s_size, RefTextIO *tio)
{
    if (s_size < 0) {
        s_size = strlen(s_p);
    }
    // TODO:未検証
    if (v != VALUE_NULL) {
        RefBytesIO *mb = Value_vp(v);
        if (mb->rh.type == fs->cls_bytesio) {
            sb = &mb->buf;
        }
    }
    if (sb != NULL) {
        // sprintfの場合tio==NULL
        if (tio == NULL || tio->cs->utf8) {
            if (!StrBuf_add(sb, s_p, s_size)) {
                return FALSE;
            }
            return TRUE;
        } else {
            if (tio->out.ic == (void*)-1) {
                if (!IconvIO_open(&tio->out, fs->cs_utf8, tio->cs, tio->trans ? "?" : NULL)) {
                    return FALSE;
                }
            }
            if (!IconvIO_conv(&tio->out, sb, s_p, s_size, TRUE, TRUE)) {
                return FALSE;
            }
            return TRUE;
        }
    } else {
        if (tio == NULL || tio->cs->utf8) {
            return stream_write_data(v, s_p, s_size);
        } else {
            Ref *r = Value_ref(v);
            IconvIO *ic = &tio->out;
            Value *vmb = &r->v[INDEX_WRITE_MEMIO];
            RefBytesIO *mb;
            int max = Value_integral(r->v[INDEX_WRITE_MAX]);

            if (max == -1) {
                throw_error_select(THROW_NOT_OPENED_FOR_WRITE);
                return FALSE;
            }
            if (tio->out.ic == (void*)-1) {
                if (!IconvIO_open(&tio->out, fs->cs_utf8, tio->cs, tio->trans ? "?" : NULL)) {
                    return FALSE;
                }
            }
            if (*vmb == VALUE_NULL) {
                *vmb = vp_Value(bytesio_new_sub(NULL, BUFFER_SIZE));
                max = BUFFER_SIZE;
                r->v[INDEX_WRITE_MAX] = int32_Value(max);
                // STDOUTだけ特別扱い
                if (fs->cgi_mode) {
                    if (r == Value_vp(fg->v_cio)) {
                        send_headers();
                    }
                }
            }
            mb = Value_vp(*vmb);

            ic->inbuf = s_p;
            ic->inbytesleft = s_size;
            ic->outbuf = mb->buf.p + mb->buf.size;
            ic->outbytesleft = max - mb->buf.size;

            for (;;) {
                switch (IconvIO_next(ic)) {
                case ICONV_OK:
                    mb->buf.size = ic->outbuf - mb->buf.p;
                    goto BREAK;
                case ICONV_OUTBUF:
                    mb->buf.size = ic->outbuf - mb->buf.p;
                    if (!stream_flush_sub(v)) {
                        return FALSE;
                    }
                    ic->outbuf = mb->buf.p;
                    ic->outbytesleft = max;
                    break;
                case ICONV_INVALID:
                    if (tio->trans) {
                        const char *ptr;
                        if (ic->outbytesleft == 0) {
                            mb->buf.size = ic->outbuf - mb->buf.p;
                            if (!stream_flush_sub(v)) {
                                return FALSE;
                            }
                            ic->outbuf = mb->buf.p;
                            ic->outbytesleft = max;
                        }
                        ptr = ic->inbuf;

                        *ic->outbuf++ = '?';
                        ic->outbytesleft--;

                        // 1文字進める
                        utf8_next(&ptr, ptr + ic->inbytesleft);
                        ic->inbytesleft -= ptr - ic->inbuf;
                        ic->inbuf = ptr;
                    } else {
                        RefCharset *cs = tio->cs;
                        throw_errorf(fs->mod_lang, "CharsetError", "Cannot convert %U to %S",
                                utf8_codepoint_at(ic->inbuf), cs->name);
                        return FALSE;
                    }
                    break;
                }
            }
        BREAK:
            return TRUE;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

static int textio_new(Value *vret, Value *v, RefNode *node)
{
    RefTextIO *tio;
    RefCharset *cs = Value_vp(v[2]);
    Ref *r = ref_new(fs->cls_textio);
    *vret = vp_Value(r);

    tio = buf_new(NULL, sizeof(RefTextIO));
    r->v[INDEX_TEXTIO_TEXTIO] = vp_Value(tio);
    r->v[INDEX_TEXTIO_STREAM] = Value_cp(v[1]);

    tio->in.ic = (void*)-1;
    tio->out.ic = (void*)-1;
    tio->cs = cs;
    tio->trans = FALSE;

    if (fg->stk_top > v + 3 && Value_bool(v[3])) {
        tio->trans = TRUE;
    }
    if (fg->stk_top > v + 4) {
        r->v[INDEX_TEXTIO_NEWLINE] = Value_cp(v[4]);
    }

    return TRUE;
}
static int textio_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefTextIO *tio = Value_vp(r->v[INDEX_TEXTIO_TEXTIO]);

    if (tio != NULL) {
        if (tio != (RefTextIO*)fv->ref_textio_utf8) {
            IconvIO_close(&tio->in);
            IconvIO_close(&tio->out);
            free(tio);
        }
        r->v[INDEX_TEXTIO_TEXTIO] = VALUE_NULL;
    }
    Value_dec(r->v[INDEX_TEXTIO_STREAM]);
    r->v[INDEX_TEXTIO_STREAM] = VALUE_NULL;

    return TRUE;
}
static int textio_gets_sub(Value *vret, Ref *ref, int trim)
{
    Value stream = ref->v[INDEX_TEXTIO_STREAM];
    StrBuf buf;
    Str result;
    int ret = TRUE;
    RefNode *s_type = Value_type(stream);

    if (s_type == fs->cls_bytesio) {
        buf.p = NULL;
        if (!bytesio_gets_sub(&result, stream)) {
            result.size = 0;
        }
    } else {
        StrBuf_init(&buf, 0);
        if (!stream_gets_sub(&buf, stream, '\n')) {
            StrBuf_close(&buf);
            return FALSE;
        }
        result = Str_new(buf.p, buf.size);
    }
    if (result.size > 0) {
        RefTextIO *tio = Value_vp(ref->v[INDEX_TEXTIO_TEXTIO]);
        if (trim) {
            // 末尾の改行を取り除く
            if (result.size > 0 && result.p[result.size - 1] == '\n') {
                result.size--;
                if (result.size > 0 && result.p[result.size - 1] == '\r') {
                    result.size--;
                }
            }
        }
        if (tio->cs == fs->cs_utf8) {
            if (invalid_utf8_pos(result.p, result.size) >= 0) {
                if (tio->trans) {
                    // 不正な文字をU+FFFDに置き換える
                    *vret = cstr_Value_conv(result.p, result.size, NULL);
                } else {
                    throw_error_select(THROW_INVALID_UTF8);
                    ret = FALSE;
                    goto FINALLY;
                }
            } else {
                *vret = cstr_Value(fs->cls_str, result.p, result.size);
            }
        } else {
            StrBuf buf2;
            if (tio->in.ic == (void*)-1) {
                if (!IconvIO_open(&tio->in, tio->cs, fs->cs_utf8, tio->trans ? UTF8_ALTER_CHAR : NULL)) {
                    ret = FALSE;
                    goto FINALLY;
                }
            }

            StrBuf_init(&buf2, result.size);
            if (!IconvIO_conv(&tio->in, &buf2, result.p, result.size, FALSE, TRUE)) {
                StrBuf_close(&buf2);
                ret = FALSE;
                goto FINALLY;
            }
            *vret = cstr_Value(fs->cls_str, buf2.p, buf2.size);
            StrBuf_close(&buf2);
        }
    }

FINALLY:
    if (buf.p != NULL) {
        StrBuf_close(&buf);
    }

    return ret;
}
static int textio_gets(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    int trim = FUNC_INT(node);

    if (!textio_gets_sub(vret, ref, trim)) {
        return FALSE;
    }
    return TRUE;
}
static int textio_next(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);

    if (!textio_gets_sub(vret, ref, TRUE)) {
        return FALSE;
    }
    if (!Value_bool(*vret)) {
        throw_stopiter();
        return FALSE;
    }
    return TRUE;
}

static int textio_print_sub(Value v_textio, StrBuf *sb, Value v, Ref *r_loc)
{
    RefNode *v_type = Value_type(v);
    int result = TRUE;
    Value vb;
    RefTextIO *tio;

    if (v_textio != VALUE_NULL) {
        Ref *ref = Value_ref(v_textio);
        vb = ref->v[INDEX_TEXTIO_STREAM];
        tio = Value_vp(ref->v[INDEX_TEXTIO_TEXTIO]);
    } else {
        vb = VALUE_NULL;
        tio = NULL;
    }

    // よく使う型
    if (Value_isint(v)) {
        char c_buf[32];
        sprintf(c_buf, "%d", Value_integral(v));
        if (!stream_write_sub_s(vb, sb, c_buf, -1, tio)) {
            result = FALSE;
        }
    } else if (v_type == fs->cls_int) {
        RefInt *mp = Value_vp(v);
        char *c_buf = malloc(BigInt_str_bufsize(&mp->bi, 10));
        BigInt_str(&mp->bi, 10, c_buf, FALSE);
        if (!stream_write_sub_s(vb, sb, c_buf, -1, tio)) {
            result = FALSE;
        }
        free(c_buf);
    } else if (v_type == fs->cls_str) {
        RefStr *rs = Value_vp(v);
        if (!stream_write_sub_s(vb, sb, rs->c, rs->size, tio)) {
            result = FALSE;
        }
    } else if (v_type == fv->cls_strio) {
        RefBytesIO *mb = Value_bytesio(v);
        if (!stream_write_sub_s(vb, sb, mb->buf.p, mb->buf.size, tio)) {
            result = FALSE;
        }
    } else if (v_type == fs->cls_module) {
        RefNode *nd = Value_vp(v);
        RefStr *rs = nd->name;
        if (!stream_write_sub_s(vb, sb, "Module(", 7, tio)) {
            return FALSE;
        }
        if (!stream_write_sub_s(vb, sb, rs->c, rs->size, tio)) {
            return FALSE;
        }
        if (!stream_write_sub_s(vb, sb, ")", 1, tio)) {
            return FALSE;
        }
    } else if (v_type != fs->cls_null) {
        Value vret;
        fs->Value_push("v", v);
        if (!call_member_func(fs->str_tostr, 0, TRUE)) {
            return FALSE;
        }
        vret = fg->stk_top[-1];
        if (Value_type(vret) == fs->cls_str) {
            RefStr *rs = Value_vp(vret);
            if (!stream_write_sub_s(vb, sb, rs->c, rs->size, tio)) {
                return FALSE;
            }
        } else {
        }
        Value_pop();
    }
    return result;
}

static int textio_print(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Value *varg = v + 1;
    int new_line = FUNC_INT(node);
    Value stream = ref->v[INDEX_TEXTIO_STREAM];
    RefNode *s_type = Value_type(stream);
    StrBuf *sb = NULL;

    if (s_type == fs->cls_bytesio) {
        sb = bytesio_get_strbuf(stream);
    }
    while (varg < fg->stk_top) {
        if (!textio_print_sub(*v, sb, *varg, NULL)) {
            return FALSE;
        }
        varg++;
    }

    if (new_line) {
        RefTextIO *tio = Value_vp(ref->v[INDEX_TEXTIO_TEXTIO]);
        if (tio == NULL || tio->cs->ascii) {
            if (sb != NULL) {
                if (!StrBuf_add_c(sb, '\n')) {
                    return FALSE;
                }
            } else if (!stream_write_data(stream, "\n", 1)) {
                return FALSE;
            }
        } else {
            if (!stream_write_sub_s(stream, sb, "\n", 1, tio)) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

/**
 * printf(format, v1, v2, ...)
 * printf(format, [v1, v2, ...])
 * printf(format, {k1:v1, k2:v2, ...})
 *
 * start_argは1から始まる
 *
 * v, sbどちらかが必要
 */
static int textio_printf_sub(Value v, StrBuf *sb, RefStr *fmt_src, int start_arg, Ref *r_loc)
{
    const char *p = fmt_src->c;
    const char *end = p + fmt_src->size;

    Value *varg = fg->stk_base + start_arg;
    int vargc = (fg->stk_top - fg->stk_base) - start_arg;
    HashValueEntry **vhash = NULL;
    Value stream;
    RefTextIO *tio;

    if (v != VALUE_NULL) {
        Ref *ref = Value_ref(v);
        stream = ref->v[INDEX_TEXTIO_STREAM];
        tio = Value_vp(ref->v[INDEX_TEXTIO_TEXTIO]);
    } else {
        stream = VALUE_NULL;
        tio = NULL;
    }
    if (sb == NULL) {
        RefNode *s_type = Value_type(stream);
        if (s_type == fs->cls_bytesio) {
            sb = bytesio_get_strbuf(stream);
        }
    }

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
                    if (p < end) {
                        p++;
                    }
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
                        // to_strを取り出す
                        RefNode *type = Value_type(*val);
                        RefNode *fn = Hash_get_p(&type->u.c.h, fs->str_tostr);
                        RefStr *vs;

                        Value_push("vS", *val, fmt);
                        if (r_loc != NULL) {
                            Value_push("r", r_loc);
                        }
                        if (!call_function(fn, (r_loc != NULL ? 2 : 1))) {
                            return FALSE;
                        }

                        vs = Value_vp(fg->stk_top[-1]);
                        if (!stream_write_sub_s(stream, sb, vs->c, vs->size, tio)) {
                            return FALSE;
                        }
                        Value_pop();
                    } else {
                        if (!textio_print_sub(v, sb, *val, r_loc)) {
                            return FALSE;
                        }
                    }
                } else {
                    throw_errorf(fs->mod_lang, "IndexError", "Index %Q not found", key);
                    return FALSE;
                }
            } else if (*p == '%') {
                // %%は%にして出力
                if (!stream_write_sub_s(stream, sb, "%", 1, tio)) {
                    return FALSE;
                }
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
                if (!stream_write_sub_s(stream, sb, top, p - top, tio)) {
                    return FALSE;
                }
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

    if (str) {
        // sprintf
        StrBuf buf;
        StrBuf_init(&buf, 0);
        if (!textio_printf_sub(VALUE_NULL, &buf, s_fmt, start_arg, r_loc)) {
            return FALSE;
        }
        *vret = cstr_Value(fs->cls_str, buf.p, buf.size);
        StrBuf_close(&buf);
    } else {
        if (!textio_printf_sub(*v, NULL, s_fmt, start_arg, r_loc)) {
            return FALSE;
        }
    }

    return TRUE;
}
int textio_flush(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Value stream = ref->v[INDEX_TEXTIO_STREAM];
    RefNode *s_type = Value_type(stream);

    if (s_type != fs->cls_bytesio) {
        Value_push("v", stream);
        if (!call_member_func(intern("flush", -1), 0, TRUE)) {
            return FALSE;
        }
    }
    return TRUE;
}
int textio_charset(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefTextIO *tio = Value_vp(ref->v[INDEX_TEXTIO_TEXTIO]);
    RefCharset *cs = (tio != NULL ? tio->cs : NULL);

    if (cs != NULL) {
        *vret = vp_Value(cs);
    }
    return TRUE;
}
int textio_translit(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    RefTextIO *tio = Value_vp(ref->v[INDEX_TEXTIO_TEXTIO]);
    *vret = bool_Value(tio != NULL && tio->trans);

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
    r->v[INDEX_TEXTIO_TEXTIO] = vp_Value(fv->ref_textio_utf8);

    return TRUE;
}

static int strio_size(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_bytesio(*v);
    *vret = int32_Value(strlen_utf8(mb->buf.p, mb->buf.size));
    return TRUE;
}
static int strio_empty(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_bytesio(*v);
    *vret = bool_Value(mb->buf.size == 0);
    return TRUE;
}
static int strio_tostr(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_bytesio(*v);

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
    RefBytesIO *mb = Value_bytesio(*v);
    int begin, end;

    string_substr_position(&begin, &end, mb->buf.p, mb->buf.size, v);
    *vret = cstr_Value(fs->cls_str, mb->buf.p + begin, end - begin);

    return TRUE;
}
static int strio_dup(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *src = Value_bytesio(*v);
    Ref *r = ref_new(fv->cls_strio);

    *vret = vp_Value(r);
    r->v[INDEX_TEXTIO_STREAM] = vp_Value(bytesio_new_sub(src->buf.p, src->buf.size));
    r->v[INDEX_TEXTIO_TEXTIO] = vp_Value(fv->ref_textio_utf8);

    return TRUE;
}
static int strio_index(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_bytesio(*v);
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

////////////////////////////////////////////////////////////////////////////////

static void v_ctextio_init(void)
{
    Ref *ref = ref_new(fs->cls_textio);

    fg->v_ctextio = vp_Value(ref);
    ref->v[INDEX_TEXTIO_STREAM] = Value_cp(fg->v_cio);
    ref->v[INDEX_TEXTIO_TEXTIO] = vp_Value(fv->ref_textio_utf8);
}
static int stdout_gets(Value *vret, Value *v, RefNode *node)
{
    int trim = FUNC_INT(node);

    if (fg->v_ctextio == VALUE_NULL) {
        v_ctextio_init();
    }
    if (!textio_gets_sub(vret, Value_vp(fg->v_ctextio), trim)) {
        return FALSE;
    }
    return TRUE;
}
static int stdout_print(Value *vret, Value *v, RefNode *node)
{
    int new_line = FUNC_INT(node);
    Value *varg = v + 1;

    if (fg->v_ctextio == VALUE_NULL) {
        v_ctextio_init();
    }
    *v = Value_cp(fg->v_ctextio);

    while (varg < fg->stk_top) {
        if (!textio_print_sub(*v, NULL, *varg, NULL)) {
            return FALSE;
        }
        varg++;
    }
    if (new_line && !stream_write_data(fg->v_cio, "\n", 1)) {
        return FALSE;
    }

    return TRUE;
}

static int stdout_printf(Value *vret, Value *v, RefNode *node)
{
    if (fg->v_ctextio == VALUE_NULL) {
        v_ctextio_init();
    }
    *v = Value_cp(fg->v_ctextio);

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
    // 文字入出力、文字コード変換
    cls = fs->cls_textio;
    cls->u.c.n_memb = INDEX_TEXTIO_NUM;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, textio_new, 2, 4, NULL, fs->cls_streamio, fs->cls_charset, fs->cls_bool, fs->cls_str);

    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, textio_close, 0, 0, NULL);

    n = define_identifier(m, cls, "gets", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_gets, 0, 0, (void*)FALSE);
    n = define_identifier(m, cls, "getln", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_gets, 0, 0, (void*)TRUE);
    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_next, 0, 0, NULL);
    n = define_identifier(m, cls, "print", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_print, 0, -1, (void*) FALSE);
    n = define_identifier(m, cls, "puts", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_print, 0, -1, (void*) TRUE);
    n = define_identifier(m, cls, "printf", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_printf, 1, -1, (void*)FALSE, NULL);
    n = define_identifier(m, cls, "flush", NODE_FUNC_N, 0);
    define_native_func_a(n, textio_flush, 0, 0, NULL);
    n = define_identifier(m, cls, "charset", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, textio_charset, 0, 0, NULL);
    n = define_identifier(m, cls, "translit", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, textio_translit, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);


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
    n = define_identifier(m, cls, "flush", NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_null, 0, 0, NULL); // 何もしない
    extends_method(cls, fs->cls_textio);
}

void init_io_text_module_1()
{
    RefTextIO *tio = Mem_get(&fg->st_mem, sizeof(RefTextIO));
    RefNode *m = fs->mod_io;

    tio->rh.type = NULL;
    tio->rh.nref = -1;
    tio->rh.n_memb = 0;
    tio->rh.weak_ref = NULL;
    tio->cs = fs->cs_utf8;
    tio->in.ic = (void*)-1;
    tio->out.ic = (void*)-1;
    tio->trans = TRUE;
    fv->ref_textio_utf8 = &tio->rh;

    define_io_text_class(m);
    define_io_text_func(m);

    m->u.m.loaded = TRUE;
}
