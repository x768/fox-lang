#include "fox_vm.h"
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
    int size;
    const char *buf;
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


static Ref *mimedata_new_ref(Value *v, int flags);
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
            Value_dec(v);
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
        Value_dec(vkey);
    } else {
        result = FALSE;
    }
    return result;
}

// 左右の空白を除去
static int add_header_to_str(RefMap *header, const char *key_p, int key_size, const char *val_p, int val_size, RefCharset *cs)
{
    Value vval = cstr_Value_conv(val_p, val_size, cs);
    int result = add_header_to_val(header, key_p, key_size, vval);
    Value_dec(vval);
    return result;
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
        Value_dec(v_key);
        return FALSE;
    }
    Value_dec(v_key);

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
int mimereader_next_sub(Value *vret, Value v, Str s_bound, RefCharset *cs)
{
    Value headers;
    Ref *r;
    RefBytesIO *mb;

    RefMap *rm_headers = refmap_new(32);
    headers = vp_Value(rm_headers);
    rm_headers->rh.type = fv->cls_mimeheader;
    if (!read_header_from_stream(rm_headers, v, cs)) {
        Value_dec(headers);
        return FALSE;
    }

    r = mimedata_new_ref(vret, STREAM_READ);
    r->v[INDEX_MIMEDATA_HEADER] = headers;
    mb = Value_vp(r->v[INDEX_MIMEDATA_BUF]);

    if (s_bound.size == 0) {
        if (!stream_read_data(v, &mb->buf, NULL, NULL, FALSE, TRUE)) {
            return FALSE;
        }
        return TRUE;
    }
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
        if (size_trim == s_bound.size + 4) {
            if (buf[0] == '-' && buf[1] == '-' && memcmp(buf + 2, s_bound.p, s_bound.size) == 0 && buf[s_bound.size + 2] == '-' && buf[s_bound.size + 3] == '-') {
                break;
            }
        } else if (size_trim == s_bound.size + 2) {
            if (buf[0] == '-' && buf[1] == '-' && memcmp(buf + 2, s_bound.p, s_bound.size) == 0) {
                break;
            }
        }
        mb->buf.size += size;
    }

    // EOF
    if (mb->buf.size == 0) {
        Value_dec(*vret);
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

static int mimereader_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_mimereader = FUNC_VP(node);
    Ref *r = ref_new(cls_mimereader);
    Value *src = &r->v[INDEX_MIMEREADER_SRC];
    RefCharset *cs = fs->cs_utf8;

    *vret = vp_Value(r);

    if (!value_to_streamio(src, v[1], FALSE, 0)) {
        return FALSE;
    }

    if (fg->stk_top > v + 3) {
        cs = Value_vp(v[3]);
    }
    r->v[INDEX_MIMEREADER_CHARSET] = vp_Value(cs);

    if (fg->stk_top > v + 2) {
        r->v[INDEX_MIMEREADER_BOUNDARY] = Value_cp(v[2]);
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
    Str boundary = Value_str(r->v[INDEX_MIMEREADER_BOUNDARY]);

    if (!mimereader_next_sub(vret, reader, boundary, cs)) {
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
    RefMap *rm, Value reader, Str boundary, RefCharset *cs,
    const char *key_p, int key_size, const char *subkey_p, int subkey_size, int keys)
{
    for (;;) {
        Value tmp = VALUE_NULL;
        int found = FALSE;
        Str name;
        Ref *r2;
        RefMap *hdr;

        if (!mimereader_next_sub(&tmp, reader, boundary, cs)) {
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
                    Value_dec(tmp);
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
                        Value_dec(tmp);
                    } else {
                        *dst = tmp;
                    }
                } else {
                    // 何もしない
                    Value_dec(tmp);
                }
            }
        } else {
            Value vkey = cstr_Value(fs->cls_str, name.p, name.size);
            HashValueEntry *ve = refmap_add(rm, vkey, FALSE, FALSE);
            Value_dec(vkey);
            if (ve == NULL) {
                Value_dec(tmp);
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
    if (!value_to_streamio(&reader, v[1], FALSE, 0)) {
        return FALSE;
    }
    if (fg->stk_top > v + 4) {
        cs = Value_vp(v[4]);
    }

    if (fg->stk_top > v + 5) {
        boundary = Value_str(v[5]);
    } else {
        int found = FALSE;
        RefMap *hdr = refmap_new(16);
        if (!read_header_from_stream(hdr, reader, cs)) {
            Value_dec(vp_Value(hdr));
            return TRUE;
        }
        if (!mimedata_get_header_sub(&found, &boundary, hdr, "Content-Type", -1, "boudary", -1)) {
            Value_dec(vp_Value(hdr));
            return TRUE;
        }
        Value_dec(vp_Value(hdr));
    }

    if (!mimerandomreader_sub(rm, reader, boundary, cs, key->c, key->size, subkey->c, subkey->size, FALSE)) {
        Value_dec(reader);
        return FALSE;
    }
    Value_dec(reader);

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
RefStr *mimetype_from_suffix(Str name)
{
    int len = 0;
    char buf[8];
    const char *p;
    RefStr *ret = NULL;

    for (p = name.p + name.size; p > name.p; p--) {
        if (p[-1] == '.') {
            int i;

            len = name.size - (p - name.p);
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
        Str fname = Value_str(v[1]);
        RefStr *mime = mimetype_from_suffix(fname);
        *vret = cstr_Value(fs->cls_mimetype, mime->c, mime->size);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_file, type, 1);
        return FALSE;
    }
    return TRUE;
}

static int open_magic_file(IniTok *tk)
{
    char *path = str_printf("%r" SEP_S "data" SEP_S "magic.txt", fs->fox_home);
    int ret = IniTok_load(tk, path);
    free(path);
    return ret;
}
static int split_number_string(int *num, Str *str, Str src)
{
    const char *end = src.p + src.size;
    char *p = (char*)src.p;

    while (p < end) {
        if (*p == '\t') {
            *p++ = '\0';

            errno = 0;
            *num = strtol(src.p, NULL, 10);
            if (errno != 0) {
                return FALSE;
            }
            str->size = escape_backslashes_sub(p, p, end - p, TRUE);
            str->p = p;
            return TRUE;
        }
        p++;
    }
    return FALSE;
}
static void load_magic_file_sub(IniTok *tk, Magic **magic)
{
    while (IniTok_next(tk)) {
        if (tk->type == INITYPE_STRING) {
            int offset;
            Str buf;
            if (split_number_string(&offset, &buf, tk->val)) {
                RefStr *pkey = intern(tk->key.p, tk->key.size);
                Magic *m = Mem_get(&fg->st_mem, sizeof(Magic));

                m->size = buf.size;
                m->offset = offset;
                m->buf = str_dup_p(buf.p, buf.size, &fg->st_mem);
                m->mime = pkey;
                m->next = NULL;

                m->sub = *magic;
                *magic = m;
            }
        } else if (tk->type == INITYPE_FILE) {
            if (tk->key.size == 1 && tk->key.p[0] == '}') {
                break;
            }
        } else if (tk->type != INITYPE_NONE) {
            break;
        }
    }
}
static void load_magic_file(Magic **magic)
{
    IniTok tk;

    if (!open_magic_file(&tk)) {
        return;
    }

    while (IniTok_next(&tk)) {
        if (tk.type == INITYPE_STRING) {
            const char *p = tk.val.p;
            const char *end = p + tk.val.size;
            RefStr *pkey;

            if (tk.key.size == 1 && tk.key.p[0] == '{') {
                pkey = NULL;
            } else {
                pkey = intern(tk.key.p, tk.key.size);
            }

            if (p == end) {
                Magic **ppm = &magic[MAGIC_INDEX_MAX];
                if (pkey == NULL) {
                    load_magic_file_sub(&tk, ppm);
                }
            } else {
                while (p < end) {
                    const char *top = p;
                    int n;

                    while (p < end && *p != '\t') {
                        p++;
                    }
                    n = escape_backslashes_sub((char*)top, top, p - top, TRUE);
                    if (p < end) {
                        p++;
                    }

                    if (n > 0 && n <= MAGIC_MAX) {
                        Magic **ppm = &magic[*top & 0xFF];

                        Magic *m = Mem_get(&fg->st_mem, sizeof(Magic));
                        m->size = n;
                        m->offset = 0;
                        m->buf = str_dup_p(top, n, &fg->st_mem);
                        m->mime = pkey;

                        // bufが長いほうが先に来る
                        while (*ppm != NULL) {
                            Magic *pm = *ppm;
                            if (pm->size <= m->size) {
                                break;
                            }
                            ppm = &pm->next;
                        }
                        m->next = *ppm;
                        m->sub = NULL;
                        *ppm = m;
                        if (pkey == NULL) {
                            load_magic_file_sub(&tk, &m->sub);
                        }
                    }
                }
            }
        }
    }
    IniTok_close(&tk);
}
RefStr *mimetype_from_magic(const char *p, int size)
{
    static Magic **magic;
    Magic *pm;

    if (size > 0) {
        if (size > MAGIC_MAX) {
            size = MAGIC_MAX;
        }
        if (magic == NULL) {
            magic = Mem_get(&fg->st_mem, sizeof(Magic*) * (MAGIC_INDEX_MAX + 1));
            memset(magic, 0, sizeof(Magic*) * (MAGIC_INDEX_MAX + 1));
            load_magic_file(magic);
        }

        for (pm = magic[p[0] & 0xFF]; pm != NULL; pm = pm->next) {
            if (size >= pm->size) {
                if (memcmp(p, pm->buf, pm->size) == 0) {
                    if (pm->mime != NULL) {
                        return pm->mime;
                    } else {
                        Magic *pm2;
                        for (pm2 = pm->sub; pm2 != NULL; pm2 = pm2->sub) {
                            if (memcmp(p + pm2->offset, pm2->buf, pm2->size) == 0) {
                                return pm2->mime;
                            }
                        }
                    }
                }
            }
        }
        {
            Magic *pm2;
            for (pm2 = magic[MAGIC_INDEX_MAX]; pm2 != NULL; pm2 = pm2->sub) {
                if (memcmp(p + pm2->offset, pm2->buf, pm2->size) == 0) {
                    return pm2->mime;
                }
            }
        }
    }
    return intern("application/octet-stream", -1);
}
static int mimetype_new_from_magic(Value *vret, Value *v, RefNode *node)
{
    RefNode *v1_type = Value_type(v[1]);
    char buf[16];
    int size = sizeof(buf);

    if (v1_type == fs->cls_bytes) {
        Str s = Value_str(v[1]);
        if (size < s.size) {
            size = s.size;
        }
        memcpy(buf, s.p, size);
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
    Str s = Value_str(*v);
    Str t = Value_str(v[1]);
    const char *src = t.p;
    const char *dst = s.p;
    const char *src_end = src + t.size;
    const char *dst_end = dst + s.size;
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
                convert_str_to_bin_sub(&buf, val->c, val->size, fs->cs_stdio, "?");
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
        Value_dec(vkey);
        return FALSE;
    }
    Value_dec(vkey);

    if (ret != NULL) {
        *vret = Value_cp(ret->val);
    }
    return TRUE;
}
static int mimeheader_index_set_sub(StrBuf *sb, Value v)
{
    RefNode *type = Value_type(v);

    if (type == fs->cls_str || type == fs->cls_bytes) {
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
        time_to_RFC2822_UTC(tm->u.i, c_buf);
        StrBuf_add_c(sb, '"');
        StrBuf_add(sb, c_buf, -1);
        StrBuf_add_c(sb, '"');
    } else if (type == fs->cls_locale) {
        RefLocale *loc = Value_vp(v);
        StrBuf_add_r(sb, loc->tag);
    } else if (type == fs->cls_charset) {
        RefCharset *cs = Value_vp(v);
        StrBuf_add_r(sb, cs->iana);
    } else {
        throw_errorf(fs->mod_lang, "TypeError", "Sequence, Number, TimeStamp, MimeType Locale or Charset required but %n", type);
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
    Value_dec(vkey);
    return TRUE;

ERROR_END:
    StrBuf_close(&sb);
    Value_dec(vkey);
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
        Value_dec(vkey);
        return FALSE;
    }
    Value_dec(vkey);

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
            Value_dec(vkey);
            return FALSE;
        }
        Value_dec(vkey);
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
                    Value_dec(vkey);
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
            Value_dec(tmp);
            return FALSE;
        }
        Value_dec(tmp);
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
    Value_dec(vkey);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

static Ref *mimedata_new_ref(Value *v, int flags)
{
    RefBytesIO *bio;
    Ref *r = ref_new(fv->cls_mimedata);

    RefMap *rm = refmap_new(32);
    r->v[INDEX_MIMEDATA_HEADER] = vp_Value(rm);
    rm->rh.type = fv->cls_mimeheader;
    *v = vp_Value(r);

    bio = buf_new(fs->cls_bytesio, sizeof(RefBytesIO));
    r->v[INDEX_MIMEDATA_BUF] = vp_Value(bio);
    StrBuf_init(&bio->buf, 0);
    init_stream_ref(r, flags);

    return r;
}

static int mimedata_new(Value *vret, Value *v, RefNode *node)
{
    mimedata_new_ref(v, STREAM_WRITE);
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

/////////////////////////////////////////////////////////////////////////////////////

static int is_valid_uri(Str s)
{
    int i;

    // scheme
    for (i = 0; i < s.size; i++) {
        int c = s.p[i];
        if (c == ':') {
            break;
        }
        if (!isalnum(c) && c != '+' && c != '-' && c != '.') {
            return FALSE;
        }
    }
    if (i >= s.size) {
        return FALSE;
    }
    i++;

    for (; i < s.size; i++) {
        int c = s.p[i];
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
static int parse_url(Str *url, Str src)
{
    int i;
    int pos;

    // scheme  [a-z+\-\.]*:
    for (i = 0; i < src.size; i++) {
        if (src.p[i] == ':') {
            if (url != NULL) {
                url[URL_SCHEME] = Str_new(src.p, i);
            }
            i++;
            break;
        }
    }
    if (i == src.size) {
        return FALSE;
    }

    if (i + 2 > src.size || src.p[i] != '/' || src.p[i + 1] != '/') {
        return FALSE;
    }
    i += 2;
    // 次に/が出現するまでを切り出す
    pos = i;
    for (; i < src.size; i++) {
        int c = src.p[i];
        if (c == '/') {
            if (i == pos) {
                return FALSE;
            }
            break;
        }
    }
    if (!parse_server(url, Str_new(src.p + pos, i - pos))) {
        return FALSE;
    }

    // #または?が出るまで
    pos = i;
    for (; i < src.size; i++) {
        int c = src.p[i];
        if (c == '?' || c == '#') {
            break;
        }
    }
    if (url != NULL) {
        url[URL_PATH] = Str_new(src.p + pos, i - pos);
    }

    if (i < src.size && src.p[i] == '?') {
        i++;
        // #が出るまで
        pos = i;
        for (; i < src.size; i++) {
            if (src.p[i] == '#') {
                break;
            }
        }
        if (url != NULL) {
            url[URL_QUERY] = Str_new(src.p + pos, i - pos);
        }
    }
    if (i < src.size && src.p[i] == '#') {
        i++;
        if (url != NULL) {
            url[URL_HASH] = Str_new(src.p + i, src.size - i);
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
    Str src = Value_str(v[1]);

    if (!is_valid_uri(src)) {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid URI string format");
        return FALSE;
    }
    *vret = cstr_Value(fs->cls_uri, src.p, src.size);
    return TRUE;
}
static int uri_get_part(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(*v);
    Str part[URL_NUM];
    int idx = FUNC_INT(node);

    memset(part, 0, sizeof(part));
    if (parse_url(part, src)) {
        Str s = part[idx];
        *vret = cstr_Value(fs->cls_str, s.p, s.size);
    } else {
        throw_errorf(fs->mod_lang, "ParseError", "Invalid URL string format");
        return FALSE;
    }
    return TRUE;
}
static int uri_get_scheme(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(*v);
    int i, len;
    RefStr *rs;

    for (i = 0; i < src.size; i++) {
        if (src.p[i] == ':') {
            break;
        }
    }
    len = i;
    rs = refstr_new_n(fs->cls_str, len);
    *vret = vp_Value(rs);
    for (i = 0; i < len; i++) {
        rs->c[i] = tolower(src.p[i]);
    }
    rs->c[i] = '\0';

    return TRUE;
}
static int uri_get_port(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(*v);
    Str part[URL_NUM];

    memset(part, 0, sizeof(part));
    if (parse_url(part, src)) {
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
    Str src = Value_str(*v);
    *vret = bool_Value(parse_url(NULL, src));
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
    Str src = Value_str(*v);
    const char *p = src.p;
    const char *end = p + src.size;

    StrBuf buf;
    char *path;
    Str path_s;

    if (src.size < 7 || memcmp(src.p, "file://", 7) != 0) {
        goto ERROR_END;
    }
    p = src.p + 7;

#ifdef WIN32
    if (p < end) {
        if (*p == '/') {
            p += 1;
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

    cls->u.c.n_memb = INDEX_MIMEDATA_NUM;
    extends_method(cls, fs->cls_streamio);


    // MimeReader
    cls = define_identifier(m, m, "MimeReader", NODE_CLASS, 0);
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mimereader_new, 1, 3, cls, NULL, fs->cls_str, fs->cls_charset);

    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, mimereader_next, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_MIMEREADER_NUM;
    extends_method(cls, fs->cls_obj);


    // MimeRandomReader
    cls = define_identifier(m, m, "MimeRandomReader", NODE_CLASS, 0);
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mimerandomreader_new, 3, 5, cls, NULL, fs->cls_str, fs->cls_str, fs->cls_charset, fs->cls_str);

    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
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

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, strclass_tostr, 0, 2, (void*)FALSE, fs->cls_str, fs->cls_locale);
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

