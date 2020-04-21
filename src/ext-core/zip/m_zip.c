#define DEFINE_GLOBALS
#include "zipfile.h"
#include "m_number.h"
#include "m_codecvt.h"
#include <string.h>
#include <stdlib.h>


enum {
    Z_STREAM_NONE,
    Z_STREAM_INFLATE,
    Z_STREAM_DEFLATE,
};
enum {
    Z_MODE_GZIP = 0x100,
};

static RefNode *cls_zipentry;
static RefNode *cls_zipentryiter;
static RefNode *cls_fileio;
static CodeCVTStatic *codecvt;


void CodeCVTStatic_init()
{
    if (codecvt == NULL) {
        RefNode *mod = fs->get_module_by_name("text.codecvt", -1, TRUE, TRUE);
        if (mod == NULL) {
            fs->fatal_errorf("Cannot load module 'text.codecvt'");
        }
        codecvt = mod->u.m.ext;
    }
}


static int get_reader_file_size(int *pret, Value reader)
{
    RefNode *ret_type;
    int64_t ret;

    fs->Value_push("v", reader);
    if (!fs->call_property(fs->intern("size", -1))) {
        return FALSE;
    }
    // 戻り値を除去
    ret_type = fs->Value_type(fg->stk_top[-1]);
    if (ret_type != fs->cls_int) {
        fs->throw_errorf(fs->mod_lang, "TypeError", "size returns not a Int value");
        return FALSE;
    }
    ret = fs->Value_int64(fg->stk_top[-1], NULL);
    fs->Value_pop();

    if (ret > INT32_MAX || ret < 0) {
        fs->throw_errorf(mod_zip, "ZipError", "Invalid zipfile format");
        return FALSE;
    }
    *pret = ret;

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int deflateio_new(Value *vret, Value *v, RefNode *node)
{
    int flags = 0;
    int level = 6; // default
    Value v1 = v[1];
    Ref *r = fs->ref_new(FUNC_VP(node));
    *vret = vp_Value(r);

    // modeの解析
    if (fg->stk_top > v + 3) {
        int lv = fs->Value_int64(v[3], NULL);
        if (lv < 0 || lv > 9) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal level value %v (0 - 9)", v[3]);
            return FALSE;
        }
        level = lv;
    }
    if (fg->stk_top > v + 2) {
        RefStr *m = Value_vp(v[2]);
        if (str_eqi(m->c, m->size, "d", -1) || str_eqi(m->c, m->size, "deflate", -1)) {
        } else if (str_eqi(m->c, m->size, "g", -1) || str_eqi(m->c, m->size, "gzip", -1)) {
            level |= Z_MODE_GZIP;
        } else {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown deflate mode %q (deflate or gzip)", m->c);
            return FALSE;
        }
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

    r->v[INDEX_Z_STREAM] = fs->Value_cp(v1);
    r->v[INDEX_Z_LEVEL] = int32_Value(level);

    return TRUE;
}

static void deflateio_close_all(Ref *r)
{
    void *p_z_in = Value_ptr(r->v[INDEX_Z_IN_BUF]);
    z_stream *zs_in = Value_ptr(r->v[INDEX_Z_IN]);
    z_stream *zs_out = Value_ptr(r->v[INDEX_Z_OUT]);

    if (zs_in != NULL) {
        inflateEnd(zs_in);
        free(zs_in);
        r->v[INDEX_Z_IN] = VALUE_NULL;
    }
    if (zs_out != NULL) {
        deflateEnd(zs_out);
        free(zs_out);
        r->v[INDEX_Z_OUT] = VALUE_NULL;
    }
    if (p_z_in != NULL) {
        free(p_z_in);
        r->v[INDEX_Z_IN_BUF] = VALUE_NULL;
    }
}
static int deflateio_init_write(Ref *r, char *out_buf, int out_len, Str data)
{
    z_stream *zs;
    int ret;
    int level = Value_integral(r->v[INDEX_Z_LEVEL]);
    int window_bits = MAX_WBITS;

    if ((level & Z_MODE_GZIP) != 0) {
        level &= ~Z_MODE_GZIP;
        window_bits += 16;
    }

    zs = malloc(sizeof(z_stream));
    memset(zs, 0, sizeof(z_stream));

    zs->next_in = (Bytef*)data.p;
    zs->avail_in = data.size;
    zs->next_out = (Bytef*)out_buf;
    zs->avail_out = out_len;
    ret = deflateInit2(zs, level, Z_DEFLATED, window_bits, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        fs->throw_errorf(mod_zip, "DeflateError", "%s", zError(ret));
        return FALSE;
    }

    r->v[INDEX_Z_OUT] = ptr_Value(zs);
    return TRUE;
}
static int deflateio_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    z_stream *zs = Value_ptr(r->v[INDEX_Z_OUT]);
    int out_len = Value_integral(r->v[INDEX_WRITE_MAX]);
    char *mb_buf = malloc(out_len);

    if (zs != NULL) {
        zs->next_in = (Bytef*)"";
        zs->avail_in = 0;
        zs->next_out = (Bytef*)mb_buf;
        zs->avail_out = out_len;

        for (;;) {
            int ret = deflate(zs, Z_FINISH);

            if (ret == Z_OK || ret == Z_BUF_ERROR || ret == Z_STREAM_END){   // 出力してバッファを空にする
                int size = out_len - zs->avail_out;

                if (!fs->stream_write_data(r->v[INDEX_Z_STREAM], mb_buf, size)) {
                    goto ERROR_END;
                }
                if (ret == Z_STREAM_END) {
                    break;
                }
                zs->next_out = (Bytef*)mb_buf;
                zs->avail_out = out_len;
            } else {
                fs->throw_errorf(mod_zip, "DeflateError", "%s", zs->msg);
                goto ERROR_END;
            }
        }
        free(mb_buf);
    }
    deflateio_close_all(r);

    return TRUE;

ERROR_END:
    free(mb_buf);
    return FALSE;
}
// 圧縮しながら書き込み
static int deflateio_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    z_stream *zs = Value_ptr(r->v[INDEX_Z_OUT]);
    int out_len = Value_integral(r->v[INDEX_WRITE_MAX]);
    RefBytesIO *mb = Value_vp(v[1]);
    Str data = Str_new(mb->buf.p, mb->buf.size);
    char *mb_buf = malloc(out_len);

    if (zs == NULL) {
        if (!deflateio_init_write(r, mb_buf, out_len, data)) {
            goto ERROR_END;
        }
        zs = Value_ptr(r->v[INDEX_Z_OUT]);
    } else {
        zs->next_in = (Bytef*)data.p;
        zs->avail_in = data.size;
        zs->next_out = (Bytef*)mb_buf;
        zs->avail_out = out_len;
    }

    for (;;) {  // 終端処理をしないので、入力がなくなったら終了
        int ret = deflate(zs, Z_NO_FLUSH);

        if (zs->avail_in == 0 || ret == Z_BUF_ERROR || ret == Z_STREAM_END) {   // 出力してバッファを空にする
            int size = out_len - zs->avail_out;

            if (size > 0 && !fs->stream_write_data(r->v[INDEX_Z_STREAM], mb_buf, size)) {
                goto ERROR_END;
            }
            if (zs->avail_in == 0 || ret == Z_STREAM_END) {
                break;
            }
            zs->next_out = (Bytef*)mb_buf;
            zs->avail_out = out_len;
        } else if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", zs->msg);
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

static int invoke_read(char *buf, int *size, Value v)
{
    static RefStr *str_read;
    RefNode *r_type;

    if (str_read == NULL) {
        str_read = fs->intern("read", -1);
    }
    fs->Value_push("vd", v, BUFFER_SIZE);
    if (!fs->call_member_func(str_read, 1, TRUE)) {
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
    fs->Value_pop();

    return TRUE;
}

// 展開して読み込み
static int deflateio_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    z_stream *zs = Value_ptr(r->v[INDEX_Z_IN]);
    Bytef *buf = Value_ptr(r->v[INDEX_Z_IN_BUF]);
    RefBytesIO *mb = Value_vp(v[1]);
    int64_t read_size = fs->Value_int64(v[2], NULL);
    char *dst_buf;

    if (read_size <= 0) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Argument out of range");
        return FALSE;
    }

    dst_buf = mb->buf.p;

    if (zs == NULL) {
        int ret;
        int max;
        buf = malloc(BUFFER_SIZE);
        r->v[INDEX_Z_IN_BUF] = ptr_Value(buf);
        if (!invoke_read((char*)buf, &max, r->v[INDEX_Z_STREAM])) {
            return FALSE;
        }

        zs = malloc(sizeof(z_stream));
        memset(zs, 0, sizeof(z_stream));

        zs->next_in = (Bytef*)buf;
        zs->avail_in = max;
        zs->next_out = (Bytef*)dst_buf;
        zs->avail_out = read_size;
        // auto detect deflate or gzip
        ret = inflateInit2(zs, MAX_WBITS + 16 + 16);
        if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", zError(ret));
            return FALSE;
        }
        r->v[INDEX_Z_IN] = ptr_Value(zs);
    } else {
        zs->next_out = (Bytef*)dst_buf;
        zs->avail_out = read_size;
    }
    while (zs->avail_out > 0) {
        int ret = inflate(zs, Z_NO_FLUSH);

        if (ret == Z_STREAM_END) {
            // 終わり
            break;
        } else if (ret == Z_BUF_ERROR) {
            int max;
            if (!invoke_read((char*)buf, &max, r->v[INDEX_Z_STREAM])) {
                return FALSE;
            }
            zs->next_in = (Bytef*)buf;
            zs->avail_in = max;
            if (max == 0) {
                break;
            }
        } else if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", zs->msg);
            return FALSE;
        }
    }
    mb->buf.size = read_size - zs->avail_out;

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @argument #2
 * ファイル名にUTF8フラグが付いていない場合のエンコーディング
 * 省略時はUTF-8とみなす
 *
 * @argument #3
 * 拡張フィールドにUTCが設定されていない場合は、DOSTIMEをUTCに変換するが
 * その時のタイムゾーンを指定する
 * 省略時はUTCとみなす
 */
static int zip_randomreader_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_zipreader = FUNC_VP(node);
    Value reader;
    CentralDirEnd *cdir = NULL;
    RefCharset *cs;
    RefTimeZone *tz;
    int file_size;

    Value v1 = v[1];
    Ref *r = fs->ref_new(cls_zipreader);
    *vret = vp_Value(r);

    if (fg->stk_top > v + 2) {
        cs = Value_vp(v[2]);
    } else {
        cs = fs->cs_utf8;
    }
    if (fg->stk_top > v + 3) {
        tz = Value_vp(v[3]);
    } else {
        tz = fs->get_local_tz();
    }

    if (!fs->value_to_streamio(&reader, v1, FALSE, 0, FALSE)) {
        return FALSE;
    }
    if (!get_reader_file_size(&file_size, reader)) {
        return FALSE;
    }
    cdir = get_central_dir(reader, file_size, cs, tz);

    if (cdir == NULL) {
        fs->unref(reader);
        return FALSE;
    }
    r->v[INDEX_ZIPREADER_READER] = reader;
    r->v[INDEX_ZIPREADER_CDIR] = ptr_Value(cdir);
    r->v[INDEX_ZIPREADER_CHARSET] = vp_Value(cs);
    r->v[INDEX_ZIPREADER_TZ] = vp_Value(tz);

    return TRUE;
}
static int zip_randomreader_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDirEnd *cdir = Value_ptr(r->v[INDEX_ZIPREADER_CDIR]);

    if (cdir != NULL) {
        CentralDirEnd_free(cdir);
        r->v[INDEX_ZIPREADER_CDIR] = VALUE_NULL;
    }

    return TRUE;
}
static int zip_randomreader_size(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDirEnd *cdir = Value_ptr(r->v[INDEX_ZIPREADER_CDIR]);

    if (cdir == NULL) {
        fs->throw_errorf(mod_zip, "ZipError", "Already closed");
        return FALSE;
    }
    *vret = int32_Value(cdir->cdir_size);
    return TRUE;
}

static void zipentry_new_sub(Value *vret, Value v, CentralDir *cd)
{
    CentralDir *p = malloc(sizeof(CentralDir));
    Ref *r = fs->ref_new(cls_zipentry);
    *vret = vp_Value(r);

    *p = *cd;
    fs->StrBuf_init(&p->filename, cd->filename.size);
    fs->StrBuf_add(&p->filename, cd->filename.p, cd->filename.size);

    fs->init_stream_ref(r, STREAM_READ);
    r->v[INDEX_ZIPENTRY_CDIR] = ptr_Value(p);
    r->v[INDEX_ZIPENTRY_REF] = fs->Value_cp(v);
}

/**
 * インデックスで検索して見つかればZipEntry、見つからなければIndexError
 * ファイル名で検索して見つかればZipEntry、見つからなければnullを返す
 */
static int zip_randomreader_index(Value *vret, Value *v, RefNode *node)
{
    RefNode *v_type = fs->Value_type(v[1]);

    if (v_type == fs->cls_int) {
        Ref *r = Value_ref(*v);
        CentralDirEnd *cdir = Value_ptr(r->v[INDEX_ZIPREADER_CDIR]);
        int64_t idx = fs->Value_int64(v[1], NULL);

        if (cdir == NULL) {
            fs->throw_errorf(mod_zip, "ZipError", "Already closed");
            return FALSE;
        }
        if (idx < 0) {
            idx += cdir->cdir_size;
        }
        if (idx < 0 || idx >= cdir->cdir_size) {
            fs->throw_errorf(fs->mod_lang, "IndexError", "Invalid array index number");
            return FALSE;
        }
        zipentry_new_sub(vret, *v, &cdir->cdir[idx]);
    } else if (v_type == fs->cls_str || v_type == fs->cls_bytes) {
        Ref *r = Value_ref(*v);
        CentralDirEnd *cdir = Value_ptr(r->v[INDEX_ZIPREADER_CDIR]);
        RefStr *name = Value_vp(v[1]);
        StrBuf sb;
        int i;
        int local_str = FALSE;

        fs->StrBuf_init(&sb, 0);
        // 文字コードを変換
        if (v_type == fs->cls_str && cdir->cs != fs->cs_utf8) {
            FConv cv;
            FCharset *fc;
            CodeCVTStatic_init();
            fc = codecvt->RefCharset_get_fcharset(cdir->cs, TRUE);
            if (fc == NULL) {
                return FALSE;
            }
            codecvt->FConv_init(&cv, fc, FALSE, "?");
            codecvt->FConv_conv_strbuf(&cv, &sb, name->c, name->size, FALSE);
            local_str = TRUE;
        }
        for (i = 0; i < cdir->cdir_size; i++) {
            CentralDir *cd = &cdir->cdir[i];
            // 設定された文字コードがUTF-8以外で、ファイル名にUTF-8フラグが立っていない
            if (cdir->cs != fs->cs_utf8 && (cd->flags & CDIR_FLAG_UTF8) == 0) {
                if (local_str) {
                    if (sb.size == cd->filename.size && memcmp(sb.p, cd->filename.p, sb.size) == 0) {
                        zipentry_new_sub(vret, *v, cd);
                        break;
                    }
                }
            } else {
                if (name->size == cd->filename.size && memcmp(name->c, cd->filename.p, name->size) == 0) {
                    zipentry_new_sub(vret, *v, cd);
                    break;
                }
            }
        }
        StrBuf_close(&sb);
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Int or Sequence required but %n", v_type);
        return FALSE;
    }

    return TRUE;
}
static int zip_randomreader_list(Value *vret, Value *v, RefNode *node)
{
    Ref *r = fs->ref_new(cls_zipentryiter);
    *vret = vp_Value(r);
    r->v[INDEX_ZIPENTRYITER_REF] = fs->Value_cp(*v);
    r->v[INDEX_ZIPENTRYITER_INDEX] = int32_Value(-1);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int zipreader_new(Value *vret, Value *v, RefNode *node)
{
    Value reader;
    RefCharset *cs;
    RefTimeZone *tz;
    RefNode *cls_zipreader = FUNC_VP(node);
    Ref *r = fs->ref_new(cls_zipreader);
    *vret = vp_Value(r);

    if (fg->stk_top > v + 2) {
        cs = Value_vp(v[2]);
    } else {
        cs = fs->cs_utf8;
    }
    if (fg->stk_top > v + 3) {
        tz = Value_vp(v[3]);
    } else {
        tz = NULL;
    }

    if (!fs->value_to_streamio(&reader, v[1], FALSE, 0, FALSE)) {
        return FALSE;
    }
    r->v[INDEX_ZIPREADER_READER] = reader;
    r->v[INDEX_ZIPREADER_CHARSET] = vp_Value(cs);
    r->v[INDEX_ZIPREADER_TZ] = vp_Value(tz);

    return TRUE;
}
static int zipreader_close(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}
static int zipreader_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd;
    RefCharset *cs = Value_vp(r->v[INDEX_ZIPREADER_CHARSET]);
    RefTimeZone *tz = Value_vp(r->v[INDEX_ZIPREADER_TZ]);

    if (!find_central_dir(&cd, r->v[INDEX_ZIPREADER_READER], cs, tz)) {
        return FALSE;
    }
    if (cd != NULL) {
        zipentry_new_sub(vret, VALUE_NULL, cd);
    } else {
        fs->throw_stopiter();
        return FALSE;
    }
    return TRUE;
}
static int zipreader_get(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int idx = FUNC_INT(node);
    *vret = fs->Value_cp(r->v[idx]);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @argument #1
 * 出力先
 * @argument #2
 * ローカルヘッダのDOSTIMEの時差を計算するときのタイムゾーンを指定する
 */
static int zipwriter_new(Value *vret, Value *v, RefNode *node)
{
    Value writer;
    CentralDirEnd *cdir;
    RefNode *cls_zipwriter = FUNC_VP(node);
    Ref *r = fs->ref_new(cls_zipwriter);
    *vret = vp_Value(r);

    if (!fs->value_to_streamio(&writer, v[1], TRUE, 0, FALSE)) {
        return FALSE;
    }

    // タイムゾーンを設定
    cdir = malloc(sizeof(CentralDirEnd));
    memset(cdir, 0, sizeof(*cdir));
    if (fg->stk_top > v + 2) {
        cdir->tz = Value_vp(v[2]);
    } else {
        cdir->tz = NULL;
    }
    r->v[INDEX_ZIPWRITER_CDIR] = ptr_Value(cdir);
    r->v[INDEX_ZIPWRITER_DEST] = writer;

    return TRUE;
}
static int zipwriter_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDirEnd *cdir = Value_ptr(r->v[INDEX_ZIPWRITER_CDIR]);

    if (cdir != NULL) {
        write_central_dir(cdir, r->v[INDEX_ZIPWRITER_DEST]);
        write_end_of_cdir(cdir, r->v[INDEX_ZIPWRITER_DEST]);
        CentralDirEnd_free(cdir);
        r->v[INDEX_ZIPWRITER_CDIR] = VALUE_NULL;
        fs->unref(r->v[INDEX_ZIPWRITER_DEST]);
        r->v[INDEX_ZIPWRITER_DEST] = VALUE_NULL;
    }

    return TRUE;
}

static int zipentry_finish_sub(CentralDir *cd);

static int zipwriter_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDirEnd *cdir = Value_ptr(r->v[INDEX_ZIPWRITER_CDIR]);
    Value v1 = v[1];
    RefNode *v1_type = fs->Value_type(v1);

    if (cdir == NULL) {
        fs->throw_errorf(fs->mod_lang, "ZipError", "ZipWriter already closed");
        return FALSE;
    }

    if (v1_type == fs->cls_bytes) {
        RefStr *data = Value_vp(v1);
        if (!write_bin_data(cdir, r->v[INDEX_ZIPWRITER_DEST], data->c, data->size)) {
            return FALSE;
        }
    } else if (v1_type == cls_zipentry) {
        Ref *r1 = Value_ref(v1);
        CentralDir *cd = Value_ptr(r1->v[INDEX_ZIPENTRY_CDIR]);
        if (cd == NULL) {
            fs->throw_errorf(fs->mod_lang, "ZipError", "ZipEntry already closed");
            return FALSE;
        }
        if (!fs->stream_flush_sub(v1)) { 
            return FALSE;
        }
        if (cd->z_init && !cd->z_finish) {
            if (!zipentry_finish_sub(cd)) {
                return FALSE;
            }
            cd->z_finish = TRUE;
        }
        if (!write_local_data(cdir, r->v[INDEX_ZIPWRITER_DEST], r1)) {
            return FALSE;
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, cls_zipentry, fs->cls_bytes, v1_type, 1);
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int zipentry_finish_sub(CentralDir *cd)
{
    int size_prev = cd->data.size;
    fs->StrBuf_alloc(&cd->data, size_prev + BUFFER_SIZE);
    cd->data.size = size_prev;

    cd->z.next_in = NULL;
    cd->z.avail_in = 0;
    cd->z.next_out = (Bytef*)&cd->data.p[size_prev];
    cd->z.avail_out = cd->data.alloc_size - size_prev;

    if (cd->z_init != Z_STREAM_DEFLATE) {
        int ret = deflateInit2(&cd->z, cd->level, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, 0);
        if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", zError(ret));
            return FALSE;
        }
        cd->z_init = Z_STREAM_DEFLATE;
    }
    for (;;) {  // 終端処理をしないので、入力がなくなったら終了
        int ret = deflate(&cd->z, Z_FINISH);

        if (ret == Z_BUF_ERROR || ret == Z_STREAM_END){   // 出力してバッファを空にする
            cd->data.size = cd->z.next_out - (Bytef*)cd->data.p;
            if (ret == Z_STREAM_END) {
                break;
            }

            size_prev = cd->data.size;
            fs->StrBuf_alloc(&cd->data, size_prev + BUFFER_SIZE);
            cd->data.size = size_prev;

            cd->z.avail_out = cd->data.alloc_size - size_prev;
            cd->z.next_out = (Bytef*)&cd->data.p[size_prev];
        } else if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", cd->z.msg);
            return FALSE;
        }
    }
    cd->size_compressed = cd->data.size;
    return TRUE;
}

static int zipentry_new(Value *vret, Value *v, RefNode *node)
{
    CentralDir *cd = malloc(sizeof(CentralDir));
    Ref *r = fs->ref_new(cls_zipentry);
    *vret = vp_Value(r);

    fs->init_stream_ref(r, STREAM_WRITE);
    memset(cd, 0, sizeof(*cd));
    fs->StrBuf_init(&cd->filename, 0);
    fs->StrBuf_init(&cd->data, 0);

    {
        RefStr *s_type = Value_vp(v[1]);
        if (str_eqi(s_type->c, s_type->size, "store", -1)) {
            cd->method = ZIP_CM_STORE;
        } else if (str_eqi(s_type->c, s_type->size, "deflate", -1)) {
            cd->method = ZIP_CM_DEFLATE;
        } else {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown compress method %q", s_type->c);
            return FALSE;
        }
    }
    if (fg->stk_top > v + 2) {
        int64_t level = fs->Value_int64(v[2], NULL);
        if (level < 0 || level > 9) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid compress level %v (0 - 9)", &v[2]);
            return FALSE;
        }
        cd->level = level;
    }

    cd->crc32 = crc32(0, Z_NULL, 0);
    cd->z_init = Z_STREAM_NONE;
    cd->level = 6;
    r->v[INDEX_ZIPENTRY_CDIR] = ptr_Value(cd);

    return TRUE;
}
static int zipentry_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    void *in_buf = Value_ptr(r->v[INDEX_ZIPENTRY_IN_BUF]);

    if (cd != NULL) {
        StrBuf_close(&cd->filename);
        StrBuf_close(&cd->data);

        switch (cd->z_init) {
        case Z_STREAM_INFLATE:
            inflateEnd(&cd->z);
            cd->z_init = Z_STREAM_NONE;
            break;
        case Z_STREAM_DEFLATE:
            deflateEnd(&cd->z);
            cd->z_init = Z_STREAM_NONE;
            break;
        }

        free(cd);
        r->v[INDEX_ZIPENTRY_CDIR] = VALUE_NULL;
    }
    if (in_buf != NULL) {
        free(in_buf);
        r->v[INDEX_ZIPENTRY_IN_BUF] = VALUE_NULL;
    }

    return TRUE;
}
static int zipentry_read_sub(char *dst, int *psize, Ref *r, Value reader)
{
    char *cbuf;
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);

    if (cd->z_init != Z_STREAM_INFLATE) {
        // 初回は、バッファのallocateとz_streamの初期化
        int ret;
        int size_read = BUFFER_SIZE;

        cbuf = malloc(BUFFER_SIZE);
        r->v[INDEX_ZIPENTRY_IN_BUF] = ptr_Value(cbuf);
        if (!read_cdir_data(cbuf, &size_read, reader, cd)) {
            *psize = 0;
            return TRUE;
        }

        cd->z.next_in = (Bytef*)cbuf;
        cd->z.avail_in = size_read;
        cd->z.next_out = (Bytef*)dst;
        cd->z.avail_out = *psize;

        ret = inflateInit2(&cd->z, -MAX_WBITS);
        if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", zError(ret));
            return FALSE;
        }
        cd->z_init = Z_STREAM_INFLATE;
    } else {
        cbuf = Value_ptr(r->v[INDEX_ZIPENTRY_IN_BUF]);
        cd->z.next_out = (Bytef*)dst;
        cd->z.avail_out = *psize;
    }

    while (cd->z.avail_out > 0) {
        int ret = inflate(&cd->z, Z_NO_FLUSH);

        if (ret == Z_STREAM_END) {
            // 終わり
            break;
        } else if (ret == Z_BUF_ERROR) {
            int size_read = BUFFER_SIZE;
            if (!read_cdir_data(cbuf, &size_read, reader, cd) || size_read == 0) {
                // 終わり
                break;
            }
            cd->z.next_in = (Bytef*)cbuf;
            cd->z.avail_in = size_read;
        } else if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", cd->z.msg);
            return FALSE;
        }
    }
    *psize -= cd->z.avail_out;

    return TRUE;
}
static int zipentry_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    Ref *zr = Value_ref(r->v[INDEX_ZIPENTRY_REF]);
    RefBytesIO *mb = Value_vp(v[1]);
    int size_read = fs->Value_int32(v[2]);

    if (zr == NULL) {
        fs->throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    } else {
        Value reader = zr->v[INDEX_ZIPREADER_READER];

        // ヘッダサイズを計算してcd->posを進める
        if (cd->pos == 0) {
            cd->pos = cd->offset;
            if (!get_local_header_size(reader, cd)) {
                return FALSE;
            }
            cd->end_pos = cd->pos + cd->size_compressed;
        }

        switch (cd->method){
        case ZIP_CM_STORE: {
            char *buf = mb->buf.p + mb->buf.size;

            if (!read_cdir_data(buf, &size_read, reader, cd)) {
                return TRUE;
            }
            mb->buf.size = size_read;
            break;
        }
        case ZIP_CM_DEFLATE: {
            char *buf = mb->buf.p + mb->buf.size;
            if (!zipentry_read_sub(buf, &size_read, r, reader)) {
                return FALSE;
            }
            mb->buf.size = size_read;
            break;
        }
        default:
            fs->throw_errorf(mod_zip, "ZipError", "Compress method '%d' not supported", cd->method);
            return FALSE;
        }
    }
    return TRUE;
}
static int zipentry_write_deflate(CentralDir *cd, const char *s_p, int s_size)
{
    int size_prev = cd->data.size;
    fs->StrBuf_alloc(&cd->data, size_prev + BUFFER_SIZE);
    cd->data.size = size_prev;

    cd->z.next_in = (Bytef*)s_p;
    cd->z.avail_in = s_size;
    cd->z.next_out = (Bytef*)&cd->data.p[size_prev];
    cd->z.avail_out = cd->data.alloc_size - size_prev;

    if (cd->z_init != Z_STREAM_DEFLATE) {
        // deflateヘッダを出力しない
        int ret = deflateInit2(&cd->z, cd->level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, 0);
        if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", zError(ret));
            return FALSE;
        }
        cd->z_init = Z_STREAM_DEFLATE;
    }
    for (;;) {  // 終端処理をしないので、入力がなくなったら終了
        int ret = deflate(&cd->z, Z_NO_FLUSH);

        if (cd->z.avail_in == 0 || ret == Z_BUF_ERROR || ret == Z_STREAM_END){   // 出力してバッファを空にする
            cd->data.size = cd->z.next_out - (Bytef*)cd->data.p;
            if (cd->z.avail_in == 0 || ret == Z_STREAM_END) {
                break;
            }

            size_prev = cd->data.size;
            fs->StrBuf_alloc(&cd->data, size_prev + BUFFER_SIZE);

            cd->z.avail_out = cd->data.alloc_size - size_prev;
            cd->z.next_out = (Bytef*)&cd->data.p[size_prev];
        } else if (ret != Z_OK) {
            fs->throw_errorf(mod_zip, "DeflateError", "%s", cd->z.msg);
            return FALSE;
        }
    }
    return TRUE;
}
static int zipentry_write_sub(CentralDir *cd, const char *p, int size)
{
    switch (cd->method) {
    case ZIP_CM_STORE:
        if (!fs->StrBuf_add(&cd->data, p, size)) {
            return FALSE;
        }
        break;
    case ZIP_CM_DEFLATE:
        if (!zipentry_write_deflate(cd, p, size)) {
            return FALSE;
        }
        break;
    default:
        break;
    }
    cd->size += size;
    cd->size_compressed = cd->data.size;
    cd->crc32 = crc32(cd->crc32, (Bytef*)p, size);
    
    return TRUE;
}
static int zipentry_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    RefBytesIO *mb = Value_vp(v[1]);

    if (cd == NULL) {
        return TRUE;
    }
    if (r->v[INDEX_ZIPENTRY_REF] != VALUE_NULL || cd->data.p == NULL) {
        fs->throw_error_select(THROW_NOT_OPENED_FOR_WRITE);
        return FALSE;
    }
    if (cd->z_finish) {
        fs->throw_errorf(fs->mod_io, "WriteError", "Already closed");
        return FALSE;
    }

    if (!zipentry_write_sub(cd, mb->buf.p, mb->buf.size)) {
        return FALSE;
    }
    *vret = int32_Value(mb->buf.size);

    return TRUE;
}
static int zipentry_finish(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);

    if (r->v[INDEX_ZIPENTRY_REF] != VALUE_NULL || cd->data.p == NULL) {
        fs->throw_error_select(THROW_NOT_OPENED_FOR_WRITE);
        return FALSE;
    }
    if (!fs->stream_flush_sub(*v)) { 
        return FALSE;
    }
    if (cd->z_init && !cd->z_finish) {
        if (!zipentry_finish_sub(cd)) {
            return FALSE;
        }
        cd->z_finish = TRUE;
    }
    return TRUE;
}
static int zipentry_size(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    int val;

    if (cd->data.p != NULL && !cd->z_finish) {
        fs->throw_errorf(mod_zip, "ZipError", "Cannot determine size until finish()");
        return FALSE;
    }
    if (FUNC_INT(node)) {
        val = cd->size_compressed;
    } else {
        val = cd->size;
    }
    *vret = int32_Value(val);

    return TRUE;
}
static int zipentry_filename(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);

    if (FUNC_INT(node)) {
        *vret = fs->cstr_Value(fs->cls_bytes, cd->filename.p, cd->filename.size);
    } else {
        RefCharset *cs;
        if ((cd->flags & CDIR_FLAG_UTF8) != 0) {
            cs = fs->cs_utf8;
        } else {
            // UTF-8フラグが立っていなければZipReaderの引数で指定した文字コード
            cs = cd->cs;
        }
        if (cs != fs->cs_utf8) {
            CodeCVTStatic_init();
            *vret = codecvt->cstr_Value_conv(cd->filename.p, cd->filename.size, cs);
        } else {
            *vret = fs->cstr_Value(NULL, cd->filename.p, cd->filename.size);
        }
    }

    return TRUE;
}
static int zipentry_set_filename(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    StrBuf *sb;
    RefStr *name = Value_vp(v[1]);

    if (name->size > 65535) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Filename must be less than 65535 bytes");
        return FALSE;
    }

    sb = &cd->filename;
    sb->size = 0;
    fs->StrBuf_add(sb, name->c, name->size);
    if (fs->Value_type(v[1]) == fs->cls_str) {
        cd->flags |= CDIR_FLAG_UTF8;
    } else {
        cd->flags &= ~CDIR_FLAG_UTF8;
    }
    return TRUE;
}
static int zipentry_mtime(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);

    if (cd->time_valid) {
        RefInt64 *rt = fs->buf_new(fs->cls_timestamp, sizeof(RefInt64));
        *vret = vp_Value(rt);
        rt->u.i = cd->modified;
    }
    return TRUE;
}
static int zipentry_set_mtime(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    RefInt64 *rt = Value_vp(v[1]);

    cd->modified = rt->u.i;
    cd->time_valid = TRUE;

    return TRUE;
}
static int zipentry_method(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    const char *name;

    switch (cd->method) {
    case ZIP_CM_STORE:
        name = "store";
        break;
    case ZIP_CM_DEFLATE:
        name = "deflate";
        break;
    default:
        name = "unknown";
        break;
    }
    *vret = fs->cstr_Value(fs->cls_str, name, -1);

    return TRUE;
}
static int zipentry_crypt(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    CentralDir *cd = Value_ptr(r->v[INDEX_ZIPENTRY_CDIR]);
    const char *name;

    if ((cd->flags & CDIR_FLAG_CRYPT) != 0) {
        name = "zipcrypt";
    } else {
        name = "none";
    }
    *vret = fs->cstr_Value(fs->cls_str, name, -1);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int zipentryiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Ref *rref = Value_ref(r->v[INDEX_ZIPENTRYITER_REF]);

    CentralDirEnd *cdir = Value_vp(rref->v[INDEX_ZIPREADER_CDIR]);
    int32_t idx = Value_integral(r->v[INDEX_ZIPENTRYITER_INDEX]);

    if (cdir == NULL) {
        fs->throw_errorf(mod_zip, "ZipError", "Already closed");
        return FALSE;
    }
    idx++;
    r->v[INDEX_ZIPENTRYITER_INDEX] = int32_Value(idx);
    if (idx < cdir->cdir_size) {
        zipentry_new_sub(vret, r->v[INDEX_ZIPENTRYITER_REF], &cdir->cdir[idx]);
    } else {
        fs->throw_stopiter();
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls = fs->define_identifier(m, m, "DeflateIO", NODE_CLASS, 0);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, deflateio_new, 1, 3, cls, fs->cls_streamio, fs->cls_str, fs->cls_int);

    n = fs->define_identifier(m, cls, "_close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, deflateio_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, deflateio_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, deflateio_write, 1, 1, NULL, fs->cls_bytesio);

    cls->u.c.n_memb = INDEX_Z_NUM;
    fs->extends_method(cls, fs->cls_streamio);


    // 最初に一覧を取得して、ランダムアクセスする
    cls = fs->define_identifier(m, m, "ZipRandomReader", NODE_CLASS, 0);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, zip_randomreader_new, 1, 3, cls, NULL, fs->cls_charset, fs->cls_timezone);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zip_randomreader_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zip_randomreader_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zip_randomreader_size, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zip_randomreader_index, 1, 1, NULL, NULL);
    n = fs->define_identifier(m, cls, "list", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zip_randomreader_list, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "charset", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipreader_get, 0, 0, (void*)INDEX_ZIPREADER_CHARSET);
    n = fs->define_identifier(m, cls, "timezone", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipreader_get, 0, 0, (void*)INDEX_ZIPREADER_TZ);

    cls->u.c.n_memb = INDEX_ZIPREADER_NUM;
    fs->extends_method(cls, fs->cls_obj);


    // 前方から順番にしかアクセスできない
    cls = fs->define_identifier(m, m, "ZipReader", NODE_CLASS, 0);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, zipreader_new, 1, 3, cls, NULL, fs->cls_charset, fs->cls_timezone);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipreader_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipreader_next, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "charset", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipreader_get, 0, 0, (void*)INDEX_ZIPREADER_CHARSET);
    n = fs->define_identifier(m, cls, "timezone", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipreader_get, 0, 0, (void*)INDEX_ZIPREADER_TZ);

    cls->u.c.n_memb = INDEX_ZIPREADER_NUM;
    fs->extends_method(cls, fs->cls_obj);


    // 前方から順番に書き込む
    cls = fs->define_identifier(m, m, "ZipWriter", NODE_CLASS, 0);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, zipwriter_new, 1, 2, cls, NULL, fs->cls_timezone);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipwriter_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipwriter_write, 1, 1, NULL, NULL);
    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipwriter_close, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_ZIPWRITER_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "ZipEntry", NODE_CLASS, 0);
    cls_zipentry = cls;

    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, zipentry_new, 1, 2, NULL, fs->cls_str, fs->cls_int);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipentry_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipentry_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipentry_write, 1, 1, NULL, fs->cls_bytesio);
    n = fs->define_identifier(m, cls, "finish", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipentry_finish, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipentry_size, 0, 0, (void*) FALSE);
    n = fs->define_identifier(m, cls, "size_compressed", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipentry_size, 0, 0, (void*) TRUE);
    n = fs->define_identifier(m, cls, "filename", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipentry_filename, 0, 0, (void*) FALSE);
    n = fs->define_identifier(m, cls, "filename_bytes", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipentry_filename, 0, 0, (void*) TRUE);
    n = fs->define_identifier(m, cls, "filename=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipentry_set_filename, 1, 1, (void*) TRUE, fs->cls_sequence);
    n = fs->define_identifier(m, cls, "mtime", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipentry_mtime, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "mtime=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipentry_set_mtime, 1, 1, NULL, fs->cls_timestamp);
    n = fs->define_identifier(m, cls, "method", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipentry_method, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "crypt", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, zipentry_crypt, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_ZIPENTRY_NUM;
    fs->extends_method(cls, fs->cls_streamio);


    cls = fs->define_identifier(m, m, "ZipEntryIter", NODE_CLASS, 0);
    cls_zipentryiter = cls;

    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, zipentryiter_next, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_ZIPENTRYITER_NUM;
    fs->extends_method(cls, fs->cls_iterator);


    cls = fs->define_identifier(m, m, "DeflateError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);

    cls = fs->define_identifier(m, m, "ZipError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

static ZipStatic *ZipStatic_new(void)
{
    ZipStatic *f = fs->Mem_get(&fg->st_mem, sizeof(ZipStatic));
    f->get_entry_map_static = get_entry_map_static;
    f->read_entry = read_entry;

    return f;
}
void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_zip = m;

    m->u.m.ext = ZipStatic_new();

    cls_fileio = fs->Hash_get(&fs->mod_io->u.m.h, "FileIO", -1);
    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\nzlib\t" ZLIB_VERSION "\n";
}
