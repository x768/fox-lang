#include "fox_vm.h"
#include <stdio.h>
#include <string.h>
#include <math.h>


enum {
    PACK_NONE,
    PACK_NIL,
    PACK_INT,
    PACK_FLOAT,
    PACK_BIN,
    PACK_ZBIN,
};

typedef struct {
    RefHeader rh;

    RefCharset *cs;
    int trans;

    IconvIO in;
    IconvIO out;
} RefTextIO;

static RefStr *str__read;
static RefStr *str__write;
static RefStr *str__seek;
static RefStr *str__close;

#define Value_bytesio(val) ((RefBytesIO*)(uintptr_t)(Value_ref((val))->v[INDEX_TEXTIO_STREAM]))

static int stream_write_sub_s(Value v, StrBuf *sb, const char *s_p, int s_size, RefTextIO *tio);


int value_to_streamio(Value *stream, Value v, int writemode, int argn)
{
    RefNode *v_type = Value_type(v);
    if (v_type == fs->cls_str || v_type == fs->cls_file) {
        // FileIOを開く
        RefNode *fileio_new = Hash_get_p(&fv->cls_fileio->u.c.h, fs->str_new);
        Value_push("Nv", v);
        if (writemode) {
            Value_push("s", "w");
        }
        if (!call_function(fileio_new, (writemode ? 2 : 1))) {
            return FALSE;
        }
        fg->stk_top--;
        *stream = *fg->stk_top;
    } else if (is_subclass(v_type, fs->cls_streamio)) {
        // foxのstreamから読み込む
        *stream = Value_cp(v);
    } else {
        throw_errorf(fs->mod_lang, "TypeError", "Str, File or StreamIO required but %n (argument #%d)", v_type, argn + 1);
        return FALSE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

void init_stream_ref(Ref *r, int mode)
{
    r->v[INDEX_READ_CUR] = int32_Value(0);
    // -1の場合はreadしたとき例外を出す
    r->v[INDEX_READ_MAX] = int32_Value(((mode & STREAM_READ) != 0 ? 0 : -1));

    r->v[INDEX_READ_OFFSET] = uint62_Value(0ULL);

    // -1の場合はwriteしたとき例外を出す
    r->v[INDEX_WRITE_MAX] = int32_Value((mode & STREAM_WRITE) != 0 ? 0 : -1);
}
static RefBytesIO *bytesio_new_streambuf(void)
{
    RefBytesIO *mb = buf_new(fs->cls_bytesio, sizeof(RefBytesIO));
    StrBuf_init(&mb->buf, BUFFER_SIZE);
    return mb;
}

static int stream_open(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int mode = FUNC_INT(node);

    // 必ず継承先でnewするので、初期化されているはず
    init_stream_ref(r, mode);

    *vret = Value_cp(*v);

    return TRUE;
}
// _write -> _close を呼び出す
static int stream_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value *v1 = &r->v[INDEX_READ_MEMIO];
    Value *v2 = &r->v[INDEX_WRITE_MEMIO];

    if (*v1 != VALUE_NULL) {
        Value_dec(*v1);
        *v1 = VALUE_NULL;
    }
    if (*v2 != VALUE_NULL) {
        if (!stream_flush_sub(*v)) {
            //TODO Stream.flush失敗時の対応
        }
        Value_dec(*v2);
        *v2 = VALUE_NULL;
    }

    if (Value_integral(r->v[INDEX_READ_MAX]) >= 0 || Value_integral(r->v[INDEX_WRITE_MAX]) >= 0) {
        Value_push("v", *v);
        if (!call_member_func(str__close, 0, FALSE)) {
            return FALSE;
        }
        Value_pop();
        r->v[INDEX_READ_MAX] = int32_Value(-1);
        r->v[INDEX_WRITE_MAX] = int32_Value(-1);
    }

    return TRUE;
}
static int stream_read_buffer(Value v, int *max, int offset)
{
    Value v1 = Value_ref(v)->v[INDEX_READ_MEMIO];
    RefBytesIO *mb = Value_vp(v1);

    StrBuf_alloc(&mb->buf, BUFFER_SIZE);
    mb->buf.size = offset;

    Value_push("vvd", v, v1, BUFFER_SIZE - offset);
    if (!call_member_func(str__read, 2, TRUE)) {
        return FALSE;
    }
    // 戻り値を除去
    Value_pop();

    if (mb->buf.size > 0) {
        *max = mb->buf.size;
    } else {
        *max = 0;
    }

    return TRUE;
}
/**
 * sb != NULLならsbに読み込む
 * sb == NULLならp, psizeに読み込む
 * keep : 読み込み後もカーソルを移動しないならtrue
 * read_all : psizeバイト読めなかったらエラー
 */
int stream_read_data(Value v, StrBuf *sb, char *p, int *psize, int keep, int read_all)
{
    Ref *r = Value_ref(v);

    int cur = Value_integral(r->v[INDEX_READ_CUR]);
    int max = Value_integral(r->v[INDEX_READ_MAX]);
    int read_to = *psize;   // 読み込むサイズ
    int last = read_to;     // 残りサイズ
    int nread = 0;          // 読み込み済み
    Value *vmb = &r->v[INDEX_READ_MEMIO];
    RefBytesIO *mb;
    char *buf;

    // BytesIO
    if (r->rh.type == fs->cls_bytesio) {
        int sz = *psize;
        mb = Value_vp(v);
        if (sz > mb->buf.size - mb->cur) {
            if (read_all) {
                mb->cur = mb->buf.size;
                throw_errorf(fs->mod_io, "ReadError", "Stream reaches EOF");
                return FALSE;
            }
            sz = mb->buf.size - mb->cur;
            *psize = sz;
        }
        if (sb != NULL) {
            if (!StrBuf_add(sb, mb->buf.p + mb->cur, sz)) {
                return FALSE;
            }
        }
        if (p != NULL) {
            memcpy(p, mb->buf.p + mb->cur, sz);
        }
        if (!keep) {
            mb->cur += sz;
        }
        return TRUE;
    }

    if (max == -1) {
        throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    }
    if (*vmb != VALUE_NULL) {
        mb = Value_vp(*vmb);
        buf = mb->buf.p;
    } else {
        mb = NULL;
        buf = NULL;
    }

    if (keep) {
        if (read_to > MAGIC_MAX) {
            throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }
        if (cur + last > max) {
            int offset = 0;

            if (*vmb == VALUE_NULL) {
                mb = bytesio_new_streambuf();
                *vmb = vp_Value(mb);
                buf = mb->buf.p;
            } else {
                // 残りの部分を先頭に移動
                uint64_t offs = Value_uint62(r->v[INDEX_READ_OFFSET]);
                r->v[INDEX_READ_OFFSET] = uint62_Value(offs + cur);

                memmove(buf, buf + cur, max - cur);
                offset = max - cur;
            }
            cur = 0;
            if (!stream_read_buffer(v, &max, offset)) {
                return FALSE;
            }
            mb->buf.size = max;
            r->v[INDEX_READ_CUR] = int32_Value(cur);
            r->v[INDEX_READ_MAX] = int32_Value(max);
        }
        memcpy(p + nread, buf + cur, last);
        nread = last;
    } else {
        // 出力バッファを開いていたら、flushを実行する
        if (!stream_flush_sub(v)) {
            return FALSE;
        }
        for (;;) {
            if (*vmb == VALUE_NULL) {
                mb = bytesio_new_streambuf();
                *vmb = vp_Value(mb);
                buf = mb->buf.p;
            } else {
                // バッファの最後か読み込みサイズまで読む
                int sz = max - cur;
                if (sz > last) {
                    sz = last;
                }
                if (sb != NULL) {
                    if (!StrBuf_add(sb, buf + cur, sz)) {
                        return FALSE;
                    }
                }
                if (p != NULL) {
                    memcpy(p + nread, buf + cur, sz);
                }
                nread += sz;
                last -= sz;
                cur += sz;
                // 全て読んだら終了
                if (last <= 0) {
                    break;
                }
            }

            {
                uint64_t offs = Value_uint62(r->v[INDEX_READ_OFFSET]);
                r->v[INDEX_READ_OFFSET] = uint62_Value(offs + cur);
            }
            if (!stream_read_buffer(v, &max, 0)) {
                return FALSE;
            }
            // read_bufferでBytesIOが変化している可能性があるため
            mb = Value_vp(*vmb);
            buf = mb->buf.p;
            cur = 0;
            // 読み取れなかったら終了
            if (max == 0) {
                break;
            }
        }

        mb->buf.size = max;
        r->v[INDEX_READ_CUR] = int32_Value(cur);
        r->v[INDEX_READ_MAX] = int32_Value(max);
    }
    if (read_all) {
        if (read_to != nread) {
            throw_errorf(fs->mod_io, "ReadError", "Stream reaches EOF");
            return FALSE;
        }
    }
    *psize = nread;

    return TRUE;
}
int stream_read_uint8(uint8_t *val, Value r)
{
    int size = 2;
    return stream_read_data(r, NULL, (char*)val, &size, FALSE, TRUE);
}
int stream_read_uint16(uint16_t *val, Value r)
{
    int size = 2;
    uint8_t buf[2];

    if (!stream_read_data(r, NULL, (char*)buf, &size, FALSE, TRUE)) {
        return FALSE;
    }
    *val = (buf[0] << 8) | buf[1];
    return TRUE;
}
int stream_read_uint32(uint32_t *val, Value r)
{
    int size = 4;
    uint8_t buf[4];

    if (!stream_read_data(r, NULL, (char*)buf, &size, FALSE, TRUE)) {
        return FALSE;
    }
    *val = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return TRUE;
}
/**
 * 改行か、limitバイトまで読み込む
 */
int stream_gets_limit(Value v, char *p, int *psize)
{
    Ref *r = Value_ref(v);

    int cur = Value_integral(r->v[INDEX_READ_CUR]);
    int max = Value_integral(r->v[INDEX_READ_MAX]);
    int read_to = *psize;   // 読み込むサイズ
    int last = read_to;     // 残りサイズ
    int nread = 0;          // 読み込み済み
    Value *vmb = &r->v[INDEX_READ_MEMIO];
    RefBytesIO *mb;
    char *buf;
    int i;

    // BytesIO
    if (r->rh.type == fs->cls_bytesio) {
        int sz = *psize;
        mb = Value_vp(v);
        if (sz > mb->buf.size - mb->cur) {
            sz = mb->buf.size - mb->cur;
            *psize = sz;
        }
        // 改行の次まで読む
        for (i = 0; i < sz; i++) {
            int c = mb->buf.p[mb->cur + i];
            p[i] = c;
            if (c == '\n') {
                i++;
                break;
            }
        }
        mb->cur += i;
        return TRUE;
    }

    if (max == -1) {
        throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    }
    if (*vmb != VALUE_NULL) {
        mb = Value_vp(*vmb);
        buf = mb->buf.p;
    } else {
        mb = NULL;
        buf = NULL;
    }

    if (!stream_flush_sub(v)) {
        return FALSE;
    }
    for (;;) {
        if (*vmb == VALUE_NULL) {
            mb = bytesio_new_streambuf();
            *vmb = vp_Value(mb);
            buf = mb->buf.p;
        } else {
            // バッファの最後か読み込みサイズまで読む
            int sz = max - cur;
            int found = FALSE;
            if (sz > last) {
                sz = last;
            }
            // 改行の次まで読む
            for (i = 0; i < last && cur + i < max; i++) {
                int c = buf[cur + i];
                p[nread + i] = c;
                if (c == '\n') {
                    found = TRUE;
                    i++;
                    break;
                }
            }
            nread += i;
            last -= i;
            cur += i;
            // 全て読み終えたか、\nが出現したら終了
            if (last <= 0 || found) {
                break;
            }
        }

        {
            int offs = Value_uint62(r->v[INDEX_READ_OFFSET]);
            r->v[INDEX_READ_OFFSET] = uint62_Value(offs + cur);
        }
        if (!stream_read_buffer(v, &max, 0)) {
            return FALSE;
        }
        // read_bufferでBytesIOが変化している可能性があるため
        mb = Value_vp(*vmb);
        buf = mb->buf.p;
        cur = 0;
        if (max == 0) {
            break;
        }
    }

    mb->buf.size = max;
    r->v[INDEX_READ_CUR] = int32_Value(cur);
    r->v[INDEX_READ_MAX] = int32_Value(max);
    *psize = nread;

    return TRUE;
}

/**
 * 文字 sep が出現するか最後まで読む
 */
int stream_gets_sub(StrBuf *sb, Value v, int sep)
{
    Ref *r = Value_ref(v);

    int cur = Value_integral(r->v[INDEX_READ_CUR]);
    int max = Value_integral(r->v[INDEX_READ_MAX]);
    Value *vmb = &r->v[INDEX_READ_MEMIO];
    RefBytesIO *mb;
    char *buf;

    // BytesIO
    if (r->rh.type == fs->cls_bytesio) {
        int i;
        int sz;

        mb = Value_vp(v);
        sz = mb->buf.size - mb->cur;
        // sepの次まで読む
        for (i = 0; i < sz; i++) {
            if (mb->buf.p[mb->cur + i] == sep) {
                i++;
                break;
            }
        }
        StrBuf_add(sb, mb->buf.p + mb->cur, i);
        mb->cur += i;
        return TRUE;
    }

    if (max == -1) {
        throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    }

    if (*vmb != VALUE_NULL) {
        mb = Value_vp(*vmb);
        buf = mb->buf.p;
    } else {
        mb = NULL;
        buf = NULL;
    }
    // 出力バッファを開いていたら、flushを実行する
    if (!stream_flush_sub(v)) {
        return FALSE;
    }
    for (;;) {
        if (*vmb == VALUE_NULL) {
            mb = bytesio_new_streambuf();
            *vmb = vp_Value(mb);
            buf = mb->buf.p;
        } else {
            // 改行まで読む
            int top = cur;
            int found = FALSE;
            int limit = fs->max_alloc - sb->size;  // 改行が見つからない場合はMAX_ALLOCに制限

            if (sep >= 0) {
                while (cur < max) {
                    if (cur >= limit) {
                        found = TRUE;
                        break;
                    }
                    if (buf[cur] == sep) {
                        found = TRUE;
                        cur++;
                        break;
                    }
                    cur++;
                }
            } else {
                cur = (max < limit ? max : limit);
            }
            if (!StrBuf_add(sb, buf + top, cur - top)) {
                return FALSE;
            }
            if (found) {
                break;
            }
        }

        if (!stream_read_buffer(v, &max, 0)) {
            return FALSE;
        }
        mb = Value_vp(*vmb);
        buf = mb->buf.p;

        cur = 0;
        if (max == 0) {
            break;
        }
    }
    mb->buf.size = max;
    r->v[INDEX_READ_CUR] = int32_Value(cur);
    r->v[INDEX_READ_MAX] = int32_Value(max);

    return TRUE;
}
static int stream_read(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;

    if (fg->stk_top > fg->stk_base + 1) {
        Value v1 = v[1];
        int64_t size64 = Value_int64(v1, NULL);
        int size;

        if (size64 > fs->max_alloc) {
            throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        } else if (size64 < 0) {
            throw_errorf(fs->mod_lang, "ValueError", "Invalid size parameter (< 0)");
            return FALSE;
        }

        size = size64;
        StrBuf_init(&buf, size);
        if (!stream_read_data(*v, NULL, buf.p, &size, FALSE, FALSE)) {
            StrBuf_close(&buf);
            return FALSE;
        }
        buf.size = size;
    } else {
        // 最後まで読む
        int size = fs->max_alloc;
        StrBuf_init(&buf, 0);
        if (!stream_read_data(*v, &buf, NULL, &size, FALSE, FALSE)) {
            StrBuf_close(&buf);
            return FALSE;
        }
    }

    if (buf.size > 0) {
        *vret = cstr_Value(fs->cls_bytes, buf.p, buf.size);
    }
    StrBuf_close(&buf);

    return TRUE;
}
static int stream_gets(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;
    int ret = TRUE;

    StrBuf_init(&buf, 0);
    if (!stream_gets_sub(&buf, *v, '\n')) {
        StrBuf_close(&buf);
        return FALSE;
    }

    if (buf.size > 0) {
        *vret = cstr_Value(fs->cls_bytes, buf.p, buf.size);
    }
    StrBuf_close(&buf);

    return ret;
}
static int stream_fetch(Value *vret, Value *v, RefNode *node)
{
    int size = Value_int64(v[1], NULL);
    RefStr *rs;
    if (size < 0 || size > MAGIC_MAX) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }

    rs = refstr_new_n(fs->cls_bytes, size);
    *vret = vp_Value(rs);
    if (!stream_read_data(*v, NULL, rs->c, &size, TRUE, FALSE)) {
        return FALSE;
    }
    rs->c[size] = '\0';
    rs->size = size;

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

int stream_flush_sub(Value v)
{
    Ref *r = Value_ref(v);
    Value v1 = r->v[INDEX_WRITE_MEMIO];
    RefBytesIO *mb;

    if (v1 == VALUE_NULL) {
        return TRUE;
    }
    mb = Value_vp(v1);
    // バッファが空になるまで実行する
    while (mb->buf.size > 0) {
        int read_size;

        fs->Value_push("vv", v, v1);
        if (!call_member_func(str__write, 1, TRUE)) {
            return FALSE;
        }
        // 戻り値=書き込めたbytes数
        read_size = Value_integral(fg->stk_top[-1]);
        if (read_size <= 0) {
            // 書き込み失敗のため中断
            break;
        }
        Value_pop();
        if (read_size < mb->buf.size) {
            memmove(mb->buf.p, mb->buf.p + read_size, mb->buf.size - read_size);
            mb->buf.size -= read_size;
        } else {
            mb->buf.size = 0;
        }
    }

    return TRUE;
}
int stream_write_data(Value v, const char *s_p, int s_size)
{
    Ref *r = Value_ref(v);
    Value *outbuf = &r->v[INDEX_WRITE_MEMIO];
    int max = Value_integral(r->v[INDEX_WRITE_MAX]);
    int cur = 0;
    int last;

    if (s_size < 0) {
        s_size = strlen(s_p);
    }
    last = s_size;

    if (Value_type(v) == fs->cls_bytesio) {
        RefBytesIO *mb = Value_vp(v);
        return StrBuf_add(&mb->buf, s_p, s_size);
    }

    if (max == -1) {
        throw_error_select(THROW_NOT_OPENED_FOR_WRITE);
        return FALSE;
    }

    if (*outbuf == VALUE_NULL) {
        *outbuf = vp_Value(bytesio_new_streambuf());
        max = BUFFER_SIZE;
        r->v[INDEX_WRITE_MAX] = int32_Value(BUFFER_SIZE);
        // STDOUTだけ特別扱い
        if (fs->cgi_mode){
            if (r == Value_vp(fg->v_cio)) {
                send_headers();
                stream_write_data(v, s_p, s_size);
                return TRUE;
            }
        }
    }

    while (last > 0) {
        RefBytesIO *mb = Value_vp(*outbuf);
        int size = last;
        if (size > max - mb->buf.size) {
            size = max - mb->buf.size;
        }
        memcpy(mb->buf.p + mb->buf.size, s_p + cur, size);
        mb->buf.size += size;
        cur += size;
        last -= size;
        if (mb->buf.size >= max) {
            if (!stream_flush_sub(v)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}
int stream_write_uint8(uint8_t val, Value w)
{
    return stream_write_data(w, (char*)&val, 1);
}
int stream_write_uint16(uint16_t val, Value w)
{
    uint8_t buf[2];
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;

    if (!stream_write_data(w, (char*)buf, 2)) {
        return FALSE;
    }
    return TRUE;
}
int stream_write_uint32(uint32_t val, Value w)
{
    uint8_t buf[4];
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;

    if (!stream_write_data(w, (char*)buf, 4)) {
        return FALSE;
    }
    return TRUE;
}
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
                *vmb = vp_Value(bytesio_new_streambuf());
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
/**
 * v <- v1
 * Stream (Not BytesIO) <- Stream (Not BytesIO)
 */
static int stream_write_stream_sub(Value v, Value v1, int64_t last)
{
    Ref *r = Value_ref(v);
    Value *vmb = &r->v[INDEX_WRITE_MEMIO];

    if (Value_integral(r->v[INDEX_WRITE_MAX]) == -1) {
        throw_error_select(THROW_NOT_OPENED_FOR_WRITE);
        return FALSE;
    }

    if (*vmb == VALUE_NULL) {
        *vmb = vp_Value(bytesio_new_streambuf());
        r->v[INDEX_WRITE_MAX] = int32_Value(BUFFER_SIZE);
        // STDOUTだけ特別扱い
        if (fs->cgi_mode) {
            if (r == Value_vp(fg->v_cio)) {
                send_headers();
            }
        }
    }
    for (;;) {
        RefBytesIO *mb = Value_vp(*vmb);
        int read_size = BUFFER_SIZE - mb->buf.size;
        if (read_size > last) {
            read_size = last;
        }

        // r1から読んで、直接rのバッファに入れる
        if (!stream_read_data(v1, NULL, mb->buf.p + mb->buf.size, &read_size, FALSE, FALSE)) {
            return FALSE;
        }
        // r1が終端に達した
        if (read_size <= 0) {
            break;
        }
        mb->buf.size += read_size;
        last -= read_size;
        if (!stream_flush_sub(v)) {
            return FALSE;
        }
        if (last <= 0) {
            break;
        }
    }
    return TRUE;
}

/**
 * バイト列の入出力
 */
static int stream_write(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    if (!stream_write_data(*v, rs->c, rs->size)) {
        return FALSE;
    }
    return TRUE;
}
static int stream_write_s(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefNode *v1_type = Value_type(v1);

    if (v1_type == fs->cls_bytesio) {
        RefBytesIO *bio = Value_vp(v1);
        int size = bio->buf.size;
        if (fg->stk_top > v + 2) {
            int64_t limit = Value_int64(v[2], NULL);
            if (size > limit) {
                size = limit;
            }
        }
        if (!stream_write_data(*v, bio->buf.p, size)) {
            return FALSE;
        }
    } else {
        int64_t limit = INT64_MAX;
        if (fg->stk_top > v + 2) {
            limit = Value_int64(v[2], NULL);
        }
        if (!stream_write_stream_sub(*v, v1, limit)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int stream_pos(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    uint64_t ret;

    if (Value_integral(r->v[INDEX_READ_MAX]) == -1) {
        throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    }
    ret = Value_uint62(r->v[INDEX_READ_OFFSET]) + Value_integral(r->v[INDEX_READ_CUR]);
    *vret = int64_Value(ret);

    return TRUE;
}

static int stream_seek_sub(Value v, int64_t offset)
{
    int64_t new_offset;
    RefNode *ret_type;

    // seek先がbufferの範囲内ならカーソルを移動するだけ
    {
        Ref *r = Value_ref(v);
        Value *vmb = &r->v[INDEX_READ_MEMIO];
        if (*vmb != VALUE_NULL) {
            RefBytesIO *mb = Value_vp(*vmb);
            int64_t cur = offset - Value_uint62(r->v[INDEX_READ_OFFSET]);
            if (cur >= 0 && cur <= mb->buf.size) {
                r->v[INDEX_READ_CUR] = int32_Value(cur);
                return TRUE;
            }
        }
    }

    Value_push("vD", v, offset);
    if (!call_member_func(str__seek, 1, TRUE)) {
        return FALSE;
    }
    // 戻り値を除去
    ret_type = Value_type(fg->stk_top[-1]);
    if (ret_type != fs->cls_int) {
        throw_errorf(fs->mod_lang, "TypeError", "_seek returns %n (must be Int)", ret_type);
        return FALSE;
    }
    new_offset = Value_int64(fg->stk_top[-1], NULL);
    Value_pop();

    // バッファを消去
    {
        Ref *r = Value_ref(v);
        int max = Value_integral(r->v[INDEX_READ_MAX]);
        if (max >= 0) {
            r->v[INDEX_READ_CUR] = int32_Value(max);
        }
        // 次の読み込み時にcur分だけ増加するため
        r->v[INDEX_READ_OFFSET] = uint62_Value(new_offset - max);
    }

    return TRUE;
}
static int stream_set_pos(Value *vret, Value *v, RefNode *node)
{
    int64_t offset = Value_int64(v[1], NULL);
    if (!stream_seek_sub(*v, offset)) {
        return FALSE;
    }
    return TRUE;
}
static int stream_get_buffering(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = Value_type(*v);
    int ret = FALSE;

    if (type != fs->cls_bytesio) {
        Ref *r = Value_ref(*v);
        int max = Value_integral(r->v[INDEX_WRITE_MAX]);

        if (max == -1) {
            throw_error_select(THROW_NOT_OPENED_FOR_WRITE);
            return FALSE;
        }
        if (Value_bool(r->v[INDEX_WRITE_BUFFERING])) {
            ret = TRUE;
        }
    }
    *vret = bool_Value(ret);

    return TRUE;
}
static int stream_set_buffering(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = Value_type(*v);

    if (type != fs->cls_bytesio) {
        Ref *r = Value_ref(*v);
        int max = Value_integral(r->v[INDEX_WRITE_MAX]);

        if (max == -1) {
            throw_error_select(THROW_NOT_OPENED_FOR_WRITE);
            return FALSE;
        }
        r->v[INDEX_WRITE_BUFFERING] = v[1];
    }
    return TRUE;
}
static int stream_pack_int(Value *vret, StrBuf *sb, Value v, int size, int le)
{
    char buf[8];
    int i;

    if (Value_type(v) != fs->cls_int) {
        throw_errorf(fs->mod_lang, "TypeError", "Int required but %n", Value_type(v));
        return FALSE;
    }
    // 整数は符号なし
    if (Value_isref(v)) {
        RefInt *mpt = Value_vp(v);

        mp_int mp;
        mp_init(&mp);
        mp_copy(&mpt->mp, &mp);

        if (mp.sign == MP_NEG) {
            // ビット反転して+1
            mp.sign = MP_ZPOS;
            for (i = 0; i < size; i++) {
                mp.dp[i] = ~mp.dp[i];
            }
            mp_add_d(&mp, 1, &mp);
        }

        for (i = 0; i < size; i++) {
            int n = i / 2;
            uint8_t val;

            if (n < mp.used) {
                int d = i % 2;
                if (d == 0) {
                    val = mp.dp[n] & 0xFF;
                } else {
                    val = mp.dp[n] >> 8;
                }
            } else {
                val = 0;
            }
            if (le) {
                buf[i] = val;
            } else {
                buf[size - i - 1] = val;
            }
        }
        mp_clear(&mp);
    } else {
        uint32_t iv = Value_integral(v);
        for (i = 0; i < size; i++) {
            if (le) {
                buf[i] = iv & 0xFF;
            } else {
                buf[size - i - 1] = iv & 0xFF;
            }
            iv >>= 8;
        }
    }
    if (sb != NULL) {
        if (!StrBuf_add(sb, buf, size)) {
            return FALSE;
        }
    } else {
        if (!stream_write_data(*vret, buf, size)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int stream_pack_uint64(Value *vret, StrBuf *sb, uint64_t iv, int size, int le)
{
    char buf[8];
    int i;

    for (i = 0; i < size; i++) {
        if (le) {
            buf[i] = iv & 0xFF;
        } else {
            buf[size - i - 1] = iv & 0xFF;
        }
        iv >>= 8;
    }

    if (sb != NULL) {
        if (!StrBuf_add(sb, buf, size)) {
            return FALSE;
        }
    } else {
        if (!stream_write_data(*vret, buf, size)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int stream_pack_bin(Value *vret, StrBuf *sb, Value v)
{
    RefStr *rs;

    if (Value_type(v) != fs->cls_bytes) {
        throw_errorf(fs->mod_lang, "TypeError", "Bytes required but %n", Value_type(v));
        return FALSE;
    }
    rs = Value_vp(v);

    if (sb != NULL) {
        if (!StrBuf_add(sb, rs->c, rs->size)) {
            return FALSE;
        }
    } else {
        if (!stream_write_data(*vret, rs->c, rs->size)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int stream_pack_nil(Value *vret, StrBuf *sb)
{
    if (sb != NULL) {
        if (!StrBuf_add_c(sb, '\0')) {
            return FALSE;
        }
    } else {
        if (!stream_write_data(*vret, "\0", 1)) {
            return FALSE;
        }
    }
    return TRUE;
}
/**
 * -1 : 残りの引数すべて繰り返し
 * 0 : エラー
 */
static int pack_format_repeat(const char **pp, const char *end)
{
    int num;
    const char *p = *pp;

    if (p < end) {
        if (*p == '*') {
            num = -1;
            p++;
            *pp = p;
        } else if (isdigit_fox(*p)) {
            num = 0;
            while (p < end && isdigit_fox(*p)) {
                num = num * 10 + (*p - '0');
                if (num > 999) {
                    return 0;
                }
                p++;
            }
            *pp = p;
        } else {
            num = 1;
        }
    } else {
        num = 1;
    }
    return num;
}
static int stream_pack_sub(Value *v, StrBuf *sb, int argc, const char *fmt_p, int fmt_size)
{
    const char *p = fmt_p;
    const char *end = p + fmt_size;
    Value *argv;
    int idx = 0;

    if (argc == 2 && Value_type(v[2]) == fs->cls_list) {
        RefArray *ra = Value_vp(v[2]);
        argv = ra->p;
        argc = ra->size;
    } else {
        argv = v + 2;
        argc = argc - 1;
    }

    while (p < end) {
        int ch = *p++ & 0xFF;
        int type = PACK_NONE;
        int size = 0;
        int repeat = 1;
        int i;

        switch (ch) {
        case 'c': case 'C':
            type = PACK_INT;
            size = 1;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'S': case 's':
            type = PACK_INT;
            size = 2;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'I': case 'i':
            type = PACK_INT;
            size = 4;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'Q': case 'q':
            type = PACK_INT;
            size = 8;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'F': case 'f':
            type = PACK_FLOAT;
            size = 4;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'D': case 'd':
            type = PACK_FLOAT;
            size = 8;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'b':
            type = PACK_BIN;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'z':
            type = PACK_ZBIN;
            repeat = pack_format_repeat(&p, end);
            break;
        case 'x':
            type = PACK_NIL;
            repeat = pack_format_repeat(&p, end);
            if (repeat < 0) {
                throw_errorf(fs->mod_lang, "FormatError", "Format 'x' needs a number");
                return FALSE;
            }
            break;
        default:
            if (!isspace_fox(ch)) {
                throw_errorf(fs->mod_lang, "FormatError", "Invalid character %c in format string", ch);
                return FALSE;
            }
            break;
        }
        if (repeat == 0) {
            goto ARGUMENT_ERROR;
        }

        for (i = 0; ; i++) {
            if (repeat > 0) {
                if (i >= repeat) {
                    break;
                }
            } else {
                if (idx >= argc) {
                    break;
                }
            }
            switch (type) {
            case PACK_INT:
                if (idx >= argc) {
                    goto ARGUMENT_ERROR;
                }
                if (!stream_pack_int(v, sb, argv[idx], size, islower_fox(ch))) {
                    return FALSE;
                }
                idx++;
                break;
            case PACK_FLOAT: {
                union {
                    float f;
                    double d;
                    uint64_t u64;
                } u;
                Value va = argv[idx];

                if (idx >= argc) {
                    goto ARGUMENT_ERROR;
                }
                if (Value_type(va) != fs->cls_float) {
                    throw_errorf(fs->mod_lang, "TypeError", "Float required but %n", Value_type(va));
                    return FALSE;
                }
                if (size == 4) {
                    u.f = (float)Value_float(va);
                } else {
                    u.d = Value_float(va);
                }
                //TODO
                if (!stream_pack_uint64(v, sb, u.u64, size, islower_fox(ch))) {
                    return FALSE;
                }
                idx++;
                break;
            }
            case PACK_BIN:
            case PACK_ZBIN:
                if (idx >= argc) {
                    goto ARGUMENT_ERROR;
                }
                if (!stream_pack_bin(v, sb, argv[idx])) {
                    return FALSE;
                }
                idx++;
                if (type == PACK_ZBIN) {
                    if (!stream_pack_nil(v, sb)) {
                        return FALSE;
                    }
                }
                break;
            case PACK_NIL:
                if (!stream_pack_nil(v, sb)) {
                    return FALSE;
                }
                break;
            default:
                break;
            }
        }
    }
    return TRUE;

ARGUMENT_ERROR:
    throw_errorf(fs->mod_lang, "ArgumentError", "Too few arguments for format");
    return FALSE;
}
static int stream_pack(Value *vret, Value *v, RefNode *node)
{
    RefStr *fmt = Value_vp(v[1]);
    int argc = (fg->stk_top - fg->stk_base) - 1;

    if (!stream_pack_sub(v, NULL, argc, fmt->c, fmt->size)) {
        return FALSE;
    }
    return TRUE;
}

// 整数は符号なし
static int stream_unpack_int(Value *vret, Value *v, int size, int le, int sign)
{
    uint8_t buf[8];
    int read_size = size;
    uint64_t u64 = 0;
    int i;

    if (!stream_read_data(*v, NULL, (char*)buf, &read_size, FALSE, TRUE)) {
        return FALSE;
    }
    if (le) {
        for (i = 0; i < size; i++) {
            u64 |= ((uint64_t)buf[i]) << (i * 8);
        }
    } else {
        for (i = 0; i < size; i++) {
            u64 |= ((uint64_t)buf[i]) << ((size - i - 1) * 8);
        }
    }
    if (sign) {
        switch (size) {
        case 1:
            u64 = (int8_t)u64;
            break;
        case 2:
            u64 = (int16_t)u64;
            break;
        case 4:
            u64 = (int32_t)u64;
            break;
        }
    }
    *vret = int64_Value(u64);

    return TRUE;
}
static int stream_unpack_float(Value *vret, Value v, int size, int le)
{
    int read_size = size;
    uint8_t buf[8];
    int i;
    uint64_t u64 = 0;
    double d;

    if (!stream_read_data(v, NULL, (char*)buf, &read_size, FALSE, TRUE)) {
        return FALSE;
    }
    if (le) {
        for (i = 0; i < size; i++) {
            u64 |= ((uint64_t)buf[i]) << (i * 8);
        }
    } else {
        for (i = 0; i < size; i++) {
            u64 |= ((uint64_t)buf[i]) << ((size - i - 1) * 8);
        }
    }
    if (size == 4) {
        uint32_t u32 = (uint32_t)u64;
        float f;
        memcpy(&f, &u32, sizeof(u32));
        d = f;
    } else {
        memcpy(&d, &u64, sizeof(u64));
    }
    if (!isnan(d) && !isinf(d)) {
        RefFloat *rd = buf_new(fs->cls_float, sizeof(RefFloat));
        rd->d = d;
        *vret = vp_Value(rd);
    }
    return TRUE;
}
static int stream_unpack_bin(Value *vret, Value v, int size)
{
    RefStr *rs;
    int read_size = size;

    if (size > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    rs = refstr_new_n(fs->cls_bytes, size);
    *vret = vp_Value(rs);
    if (!stream_read_data(v, NULL, rs->c, &read_size, FALSE, TRUE)) {
        return FALSE;
    }
    rs->c[read_size] = '\0';

    return TRUE;
}
static int stream_unpack(Value *vret, Value *v, RefNode *node)
{
    int single = FUNC_INT(node);
    RefStr *fmt = Value_vp(v[1]);
    const char *p = fmt->c;
    const char *end = p + fmt->size;
    RefArray *ra;

    if (single) {
        ra = NULL;
    } else {
        ra = refarray_new(0);
        *vret = vp_Value(ra);
    }

    while (p < end) {
        int ch = *p++ & 0xFF;
        int type = PACK_NONE;
        int sign = FALSE;
        int size;

        if (ch == '+') {
            sign = TRUE;
            ch = *p++ & 0xFF;
        }
        switch (ch) {
        case 'c': case 'C':
            type = PACK_INT;
            size = 1;
            break;
        case 'S': case 's':
            type = PACK_INT;
            size = 2;
            break;
        case 'I': case 'i':
            type = PACK_INT;
            size = 4;
            break;
        case 'Q': case 'q':
            type = PACK_INT;
            size = 8;
            break;
        case 'F': case 'f':
            type = PACK_FLOAT;
            size = 4;
            break;
        case 'D': case 'd':
            type = PACK_FLOAT;
            size = 8;
            break;
        case 'b':
            if (sign) {
                throw_errorf(fs->mod_lang, "FormatError", "Unknown option '+'");
                return FALSE;
            }
            type = PACK_BIN;
            size = pack_format_repeat(&p, end);
            if (size < 0) {
                throw_errorf(fs->mod_lang, "FormatError", "Format 'b' needs a number");
                return FALSE;
            }
            break;
        case 'z':
            if (sign) {
                throw_errorf(fs->mod_lang, "FormatError", "Unknown option '+'");
                return FALSE;
            }
            type = PACK_ZBIN;
            size = 0;
            break;
        case 'x':
            if (sign) {
                throw_errorf(fs->mod_lang, "FormatError", "Unknown option '+'");
                return FALSE;
            }
            type = PACK_NIL;
            size = pack_format_repeat(&p, end);
            if (size < 0) {
                throw_errorf(fs->mod_lang, "FormatError", "Format 'x' needs a number");
                return FALSE;
            }
            break;
        default:
            if (!isspace_fox(ch)) {
                throw_errorf(fs->mod_lang, "FormatError", "Invalid character %c in format string", ch);
                return FALSE;
            }
            break;
        }

        switch (type) {
        case PACK_INT: {
            Value *dst = (single ? vret : refarray_push(ra));
            if (!stream_unpack_int(dst, v, size, islower_fox(ch), sign)) {
                return FALSE;
            }
            if (single) {
                return TRUE;
            }
            break;
        }
        case PACK_FLOAT: {
            Value *dst = (single ? vret : refarray_push(ra));
            if (!stream_unpack_float(dst, *v, size, islower_fox(ch))) {
                return FALSE;
            }
            if (single) {
                return TRUE;
            }
            break;
        }
        case PACK_BIN: {
            Value *dst = (single ? vret : refarray_push(ra));
            if (!stream_unpack_bin(dst, *v, size)) {
                return FALSE;
            }
            if (single) {
                return TRUE;
            }
            break;
        }
        case PACK_ZBIN: {
            StrBuf sb;
            RefStr *rs;
            Value *dst = (single ? vret : refarray_push(ra));

            StrBuf_init_refstr(&sb, 16);
            if (!stream_gets_sub(&sb, *v, '\0')) {
                StrBuf_close(&sb);
                return FALSE;
            }
            *dst = StrBuf_str_Value(&sb, fs->cls_bytes);
            rs = Value_vp(*dst);
            if (rs->size > 0 && rs->c[rs->size] == '\0') {
                rs->size--;
            }
            if (single) {
                return TRUE;
            }
            break;
        }
        case PACK_NIL: { // 空読み
            int read_size = size;
            if (!stream_read_data(*v, NULL, NULL, &read_size, FALSE, TRUE)) {
                return FALSE;
            }
            break;
        }
        default:
            break;
        }
    }

    return TRUE;
}
static int stream_flush(Value *vret, Value *v, RefNode *node)
{
    if (!stream_flush_sub(*v)) {
        return FALSE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

StrBuf *bytesio_get_strbuf(Value v)
{
    RefBytesIO *mb = Value_vp(v);
    return &mb->buf;
}

RefBytesIO *bytesio_new_sub(const char *src, int size)
{
    RefBytesIO *mb = buf_new(fs->cls_bytesio, sizeof(RefBytesIO));
    StrBuf_init(&mb->buf, size);
    StrBuf_add(&mb->buf, src, size);
    mb->cur = 0;
    return mb;
}
static int bytesio_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *src;

    if (fg->stk_top > v + 1) {
        src = Value_vp(v[1]);
    } else {
        src = fs->str_0;
    }
    *vret = vp_Value(bytesio_new_sub(src->c, src->size));

    return TRUE;
}
static int bytesio_new_sized(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb;
    int64_t size = Value_int64(v[1], NULL);
    int val = 0;

    if (size < 0) {
        throw_errorf(fs->mod_lang, "ValueError", "Negative value is not allowed");
        return FALSE;
    }
    if (size > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        val = Value_int64(v[2], NULL);
        if (val < 0 || val > 255) {
            throw_errorf(fs->mod_lang, "ValueError", "Invalid value (0 - 255)");
            return FALSE;
        }
    }

    mb = buf_new(fs->cls_bytesio, sizeof(RefBytesIO));
    *vret = vp_Value(mb);
    StrBuf_init(&mb->buf, size);
    mb->buf.size = size;
    mb->cur = 0;
    memset(mb->buf.p, val, size);

    return TRUE;
}
static int bytesio_dispose(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    StrBuf_close(&mb->buf);

    return TRUE;
}

static int bytesio_size(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    int size = mb->buf.size;
    *vret = int32_Value(size);

    return TRUE;
}
static int bytesio_set_size(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    int64_t size = Value_int64(v[1], NULL);

    if (size < 0) {
        throw_errorf(fs->mod_lang, "ValueError", "size must be a positive value");
        goto ERROR_END;
    } else if (size > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        goto ERROR_END;
    }

    if (size > mb->buf.size) {
        // サイズを増やした場合は0で埋める
        int size_old = mb->buf.size;
        StrBuf_alloc(&mb->buf, size);
        memset(mb->buf.p + size_old, 0, size - size_old);
    }
    mb->buf.size = size;

    return TRUE;
ERROR_END:
    return FALSE;
}
static int bytesio_empty(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    int size = mb->buf.size;
    *vret = bool_Value(size == 0);

    return TRUE;
}

/**
 * 配列のようにアクセス
 */
static int bytesio_index(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    int size = mb->buf.size;
    int64_t idx = Value_int64(v[1], NULL);

    if (idx < 0) {
        idx += size;
    }
    if (idx < 0 || idx >= size) {
        throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], size);
        return FALSE;
    }
    *vret = int32_Value(mb->buf.p[idx] & 0xFF);

    return TRUE;
}
static int bytesio_index_set(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    int size = mb->buf.size;
    int64_t idx = Value_int64(v[1], NULL);
    int64_t val = Value_int64(v[2], NULL);

    if (idx < 0) {
        idx += size;
    }
    if (idx < 0 || idx >= size) {
        throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], size);
        return FALSE;
    }
    if (val < 0 || val > 255) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid value (must be 0 - 255)");
        return FALSE;
    }
    mb->buf.p[idx] = val;

    return TRUE;
}
static int bytesio_fill(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    int size = mb->buf.size;
    int64_t val = Value_int64(v[1], NULL);
    int64_t begin = Value_int64(v[2], NULL);
    int64_t end = Value_int64(v[3], NULL);

    if (begin > end) {
        int tmp = end;
        end = begin;
        begin = tmp;
    }
    if (begin < 0) {
        begin = 0;
    }
    if (end >= size) {
        end = size - 1;
    }
    if (begin < end) {
        memset(mb->buf.p + begin, val, end - begin);
    }
    return TRUE;
}
static int bytesio_read(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    const char *str_p = mb->buf.p;
    int str_size = mb->buf.size;

    if (mb->cur < str_size) {
        int64_t size;

        if (fg->stk_top > fg->stk_base + 1) {
            size = Value_int64(v[1], NULL);
            if (size > fs->max_alloc) {
                throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
                return FALSE;
            } else if (size < 0) {
                throw_errorf(fs->mod_lang, "ValueError", "Invalid size parameter");
                return FALSE;
            }
        } else {
            size = fs->max_alloc;
        }
        if (size > str_size - mb->cur) {
            size = str_size - mb->cur;
        }

        if (FUNC_INT(node)) {
            *vret = vp_Value(bytesio_new_sub(str_p + mb->cur, size));
        } else {
            *vret = cstr_Value(fs->cls_bytes, str_p + mb->cur, size);
        }
        mb->cur += size;
    }

    return TRUE;
}
static int bytesio_gets_sub(Str *result, Value v)
{
    RefBytesIO *mb = Value_vp(v);
    const char *str_p = mb->buf.p;
    int str_size = mb->buf.size;

    if (mb->cur >= str_size) {
        return FALSE;
    } else {
        int pos = mb->cur;
        while (pos < str_size && str_p[pos] != '\n') {
            pos++;
        }
        if (pos < str_size) {
            pos++;
        }
        result->p = &str_p[mb->cur];
        result->size = pos - mb->cur;
        mb->cur = pos;
    }

    return TRUE;
}
static int bytesio_gets(Value *vret, Value *v, RefNode *node)
{
    Str result;
    int ret = TRUE;

    if (!bytesio_gets_sub(&result, *v)) {
        result.size = 0;
    }
    if (result.size > 0) {
        *vret = cstr_Value(fs->cls_bytes, result.p, result.size);
    }

    return ret;
}

/**
 * sb <- v1
 * BytesIO <- Stream (Not BytesIO)
 */
static int bytesio_write_stream_sub(StrBuf *sb, Value v1, int64_t last)
{
    for (;;) {
        int read_size = (last > BUFFER_SIZE ? BUFFER_SIZE : last);
        if (!stream_read_data(v1, sb, NULL, &read_size, FALSE, FALSE)) {
            return FALSE;
        }
        // r1が終端に達した
        if (read_size <= 0) {
            break;
        }
        last -= read_size;
        if (last <= 0) {
            break;
        }
    }
    return TRUE;
}
static int bytesio_write(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    RefStr *rs = Value_vp(v[1]);

    if (!StrBuf_add_r(&mb->buf, rs)) {
        return FALSE;
    }
    *vret = int32_Value(rs->size);

    return TRUE;
}
static int bytesio_write_s(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    Value v1 = v[1];
    RefNode *v1_type = Value_type(v1);

    if (v1_type == fs->cls_bytesio) {
        RefBytesIO *bio = Value_vp(v1);
        int size = bio->buf.size;
        if (fg->stk_top > v + 2) {
            int64_t limit = Value_int64(v[2], NULL);
            if (size > limit) {
                size = limit;
            }
        }
        if (!StrBuf_add(&mb->buf, bio->buf.p, size)) {
            goto ERROR_END;
        }
    } else {
        int64_t limit = INT64_MAX;
        if (fg->stk_top > v + 2) {
            limit = Value_int64(v[2], NULL);
        }
        if (!bytesio_write_stream_sub(&mb->buf, v1, limit)) {
            goto ERROR_END;
        }
    }

    return TRUE;

ERROR_END:
    return FALSE;
}
static int bytesio_pack(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    RefStr *fmt = Value_vp(v[1]);
    int argc = (fg->stk_top - fg->stk_base) - 1;

    if (!stream_pack_sub(v, &mb->buf, argc, fmt->c, fmt->size)) {
        return FALSE;
    }
    return TRUE;
}
static int bytesio_data(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    const char *str_p = mb->buf.p;
    int str_size = mb->buf.size;
    int begin = 0;
    int end = str_size;

    if (fg->stk_top > v + 1) {
        begin = Value_int64(v[1], NULL);
        if (begin < 0) {
            begin += str_size;
        }
        if (begin < 0) {
            begin = 0;
        } else if (begin > str_size) {
            begin = str_size;
        }
    }
    if (fg->stk_top > v + 2) {
        end = Value_int64(v[2], NULL);
        if (end < 0) {
            end += str_size;
        }
        if (end < begin) {
            end = begin;
        } else if (end > str_size) {
            end = str_size;
        }
    }

    str_size = end - begin;
    str_p += begin;
    *vret = cstr_Value(fs->cls_bytes, str_p, str_size);

    return TRUE;
}
static int bytesio_dup(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    *vret = vp_Value(bytesio_new_sub(mb->buf.p, mb->buf.size));
    return TRUE;
}
static int bytesio_pos(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    *vret = int32_Value(mb->cur);
    return TRUE;
}
static int bytes_seek_sub(Value v, int64_t offset)
{
    RefBytesIO *mb = Value_vp(v);

    if (offset >= 0 && offset < mb->buf.size) {
        mb->cur = offset;
    } else {
        throw_errorf(fs->mod_io, "ReadError", "Seek position out of range");
        return FALSE;
    }
    return TRUE;
}
static int bytesio_set_pos(Value *vret, Value *v, RefNode *node)
{
    int64_t offset = Value_int64(v[1], NULL);
    if (!bytes_seek_sub(*v, offset)) {
        return FALSE;
    }
    return TRUE;
}
static int bytesio_clear(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    mb->buf.size = 0;
    mb->cur = 0;
    return TRUE;
}
static int validate_arguments_as_char(Value *v1, int argc)
{
    int i;
    for (i = 0; i < argc; i++) {
        Value vp = v1[i];
        int32_t val;
        RefNode *type = Value_type(vp);
        if (type != fs->cls_int) {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_int, type, i + 1);
            return FALSE;
        }
        if (!Value_isint(vp)) {
            goto ERROR_END;
        }
        val = Value_integral(vp);
        if (val < 0 || val > 255) {
            goto ERROR_END;
        }
    }
    return TRUE;
ERROR_END:
    throw_errorf(fs->mod_lang, "ValueError", "Out of range (0-255)");
    return FALSE;
}
/**
 * 要素の後ろに追加する
 * 複数追加可能
 */
static int bytesio_push(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    Value *v1 = v + 1;
    int argc = fg->stk_top - v1;
    int prev_size = mb->buf.size;
    int i;

    if (!validate_arguments_as_char(v1, argc)) {
        return FALSE;
    }

    StrBuf_alloc(&mb->buf, prev_size + argc);
    for (i = 0; i < argc; i++) {
        mb->buf.p[prev_size + i] = Value_integral(v1[i]);
    }

    return TRUE;
}
/**
 * 末尾の要素を除去
 * 取り除いた値を返す
 * 配列の長さが0の場合は例外
 */
static int bytesio_pop(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);

    if (mb->buf.size >= 1) {
        *vret = int32_Value(mb->buf.p[mb->buf.size - 1] & 0xFF);
        mb->buf.size--;
    } else {
        throw_errorf(fs->mod_lang, "IndexError", "BytesIO has no data.");
        return FALSE;
    }
    return TRUE;
}
/**
 * 要素の前に追加する
 * 複数追加可能
 */
static int bytesio_unshift(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    Value *v1 = v + 1;
    int argc = fg->stk_top - fg->stk_base - 1;
    int prev_size = mb->buf.size;
    int i;

    if (!validate_arguments_as_char(v1, argc)) {
        return FALSE;
    }

    StrBuf_alloc(&mb->buf, prev_size + argc);
    memmove(&mb->buf.p[argc], &mb->buf.p[0], prev_size);
    for (i = 0; i < argc; i++) {
        mb->buf.p[i] = Value_integral(v1[i]);
    }

    return TRUE;
}
/**
 * 配列の先頭の要素を除去
 * 取り除いた値を返す
 * 配列の長さが0の場合は例外
 */
static int bytesio_shift(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);

    if (mb->buf.size >= 1) {
        *vret= int32_Value(mb->buf.p[0] & 0xFF);
        mb->buf.size--;
        memmove(&mb->buf.p[0], &mb->buf.p[1], mb->buf.size);
    } else {
        throw_errorf(fs->mod_lang, "IndexError", "BytesIO has no elements.");
        return FALSE;
    }
    return TRUE;
}
static int bytesio_splice(Value *vret, Value *v, RefNode *node)
{
    RefBytesIO *mb = Value_vp(*v);
    int start, len;
    Str append;
    char ch;

    if (fg->stk_top > v + 3) {
        RefNode *v3_type = Value_type(v[3]);
        if (v3_type == fs->cls_bytes) {
            append = Value_str(v[3]);
        } else if (v3_type == fs->cls_int) {
            int64_t n = Value_int64(v[3], NULL);
            if (n < 0 || n > 255) {
                throw_errorf(fs->mod_lang, "ValueError", "Invalid value (must be 0 - 255)");
                return FALSE;
            }
            ch = n;
            append.p = &ch;
            append.size = 1;
        } else {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_bytes, fs->cls_int, v3_type, 3);
            return FALSE;
        }
    } else {
        append.p = NULL;
        append.size = 0;
    }
    calc_splice_position(&start, &len, mb->buf.size, v);
    *vret = cstr_Value(fs->cls_bytes, mb->buf.p + start, len);

    if (append.size > len) {
        StrBuf_alloc(&mb->buf, mb->buf.size + append.size - len);
        memmove(mb->buf.p + start + append.size, mb->buf.p + start + len, mb->buf.size - start - append.size);
        memcpy(mb->buf.p + start, append.p, append.size);
    } else {
        if (append.size < len) {
            memmove(mb->buf.p + start + append.size, mb->buf.p + start + len, mb->buf.size - start - len);
        }
        memcpy(mb->buf.p + start, append.p, append.size);
        mb->buf.size = mb->buf.size + append.size - len;
    }

    return TRUE;
}

static int io_pack(Value *vret, Value *v, RefNode *node)
{
    StrBuf sb;
    RefStr *fmt = Value_vp(v[1]);
    int argc = (fg->stk_top - fg->stk_base) - 1;

    StrBuf_init_refstr(&sb, 0);
    if (!stream_pack_sub(v, &sb, argc, fmt->c, fmt->size)) {
        return FALSE;
    }
    *vret = StrBuf_str_Value(&sb, fs->cls_bytes);

    return TRUE;
}
int stream_seek(Value v, int64_t offset)
{
    if (Value_type(v) == fs->cls_bytesio) {
        return bytes_seek_sub(v, offset);
    } else {
        return stream_seek_sub(v, offset);
    }
}

////////////////////////////////////////////////////////////////////////////////

static int nullio_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = ref_new(fv->cls_nullio);
    *vret = vp_Value(r);
    init_stream_ref(r, STREAM_READ | STREAM_WRITE);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

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

static int textio_print_sub(Value v_textio, StrBuf *sb, Value v, RefLocale *loc)
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
        char *c_buf = malloc(mp_radix_size(&mp->mp, 10) + 2);
        mp_toradix(&mp->mp, (unsigned char *)c_buf, 10);
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
static int textio_printf_sub(Value v, StrBuf *sb, Str fmt_src, int start_arg, RefLocale *loc, RefResource *res)
{
    const char *p = fmt_src.p;
    const char *end = p + fmt_src.size;

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
                Value val_tmp;

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

                // 引数から見つからない場合は
                if (val == NULL && res != NULL) {
                    // Resourceから取り出す
                    Str s;
                    if (Resource_get(&s, res, key)) {
                        val_tmp = cstr_Value(fs->cls_str, s.p, s.size);
                        val = &val_tmp;
                    }
                }
                if (val != NULL) {
                    if (fmt.size > 0) {
                        // to_strを取り出す
                        RefNode *type = Value_type(*val);
                        RefNode *fn = Hash_get_p(&type->u.c.h, fs->str_tostr);
                        RefStr *vs;

                        Value_push("vS", *val, fmt);
                        if (loc != NULL) {
                            Value_push("r", loc);
                        }
                        if (!call_function(fn, (loc != NULL ? 2 : 1))) {
                            return FALSE;
                        }

                        vs = Value_vp(fg->stk_top[-1]);
                        if (!stream_write_sub_s(stream, sb, vs->c, vs->size, tio)) {
                            return FALSE;
                        }
                        Value_pop();
                    } else {
                        if (!textio_print_sub(v, sb, *val, loc)) {
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
    Str s_fmt;
    RefLocale *loc;
    RefResource *res = NULL;
    int start_arg;

    if (v1_type == fs->cls_str) {
        loc = NULL;
        s_fmt = Value_str(v[1]);
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
        loc = Value_vp(v[1]);
        s_fmt = Value_str(v[2]);
        start_arg = 3;
    } else if (v1_type == fs->cls_resource) {
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
        res = Value_vp(v[1]);
        if (!Resource_get(&s_fmt, res, Value_str(v[2]))) {
            return FALSE;
        }
        loc = res->locale;
        start_arg = 3;
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_locale, v1_type, 1);
        return FALSE;
    }

    if (str) {
        // sprintf
        StrBuf buf;
        StrBuf_init(&buf, 0);
        if (!textio_printf_sub(VALUE_NULL, &buf, s_fmt, start_arg, loc, res)) {
            return FALSE;
        }
        *vret = cstr_Value(fs->cls_str, buf.p, buf.size);
        StrBuf_close(&buf);
    } else {
        if (!textio_printf_sub(*v, NULL, s_fmt, start_arg, loc, res)) {
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
    Str s = Str_new(mb->buf.p, mb->buf.size);
    int begin;
    int end;

    string_substr_position(&begin, &end, s, v);
    s.size = end - begin;
    s.p += begin;
    *vret = cstr_Value(fs->cls_str, s.p, s.size);

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
static int get_stdio(Value *vret, Value *v, RefNode *node)
{
    *vret = Value_cp(fg->v_cio);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

static void define_io_const(RefNode *m)
{
    RefNode *n;
//  Ref *r;

    n = define_identifier(m, m, "STDIO", NODE_CONST_U_N, 0);
    define_native_func_a(n, get_stdio, 0, 0, NULL);
}

static void define_io_func(RefNode *m)
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

    n = define_identifier(m, m, "pack", NODE_FUNC_N, 0);
    define_native_func_a(n, io_pack, 1, -1, NULL, fs->cls_str);
}
static void define_io_class(RefNode *m)
{
    RefNode *cls, *cls2;
    RefNode *n;

    str__read = intern("_read", -1);
    str__write = intern("_write", -1);
    str__seek = intern("_seek", -1);
    str__close = intern("_close", -1);


    // StreamIO
    // バイト単位での入出力
    cls = fs->cls_streamio;
    cls->u.c.n_memb = INDEX_STREAM_NUM;
    n = define_identifier(m, cls, "_reader", NODE_NEW_N, 0);
    define_native_func_a(n, stream_open, 0, 0, (void*)STREAM_READ);
    n = define_identifier(m, cls, "_writer", NODE_NEW_N, 0);
    define_native_func_a(n, stream_open, 0, 0, (void*)STREAM_WRITE);
    n = define_identifier(m, cls, "_both", NODE_NEW_N, 0);
    define_native_func_a(n, stream_open, 0, 0, (void*)(STREAM_READ|STREAM_WRITE));

    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, stream_close, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_read, NODE_FUNC_N, 0);
    define_native_func_a(n, stream_read, 0, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "gets", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_gets, 0, 0, NULL);
    n = define_identifier(m, cls, "fetch", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_fetch, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->str_write, NODE_FUNC_N, 0);
    define_native_func_a(n, stream_write, 1, 1, NULL, fs->cls_bytes);
    n = define_identifier(m, cls, "write_s", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_write_s, 1, 2, NULL, fs->cls_streamio, fs->cls_int);
    n = define_identifier(m, cls, "pos", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, stream_pos, 0, 0, NULL);
    n = define_identifier(m, cls, "pos=", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_set_pos, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "buffering", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, stream_get_buffering, 0, 0, NULL);
    n = define_identifier(m, cls, "buffering=", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_set_buffering, 1, 1, NULL, fs->cls_bool);
    n = define_identifier(m, cls, "pack", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_pack, 1, -1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "unpack", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_unpack, 1, 1, (void*)FALSE, fs->cls_str);
    n = define_identifier(m, cls, "peek", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_unpack, 1, 1, (void*)TRUE, fs->cls_str);
    n = define_identifier(m, cls, "flush", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_flush, 0, 0, NULL);
    n = define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    define_native_func_a(n, stream_close, 0, 0, NULL);
    extends_method(cls, fs->cls_obj);


    // NullIO(R,W) なにもしない
    cls = define_identifier(m, m, "NullIO", NODE_CLASS, 0);
    fv->cls_nullio = cls;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, nullio_new, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, nullio_new, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, native_return_bool, 0, 0, (void*)TRUE);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_null, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, str__read, NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_null, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = define_identifier_p(m, cls, str__write, NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_null, 1, 1, NULL, fs->cls_bytesio);
    cls->u.c.n_memb = fs->cls_streamio->u.c.n_memb;
    extends_method(cls, fs->cls_streamio);


    // BytesIO(R,W)
    cls = fs->cls_bytesio;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, bytesio_new, 0, 1, NULL, fs->cls_bytes);
    n = define_identifier(m, cls, "sized", NODE_NEW_N, 0);
    define_native_func_a(n, bytesio_new_sized, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_dispose, 0, 0, NULL);
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, bytesio_empty, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_index, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_index_set, 2, 2, NULL, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "fill", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_fill, 3, 3, NULL, fs->cls_int, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, bytesio_size, 0, 0, NULL);
    n = define_identifier(m, cls, "size=", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_set_size, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "push", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_push, 1, -1, NULL, NULL);
    n = define_identifier(m, cls, "pop", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_pop, 0, 0, NULL, NULL);
    n = define_identifier(m, cls, "unshift", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_unshift, 1, -1, NULL, NULL);
    n = define_identifier(m, cls, "shift", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_shift, 0, 0, NULL, NULL);
    n = define_identifier(m, cls, "splice", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_splice, 0, 3, NULL, fs->cls_int, fs->cls_int, NULL);
    n = define_identifier(m, cls, "data", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, bytesio_data, 0, 0, NULL);
    n = define_identifier(m, cls, "sub", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_data, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_dup, 0, 0, NULL);
    n = define_identifier(m, cls, "pos", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, bytesio_pos, 0, 0, NULL, NULL);
    n = define_identifier(m, cls, "pos=", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_set_pos, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->str_read, NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_read, 0, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "gets", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_gets, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_write, NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_write, 1, 1, NULL, fs->cls_bytes);
    n = define_identifier(m, cls, "write_s", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_write_s, 1, 2, NULL, fs->cls_streamio, fs->cls_int);
    n = define_identifier(m, cls, "pack", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_pack, 1, -1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "flush", NODE_FUNC_N, 0);
    define_native_func_a(n, native_return_null, 0, 0, NULL); // 何もしない
    n = define_identifier(m, cls, "clear", NODE_FUNC_N, 0);
    define_native_func_a(n, bytesio_clear, 0, 0, NULL);
    extends_method(cls, fs->cls_streamio);


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


    cls = define_identifier(m, m, "IOError", NODE_CLASS, NODEOPT_ABSTRACT);
    define_error_class(cls, fs->cls_error, m);
    cls2 = cls;

    cls = define_identifier(m, m, "ReadError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "WriteError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "SocketError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "FileTypeError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
}

void init_io_module_stubs()
{
    RefNode *m = Module_new_sys("io");

    fs->cls_streamio = define_identifier(m, m, "StreamIO", NODE_CLASS, NODEOPT_ABSTRACT);
    fs->cls_bytesio = define_identifier(m, m, "BytesIO", NODE_CLASS, 0);
    fs->cls_textio = define_identifier(m, m, "TextIO", NODE_CLASS, NODEOPT_ABSTRACT);

    fv->cls_strio = define_identifier(m, m, "StrIO", NODE_CLASS, 0);

    fs->mod_io = m;
}
void init_io_module_1()
{
    RefNode *m = fs->mod_io;
    RefTextIO *tio = Mem_get(&fg->st_mem, sizeof(RefTextIO));

    tio->rh.type = NULL;
    tio->rh.nref = -1;
    tio->rh.n_memb = 0;
    tio->rh.weak_ref = NULL;
    tio->cs = fs->cs_utf8;
    tio->in.ic = (void*)-1;
    tio->out.ic = (void*)-1;
    tio->trans = TRUE;
    fv->ref_textio_utf8 = &tio->rh;

    define_io_class(m);
    define_io_func(m);
    define_io_const(m);

    m->u.m.loaded = TRUE;
}

