#include "m_codecvt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>


enum {
    ALT_MAX_LEN = 32,
};

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

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 文字コードをUTF-8に変換してセットする
 * 変換できない文字は置き換え
 */
Value cstr_Value_conv(const char *p, int size, RefCharset *cs)
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
/*
    RefNode *n;
    RefNode *root = &m->root;

    n = fs->define_identifier(m, root, "convert_table", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, convert_kana, 2, 2, NULL, fs->cls_str, fs->cls_str);
*/
}
static void define_class(RefNode *m)
{
//    RefNode *cls;
//    RefNode *n;
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

