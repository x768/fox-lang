#include "fox_io.h"
#include "fconv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
    FConv in;
    FConv out;
} FConvIO;


const FoxStatic *fs;
FoxGlobal *fg;

static RefNode *mod_codecvt;

//////////////////////////////////////////////////////////////////////////////////////////////////////

static FCharset *RefCharset_get_fcharset(RefCharset *rc, int throw_error)
{
    if (rc->cs == NULL) {
        rc->cs = FCharset_new(rc);
    }
    if (rc->cs == NULL && throw_error) {
        fs->throw_errorf(fs->mod_lang, "NotSupportedError", "Converting %s is not supported", rc->name->c);
    }
    return rc->cs;
}

static int convio_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_convio = FUNC_VP(node);
    FConvIO *cio;
    FCharset *fc;
    Ref *r = fs->ref_new(cls_convio);
    *vret = vp_Value(r);

    r->v[INDEX_TEXTIO_STREAM] = fs->Value_cp(v[1]);
    cio = (FConvIO*)malloc(sizeof(FConvIO));
    r->v[INDEX_CONVIO_FCONV] = ptr_Value(cio);

    cio->cs = Value_vp(v[2]);
    fc = RefCharset_get_fcharset(cio->cs, TRUE);
    if (fc == NULL) {
        return FALSE;
    }

    if (fg->stk_top > v + 3 && Value_bool(v[3])) {
        cio->trans = TRUE;
    }

    FConv_init(&cio->in, fc, TRUE, cio->trans ? UTF8_ALTER_CHAR : NULL);
    FConv_init(&cio->out, fc, TRUE, cio->trans ? "?" : NULL);

    return TRUE;
}
static int convio_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    FConvIO *cio = Value_ptr(r->v[INDEX_CONVIO_FCONV]);

    if (cio != NULL) {
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
        fs->StrBuf_init(&buf2, buf.size);
        FConv_conv_strbuf(&cio->in, &buf2, buf.p, buf.size, TRUE);
        if (cio->in.status != FCONV_OK) {
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
    Value *vp;
    int max;

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

            FConv_set_src(&cio->out, rs->c, rs->size, TRUE);
            FConv_set_dst(&cio->out, mb->buf.p + mb->buf.size, max - mb->buf.size);

            for (;;) {
                if (!FConv_next(&cio->out, TRUE)) {
                    return FALSE;
                }
                switch (cio->out.status) {
                case FCONV_OK:
                    mb->buf.size = cio->out.dst - mb->buf.p;
                    goto BREAK;
                case FCONV_OUTPUT_REQUIRED:
                    mb->buf.size = cio->out.dst - mb->buf.p;
                    if (!fs->stream_flush_sub(stream)) {
                        return FALSE;
                    }
                    FConv_set_dst(&cio->out, mb->buf.p, max);
                    break;
                case FCONV_ERROR:
                    return FALSE;
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

static int conv_tobytes(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    RefCharset *cs = Value_vp(v[2]);
    int alter = (fg->stk_top > v + 3) ? Value_bool(v[3]) : FALSE;
    FCharset *fc = RefCharset_get_fcharset(cs, FALSE);

    if (cs != NULL) {
        FConv cv;
        StrBuf buf;
        fs->StrBuf_init_refstr(&buf, 0);
        FConv_init(&cv, fc, FALSE, alter ? "?" : NULL);
        if (!FConv_conv_strbuf(&cv, &buf, rs->c, rs->size, TRUE)) {
            return FALSE;
        }
        *vret = fs->StrBuf_str_Value(&buf, fs->cls_bytes);
        return TRUE;
    } else {
        return FALSE;
    }
}
static int conv_tostr(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    RefCharset *cs = Value_vp(v[2]);
    int alter = (fg->stk_top > v + 3) ? Value_bool(v[3]) : FALSE;
    FCharset *fc = RefCharset_get_fcharset(cs, FALSE);

    if (cs != NULL) {
        FConv cv;
        StrBuf buf;
        fs->StrBuf_init_refstr(&buf, 0);
        FConv_init(&cv, fc, TRUE, alter ? UTF8_ALTER_CHAR : NULL);
        if (!FConv_conv_strbuf(&cv, &buf, rs->c, rs->size, TRUE)) {
            return FALSE;
        }
        *vret = fs->StrBuf_str_Value(&buf, fs->cls_str);
        return TRUE;
    } else {
        return FALSE;
    }
}
static int is_valid_encoding(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    RefCharset *cs = Value_vp(v[2]);
    FCharset *fc = RefCharset_get_fcharset(cs, FALSE);
    int to_utf8 = FUNC_INT(node);

    if (cs != NULL) {
        char buf[BUFFER_SIZE];
        FConv cv;
        FConv_init(&cv, fc, to_utf8, NULL);
        FConv_set_src(&cv, rs->c, rs->size, TRUE);
        for (;;) {
            FConv_set_dst(&cv, buf, BUFFER_SIZE);
            if (!FConv_next(&cv, FALSE)) {
                *vret = bool_Value(FALSE);
                return TRUE;
            }
            if (cv.status != FCONV_OUTPUT_REQUIRED) {
                break;
            }
        }
        *vret = bool_Value(TRUE);
        return TRUE;
    } else {
        return FALSE;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 文字コードをUTF-8に変換してセットする
 * 変換できない文字は置き換え
 */
static Value cstr_Value_conv(const char *p, int size, RefCharset *rc)
{
    FCharset *cs = RefCharset_get_fcharset(rc, FALSE);
    if (cs != NULL) {
        FConv fc;
        StrBuf buf;
        fs->StrBuf_init_refstr(&buf, 0);
        FConv_init(&fc, cs, TRUE, UTF8_ALTER_CHAR);
        FConv_conv_strbuf(&fc, &buf, p, size, FALSE);
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
    n = fs->define_identifier(m, m, "is_representable", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, is_valid_encoding, 2, 2, (void*)FALSE, fs->cls_str, fs->cls_charset);
    n = fs->define_identifier(m, m, "is_valid_encoding", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, is_valid_encoding, 2, 2, (void*)TRUE, fs->cls_bytes, fs->cls_charset);
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

    f->RefCharset_get_fcharset = RefCharset_get_fcharset;

    f->FConv_init = FConv_init;
    f->FConv_conv_strbuf = FConv_conv_strbuf;
    f->FConv_next = FConv_next;
    f->FConv_set_src = FConv_set_src;
    f->FConv_set_dst = FConv_set_dst;

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
    static char *buf = NULL;

    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }

    fs = a_fs;
    if (buf == NULL) {
        StrBuf sb;
        int first = TRUE;
        RefCharset *cs = fs->cs_enumerate;

        fs->StrBuf_init(&sb, 0);
        fs->StrBuf_add(&sb, "Build at\t" __DATE__ "\nSupported encodings\t", -1);

        for (cs = fs->cs_enumerate; cs != NULL; cs = cs->next) {
            if (cs->type >= FCHARSET_UTF16LE || cs->files != NULL) {
                if (first) {
                    first = FALSE;
                } else {
                    fs->StrBuf_add(&sb, ", ", 2);
                }
                fs->StrBuf_add_r(&sb, cs->name);
            }
        }

        fs->StrBuf_add(&sb, "\n\0", 2);
        buf = sb.p;
    }
    return buf;
}

