#define DEFINE_GLOBALS
#include "fox.h"
#include "fox_io.h"

#ifdef WIN32
#define LZMA_API_STATIC
#endif
#include <lzma.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


enum {
    INDEX_LZMA_STREAM = INDEX_STREAM_NUM,
    INDEX_LZMA_IN,
    INDEX_LZMA_OUT,
    INDEX_LZMA_IN_BUF,
    INDEX_LZMA_LEVEL,
    INDEX_LZMA_NUM,
};
enum {
    LZMA_MEM_LIMIT = 128 * 1024 * 1024,
};
enum {
    CHECK_MODE_LEVEL  = 0x0F,
    CHECK_MODE_MASK   = 0xF0,
    CHECK_MODE_CRC32  = 0x10,
    CHECK_MODE_CRC64  = 0x20,
    CHECK_MODE_SHA256 = 0x30,
};

static const FoxStatic *fs;
static FoxGlobal *fg;
static RefNode *mod_lzma;

static const char *error_code_to_str(int err)
{
    switch (err) {
    case LZMA_MEM_ERROR:
        return "Cannot allocate memory";
    case LZMA_MEMLIMIT_ERROR:
        return "Memory usage limit was reached";
    case LZMA_FORMAT_ERROR:
        return "File format not recognized";
    case LZMA_OPTIONS_ERROR:
        return "Invalid or unsupported options";
    case LZMA_DATA_ERROR:
        return "Data is corrupt";
    case LZMA_BUF_ERROR:
        return "No progress is possible";
    default:
        return "";
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////

static int lzmaio_new(Value *vret, Value *v, RefNode *node)
{
    int level = 6;
    int flags = 0;
    Value v1 = v[1];
    Ref *r = fs->ref_new(FUNC_VP(node));
    *vret = vp_Value(r);

    // modeの解析
    if (fg->stk_top > v + 2) {
        int lv = fs->Value_int64(v[3], NULL);
        if (lv < 0 || lv > 9) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal level value (0 - 9)");
            return FALSE;
        }
        level = lv;
    }
    if (fg->stk_top > v + 3) {
        RefStr *m = Value_vp(v[3]);
        if (str_eqi(m->c, m->size, "crc32", -1)) {
            level |= CHECK_MODE_CRC32;
        } else if (str_eqi(m->c, m->size, "crc64", -1)) {
            level |= CHECK_MODE_CRC64;
        } else if (str_eqi(m->c, m->size, "sha256", -1)) {
            level |= CHECK_MODE_SHA256;
        } else {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown check mode (crc32, crc64 or sha256)");
            return FALSE;
        }
    } else {
        level |= CHECK_MODE_CRC64;
    }

    if (fs->Value_type(v1) == fs->cls_bytesio) {
        flags = STREAM_READ|STREAM_WRITE;
    } else {
        Ref *r1 = Value_ref(v1);
        // 引数のStreamに応じて読み書き可能モードを設定する
        if (Value_integral(r1->v[INDEX_READ_MAX]) >= 0) {
            flags = STREAM_READ;
        }
        if (Value_integral(r1->v[INDEX_WRITE_MAX]) >= 0) {
            flags |= STREAM_WRITE;
        }
    }
    fs->init_stream_ref(r, flags);

    r->v[INDEX_LZMA_STREAM] = fs->Value_cp(v1);
    r->v[INDEX_LZMA_LEVEL] = int32_Value(level);

    return TRUE;
}

static void lzma_close_all(Ref *r)
{
    void *p_z_in = Value_ptr(r->v[INDEX_LZMA_IN_BUF]);
    lzma_stream *zs_in = Value_ptr(r->v[INDEX_LZMA_IN]);
    lzma_stream *zs_out = Value_ptr(r->v[INDEX_LZMA_OUT]);

    if (zs_in != NULL) {
        lzma_end(zs_in);
        free(zs_in);
        r->v[INDEX_LZMA_IN] = VALUE_NULL;
    }
    if (zs_out != NULL) {
        lzma_end(zs_out);
        free(zs_out);
        r->v[INDEX_LZMA_OUT] = VALUE_NULL;
    }
    if (p_z_in != NULL) {
        free(p_z_in);
        r->v[INDEX_LZMA_IN_BUF] = VALUE_NULL;
    }
}
static int lzma_init_write(Ref *r, char *out_buf, int out_len, Str data)
{
    lzma_stream *zs;
    int ret;
    int level = Value_integral(r->v[INDEX_LZMA_LEVEL]);
    int check;

    switch (level & CHECK_MODE_MASK) {
    case CHECK_MODE_CRC32:
        check = LZMA_CHECK_CRC32;
        break;
    case CHECK_MODE_CRC64:
        check = LZMA_CHECK_CRC64;
        break;
    case CHECK_MODE_SHA256:
        check = LZMA_CHECK_SHA256;
        break;
    default:
        check = LZMA_CHECK_CRC64;
        break;
    }
    level &= CHECK_MODE_LEVEL;

    zs = malloc(sizeof(lzma_stream));
    memset(zs, 0, sizeof(lzma_stream));

    zs->next_in = (uint8_t*)data.p;
    zs->avail_in = data.size;
    zs->next_out = (uint8_t*)out_buf;
    zs->avail_out = out_len;
    ret = lzma_easy_encoder(zs, level, check);

    if (ret != LZMA_OK) {
        fs->throw_errorf(mod_lzma, "LzmaError", "%s", error_code_to_str(ret));
        return FALSE;
    }

    r->v[INDEX_LZMA_OUT] = ptr_Value(zs);
    return TRUE;
}
static int lzmaio_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    lzma_stream *zs = Value_ptr(r->v[INDEX_LZMA_OUT]);
    int out_len = Value_integral(r->v[INDEX_WRITE_MAX]);
    char *mb_buf = malloc(out_len);

    if (zs != NULL) {
        zs->next_in = (uint8_t*)"";
        zs->avail_in = 0;
        zs->next_out = (uint8_t*)mb_buf;
        zs->avail_out = out_len;

        for (;;) {
            int ret = lzma_code(zs, LZMA_FINISH);

            if (ret == LZMA_OK || ret == LZMA_STREAM_END){   // 出力してバッファを空にする
                int size = out_len - zs->avail_out;

                if (!fs->stream_write_data(r->v[INDEX_LZMA_STREAM], mb_buf, size)) {
                    goto ERROR_END;
                }
                if (ret == LZMA_STREAM_END) {
                    break;
                }
                zs->next_out = (uint8_t*)mb_buf;
                zs->avail_out = out_len;
            } else {
                fs->throw_errorf(mod_lzma, "LzmaError", "%s", error_code_to_str(ret));
                goto ERROR_END;
            }
        }
        free(mb_buf);
    }
    lzma_close_all(r);
    return TRUE;

ERROR_END:
    free(mb_buf);
    return FALSE;
}
static int invoke_read(char *buf, int *size, Value v)
{
    RefNode *r_type;

    fs->Value_push("vd", v, BUFFER_SIZE);
    if (!fs->call_member_func(fs->str_read, 1, TRUE)) {
        return FALSE;
    }

    r_type = fs->Value_type(fg->stk_top[-1]);
    if (r_type == fs->cls_bytes) {
        RefStr *s = Value_vp(fg->stk_top[-1]);
        if (s->size <= BUFFER_SIZE) {
            memcpy(buf, s->c, s->size);
            *size = s->size;
        } else {
            memcpy(buf, s->c, BUFFER_SIZE);
            *size = BUFFER_SIZE;
        }
    } else {
        *size = 0;
    }

    return TRUE;
}
static int lzmaio_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    lzma_stream *zs = Value_ptr(r->v[INDEX_LZMA_IN]);
    uint8_t *buf = Value_ptr(r->v[INDEX_LZMA_IN_BUF]);
    RefBytesIO *mb = Value_vp(v[1]);
    int read_size = fs->Value_int64(v[2], NULL);
    char *dst_buf;
    int action;

    if (read_size <= 0) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Argument out of range");
        return FALSE;
    }

    dst_buf = mb->buf.p;

    if (zs == NULL) {
        int ret;
        int max;
        buf = malloc(BUFFER_SIZE);
        r->v[INDEX_LZMA_IN_BUF] = ptr_Value(buf);
        if (!invoke_read((char*)buf, &max, r->v[INDEX_LZMA_STREAM])) {
            return FALSE;
        }

        zs = malloc(sizeof(lzma_stream));
        memset(zs, 0, sizeof(lzma_stream));

        zs->next_in = (uint8_t*)buf;
        zs->avail_in = max;
        zs->next_out = (uint8_t*)dst_buf;
        zs->avail_out = read_size;

        ret = lzma_stream_decoder(zs, LZMA_MEM_LIMIT, 0);
        if (ret != LZMA_OK) {
            fs->throw_errorf(mod_lzma, "LzmaError", "%s", error_code_to_str(ret));
            return FALSE;
        }

        r->v[INDEX_LZMA_IN] = ptr_Value(zs);
    } else {
        zs->next_out = (uint8_t*)dst_buf;
        zs->avail_out = read_size;
    }

    action = LZMA_RUN;
    while (zs->avail_out > 0) {
        int ret = lzma_code(zs, action);
        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            fs->throw_errorf(mod_lzma, "LzmaError", "%s", error_code_to_str(ret));
            return FALSE;
        }

        if (action == LZMA_FINISH) {
            // 終わり
            break;
        } else if (zs->avail_out > 0) {
            int max;
            if (!invoke_read((char*)buf, &max, r->v[INDEX_LZMA_STREAM])) {
                return FALSE;
            }
            zs->next_in = (uint8_t*)buf;
            zs->avail_in = max;
            if (max == 0) {
                action = LZMA_FINISH;
                break;
            }
        }
    }
    mb->buf.size = read_size - zs->avail_out;
    return TRUE;
}
static int lzmaio_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    lzma_stream *zs = Value_ptr(r->v[INDEX_LZMA_OUT]);
    int out_len = Value_integral(r->v[INDEX_WRITE_MAX]);
    RefBytesIO *mb = Value_vp(v[1]);
    Str data = Str_new(mb->buf.p, mb->buf.size);
    char *mb_buf = malloc(out_len);

    if (zs == NULL) {
        if (!lzma_init_write(r, mb_buf, out_len, data)) {
            goto ERROR_END;
        }
        zs = Value_ptr(r->v[INDEX_LZMA_OUT]);
    } else {
        zs->next_in = (uint8_t*)data.p;
        zs->avail_in = data.size;
        zs->next_out = (uint8_t*)mb_buf;
        zs->avail_out = out_len;
    }

    for (;;) {  // 終端処理をしないので、入力がなくなったら終了
        int ret = lzma_code(zs, LZMA_RUN);

        if (zs->avail_in == 0 || ret == LZMA_OK || ret == LZMA_STREAM_END) {   // 出力してバッファを空にする
            int size = out_len - zs->avail_out;

            if (!fs->stream_write_data(r->v[INDEX_LZMA_STREAM], mb_buf, size)) {
                goto ERROR_END;
            }
            if (zs->avail_in == 0 || ret == LZMA_STREAM_END) {
                break;
            }
            zs->next_out = (uint8_t*)mb_buf;
            zs->avail_out = out_len;
        } else {
            fs->throw_errorf(mod_lzma, "LzmaError", "%s", error_code_to_str(ret));
            goto ERROR_END;
        }
    }

    free(mb_buf);
    *vret = int32_Value(data.size - zs->avail_in);
    return TRUE;

ERROR_END:
    free(mb_buf);
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;


    cls = fs->define_identifier(m, m, "LzmaIO", NODE_CLASS, 0);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, lzmaio_new, 1, 3, cls, fs->cls_streamio, fs->cls_int, fs->cls_str);

    n = fs->define_identifier(m, cls, "_close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, lzmaio_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, lzmaio_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, lzmaio_write, 1, 1, NULL, fs->cls_bytesio);

    cls->u.c.n_memb = INDEX_LZMA_NUM;
    fs->extends_method(cls, fs->cls_streamio);


    cls = fs->define_identifier(m, m, "LzmaError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_lzma = m;

    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    if (buf == NULL) {
        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\nXZ Util\t%d.%d.%d\n",
                LZMA_VERSION_MAJOR, LZMA_VERSION_MINOR, LZMA_VERSION_PATCH);
    }
    return buf;
}
