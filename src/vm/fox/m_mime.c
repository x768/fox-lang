#include "fox_vm.h"
#include "fox_parse.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>


enum {
    MT_ERROR,
    MT_EOF,
    MT_STR,    // abc  "abc"
    MT_SEMICL, // ;
    MT_EQUAL,  // =
    MT_COMMA,  // ,
};
enum {
    MAGIC_INDEX_MAX = 256,
};
enum {
    PORT_NONE = 0,
    PORT_UNKNOWN = -1,
};
enum {
    URL_SCHEME,
    URL_USER,
    URL_PASS,
    URL_HOST,
    URL_PORT,
    URL_PATH,
    URL_QUERY,
    URL_HASH,
    URL_NUM,
};

// class MimeReader
enum {
    INDEX_MIMEREADER_SRC,
    INDEX_MIMEREADER_BOUNDARY,
    INDEX_MIMEREADER_HEADER,
    INDEX_MIMEREADER_CHARSET,
    INDEX_MIMEREADER_NUM,
};
// class MimeWriter
enum {
    INDEX_MIMEWRITER_SRC,
    INDEX_MIMEWRITER_BOUNDARY,
    INDEX_MIMEWRITER_HEADER,
    INDEX_MIMEWRITER_NUM,
};

typedef struct Magic
{
    struct Magic *next;
    struct Magic *sub;
    int offset;

    Str s;
    RefStr *mime;
} Magic;

typedef struct
{
    const char *start;
    const char *end;
    const char *p;

    int type;
    Str str_val;
} MimeTok;

typedef struct {
    Value header;
    int offset;
    int size;
} MimeData;


static Ref *mimedata_new_ref(int flags);
static RefStr *mimetype_from_abbr(RefStr *name);

static void MimeTok_init(MimeTok *tk, const char *p, int size)
{
    tk->p = p;
    tk->start = p;
    tk->end = p + size;
}
static void MimeTok_next(MimeTok *tk)
{
    const char *ptr;

    while (tk->p < tk->end && isspace_fox(*tk->p)) {
        tk->p++;
    }
    if (tk->p >= tk->end) {
        tk->type = MT_EOF;
        return;
    }
    switch (*tk->p) {
    case ';':
        tk->p++;
        tk->type = MT_SEMICL;
        break;
    case '=':
        tk->p++;
        tk->type = MT_EQUAL;
        break;
    case '"':
        tk->p++;
        ptr = tk->p;
        while (tk->p < tk->end) {
            char c = *tk->p;
            if (c == '\0' || c == '"') {
                break;
            }
            tk->p++;
        }
        tk->str_val = Str_new(ptr, tk->p - ptr);
        tk->type = MT_STR;

        if (*tk->p == '"') {
            tk->p++;
        }
        break;
    default:
        ptr = tk->p;
        while (tk->p < tk->end) {
            char c = *tk->p;
            if (c == '=' || c == ';' || c == '"') {
                break;
            }
            tk->p++;
        }
        tk->str_val = Str_trim(Str_new(ptr, tk->p - ptr));
        tk->type = MT_STR;

        break;
    }
}
static void MimeTok_next_comma(MimeTok *tk)
{
    const char *ptr;

    while (tk->p < tk->end && isspace_fox(*tk->p)) {
        tk->p++;
    }
    if (tk->p >= tk->end) {
        tk->type = MT_EOF;
        return;
    }
    switch (*tk->p) {
    case ',':
        tk->p++;
        tk->type = MT_COMMA;
        break;
    case '"':
        tk->p++;
        ptr = tk->p;
        while (tk->p < tk->end) {
            char c = *tk->p;
            if (c == '\0' || c == '"') {
                break;
            }
            tk->p++;
        }
        tk->str_val = Str_new(ptr, tk->p - ptr);
        tk->type = MT_STR;

        if (*tk->p == '"') {
            tk->p++;
        }
        break;
    default:
        ptr = tk->p;
        while (tk->p < tk->end) {
            char c = *tk->p;
            if (c == ',' || c == '"') {
                break;
            }
            tk->p++;
        }
        tk->str_val = Str_trim(Str_new(ptr, tk->p - ptr));
        tk->type = MT_STR;

        break;
    }
}
static Value header_key_normalize(const char *src, int size)
{
    int i;
    int up = TRUE;
    RefStr *rs = refstr_new_n(fs->cls_str, size);
    Value v = vp_Value(rs);

    for (i = 0; i < size; i++) {
        int ch = src[i];
        if (!isalnumu_fox(ch) && ch != '-') {
            unref(v);
            return VALUE_NULL;
        }
        if (ch == '_') {
            ch = '-';
        }
        if (up) {
            rs->c[i] = toupper(ch);
            up = FALSE;
        } else {
            rs->c[i] = tolower(ch);
        }
        if (ch == '_' || ch == '-') {
            up = TRUE;
        }
    }
    rs->c[size] = '\0';
    return v;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int add_header_to_val(RefMap *header, const char *key_p, int key_size, Value val)
{
    int result;
    Value vkey = header_key_normalize(key_p, key_size);

    if (vkey != VALUE_NULL) {
        HashValueEntry *ve = refmap_add(header, vkey, TRUE, FALSE);
        if (ve != NULL) {
            ve->val = Value_cp(val);
            result = TRUE;
        } else {
            result = FALSE;
        }
        unref(vkey);
    } else {
        result = FALSE;
    }
    return result;
}

// 左右の空白を除去
static int add_header_to_str(RefMap *header, const char *key_p, int key_size, const char *val_p, int val_size, RefCharset *cs)
{
    Value vval;
    if (cs != fs->cs_utf8) {
        CodeCVTStatic_init();
        vval = codecvt->cstr_Value_conv(val_p, val_size, cs);
    } else {
        vval = cstr_Value(NULL, val_p, val_size);
    }
    {
        int result = add_header_to_val(header, key_p, key_size, vval);
        unref(vval);
        return result;
    }
}
int parse_header_sub(Str *s_ret, const char *subkey_p, int subkey_size, const char *src_p, int src_size)
{
    MimeTok tk;
    Str skey = fs->Str_EMPTY;
    Str sval = fs->Str_EMPTY;
    int phase = 0;

    if (subkey_size < 0) {
        subkey_size = strlen(subkey_p);
    }
    if (src_size < 0) {
        src_size = strlen(src_p);
    }
    MimeTok_init(&tk, src_p, src_size);

    for (;;) {
        MimeTok_next(&tk);
        switch (tk.type) {
        case MT_STR:
            switch (phase) {
            case 0:
                skey = tk.str_val;
                phase = 1;
                break;
            case 2:
                sval = tk.str_val;
                phase = 3;
                break;
            }
            break;
        case MT_EQUAL:
            phase = 2;
            break;
        default:
            if (subkey_size == 0) {
                if (phase == 1) {
                    *s_ret = skey;
                    return TRUE;
                }
            } else {
                if (phase == 3 && skey.size == subkey_size && memcmp(skey.p, subkey_p, subkey_size) == 0) {
                    *s_ret = sval;
                    return TRUE;
                }
            }
            skey = fs->Str_EMPTY;
            sval = fs->Str_EMPTY;
            phase = 0;
            if (tk.type == MT_EOF || tk.type == MT_ERROR) {
                goto PARSE_END;
            }
            break;
        }
    }
PARSE_END:
    return FALSE;
}
int mimedata_get_header_sub(int *p_found, Str *s_ret, RefMap *map, const char *key_p, int key_size, const char *subkey_p, int subkey_size)
{
    Value v_key;
    HashValueEntry *ret = NULL;

    v_key = cstr_Value(fs->cls_str, key_p, key_size);
    if (!refmap_get(&ret, map, v_key)) {
        unref(v_key);
        return FALSE;
    }
    unref(v_key);

    if (ret != NULL) {
        RefStr *rs = Value_vp(ret->val);
        if (parse_header_sub(s_ret, subkey_p, subkey_size, rs->c, rs->size)) {
            *p_found = TRUE;
        }
    }
    return TRUE;
}
static int read_header_from_stream(RefMap *header, Value r, RefCharset *cs)
{
    StrBuf line;
    StrBuf key;
    StrBuf val;

    StrBuf_init(&line, 256);
    StrBuf_init(&key, 256);
    StrBuf_init(&val, 256);

    for (;;) {
        char *p, *end;

        line.size = 0;
        if (!stream_gets_sub(&line, r, '\n')) {
            StrBuf_close(&line);
            StrBuf_close(&key);
            StrBuf_close(&val);
            return FALSE;
        }
        p = line.p;
        // 末尾の改行を除去
        if (line.size >= 1 && p[line.size - 1] == '\n') {
            line.size--;
        }
        if (line.size >= 1 && p[line.size - 1] == '\r') {
            line.size--;
        }
        if (line.size == 0) {
            // ヘッダ終端
            if (key.size > 0) {
                add_header_to_str(header, key.p, key.size, val.p, val.size, cs);
            }
            break;
        }

        end = p + line.size;
        if (isspace_fox(*p)) {
            StrBuf_add(&val, line.p, line.size);
        } else {
            if (key.size > 0) {
                add_header_to_str(header, key.p, key.size, val.p, val.size, cs);
                key.size = 0;
                val.size = 0;
            }
            while (p < end && *p != ':') {
                p++;
            }
            if (p < end) {
                StrBuf_add(&key, line.p, p - line.p);
                p++;
                StrBuf_add(&val, p, end - p);
            }
        }
    }

    StrBuf_close(&line);
    StrBuf_close(&key);
    StrBuf_close(&val);

    return TRUE;
}
/**
 * v(StreamIO)から1つ読み出して、MimeDataをvretに返す
 * keysが指定されている場合、keysに含まれるキー以外は無視
 * keysが指定されていない場合、全て
 * 終端に達した場合、vretは変更されない
 */
int mimereader_next_sub(Value *vret, Value v, const char *boundary_p, int boundary_size, RefCharset *cs)
{
    Value headers;
    Ref *r;
    RefBytesIO *mb;

    RefMap *rm_headers = refmap_new(32);
    headers = vp_Value(rm_headers);
    rm_headers->rh.type = fv->cls_mimeheader;
    if (!read_header_from_stream(rm_headers, v, cs)) {
        unref(headers);
        return FALSE;
    }

    r = mimedata_new_ref(STREAM_READ);
    *vret = vp_Value(r);

    r->v[INDEX_MIMEDATA_HEADER] = headers;
    mb = Value_vp(r->v[INDEX_MIMEDATA_BUF]);

    if (boundary_size == 0) {
        int size_read = fs->max_alloc;
        if (!stream_read_data(v, &mb->buf, NULL, &size_read, FALSE, FALSE)) {
            return FALSE;
        }
    } else {
        for (;;) {
            int size = BUFFER_SIZE;
            int size_trim;
            char *buf;

            // mb->bufの後ろに領域を確保
            StrBuf_alloc(&mb->buf, mb->buf.size + BUFFER_SIZE);
            mb->buf.size -= BUFFER_SIZE;
            buf = mb->buf.p + mb->buf.size;

            // 次の改行までを取得
            if (!stream_gets_limit(v, buf, &size)) {
                return FALSE;
            }
            if (size <= 0) {
                break;
            }

            size_trim = size;
            if (size_trim >= 1 && buf[size_trim - 1] == '\n') {
                size_trim--;
            }
            if (size_trim >= 1 && buf[size_trim - 1] == '\r') {
                size_trim--;
            }
            if (size_trim == boundary_size + 4) {
                if (buf[0] == '-' && buf[1] == '-' && memcmp(buf + 2, boundary_p, boundary_size) == 0 && buf[boundary_size + 2] == '-' && buf[boundary_size + 3] == '-') {
                    break;
                }
            } else if (size_trim == boundary_size + 2) {
                if (buf[0] == '-' && buf[1] == '-' && memcmp(buf + 2, boundary_p, boundary_size) == 0) {
                    break;
                }
            }
            mb->buf.size += size;
        }
    }

    // EOF
    if (mb->buf.size == 0) {
        unref(*vret);
        *vret = VALUE_NULL;
        return TRUE;
    }

    // 末尾の改行を除去
    if (mb->buf.size >= 1 && mb->buf.p[mb->buf.size - 1] == '\n') {
        mb->buf.size--;
    }
    if (mb->buf.size >= 1 && mb->buf.p[mb->buf.size - 1] == '\r') {
        mb->buf.size--;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 1:source
 * 2:charset
 * 3:boundary
 */
static int mimereader_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_mimereader = FUNC_VP(node);
    Ref *r = ref_new(cls_mimereader);
    Value *src = &r->v[INDEX_MIMEREADER_SRC];
    RefCharset *cs = fs->cs_utf8;

    *vret = vp_Value(r);

    if (!value_to_streamio(src, v[1], FALSE, 0, TRUE)) {
        return FALSE;
    }

    if (fg->stk_top > v + 2) {
        cs = Value_vp(v[2]);
    }
    r->v[INDEX_MIMEREADER_CHARSET] = vp_Value(cs);

    if (fg->stk_top > v + 3) {
        r->v[INDEX_MIMEREADER_BOUNDARY] = Value_cp(v[3]);
    } else {
        Str boundary;
        int found = FALSE;
        RefMap *hdr = refmap_new(16);
        r->v[INDEX_MIMEREADER_HEADER] = vp_Value(hdr);
        if (!read_header_from_stream(hdr, *src, cs)) {
            return TRUE;
        }
        if (!mimedata_get_header_sub(&found, &boundary, hdr, "Content-Type", -1, "boudary", -1)) {
            return TRUE;
        }
        if (found) {
            r->v[INDEX_MIMEREADER_BOUNDARY] = cstr_Value(fs->cls_str, boundary.p, boundary.size);
        }
    }

    return TRUE;
}
static int mimereader_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefCharset *cs = Value_vp(r->v[INDEX_MIMEREADER_CHARSET]);
    Value reader = r->v[INDEX_MIMEREADER_SRC];
    RefStr *boundary = Value_vp(r->v[INDEX_MIMEREADER_BOUNDARY]);

    if (boundary == NULL) {
        boundary = fs->str_0;
    }
    if (!mimereader_next_sub(vret, reader, boundary->c, boundary->size, cs)) {
        return FALSE;
    }
    if (*vret == VALUE_NULL) {
        throw_stopiter();
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

int mimerandomreader_sub(
    RefMap *rm, Value reader, const char *boundary_p, int boundary_size, RefCharset *cs,
    const char *key_p, int key_size, const char *subkey_p, int subkey_size, int keys)
{
    for (;;) {
        Value tmp = VALUE_NULL;
        int found = FALSE;
        Str name;
        Ref *r2;
        RefMap *hdr;

        if (!mimereader_next_sub(&tmp, reader, boundary_p, boundary_size, cs)) {
            return FALSE;
        }
        if (tmp == VALUE_NULL) {
            break;
        }
        r2 = Value_vp(tmp);
        hdr = Value_vp(r2->v[INDEX_MIMEDATA_HEADER]);
        if (!mimedata_get_header_sub(&found, &name, hdr, key_p, key_size, subkey_p, subkey_size)) {
            return FALSE;
        }
        if (!found) {
            continue;
        }
        if (keys) {
            HashValueEntry *he = refmap_get_strkey(rm, name.p, name.size);
            if (he != NULL) {
                Value val = he->val;

                if (val == VALUE_NULL) {
                    RefBytesIO *mb = Value_vp(r2->v[INDEX_MIMEDATA_BUF]);
                    he->val = cstr_Value(fs->cls_str, mb->buf.p, mb->buf.size);
                    unref(tmp);
                } else if (val == VALUE_FALSE) {
                    he->val = tmp;
                } else if (Value_type(val) == fs->cls_list) {
                    // filenameが無い場合はStrに変換する
                    Value *dst;
                    if (!mimedata_get_header_sub(&found, &name, hdr, key_p, key_size, "filename", -1)) {
                        return FALSE;
                    }
                    dst = refarray_push(Value_vp(val));
                    if (!found) {
                        RefBytesIO *mb = Value_vp(r2->v[INDEX_MIMEDATA_BUF]);
                        *dst = cstr_Value(fs->cls_str, mb->buf.p, mb->buf.size);
                        unref(tmp);
                    } else {
                        *dst = tmp;
                    }
                } else {
                    // 何もしない
                    unref(tmp);
                }
            }
        } else {
            Value vkey = cstr_Value(fs->cls_str, name.p, name.size);
            HashValueEntry *ve = refmap_add(rm, vkey, FALSE, FALSE);
            unref(vkey);
            if (ve == NULL) {
                unref(tmp);
                return FALSE;
            }
            ve->val = tmp;
        }
    }
    return TRUE;
}

/**
 * 1:source
 * 2:key
 * 3:subkey
 * 4:charset
 * 5:boundary
 */
static int mimerandomreader_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_mimerandomreader = FUNC_VP(node);
    RefMap *rm = refmap_new(16);
    RefCharset *cs = fs->cs_utf8;
    RefStr *key = Value_vp(v[2]);
    RefStr *subkey = Value_vp(v[3]);
    Str boundary = fs->Str_EMPTY;
    Value reader;

    *vret = vp_Value(rm);
    rm->rh.type = cls_mimerandomreader;
    if (!value_to_streamio(&reader, v[1], FALSE, 0, TRUE)) {
        return FALSE;
    }
    if (fg->stk_top > v + 4) {
        cs = Value_vp(v[4]);
    }

    if (fg->stk_top > v + 5) {
        RefStr *rs = Value_vp(v[5]);
        boundary = Str_new(rs->c, rs->size);
    } else {
        int found = FALSE;
        RefMap *hdr = refmap_new(16);
        if (!read_header_from_stream(hdr, reader, cs)) {
            unref(vp_Value(hdr));
            return TRUE;
        }
        if (!mimedata_get_header_sub(&found, &boundary, hdr, "Content-Type", -1, "boudary", -1)) {
            unref(vp_Value(hdr));
            return TRUE;
        }
        unref(vp_Value(hdr));
    }

    if (!mimerandomreader_sub(rm, reader, boundary.p, boundary.size, cs, key->c, key->size, subkey->c, subkey->size, FALSE)) {
        unref(reader);
        return FALSE;
    }
    unref(reader);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

int mimetype_new_sub(Value *v, RefStr *src)
{
    int i;
    int cnt = 0;
    RefStr *rs = refstr_new_n(fs->cls_mimetype, src->size);
    *v = vp_Value(rs);

    for (i = 0; i < src->size; i++) {
        char c = src->c[i] & 0xFF;
        if (isalnumu_fox(c)) {
            rs->c[i] = tolower(c);
        } else if (c == '.' || c == '-' || c == '+') {
            rs->c[i] = c;
        } else if (c == '/') {
            rs->c[i] = c;
            cnt++;
        } else {
            rs->c[i] = '\0';
            return FALSE;
        }
    }
    rs->c[i] = '\0';
    return cnt == 1;
}

RefStr *resolve_mimetype_alias(RefStr *name)
{
    static Hash aliases;
    RefStr *pret;

    if (aliases.entry == NULL) {
        Hash_init(&aliases, &fg->st_mem, 64);
        load_aliases_file(&aliases, "data" SEP_S "mime-alias.txt");
    }
    pret = Hash_get(&aliases, name->c, name->size);

    if (pret != NULL) {
        return pret;
    } else {
        return name;
    }
}
static int mimetype_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *v1_type = Value_type(v[1]);

    if (v1_type != fs->cls_str && v1_type != fs->cls_mimetype) {
        throw_errorf(fs->mod_lang, "TypeError", "Str or Type required but %n (argument #1)", v1_type);
        return FALSE;
    }

    if (v1_type == fs->cls_mimetype) {
        *vret = Value_cp(v[1]);
    } else {
        RefStr *name = Value_vp(v[1]);
        RefStr *abbr = mimetype_from_abbr(name);
        if (abbr != NULL) {
            name = abbr;
        } else {
            name = resolve_mimetype_alias(name);
        }
        if (!mimetype_new_sub(vret, name)) {
            throw_errorf(fs->mod_lang, "ValueError", "Illigal MIME-Type format");
            return FALSE;
        }
    }

    return TRUE;
}
static RefStr *mimetype_from_abbr(RefStr *name)
{
    RefStr *ret = NULL;

    if (name->size > 0) {
        static Hash suffix;
        RefStr *pret;

        if (suffix.entry == NULL) {
            Hash_init(&suffix, &fg->st_mem, 64);
            load_aliases_file(&suffix, "data" SEP_S "suffix.txt");
        }
        pret = Hash_get(&suffix, name->c, name->size);
        if (pret != NULL) {
            ret = pret;
        }
    }
    return ret;
}
RefStr *mimetype_get_parent(RefStr *name)
{
    RefStr *ret = NULL;

    if (name->size > 0) {
        static Hash suffix;
        RefStr *pret;

        if (suffix.entry == NULL) {
            Hash_init(&suffix, &fg->st_mem, 64);
            load_aliases_file(&suffix, "data" SEP_S "mime-tree.txt");
        }
        pret = Hash_get(&suffix, name->c, name->size);
        if (pret != NULL) {
            ret = pret;
        }
    }
    if (ret == NULL) {
        if (name->size > 5 && memcmp(name->c, "text/", 5) == 0) {
            if (name->size != 10 || memcmp(name->c, "text/plain", 10) != 0) {
                ret = intern("text/plain", 10);
            }
        }
        if (ret == NULL) {
            ret = intern("application/octet-stream", 24);
        }
    }

    return ret;
}
RefStr *mimetype_from_name_refstr(RefStr *name)
{
    RefStr *abbr = mimetype_from_abbr(name);
    if (abbr != NULL) {
        return abbr;
    } else {
        return resolve_mimetype_alias(name);
    }
}
/**
 * name : ファイル名
 * @return : mime-type 見つからなければapplication/octet-stream
 */
RefStr *mimetype_from_suffix(const char *name_p, int name_size)
{
    int len = 0;
    char buf[8];
    const char *p;
    RefStr *ret = NULL;

    if (name_size < 0) {
        name_size = strlen(name_p);
    }

    for (p = name_p + name_size; p > name_p; p--) {
        if (p[-1] == '.') {
            int i;

            len = name_size - (p - name_p);
            if (len >= sizeof(buf)) {
                len = sizeof(buf) - 1;
            }

            for (i = 0; i < len; i++) {
                buf[i] = tolower(p[i]);
            }
            buf[i] = '\0';
            break;
        } else if (p[-1] == SEP_C) {
            break;
        }
    }

    ret = mimetype_from_abbr(intern(buf, len));
    if (ret != NULL) {
        return ret;
    } else {
        return intern("application/octet-stream", 24);
    }
}
static int mimetype_new_from_name(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = Value_type(v[1]);

    if (type == fs->cls_str || type == fs->cls_file) {
        RefStr *fname = Value_vp(v[1]);
        RefStr *mime = mimetype_from_suffix(fname->c, fname->size);
        *vret = cstr_Value(fs->cls_mimetype, mime->c, mime->size);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_file, type, 1);
        return FALSE;
    }
    return TRUE;
}

static Magic *get_add_magic_node_sub(Magic **ppm, const char *p, int size)
{
    while (*ppm != NULL) {
        Magic *pm = *ppm;
        if (pm->s.size < size) {
            break;
        }
        if (pm->s.size == size) {
            int cmp = memcmp(pm->s.p, p, size);
            if (cmp < 0) {
                break;
            } else if (cmp == 0) {
                return pm;
            }
        }
        ppm = &pm->next;
    }
    {
        Magic *m = Mem_get(&fg->st_mem, sizeof(Magic));
        m->next = *ppm;
        m->sub = NULL;
        m->s.p = str_dup_p(p, size, &fg->st_mem);
        m->s.size = size;
        m->mime = NULL;
        *ppm = m;
        return m;
    }
}
/**
 * なければ追加して返す
 */
static Magic *get_add_magic_node(Magic **magic, const char *p, int size)
{
    Magic **ppm;

    if (size == 0) {
        ppm = &magic[MAGIC_INDEX_MAX];
    } else {
        ppm = &magic[*p & 0xFF];
    }
    return get_add_magic_node_sub(ppm, p, size);
}
static void load_magic_file(Magic **magic)
{
    Tok tk;
    char *path = path_normalize(NULL, fs->fox_home, "data" SEP_S "magic.txt", -1, NULL);
    char *buf = read_from_file(NULL, path, NULL);

    if (buf == NULL) {
        fatal_errorf("Cannot read file %s", path);
        free(path);
        return;
    }

    Tok_simple_init(&tk, buf);
    Tok_simple_next(&tk);

    for (;;) {
        switch (tk.v.type) {
        case TL_STR: {
            RefStr *mime_str = intern(tk.str_val.p, tk.str_val.size);
            Tok_simple_next(&tk);
            if (tk.v.type != T_LET) {
                goto ERROR_END;
            }
            Tok_simple_next(&tk);
            for (;;) {
                Magic *m;
                if (tk.v.type == TL_BYTES) {
                    if (tk.str_val.size > MAGIC_MAX) {
                        goto ERROR_END;
                    }
                    m = get_add_magic_node(magic, tk.str_val.p, tk.str_val.size);
                    m->offset = 0;
                    Tok_simple_next(&tk);
                } else {
                    m = get_add_magic_node(magic, "", 0);
                }

                while (tk.v.type == TL_INT) {
                    int offset = tk.int_val;
                    Tok_simple_next(&tk);
                    if (tk.v.type != TL_BYTES) {
                        goto ERROR_END;
                    }
                    if (tk.str_val.size + offset > MAGIC_MAX) {
                        goto ERROR_END;
                    }
                    m = get_add_magic_node_sub(&m->sub, tk.str_val.p, tk.str_val.size);
                    m->offset = offset;
                    Tok_simple_next(&tk);
                }
                m->mime = mime_str;
                if (tk.v.type == T_OR) {
                    Tok_simple_next(&tk);
                } else {
                    break;
                }
            }
            break;
        }
        case T_NL:
        case T_SEMICL:
            Tok_simple_next(&tk);
            break;
        case T_EOF:
            free(path);
            free(buf);
            return;
        default:
            goto ERROR_END;
        }
    }

ERROR_END:
    fatal_errorf("Error at line %d (%s)", tk.v.line, path);
}
RefStr *mimetype_from_magic_sub(Magic *pm, const char *p, int size)
{
    for (; pm != NULL; pm = pm->next) {
        if (pm->offset + pm->s.size <= size) {
            if (memcmp(p + pm->offset, pm->s.p, pm->s.size) == 0) {
                RefStr *ret = mimetype_from_magic_sub(pm->sub, p, size);
                if (ret != NULL) {
                    return ret;
                }
                if (pm->mime != NULL) {
                    return pm->mime;
                }
            }
        }
    }
    return NULL;
}
RefStr *mimetype_from_magic(const char *p, int size)
{
    if (size > 0) {
        static Magic **magic;
        RefStr *ret;

        if (size > MAGIC_MAX) {
            size = MAGIC_MAX;
        }
        if (magic == NULL) {
            magic = Mem_get(&fg->st_mem, sizeof(Magic*) * (MAGIC_INDEX_MAX + 1));
            memset(magic, 0, sizeof(Magic*) * (MAGIC_INDEX_MAX + 1));
            load_magic_file(magic);
        }
        ret = mimetype_from_magic_sub(magic[p[0] & 0xFF], p, size);
        if (ret != NULL) {
            return ret;
        }
        ret = mimetype_from_magic_sub(magic[MAGIC_INDEX_MAX], p, size);
        if (ret != NULL) {
            return ret;
        }
    }
    return intern("application/octet-stream", -1);
}
static int mimetype_new_from_magic(Value *vret, Value *v, RefNode *node)
{
    RefNode *v1_type = Value_type(v[1]);
    char buf[MAGIC_MAX];
    int size = sizeof(buf);

    if (v1_type == fs->cls_bytes) {
        RefStr *s = Value_vp(v[1]);
        if (size < s->size) {
            size = s->size;
        }
        memcpy(buf, s->c, size);
    } else if (v1_type == fs->cls_str || v1_type == fs->cls_file) {
        int fd;
        Str path_s;
        char *path = file_value_to_path(&path_s, v[1], 1);

        if (path == NULL) {
            return FALSE;
        }
        fd = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
        if (fd == -1) {
            throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
            free(path);
            return FALSE;
        }
        size = read_fox(fd, buf, size);
        close_fox(fd);
    } else if (is_subclass(v1_type, fs->cls_streamio)) {
        if (!stream_read_data(v[1], NULL, buf, &size, TRUE, FALSE)) {
            return FALSE;
        }
    } else {
        throw_errorf(fs->mod_lang, "TypeError", "Bytes, Str, File or StreamIO required but %n (argument #1)", v1_type);
        return FALSE;
    }
    {
        RefStr *rs = mimetype_from_magic(buf, size);
        *vret = cstr_Value(fs->cls_mimetype, rs->c, rs->size);
    }

    return TRUE;
}
static int mimetype_get_name(Value *vret, Value *v, RefNode *node)
{
    const RefStr *rs = Value_vp(*v);
    *vret = cstr_Value(fs->cls_str, rs->c, rs->size);
    return TRUE;
}

/**
 * t.match("text / *")
 * t.match("* / *")
 */
static int mimetype_match(Value *vret, Value *v, RefNode *node)
{
    RefStr *s = Value_vp(*v);
    RefStr *t = Value_vp(v[1]);
    const char *src = t->c;
    const char *dst = s->c;
    const char *src_end = src + t->size;
    const char *dst_end = dst + s->size;
    int result = TRUE;

    while (src < src_end) {
        if (*src == '*') {
            src++;
            while (dst < dst_end && *dst != '/') {
                dst++;
            }
        } else {
            while (src < src_end && dst < dst_end) {
                if (tolower_fox(*src) != tolower_fox(*dst)) {
                    result = FALSE;
                    goto FINISH;
                }
                if (*src == '/') {
                    break;
                }
                src++;
                dst++;
            }
        }
        if (src == src_end && dst == dst_end) {
            goto FINISH;
        }
        if (src < src_end && dst < dst_end) {
            if (*src == '/' && *dst == '/') {
                src++;
                dst++;
            } else {
                result = FALSE;
                goto FINISH;
            }
        } else {
            result = FALSE;
            goto FINISH;
        }
    }
FINISH:
    *vret = bool_Value(result);

    return TRUE;
}
static int mimetype_in(Value *vret, Value *v, RefNode *node)
{
    RefStr *base = Value_vp(*v);
    RefStr *type = Value_vp(v[1]);

    for (;;) {
        if (type->size == base->size && memcmp(type->c, base->c, type->size) == 0) {
            *vret = VALUE_TRUE;
            return TRUE;
        }
        if (strcmp(type->c, "application/octet-stream") == 0) {
            break;
        }
        type = mimetype_get_parent(type);
    }
    *vret = VALUE_FALSE;

    return TRUE;
}
static int mimetype_get_base(Value *vret, Value *v, RefNode *node)
{
    RefStr *parent = mimetype_get_parent(Value_vp(*v));
    if (parent->rh.type == fs->cls_mimetype) {
        *vret = Value_cp(vp_Value(parent));
    } else {
        *vret = cstr_Value(fs->cls_mimetype, parent->c, parent->size);
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int mimeheader_new(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = refmap_new(0);
    *vret = vp_Value(rm);
    rm->rh.type = fv->cls_mimeheader;
    return TRUE;
}
static int mimeheader_read(Value *vret, Value *v, RefNode *node)
{
    RefCharset *cs = fs->cs_utf8;
    RefMap *rm = refmap_new(0);
    *vret = vp_Value(rm);
    rm->rh.type = fv->cls_mimeheader;

    if (fg->stk_top > v + 2) {
        cs = Value_vp(v[3]);
    }
    if (!read_header_from_stream(rm, v[1], cs)) {
        return TRUE;
    }

    return TRUE;
}
static int mimeheader_write(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    Value v1 = v[1];
    RefCharset *cs = fs->cs_utf8;
    int i;
    StrBuf buf;

    if (fg->stk_top > v + 2) {
        cs = Value_vp(v[3]);
    }

    if (cs != fs->cs_utf8) {
        StrBuf_init(&buf, 256);
    }
    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *et;
        for (et = rm->entry[i]; et != NULL; et = et->next) {
            RefStr *key = Value_vp(et->key);
            RefStr *val = Value_vp(et->val);
            stream_write_data(v1, key->c, key->size);
            stream_write_data(v1, ": ", 2);
            if (cs == fs->cs_utf8) {
                stream_write_data(v1, val->c, val->size);
            } else {
                buf.size = 0;
                convert_str_to_bin_sub(&buf, val->c, val->size, cs, "?");
                stream_write_data(v1, buf.p, buf.size);
            }
            stream_write_data(v1, "\n", 1);
        }
    }
    if (cs != fs->cs_utf8) {
        StrBuf_close(&buf);
    }

    return TRUE;
}
static int mimeheader_index(Value *vret, Value *v, RefNode *node)
{
    HashValueEntry *ret = NULL;
    RefStr *rkey = Value_vp(v[1]);
    Value vkey = header_key_normalize(rkey->c, rkey->size);

    if (vkey == VALUE_NULL) {
        throw_errorf(fs->mod_lang, "ValueError", "Illigal character in header key");
        return FALSE;
    }

    if (!refmap_get(&ret, Value_vp(*v), vkey)) {
        unref(vkey);
        return FALSE;
    }
    unref(vkey);

    if (ret != NULL) {
        *vret = Value_cp(ret->val);
    }
    return TRUE;
}
static int mimeheader_index_set_sub(StrBuf *sb, Value v)
{
    RefNode *type = Value_type(v);

    if (type == fs->cls_str || type == fs->cls_bytes || type == fs->cls_uri || type == fs->cls_mimetype) {
        RefStr *rs = Value_vp(v);
        int i;
        for (i = 0; i < rs->size; i++) {
            int ch = rs->c[i] & 0xFF;
            if (ch < ' ' || ch == 0x7F) {
                throw_errorf(fs->mod_lang, "ValueError", "Illigal character in header value");
                return FALSE;
            }
        }
        StrBuf_add_r(sb, rs);
    } else if (type == fs->cls_int || type == fs->cls_float || type == fs->cls_frac) {
        StrBuf_add_v(sb, v);
    } else if (type == fs->cls_timestamp) {
        char c_buf[48];
        RefInt64 *tm = Value_vp(v);
        timestamp_to_RFC2822_UTC(tm->u.i, c_buf);
        StrBuf_add_c(sb, '"');
        StrBuf_add(sb, c_buf, -1);
        StrBuf_add_c(sb, '"');
    } else if (type == fs->cls_locale) {
        Ref *r = Value_vp(v);
        StrBuf_add_r(sb, Value_vp(r->v[INDEX_LOCALE_LANGTAG]));
    } else if (type == fs->cls_charset) {
        RefCharset *cs = Value_vp(v);
        StrBuf_add_r(sb, cs->iana);
    } else {
        throw_errorf(fs->mod_lang, "TypeError", "Sequence, Number, TimeStamp, MimeType, Uri, Locale or Charset required but %n", type);
        return FALSE;
    }
    return TRUE;
}
static int mimeheader_index_set(Value *vret, Value *v, RefNode *node)
{
    StrBuf sb;
    RefNode *type = Value_type(v[2]);
    RefStr *rkey = Value_vp(v[1]);
    Value vkey = header_key_normalize(rkey->c, rkey->size);

    if (vkey == VALUE_NULL) {
        throw_errorf(fs->mod_lang, "ValueError", "Illigal character in header key");
        return FALSE;
    }

    StrBuf_init_refstr(&sb, 32);
    if (type == fs->cls_list) {
        RefArray *ra = Value_vp(v[2]);
        int i;
        for (i = 0; i < ra->size; i++) {
            if (i > 0) {
                StrBuf_add(&sb, ", ", 2);
            }
            if (!mimeheader_index_set_sub(&sb, ra->p[i])) {
                goto ERROR_END;
            }
        }
    } else if (type == fs->cls_map) {
        RefMap *rm = Value_vp(v[2]);
        int i;
        {
            HashValueEntry *et = refmap_get_strkey(rm, "$", 1);
            if (et != NULL) {
                RefStr *sval = Value_vp(et->val);
                StrBuf_add_r(&sb, sval);
            }
        }
        for (i = 0; i < rm->entry_num; i++) {
            HashValueEntry *et;
            for (et = rm->entry[i]; et != NULL; et = et->next) {
                RefNode *tkey = Value_type(et->key);
                RefStr *skey;
                if (tkey != fs->cls_str) {
                    throw_errorf(fs->mod_lang, "TypeError", "Str required but %n", type);
                    goto ERROR_END;
                }
                skey = Value_vp(et->key);
                if (strcmp(skey->c, "$") != 0) {
                    StrBuf_add(&sb, "; ", 2);
                    StrBuf_add_r(&sb, skey);
                    StrBuf_add_c(&sb, '=');
                    if (!mimeheader_index_set_sub(&sb, et->val)) {
                        goto ERROR_END;
                    }
                }
            }
        }
    } else {
        if (!mimeheader_index_set_sub(&sb, v[2])) {
            goto ERROR_END;
        }
    }

    {
        HashValueEntry *ve = refmap_add(Value_vp(*v), vkey, TRUE, FALSE);
        if (ve == NULL) {
            goto ERROR_END;
        }
        ve->val = StrBuf_str_Value(&sb, fs->cls_str);
    }
    unref(vkey);
    return TRUE;

ERROR_END:
    StrBuf_close(&sb);
    unref(vkey);
    return FALSE;
}

static int mimeheader_get_by_name(Value *vret, Value *v, RefNode *node)
{
    RefStr *key = Value_vp(v[1]);
    RefStr *subkey;
    int found = FALSE;
    Str ret;

    if (fg->stk_top > v + 2) {
        subkey = Value_vp(v[2]);
    } else {
        subkey = fs->str_0;
    }
    if (!mimedata_get_header_sub(&found, &ret, Value_vp(*v), key->c, key->size, subkey->c, subkey->size)) {
        return TRUE;
    }
    if (found) {
        *vret = cstr_Value(fs->cls_str, ret.p, ret.size);
    }
    return TRUE;
}
static int mimeheader_get_list(Value *vret, Value *v, RefNode *node)
{
    RefStr *rkey = Value_vp(v[1]);
    HashValueEntry *ret;
    Value vkey = header_key_normalize(rkey->c, rkey->size);

    if (vkey == VALUE_NULL) {
        throw_errorf(fs->mod_lang, "ValueError", "Illigal character in header key");
        return FALSE;
    }
    if (!refmap_get(&ret, Value_vp(*v), vkey)) {
        unref(vkey);
        return FALSE;
    }
    unref(vkey);

    if (ret != NULL) {
        MimeTok tk;
        RefArray *ra = refarray_new(0);
        RefStr *src = Value_vp(ret->val);
        *vret = vp_Value(ra);

        MimeTok_init(&tk, src->c, src->size);
        for (;;) {
            MimeTok_next_comma(&tk);
            switch (tk.type) {
            case MT_STR:
                refarray_push_str(ra, tk.str_val.p, tk.str_val.size, fs->cls_str);
                break;
            case MT_COMMA:
                break;
            default:
                return TRUE;
            }
        }
    }
    return TRUE;
}
static int mimeheader_get_map(Value *vret, Value *v, RefNode *node)
{
    HashValueEntry *ret;

    {
        RefStr *rkey = Value_vp(v[1]);
        Value vkey = header_key_normalize(rkey->c, rkey->size);

        if (vkey == VALUE_NULL) {
            throw_errorf(fs->mod_lang, "ValueError", "Illigal character in header key");
            return FALSE;
        }
        if (!refmap_get(&ret, Value_vp(*v), vkey)) {
            unref(vkey);
            return FALSE;
        }
        unref(vkey);
    }

    if (ret != NULL) {
        MimeTok tk;
        RefMap *rm = refmap_new(0);
        RefStr *src = Value_vp(ret->val);
        Str skey = fs->Str_EMPTY;
        Str sval = fs->Str_EMPTY;

        *vret = vp_Value(rm);
        MimeTok_init(&tk, src->c, src->size);
        for (;;) {
            MimeTok_next(&tk);
            switch (tk.type) {
            case MT_STR:
                sval = tk.str_val;
                break;
            case MT_EQUAL:
                skey = sval;
                sval = fs->Str_EMPTY;
                break;
            case MT_SEMICL:
            case MT_EOF:
                if (sval.size > 0) {
                    Value vkey;
                    HashValueEntry *ve;

                    if (skey.size == 0) {
                        vkey = vp_Value(intern("$", 1));
                    } else {
                        vkey = cstr_Value(fs->cls_str, skey.p, skey.size);
                    }
                    ve = refmap_add(rm, vkey, TRUE, FALSE);
                    ve->val = cstr_Value(fs->cls_str, sval.p, sval.size);
                    unref(vkey);
                }
                skey = fs->Str_EMPTY;
                sval = fs->Str_EMPTY;
                if (tk.type == MT_EOF) {
                    return TRUE;
                }
                break;
            default:
                return TRUE;
            }
        }
    }
    return TRUE;
}

static int mimeheader_content_type(Value *vret, Value *v, RefNode *node)
{
    Str name;
    int found = FALSE;

    if (!mimedata_get_header_sub(&found, &name, Value_vp(*v), "Content-Type", -1, NULL, 0)) {
        return TRUE;
    }
    if (found) {
        RefStr *rs = Value_vp(cstr_Value(fs->cls_mimetype, name.p, name.size));
        Value tmp = vp_Value(rs);

        rs = resolve_mimetype_alias(rs);
        if (!mimetype_new_sub(vret, rs)) {
            throw_errorf(fs->mod_lang, "ValueError", "Illigal MIME-Type format");
            unref(tmp);
            return FALSE;
        }
        unref(tmp);
    } else {
        throw_errorf(fs->mod_lang, "ValueError", "Content-Type not found");
        return FALSE;
    }
    return TRUE;
}
static int mimeheader_charset(Value *vret, Value *v, RefNode *node)
{
    Str name;
    int found = FALSE;

    if (!mimedata_get_header_sub(&found, &name, Value_vp(*v), "Content-Type", -1, "charset", -1)) {
        return TRUE;
    }
    if (found) {
        RefCharset *cs = get_charset_from_name(name.p, name.size);
        if (cs != NULL) {
            *vret = vp_Value(cs);
        }
    } else {
        throw_errorf(fs->mod_lang, "ValueError", "Content-Type not found");
        return FALSE;
    }
    return TRUE;
}
static int mimeheader_set_content_type(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = Value_type(v[1]);
    StrBuf buf;
    Value vkey;
    HashValueEntry *ve;

    StrBuf_init_refstr(&buf, 32);
    if (type == fs->cls_mimetype) {
        StrBuf_add_r(&buf, Value_vp(v[1]));
    } else if (type == fs->cls_str) {
        RefStr *s = Value_vp(v[1]);
        RefStr *rs = Value_vp(cstr_Value(fs->cls_mimetype, s->c, s->size));
        rs = resolve_mimetype_alias(rs);
        StrBuf_add_r(&buf, rs);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_mimetype, type, 1);
        StrBuf_close(&buf);
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        RefCharset *cs = Value_vp(v[2]);
        StrBuf_add(&buf, "; charset=", -1);
        StrBuf_add_r(&buf, cs->iana);
    }

    vkey = cstr_Value(fs->cls_str, "Content-Type", -1);
    ve = refmap_add(Value_vp(*v), vkey, TRUE, FALSE);
    if (ve == NULL) {
        return FALSE;
    }
    ve->val = StrBuf_str_Value(&buf, fs->cls_str);
    unref(vkey);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static Ref *mimedata_new_ref(int flags)
{
    RefBytesIO *bio;
    Ref *r = ref_new(fv->cls_mimedata);

    RefMap *rm = refmap_new(32);
    r->v[INDEX_MIMEDATA_HEADER] = vp_Value(rm);
    rm->rh.type = fv->cls_mimeheader;

    bio = buf_new(fs->cls_bytesio, sizeof(RefBytesIO));
    r->v[INDEX_MIMEDATA_BUF] = vp_Value(bio);
    StrBuf_init(&bio->buf, 0);
    init_stream_ref(r, flags);

    return r;
}

static int mimedata_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = mimedata_new_ref(STREAM_WRITE);
    *vret = vp_Value(r);
    return TRUE;
}
static int mimedata_header(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int idx = FUNC_INT(node);
    *vret = Value_cp(r->v[idx]);
    return TRUE;
}
static int mimedata_headers_iter(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefNode nd;
    int idx = FUNC_INT(node);

    nd.u.f.vp = (void*)ITERATOR_BOTH;
    map_iterator(vret, &r->v[idx], &nd);

    return TRUE;
}

static int mimedata_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *rd = Value_vp(r->v[INDEX_MIMEDATA_BUF]);
    RefBytesIO *mb = Value_vp(v[1]);
    int read_size = Value_int64(v[2], NULL);

    mb->buf.size = 0;

    if (read_size > rd->buf.size - rd->cur) {
        read_size = rd->buf.size - rd->cur;
    }
    if (!StrBuf_add(&mb->buf, rd->buf.p + rd->cur, read_size)) {
        return FALSE;
    }
    rd->cur += read_size;

    return TRUE;
}

static int mimedata_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *wr = Value_vp(r->v[INDEX_MIMEDATA_BUF]);
    RefBytesIO *mb = Value_vp(v[1]);

    if (!StrBuf_add(&wr->buf, mb->buf.p, mb->buf.size)) {
        return FALSE;
    }

    return TRUE;
}
static int mimedata_get_pos(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *mb = Value_vp(r->v[INDEX_MIMEDATA_BUF]);
    *vret = int32_Value(mb->cur);
    return TRUE;
}
static int mimedata_set_pos(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *mb = Value_vp(r->v[INDEX_MIMEDATA_BUF]);
    int64_t offset = Value_int64(v[1], NULL);

    if (offset >= 0 && offset < mb->buf.size) {
        mb->cur = offset;
    } else {
        throw_errorf(fs->mod_io, "ReadError", "Seek position out of range");
        return FALSE;
    }
    return TRUE;
}
static int mimedata_size(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *mb = Value_vp(r->v[INDEX_MIMEDATA_BUF]);
    *vret = int32_Value(mb->buf.size);
    return TRUE;
}
static int mimedata_data(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *mb = Value_vp(r->v[INDEX_MIMEDATA_BUF]);
    *vret = cstr_Value(fs->cls_bytes, mb->buf.p, mb->buf.size);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static int is_valid_uri(const char *s_p, int s_size)
{
    int i;

    // scheme
    for (i = 0; i < s_size; i++) {
        int c = s_p[i];
        if (c == ':') {
            break;
        }
        if (!isalnum(c) && c != '+' && c != '-' && c != '.') {
            return FALSE;
        }
    }
    if (i >= s_size) {
        return FALSE;
    }
    i++;

    for (; i < s_size; i++) {
        int c = s_p[i];
        if (!isalnum(c) && strchr("-_.!~*'();/?#:@&=+$,%[]", c) == NULL) {
            return FALSE;
        }
    }
    return TRUE;
}
static int parse_server(Str *url, Str src)
{
    int i;

    // user:pass@host
    for (i = src.size - 1; i >= 0; i--) {
        if (src.p[i] == '@') {
            int end = i;

            for (i = 0; i < end; i++) {
                if (src.p[i] == ':') {
                    if (url != NULL) {
                        url[URL_USER] = Str_new(src.p, i);
                        url[URL_PASS] = Str_new(src.p + i + 1, end - i - 1);
                    }
                    break;
                }
            }
            if (i == end && url != NULL) {
                url[URL_USER] = Str_new(src.p, end);
            }
            src.p += end + 1;
            src.size -= end + 1;
            break;
        }
    }
    // host:port (\d+)
    for (i = src.size - 1; i >= 0; i--) {
        if (src.p[i] == ':') {
            Str port = Str_new(src.p + i + 1, src.size - i - 1);
            int n = parse_int(port.p, port.size, 65535);
            if (n < 0) {
                return FALSE;
            }
            if (url != NULL) {
                url[URL_PORT] = port;
            }
            src.size = i;
            break;
        }
        if (!isdigit(src.p[i])) {
            break;
        }
    }
    if (url != NULL) {
        url[URL_HOST] = src;
    }

    return TRUE;
}
static int parse_url(Str *url, const char *src_p, int src_size)
{
    int i;
    int pos;

    if (src_size < 0) {
        src_size = strlen(src_p);
    }

    // scheme  [a-z+\-\.]*:
    for (i = 0; i < src_size; i++) {
        if (src_p[i] == ':') {
            if (url != NULL) {
                url[URL_SCHEME] = Str_new(src_p, i);
            }
            i++;
            break;
        }
    }
    if (i == src_size) {
        return FALSE;
    }

    if (i + 2 > src_size || src_p[i] != '/' || src_p[i + 1] != '/') {
        return FALSE;
    }
    i += 2;
    // 次に/が出現するまでを切り出す
    pos = i;
    for (; i < src_size; i++) {
        int c = src_p[i];
        if (c == '/') {
            break;
        }
    }
    if (i > pos) {
        if (!parse_server(url, Str_new(src_p + pos, i - pos))) {
            return FALSE;
        }
    } else {
        if (url != NULL) {
            url[URL_USER] = fs->Str_EMPTY;
            url[URL_PASS] = fs->Str_EMPTY;
            url[URL_HOST] = fs->Str_EMPTY;
        }
    }

    // #または?が出るまで
    pos = i;
    for (; i < src_size; i++) {
        int c = src_p[i];
        if (c == '?' || c == '#') {
            break;
        }
    }
    if (url != NULL) {
        url[URL_PATH] = Str_new(src_p + pos, i - pos);
    }

    if (i < src_size && src_p[i] == '?') {
        i++;
        // #が出るまで
        pos = i;
        for (; i < src_size; i++) {
            if (src_p[i] == '#') {
                break;
            }
        }
        if (url != NULL) {
            url[URL_QUERY] = Str_new(src_p + pos, i - pos);
        }
    }
    if (i < src_size && src_p[i] == '#') {
        i++;
        if (url != NULL) {
            url[URL_HASH] = Str_new(src_p + i, src_size - i);
        }
    }

    return TRUE;
}
static int scheme_default_port(const char *p, int size)
{
    if (str_eqi(p, size, "http", -1)) {
        return 80;
    } else if (str_eqi(p, size, "https", -1)) {
        return 443;
    } else if (str_eqi(p, size, "ftp", -1)) {
        return 20;
    } else if (str_eqi(p, size, "sftp", -1)) {
        return 22;
    } else if (str_eqi(p, size, "ftps", -1)) {
        return 900;
    } else if (str_eqi(p, size, "telnet", -1)) {
        return 23;
    } else if (str_eqi(p, size, "smb", -1)) {
        return 445;
    } else if (str_eqi(p, size, "file", -1)) {
        return PORT_NONE;
    }
    return PORT_UNKNOWN;
}

static int uri_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);

    if (!is_valid_uri(rs->c, rs->size)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid URI string format");
        return FALSE;
    }
    *vret = cstr_Value(fs->cls_uri, rs->c, rs->size);
    return TRUE;
}
static int uri_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint32_t size;
    int rd_size;
    RefStr *rs;

    if (!stream_read_uint32(r, &size)) {
        return FALSE;
    }
    if (size > 0xffffff) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        return FALSE;
    }
    if (size > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }

    rs = refstr_new_n(fs->cls_uri, size);
    *vret = vp_Value(rs);
    rd_size = size;
    if (!stream_read_data(r, NULL, rs->c, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    rs->c[size] = '\0';
    if (!is_valid_uri(rs->c, rs->size)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid URI string format");
        return FALSE;
    }
    return TRUE;
}
static int uri_get_part(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(*v);
    Str part[URL_NUM];
    int idx = FUNC_INT(node);

    memset(part, 0, sizeof(part));
    if (parse_url(part, src->c, src->size)) {
        *vret = cstr_Value(fs->cls_str, part[idx].p, part[idx].size);
    } else {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid URL string format");
        return FALSE;
    }
    return TRUE;
}
static int uri_get_scheme(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(*v);
    int i, len;
    RefStr *rs;

    for (i = 0; i < src->size; i++) {
        if (src->c[i] == ':') {
            break;
        }
    }
    len = i;
    rs = refstr_new_n(fs->cls_str, len);
    *vret = vp_Value(rs);
    for (i = 0; i < len; i++) {
        rs->c[i] = tolower(src->c[i]);
    }
    rs->c[i] = '\0';

    return TRUE;
}
static int uri_get_port(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(*v);
    Str part[URL_NUM];

    memset(part, 0, sizeof(part));
    if (parse_url(part, src->c, src->size)) {
        Str port = part[URL_PORT];
        if (port.size > 0) {
            int port_i = parse_int(port.p, port.size, 65535);
            if (port_i >= 0) {
                *vret = int32_Value(port_i);
            }
        } else {
            Str scheme = part[URL_SCHEME];
            int n = scheme_default_port(scheme.p, scheme.size);
            if (n != PORT_UNKNOWN) {
                *vret = int32_Value(n);
            }
        }
    } else {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid URL string format");
        return FALSE;
    }
    return TRUE;
}
static int uri_is_url(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(*v);
    *vret = bool_Value(parse_url(NULL, src->c, src->size));
    return TRUE;
}

static int uri_add(Value *vret, Value *v, RefNode *node)
{
    RefStr *src1 = Value_vp(*v);
    RefStr *src2 = Value_vp(v[1]);
    Str part[URL_NUM];

    memset(part, 0, sizeof(part));
    if (parse_url(part, src1->c, src1->size)) {
        int path_part = (part[URL_PATH].p + part[URL_PATH].size) - src1->c;
        Str s1, s2, s3;
        RefStr *rs;
        char *dst;

        s1.p = src1->c;
        s1.size = path_part;
        s2.p = src2->c;
        s2.size = src2->size;
        s3.p = src1->c + path_part;
        s3.size = src1->size - path_part;
        if (s1.size > 0 && s1.p[s1.size - 1] == '/') {
            s1.size--;
        }
        if (s2.size > 0 && s2.p[0] == '/') {
            s2.p++;
            s2.size--;
        }
        rs = refstr_new_n(fs->cls_uri, s1.size + s2.size + s3.size + 1);
        *vret = vp_Value(rs);
        dst = rs->c;
        memcpy(dst, s1.p, s1.size);
        dst += s1.size;
        *dst++ = '/';
        memcpy(dst, s2.p, s2.size);
        dst += s2.size;
        if (s3.size > 0) {
            memcpy(dst, s3.p, s3.size);
            dst += s3.size;
        }
        *dst = '\0';
    } else {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid URL string format");
        return FALSE;
    }
    return TRUE;
}

/**
 * unix
 * file:///dir/dir/file       -> File(/dir/dir/file)
 *
 * win
 * file:///C:/dir/file        -> File(C:\dir\file)
 * file://server/dir/dir/file -> File(\\server\dir\dir\file)
 */
static int uri_tofile(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(*v);
    const char *p = src->c;
    const char *end = p + src->size;

    StrBuf buf;
    char *path;
    Str path_s;

    if (src->size < 7 || memcmp(src->c, "file://", 7) != 0) {
        goto ERROR_END;
    }
    p = src->c + 7;

#ifdef WIN32
    if (p < end) {
        if (*p == '/') {
            if (isalpha(p[1] & 0xFF) && p[2] == ':') {
                p += 1;
            }
        } else {
            p -= 2;
        }
    } else {
        goto ERROR_END;
    }
#else
    if (p >= end || *p != '/') {
        goto ERROR_END;
    }
    p++;
#endif
    StrBuf_init(&buf, end - p);
    urldecode_sub(&buf, p, end - p);
    path = path_normalize(&path_s, fv->cur_dir, buf.p, buf.size, NULL);

    if (path_s.size > 0 && path_s.p[path_s.size - 1] == SEP_C) {
        path_s.size--;
    }

    *vret = cstr_Value(fs->cls_file, path_s.p, path_s.size);
    StrBuf_close(&buf);
    free(path);

    return TRUE;

ERROR_END:
    throw_errorf(fs->mod_lang, "ValueError", "Not a file URI");
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////

static void define_mime_type_subconst(RefNode *m, RefNode *cls)
{
    RefNode *n;

    n = define_identifier(m, cls, "TEXT", NODE_CONST, 0);
    n->u.k.val = cstr_Value(fs->cls_mimetype, "text/plain", -1);
    n = define_identifier(m, cls, "HTML", NODE_CONST, 0);
    n->u.k.val = cstr_Value(fs->cls_mimetype, "text/html", -1);
    n = define_identifier(m, cls, "BIN", NODE_CONST, 0);
    n->u.k.val = cstr_Value(fs->cls_mimetype, "application/octet-stream", -1);
}
static void define_mime_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    fv->cls_mimeheader = define_identifier(m, m, "MimeHeader", NODE_CLASS, 0);
    fv->cls_mimedata = define_identifier(m, m, "MimeData", NODE_CLASS, 0);


    // MimeHeader
    cls = fv->cls_mimeheader;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mimeheader_new, 0, 0, cls, NULL);
    n = define_identifier(m, cls, "read", NODE_NEW_N, 0);
    define_native_func_a(n, mimeheader_read, 1, 2, cls, fs->cls_streamio, fs->cls_charset);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, col_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_index, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_index_set, 2, 2, NULL, fs->cls_str, NULL);
    n = define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_index, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_missing_set, NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_index_set, 2, 2, NULL, NULL, NULL);

    n = define_identifier(m, cls, "write", NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_write, 1, 2, NULL, fs->cls_streamio, fs->cls_charset);
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_empty, 0, 0, NULL);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_size, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, map_iterator, 0, 0, (void*)ITERATOR_BOTH);
    n = define_identifier(m, cls, "entries", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_BOTH);
    n = define_identifier(m, cls, "keys", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_KEY);
    n = define_identifier(m, cls, "values", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_VAL);
    n = define_identifier(m, cls, "get", NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_get_by_name, 1, 2, NULL, fs->cls_str, fs->cls_str);
    n = define_identifier(m, cls, "get_list", NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_get_list, 1, 1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "get_map", NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_get_map, 1, 1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "content_type", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimeheader_content_type, 0, 0, NULL);
    n = define_identifier(m, cls, "charset", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimeheader_charset, 0, 0, NULL);
    n = define_identifier(m, cls, "set_content_type", NODE_FUNC_N, 0);
    define_native_func_a(n, mimeheader_set_content_type, 1, 2, NULL, NULL, fs->cls_charset);
    extends_method(cls, fs->cls_obj);


    // MimeData
    cls = fv->cls_mimedata;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mimedata_new, 0, 0, cls);

    n = define_identifier(m, cls, "header", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimedata_header, 0, 0, (void*)INDEX_MIMEDATA_HEADER);
    n = define_identifier(m, cls, "headers", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimedata_headers_iter, 0, 0, (void*)INDEX_MIMEDATA_HEADER);
    n = define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    define_native_func_a(n, mimedata_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    define_native_func_a(n, mimedata_write, 1, 1, NULL, fs->cls_bytesio);
    n = define_identifier(m, cls, "pos", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimedata_get_pos, 0, 0, NULL);
    n = define_identifier(m, cls, "pos=", NODE_FUNC_N, 0);
    define_native_func_a(n, mimedata_set_pos, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimedata_size, 0, 0, NULL);
    n = define_identifier(m, cls, "data", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimedata_data, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_MIMEDATA_NUM;
    extends_method(cls, fs->cls_streamio);


    // MimeReader
    cls = define_identifier(m, m, "MimeReader", NODE_CLASS, 0);
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mimereader_new, 1, 3, cls, NULL, fs->cls_charset, fs->cls_str);

    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, mimereader_next, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_MIMEREADER_NUM;
    extends_method(cls, fs->cls_obj);


    // MimeRandomReader
    cls = define_identifier(m, m, "MimeRandomReader", NODE_CLASS, 0);
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mimerandomreader_new, 3, 5, cls, NULL, fs->cls_str, fs->cls_str, fs->cls_charset, fs->cls_str);

    n = define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    define_native_func_a(n, map_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, map_index, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    define_native_func_a(n, map_index, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_empty, 0, 0, NULL);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_size, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_VAL);
    n = define_identifier(m, cls, "has_key", NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, NULL);
    extends_method(cls, fs->cls_obj);


    // MimeWriter
    cls = define_identifier(m, m, "MimeRandomWriter", NODE_CLASS, 0);
    cls->u.c.n_memb = INDEX_MIMEWRITER_NUM;
    extends_method(cls, fs->cls_obj);


    // MimeType
    cls = fs->cls_mimetype;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mimetype_new, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "from_name", NODE_NEW_N, 0);
    define_native_func_a(n, mimetype_new_from_name, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "from_magic", NODE_NEW_N, 0);
    define_native_func_a(n, mimetype_new_from_magic, 1, 1, NULL, NULL);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, strclass_tostr, 0, 2, (void*)TRUE, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, mimetype_in, 1, 1, NULL, fs->cls_mimetype);
    n = define_identifier(m, cls, "name", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimetype_get_name, 0, 0, NULL);
    n = define_identifier(m, cls, "match", NODE_FUNC_N, 0);
    define_native_func_a(n, mimetype_match, 1, 1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "base", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, mimetype_get_base, 0, 0, NULL);

    define_mime_type_subconst(m, cls);
    extends_method(cls, fs->cls_obj);


    // Uri
    cls = fs->cls_uri;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, uri_new, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, uri_marshal_read, 1, 1, (void*) TRUE, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, strclass_tostr, 0, 2, (void*)FALSE, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    define_native_func_a(n, uri_add, 0, 2, NULL, fs->cls_str);
    n = define_identifier(m, cls, "to_file", NODE_FUNC_N, 0);
    define_native_func_a(n, uri_tofile, 0, 0, NULL);
    n = define_identifier(m, cls, "scheme", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_scheme, 0, 0, NULL);
    n = define_identifier(m, cls, "user", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_part, 0, 0, (void*)URL_USER);
    n = define_identifier(m, cls, "pass", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_part, 0, 0, (void*)URL_PASS);
    n = define_identifier(m, cls, "host", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_part, 0, 0, (void*)URL_HOST);
    n = define_identifier(m, cls, "port", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_port, 0, 0, NULL);
    n = define_identifier(m, cls, "path", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_part, 0, 0, (void*)URL_PATH);
    n = define_identifier(m, cls, "query", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_part, 0, 0, (void*)URL_QUERY);
    n = define_identifier(m, cls, "fragment", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_get_part, 0, 0, (void*)URL_HASH);
    n = define_identifier(m, cls, "is_url", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, uri_is_url, 0, 0, NULL);
    extends_method(cls, fs->cls_obj);
}
void init_mime_module_stubs()
{
    RefNode *m = Module_new_sys("mime");

    fs->cls_mimetype = define_identifier(m, m, "MimeType", NODE_CLASS, NODEOPT_STRCLASS);
    fs->cls_uri = define_identifier(m, m, "Uri", NODE_CLASS, NODEOPT_STRCLASS);
    fs->mod_mime = m;
}
void init_mime_module_1()
{
    RefNode *m = fs->mod_mime;
    define_mime_class(m);
    m->u.m.loaded = TRUE;
}

