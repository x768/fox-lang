#include "fox_windows.h"


int codepageio_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r;
    int cp = Value_integral(v[2]);

    if (cp < 0 || cp > 65536) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Codepage range error (%d)", cp);
        return FALSE;
    }

    r = fs->ref_new(FUNC_VP(node));
    *vret = vp_Value(r);
    r->v[INDEX_TEXTIO_STREAM] = fs->Value_cp(v[1]);
    r->v[INDEX_CODEPAGEIO_CODEPAGE] = int32_Value(cp);
    return TRUE;
}
int codepageio_gets(Value *vret, Value *v, RefNode *node)
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
        if (gets_type != TEXTIO_M_GETS) {
            // 末尾の改行を取り除く
            if (buf.p[buf.size - 1] == '\n') {
                buf.size--;
                if (buf.size > 0 && buf.p[buf.size - 1] == '\r') {
                    buf.size--;
                }
            }
        }
        {
            int cp = Value_integral(ref->v[INDEX_CODEPAGEIO_CODEPAGE]);
            int length = MultiByteToWideChar(cp, 0, buf.p, buf.size, NULL, 0);
            wchar_t *wbuf = malloc(sizeof(wchar_t) * (length + 1));
            MultiByteToWideChar(cp, 0, buf.p, buf.size, wbuf, length);
            *vret = fs->wstr_Value(fs->cls_str, wbuf, length);
            free(wbuf);
        }
    } else {
        if (gets_type == TEXTIO_M_NEXT) {
            fs->throw_stopiter();
            ret = FALSE;
        }
    }
    StrBuf_close(&buf);
    return ret;
}

static void codepage_conv(StrBuf *buf, StrBuf *wbuf, int codepage, RefStr *rs)
{
    int len;
    int wlen = utf8_to_utf16(NULL, rs->c, rs->size);
    fs->StrBuf_alloc(wbuf, (wlen + 1) * sizeof(wchar_t));
    utf8_to_utf16((wchar_t*)wbuf->p, rs->c, rs->size);
    wbuf->size = wlen * sizeof(wchar_t);

    len = WideCharToMultiByte(codepage, 0, (const wchar_t*)wbuf->p, wbuf->size / sizeof(wchar_t), NULL, 0, NULL, NULL);
    fs->StrBuf_alloc(buf, len + 1);
    WideCharToMultiByte(codepage, 0, (const wchar_t*)wbuf->p, wbuf->size / sizeof(wchar_t), buf->p, buf->size + 1, NULL, NULL);
    buf->size = len;
}
int codepageio_write(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    int cp = Value_integral(ref->v[INDEX_CODEPAGEIO_CODEPAGE]);
    Value stream = ref->v[INDEX_TEXTIO_STREAM];
    Value *vp;
    StrBuf buf;
    StrBuf wbuf;
    int ret = FALSE;

    fs->StrBuf_init(&buf, 0);
    fs->StrBuf_init(&wbuf, 0);

    for (vp = v + 1; vp < fg->stk_top; vp++) {
        if (fs->Value_type(*vp) == fs->cls_str) {
            codepage_conv(&buf, &wbuf, cp, Value_vp(*vp));
            if (!fs->stream_write_data(stream, buf.p, buf.size)) {
                goto FINALLY;
            }
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, fs->Value_type(*vp), (int)(vp - v));
            goto FINALLY;
        }
    }
    ret = TRUE;

FINALLY:
    StrBuf_close(&buf);
    StrBuf_close(&wbuf);
    return ret;
}
int codepageio_codepage(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = int32_Value(r->v[INDEX_CODEPAGEIO_CODEPAGE]);
    return TRUE;
}

int cpio_tobytes(Value *vret, Value *v, RefNode *node)
{
    int cp = Value_integral(v[2]);
    RefStr *rs = Value_vp(v[1]);
    RefStr *dst;
    int len;
    int wlen = utf8_to_utf16(NULL, rs->c, rs->size);
    wchar_t *wbuf = malloc((wlen + 1) * sizeof(wchar_t));
    utf8_to_utf16(wbuf, rs->c, rs->size);

    len = WideCharToMultiByte(cp, 0, wbuf, wlen, NULL, 0, NULL, NULL);
    dst = fs->refstr_new_n(fs->cls_bytes, len);
    WideCharToMultiByte(cp, 0, wbuf, wlen, dst->c, len + 1, NULL, NULL);
    *vret = vp_Value(dst);

    free(wbuf);
    return TRUE;
}
int cpio_tostr(Value *vret, Value *v, RefNode *node)
{
    int cp = Value_integral(v[2]);
    RefStr *rs = Value_vp(v[1]);
    RefStr *dst;
    int len;
    int wlen = MultiByteToWideChar(cp, 0, rs->c, rs->size, NULL, 0);
    wchar_t *wbuf = malloc((wlen + 1) * sizeof(wchar_t));
    MultiByteToWideChar(cp, 0, rs->c, rs->size, wbuf, wlen + 1);

    len = utf16_to_utf8(NULL, wbuf, wlen);
    dst = fs->refstr_new_n(fs->cls_str, len);
    utf16_to_utf8(dst->c, wbuf, wlen);
    *vret = vp_Value(dst);

    free(wbuf);
    return TRUE;
}
