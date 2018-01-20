#include "m_codecvt.h"
#include "fox_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>


enum {
    ALT_MAX_LEN = 32,
};
enum {
    INDEX_CONVIO_FCONV = INDEX_TEXTIO_NUM,
    INDEX_CONVIO_NUM,
};
enum {
    TEXTIO_M_GETS,
    TEXTIO_M_GETLN,
    TEXTIO_M_NEXT,
};

typedef struct {
    RefCharset *cs;
    int trans;

    CodeCVT in;
    CodeCVT out;
} FConvIO;


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_codecvt;

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void RefStr_copy(char *dst, RefStr *src, int max)
{
    int size = src->size;
    if (size > max - 1) {
        size = max - 1;
    }
    memcpy(dst, src->c, size);
    dst[size] = '\0';
}
static int CodeCVT_open(CodeCVT *ic, RefCharset *from, RefCharset *to, const char *trans)
{
    enum {
        MAX_LEN = 32,
    };
    char c_src[MAX_LEN];
    char c_dst[MAX_LEN];

    ic->cs_from = from;
    ic->cs_to = to;
    ic->trans = trans;

    RefStr_copy(c_src, from->ic_name, MAX_LEN);
    RefStr_copy(c_dst, to->ic_name, MAX_LEN);

    ic->ic = iconv_open(c_dst, c_src);

    if (ic->ic == (iconv_t)-1) {
        fs->throw_errorf(fs->mod_lang, "InternalError", "iconv_open fault");
        return FALSE;
    }
    return TRUE;
}
static void CodeCVT_close(CodeCVT *ic)
{
    if (ic->ic != (iconv_t)-1) {
        iconv_close(ic->ic);
        ic->ic = (iconv_t)-1;
    }
}

static int CodeCVT_next(CodeCVT *ic)
{
    errno = 0;
    if (iconv(ic->ic, (char**)&ic->inbuf, &ic->inbytesleft, &ic->outbuf, &ic->outbytesleft) == (size_t)-1) {
        switch (errno) {
        case EINVAL:
            return CODECVT_OK;
        case E2BIG:
            return CODECVT_OUTBUF;
        case EILSEQ:
            return CODECVT_INVALID;
        }
    }
    return CODECVT_OK;
}

static int make_alt_string(char *dst, const char *alt, int ch)
{
    enum {
        SURROGATE_NONE = 0,
        SURROGATE_DONE = -1,
    };
    const char *p = alt;
    const char *top = dst;
    int lo_sur = SURROGATE_NONE;  // Low Surrogate

    for (;;) {
        switch (*p) {
        case '\0':
            if (lo_sur > 0) {
                p = alt;
                ch = lo_sur;
                lo_sur = SURROGATE_DONE;
            } else {
                return dst - top;
            }
            break;
        case 'd':
            sprintf(dst, "%d", ch);
            dst += strlen(dst);
            p++;
            break;
        case 'x':
            sprintf(dst, "%02X", ch);
            dst += strlen(dst);
            p++;
            break;
        case 'u':
            if (lo_sur == SURROGATE_NONE) {
                if (ch > 0x10000) {
                    ch -= 0x10000;
                    lo_sur = ch % 0x400 + 0xDC00;
                    ch = ch / 0x400 + 0xD800;
                }
            }
            sprintf(dst, "%04X", ch);
            dst += strlen(dst);
            p++;
            break;
        case 'U':
            sprintf(dst, "%04X", ch);
            dst += strlen(dst);
            p++;
            break;
        default:
            *dst++ = *p;
            p++;
            break;
        }
    }
}
static int CodeCVT_conv(CodeCVT *ic, StrBuf *dst, const char *src_p, int src_size, int from_uni, int raise_error)
{
    char *tmp_buf = malloc(BUFFER_SIZE);
    char alt[ALT_MAX_LEN];
    int alt_size;

    ic->inbuf = src_p;
    ic->inbytesleft = src_size;
    ic->outbuf = tmp_buf;
    ic->outbytesleft = BUFFER_SIZE;

    for (;;) {
        switch (CodeCVT_next(ic)) {
        case CODECVT_OK:
            if (ic->inbuf != NULL) {
                ic->inbuf = NULL;
            } else {
                goto BREAK;
            }
            break;
        case CODECVT_OUTBUF:
            if (!fs->StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
                return FALSE;
            }
            ic->outbuf = tmp_buf;
            ic->outbytesleft = BUFFER_SIZE;
            break;
        case CODECVT_INVALID:
            if (ic->trans != NULL) {
                const char *psrc;
                char *pdst;

                if (from_uni) {
                    alt_size = make_alt_string(alt, ic->trans, fs->utf8_codepoint_at(ic->inbuf));
                } else {
                    alt_size = make_alt_string(alt, ic->trans, *ic->inbuf);
                }
                if (ic->outbytesleft < alt_size) {
                    if (!fs->StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
                        return FALSE;
                    }
                    ic->outbuf = tmp_buf;
                    ic->outbytesleft = BUFFER_SIZE;
                }
                psrc = ic->inbuf;
                pdst = ic->outbuf;

                memcpy(pdst, alt, alt_size);
                ic->outbuf += alt_size;
                ic->outbytesleft -= alt_size;

                if (from_uni) {
                    fs->utf8_next(&psrc, psrc + ic->inbytesleft);
                } else {
                    psrc++;
                }

                ic->inbytesleft -= psrc - ic->inbuf;
                ic->inbuf = psrc;
            } else {
                if (raise_error) {
                    if (from_uni) {
                        int ch = fs->utf8_codepoint_at(ic->inbuf);
                        fs->throw_errorf(fs->mod_lang, "CharsetError", "Cannot convert to %r (%U)", ic->cs_to->name, ch);
                    } else {
                        fs->throw_errorf(fs->mod_lang, "CharsetError", "Invalid byte sequence detected (%r)", ic->cs_from->name);
                    }
                }
                free(tmp_buf);
                return FALSE;
            }
            break;
        }
    }
BREAK:
    if (!fs->StrBuf_add(dst, tmp_buf, BUFFER_SIZE - ic->outbytesleft)) {
        return FALSE;
    }
    free(tmp_buf);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int convio_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_convio = FUNC_VP(node);
    FConvIO *cio;
    Ref *r = fs->ref_new(cls_convio);
    *vret = vp_Value(r);

    r->v[INDEX_TEXTIO_STREAM] = fs->Value_cp(v[1]);
    cio = (FConvIO*)malloc(sizeof(FConvIO));
    r->v[INDEX_CONVIO_FCONV] = ptr_Value(cio);
    cio->in.ic = (void*)-1;
    cio->out.ic = (void*)-1;
    cio->cs = Value_vp(v[2]);
    cio->trans = TRUE;

    if (fg->stk_top > v + 3 && Value_bool(v[3])) {
        cio->trans = TRUE;
    }

    return TRUE;
}
static int convio_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    FConvIO *cio = Value_ptr(r->v[INDEX_CONVIO_FCONV]);

    if (cio != NULL) {
        CodeCVT_close(&cio->in);
        CodeCVT_close(&cio->out);
        free(cio);
        r->v[INDEX_CONVIO_FCONV] = VALUE_NULL;
    }

    fs->unref(r->v[INDEX_TEXTIO_STREAM]);
    r->v[INDEX_TEXTIO_STREAM] = VALUE_NULL;
    return TRUE;
}
static int convio_gets(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Value stream = ref->v[INDEX_TEXTIO_STREAM];
    StrBuf buf;
    int ret = TRUE;
    int gets_type = FUNC_INT(node);

    fs->StrBuf_init(&buf, 0);
    if (!fs->stream_gets_sub(&buf, stream, '\n')) {
        StrBuf_close(&buf);
        return FALSE;
    }

    if (buf.size > 0) {
        FConvIO *cio = Value_ptr(ref->v[INDEX_CONVIO_FCONV]);
        StrBuf buf2;

        if (gets_type != TEXTIO_M_GETS) {
            // 末尾の改行を取り除く
            if (buf.p[buf.size - 1] == '\n') {
                buf.size--;
                if (buf.size > 0 && buf.p[buf.size - 1] == '\r') {
                    buf.size--;
                }
            }
        }
        if (cio->in.ic == (void*)-1) {
            if (!CodeCVT_open(&cio->in, cio->cs, fs->cs_utf8, cio->trans ? UTF8_ALTER_CHAR : NULL)) {
                ret = FALSE;
                goto FINALLY;
            }
        }
        fs->StrBuf_init(&buf2, buf.size);
        if (!CodeCVT_conv(&cio->in, &buf2, buf.p, buf.size, FALSE, TRUE)) {
            StrBuf_close(&buf2);
            ret = FALSE;
            goto FINALLY;
        }
        *vret = fs->cstr_Value(fs->cls_str, buf2.p, buf2.size);
        StrBuf_close(&buf2);
    } else {
        if (gets_type == TEXTIO_M_NEXT) {
            fs->throw_stopiter();
            ret = FALSE;
        }
    }

FINALLY:
    StrBuf_close(&buf);
    return ret;
}
static int convio_write(Value *vret, Value *v, RefNode *node)
{
    Ref *ref_textio = Value_ref(*v);
    Value stream = ref_textio->v[INDEX_TEXTIO_STREAM];
    FConvIO *cio = Value_ptr(ref_textio->v[INDEX_CONVIO_FCONV]);
    RefBytesIO *mb;
    CodeCVT *ic = &cio->out;
    Value *vp;
    int max;

    if (cio->out.ic == (void*)-1) {
        if (!CodeCVT_open(&cio->out, fs->cs_utf8, cio->cs, cio->trans ? "?" : NULL)) {
            return FALSE;
        }
    }
    {
        Value vmb;
        if (fs->stream_get_write_memio(stream, &vmb, &max)) {
            return FALSE;
        }
        mb = Value_vp(vmb);
    }


    for (vp = v + 1; vp < fg->stk_top; vp++) {
        if (fs->Value_type(*vp) == fs->cls_str) {
            RefStr *rs = Value_vp(*vp);

            ic->inbuf = rs->c;
            ic->inbytesleft = rs->size;
            ic->outbuf = mb->buf.p + mb->buf.size;
            ic->outbytesleft = max - mb->buf.size;

            for (;;) {
                switch (CodeCVT_next(ic)) {
                case CODECVT_OK:
                    mb->buf.size = ic->outbuf - mb->buf.p;
                    goto BREAK;
                case CODECVT_OUTBUF:
                    mb->buf.size = ic->outbuf - mb->buf.p;
                    if (!fs->stream_flush_sub(stream)) {
                        return FALSE;
                    }
                    ic->outbuf = mb->buf.p;
                    ic->outbytesleft = max;
                    break;
                case CODECVT_INVALID:
                    if (cio->trans) {
                        const char *ptr;
                        if (ic->outbytesleft == 0) {
                            mb->buf.size = ic->outbuf - mb->buf.p;
                            if (!fs->stream_flush_sub(stream)) {
                                return FALSE;
                            }
                            ic->outbuf = mb->buf.p;
                            ic->outbytesleft = max;
                        }
                        ptr = ic->inbuf;

                        *ic->outbuf++ = '?';
                        ic->outbytesleft--;

                        // 1文字進める
                        fs->utf8_next(&ptr, ptr + ic->inbytesleft);
                        ic->inbytesleft -= ptr - ic->inbuf;
                        ic->inbuf = ptr;
                    } else {
                        RefCharset *cs = cio->cs;
                        fs->throw_errorf(fs->mod_lang, "CharsetError", "Cannot convert %U to %S",
                                         fs->utf8_codepoint_at(ic->inbuf), cs->name);
                        return FALSE;
                    }
                    break;
                }
            }
        BREAK: // for
            ;
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, fs->Value_type(*vp), (int)(vp - v));
            return FALSE;
        }
    }
    return TRUE;
}
static int convio_charset(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    FConvIO *cio = Value_ptr(ref->v[INDEX_CONVIO_FCONV]);
    *vret = vp_Value(cio->cs);
    return TRUE;
}
static int convio_translit(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    FConvIO *cio = Value_ptr(ref->v[INDEX_CONVIO_FCONV]);
    *vret = bool_Value(cio->trans);
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int conv_tobytes(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}
int conv_tostr(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 文字コードをUTF-8に変換してセットする
 * 変換できない文字は置き換え
 */
static Value cstr_Value_conv(const char *p, int size, RefCharset *cs)
{
    CodeCVT ic;
    if (CodeCVT_open(&ic, cs, fs->cs_utf8, UTF8_ALTER_CHAR)) {
        StrBuf buf;
        fs->StrBuf_init_refstr(&buf, 0);
        CodeCVT_conv(&ic, &buf, p, size, FALSE, TRUE);
        CodeCVT_close(&ic);
        return fs->StrBuf_str_Value(&buf, fs->cls_str);
    }
    return VALUE_NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "conv_tobytes", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conv_tobytes, 2, 3, NULL, fs->cls_str, fs->cls_charset, fs->cls_bool);
    n = fs->define_identifier(m, m, "conv_tostr", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conv_tostr, 2, 3, NULL, fs->cls_bytes, fs->cls_charset, fs->cls_bool);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    RefNode *cls_convio = fs->define_identifier(m, m, "ConvIO", NODE_CLASS, 0);


    // ConvIO
    cls = cls_convio;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, convio_new, 2, 3, cls_convio, fs->cls_streamio, fs->cls_charset, fs->cls_bool);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, convio_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "gets", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, convio_gets, 0, 0, (void*)TEXTIO_M_GETS);
    n = fs->define_identifier(m, cls, "getln", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, convio_gets, 0, 0, (void*)TEXTIO_M_GETLN);
    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, convio_gets, 0, 0, (void*)TEXTIO_M_NEXT);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, convio_write, 0, -1, NULL);
    n = fs->define_identifier(m, cls, "charset", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, convio_charset, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "translit", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, convio_translit, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_TEXTIO_NUM;
    fs->extends_method(cls, fs->cls_textio);
}

static CodeCVTStatic *CodeCVTStatic_new(void)
{
    CodeCVTStatic *f = fs->Mem_get(&fg->st_mem, sizeof(CodeCVTStatic));

    f->CodeCVT_open = CodeCVT_open;
    f->CodeCVT_close = CodeCVT_close;
    f->CodeCVT_next = CodeCVT_next;
    f->CodeCVT_conv = CodeCVT_conv;

    f->cstr_Value_conv = cstr_Value_conv;

    return f;
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_codecvt = m;

    define_func(m);
    define_class(m);

    m->u.m.ext = CodeCVTStatic_new();
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}

