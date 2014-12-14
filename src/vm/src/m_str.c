#include "fox_vm.h"


enum {
    INDEX_SEQITER_SRC,
    INDEX_SEQITER_CUR,
    INDEX_SEQITER_IS_STR,
    INDEX_SEQITER_NUM,
};

typedef struct {
    RefHeader rh;

    Value source;
    int bin;
    int count;

    int name_count;
    int name_size;
    char *name_entry;
    int *matches;

    char buf[];
} RefMatches;

typedef struct MapReplace {
    struct MapReplace *next;
    Str key;
    Str val;
} MapReplace;

#define Value_pcre_ptr(val) (Value_ptr(Value_ref((val))->v[INDEX_REGEX_PTR]))

///////////////////////////////////////////////////////////////////////////////////////////////////////////

static int lang_strcat(Value *vret, Value *v, RefNode *node)
{
    Value *vtop = fg->stk_top;
    Value *vp;
    StrBuf buf;

    StrBuf_init_refstr(&buf, 0);

    // 引数をstrに変換
    for (vp = fg->stk_base + 1; vp < vtop; vp++) {
        StrBuf_add_v(&buf, *vp);
    }
    *vret = StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}

/*
 * hexエンコード
 * dstにはsrcの2倍のサイズが必要
 */
void encode_hex_sub(char *dst, const char *src, int len, int upper)
{
    int i;

    if (upper) {
        for (i = 0; i < len; i++) {
            char c = src[i];
            dst[i * 2] = hex2uchar((c >> 4) & 0xF);
            dst[i * 2 + 1] = hex2uchar(c & 0xF);
        }
    } else {
        for (i = 0; i < len; i++) {
            char c = src[i];
            dst[i * 2] = hex2lchar((c >> 4) & 0xF);
            dst[i * 2 + 1] = hex2lchar(c & 0xF);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

static int StrBuf_add_codepoint(StrBuf *sb, int ch)
{
    char cbuf[4];
    char *cp = cbuf;

    if (!str_add_codepoint(&cp, ch, "ValueError")) {
        return FALSE;
    }
    return StrBuf_add(sb, cbuf, cp - cbuf);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 文字列フォーマットを解析して変換
 * エラー時は例外を出してFALSEを返す
 */
int string_format_sub(Value *v, Str src, const char *fmt_p, int fmt_size, int utf8)
{
    StrBuf buf, tmp;
    const char *ptr = fmt_p;
    const char *pend = ptr + fmt_size;

    buf.p = NULL;
    tmp.p = NULL;

    while (ptr < pend) {
        Str fm;
        int found = FALSE;

        // fmtをトークン分解
        while (ptr < pend && isspace_fox(*ptr)) {
            ptr++;
        }
        fm.p = ptr;
        while (ptr < pend) {
            int c = *ptr;
            if (islexspace_fox(c)) {
                break;
            }
            ptr++;
        }
        fm.size = ptr - fm.p;

        StrBuf_init(&buf, src.size);

        switch (fm.size >= 1 ? fm.p[0] : '\0') {
        case 'b':
            if (Str_eq_p(fm, "b64")) {
                int size;
                StrBuf_alloc(&buf, src.size * 4 / 3 + 4);
                size = encode_b64_sub(buf.p, src.p, src.size, FALSE);
                buf.size = size;
                found = TRUE;
                utf8 = TRUE;
            } else if (Str_eq_p(fm, "b64url")) {
                int size;
                StrBuf_alloc(&buf, src.size * 4 / 3 + 4);
                size = encode_b64_sub(buf.p, src.p, src.size, TRUE);
                buf.size = size;
                found = TRUE;
                utf8 = TRUE;
            }
            break;
        case 'h':
            if (Str_eq_p(fm, "hex")) {
                StrBuf_alloc(&buf, src.size * 2);
                encode_hex_sub(buf.p, src.p, src.size, FALSE);
                found = TRUE;
                utf8 = TRUE;
            }
            break;
        case 'H':
            if (Str_eq_p(fm, "HEX")) {
                StrBuf_alloc(&buf, src.size * 2);
                encode_hex_sub(buf.p, src.p, src.size, TRUE);
                found = TRUE;
                utf8 = TRUE;
            }
            break;
        case 's':
            if (Str_eq_p(fm, "str")) {
                found = TRUE;
                if (!utf8) {
                    alt_utf8_string(&buf, src.p, src.size);
                    utf8 = TRUE;
                }
            }
            break;
        case 'j':
            if (Str_eq_p(fm, "js")) {
                found = TRUE;
                if (utf8) {
                    add_backslashes_sub(&buf, src.p, src.size, ADD_BACKSLASH_UCS2);
                } else {
                    throw_errorf(fs->mod_lang, "FormatError", "Illigal format 'js' for Bytes");
                    StrBuf_close(&buf);
                    StrBuf_close(&tmp);
                    return FALSE;
                }
            }
            break;
        case 'u':   // urlencode
            if (Str_eq_p(fm, "url")) {
                urlencode_sub(&buf, src.p, src.size, FALSE);
                found = TRUE;
                utf8 = TRUE;
            } else if (Str_eq_p(fm, "url+")) {
                urlencode_sub(&buf, src.p, src.size, TRUE);
                found = TRUE;
                utf8 = TRUE;
            }
            break;
        case 'x':   // html escape
            if (fm.size == 1) {
                convert_html_entity(&buf, src.p, src.size, FALSE);
                found = TRUE;
            }
            break;
        }
        if (!found) {
            throw_errorf(fs->mod_lang, "FormatError", "Unknown format %Q", fm);
            StrBuf_close(&buf);
            StrBuf_close(&tmp);
            return FALSE;
        }

        src = Str_new(buf.p, buf.size);
        StrBuf_close(&tmp);
        tmp = buf;
    }

    if (!utf8 && invalid_utf8_pos(buf.p, buf.size) >= 0) {
        throw_error_select(THROW_INVALID_UTF8);
        StrBuf_close(&buf);
        return FALSE;
    }
    *v = cstr_Value(fs->cls_str, buf.p, buf.size);

    StrBuf_close(&buf);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * StrClass(中身がByte列)共通のto_str関数
 * 内容は表示可能な文字のみ
 */
int strclass_tostr(Value *vret, Value *v, RefNode *node)
{
    if (FUNC_INT(node)) {
        RefNode *type = Value_type(*v);
        RefStr *rs = Value_vp(*v);
        *vret = printf_Value("%n(%r)", type, rs);
    } else {
        RefStr *rs = Value_vp(*v);
        *vret = cstr_Value(fs->cls_str, rs->c, rs->size);
    }
    return TRUE;
}
static int sequence_from_iterator(Value *vret, Value *v, int is_str)
{
    StrBuf sb;
    StrBuf_init(&sb, 0);

    Value_push("v", v);
    if (!call_member_func(fs->str_iterator, 0, FALSE)) {
        goto ERROR_END;
    }

    for (;;) {
        RefNode *type;
        int64_t val;
        int err;

        Value_push("v", &fg->stk_top[-1]);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                goto ERROR_END;
            }
        }
        type = Value_type(fg->stk_top[-1]);
        if (type != fs->cls_int) {
            throw_errorf(fs->mod_lang, "TypeError", "Int required but %n", type);
            goto ERROR_END;
        }
        err = FALSE;
        val = Value_int64(fg->stk_top[-1], &err);
        Value_pop();
        if (err || val < 0 || (is_str && val > 0x10FFFF) || (!is_str && val > 255)) {
            throw_errorf(fs->mod_lang, "ValueError", "Number out of range (0 - %s)", is_str ? "0x10FFFF" : "255");
            goto ERROR_END;
        }
        if (is_str) {
            if (!StrBuf_add_codepoint(&sb, val)) {
                goto ERROR_END;
            }
        } else {
            if (!StrBuf_add_c(&sb, val)) {
                goto ERROR_END;
            }
        }
    }
    Value_pop();

    *vret = cstr_Value((is_str ? fs->cls_str : fs->cls_bytes), sb.p, sb.size);
    StrBuf_close(&sb);
    return TRUE;

ERROR_END:
    StrBuf_close(&sb);
    return FALSE;
}

int sequence_eq(Value *vret, Value *v, RefNode *node)
{
    RefStr *r1 = Value_vp(*v);
    RefStr *r2 = Value_vp(v[1]);
    *vret = bool_Value(str_eq(r1->c, r1->size, r2->c, r2->size));
    return TRUE;
}

int sequence_cmp(Value *vret, Value *v, RefNode *node)
{
    RefStr *r1 = Value_vp(*v);
    RefStr *r2 = Value_vp(v[1]);
    int len = (r1->size < r2->size ? r1->size : r2->size);

    int cmp = memcmp(r1->c, r2->c, len);
    if (cmp == 0) {
        cmp = r1->size - r2->size;
    }
    if (cmp < 0) {
        cmp = -1;
    } else if (cmp > 0) {
        cmp = 1;
    }
    *vret = int32_Value(cmp);

    return TRUE;
}
int sequence_hash(Value *vret, Value *v, RefNode *node)
{
    const RefStr *rs = Value_vp(*v);
    uint32_t hash = str_hash(rs->c, rs->size);
    *vret = int32_Value(hash);
    return TRUE;
}
static int sequence_empty(Value *vret, Value *v, RefNode *node)
{
    Str s1 = Value_str(*v);
    *vret = bool_Value(s1.size == 0);

    return TRUE;
}
int sequence_iter(Value *vret, Value *v, RefNode *node)
{
    RefNode *seqiter = FUNC_VP(node);
    int is_str = Value_type(*v) == fs->cls_str;
    Ref *r = ref_new(seqiter);

    *vret = vp_Value(r);
    r->v[INDEX_SEQITER_SRC] = Value_cp(*v);
    r->v[INDEX_SEQITER_CUR] = int32_Value(0);
    r->v[INDEX_SEQITER_IS_STR] = bool_Value(is_str);

    return TRUE;
}
static int sequence_marshal_read(Value *vret, Value *v, RefNode *node)
{
    int is_str = FUNC_INT(node);
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint32_t size;
    int rd_size;
    RefStr *rs;

    if (!stream_read_uint32(&size, r)) {
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

    rs = refstr_new_n((is_str ? fs->cls_str : fs->cls_bytes), size);
    *vret = vp_Value(rs);
    rd_size = size;
    if (!stream_read_data(r, NULL, rs->c, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    rs->c[size] = '\0';
    if (is_str) {
        if (invalid_utf8_pos(rs->c, rs->size) != -1) {
            throw_error_select(THROW_INVALID_UTF8);
            return FALSE;
        }
    }

    return TRUE;
}
static int sequence_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefStr *rs = Value_vp(*v);

    if (!stream_write_uint32(rs->size, w)) {
        return FALSE;
    }
    if (!stream_write_data(w, rs->c, rs->size)) {
        return FALSE;
    }

    return TRUE;
}
static int sequence_mul(Value *vret, Value *v, RefNode *node)
{
    int32_t num = Value_int64(v[1], NULL);

    if (num < 0) {
        throw_errorf(fs->mod_lang, "ValueError", "Must not be a negative value");
        return FALSE;
    } else if (num == 0) {
        RefNode *type = Value_type(*v);
        if (type == fs->cls_str) {
            *vret = vp_Value(fs->str_0);
        } else {
            *vret = cstr_Value(type, NULL, 0);
        }
    } else if (num == 1) {
        *vret = *v;
        *v = VALUE_NULL;
    } else if (num < fs->max_alloc) {
        RefNode *type = Value_type(*v);
        Str s = Value_str(*v);
        int64_t size = (int64_t)s.size * num;
        RefStr *rs;
        int i;

        if (size > fs->max_alloc) {
            throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }
        rs = refstr_new_n(type, size);
        *vret = vp_Value(rs);
        for (i = 0; i < num; i++) {
            memcpy(rs->c + s.size * i, s.p, s.size);
        }
        rs->c[size] = '\0';
    } else {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }

    return TRUE;
}
static int sequence_find_sub(Str s1, Str s2, int offset)
{
    const char *p1 = s1.p;
    const char *p2 = s2.p;
    int len1 = s1.size - s2.size;
    int len2 = s2.size;
    int i, j;

    for (i = offset; i <= len1; i++) {
        const char *p = p1 + i;

        for (j = 0; j < len2; j++) {
            if (p[j] != p2[j]) {
                break;
            }
        }
        if (j == len2) {
            return i;
        }
    }

    return -1;
}
static int sequence_find(Value *vret, Value *v, RefNode *node)
{
    Value v0 = *v;
    Value v1 = v[1];
    RefNode *v0_type = Value_type(v0);
    RefNode *v1_type = Value_type(v1);

    Str s1 = Value_str(v0);
    int idx = -1;

    if (v1_type == v0_type) {
        Str s2 = Value_str(v1);
        idx = sequence_find_sub(s1, s2, 0);
    } else if (v1_type == fs->cls_regex) {
        pcre *re = Value_pcre_ptr(v1);
        int *matches = NULL;
        int m_count = 0, m_size = 0;

        pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &m_count);
        m_size = (m_count + 1) * 3;
        matches = malloc(sizeof(int) * m_size);

        if (pcre_exec(re, NULL, s1.p, s1.size, 0, 0, matches, m_size) >= 1) {
            idx = matches[0];
        }
        free(matches);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, v0_type, fs->cls_regex, v1_type, 1);
        return FALSE;
    }
    *vret = bool_Value(idx >= 0);

    return TRUE;
}
static int sequence_match(Value *vret, Value *v, RefNode *node)
{
    Value v0 = *v;
    Value v1 = v[1];
    RefNode *v0_type = Value_type(v0);
    RefNode *v1_type = Value_type(v1);
    Str src = Value_str(v0);

    RefNode *cls_matches = FUNC_VP(node);
    RefMatches *r = NULL;
    int offset = 0;

    if (v1_type == v0_type) {
        Str s1 = Value_str(v1);
        int idx = sequence_find_sub(src, s1, offset);
        if (idx >= 0) {
            r = buf_new(cls_matches, sizeof(RefMatches) + sizeof(int) * 2);
            *vret = vp_Value(r);

            r->matches = (int*)r->buf;
            r->matches[0] = idx;
            r->matches[1] = idx + s1.size;
            r->bin = (v0_type == fs->cls_bytes);
            r->count = 1;
        }
    } else if (v1_type == fs->cls_regex) {
        pcre *re = Value_pcre_ptr(v1);
        int m_count = 0, m_size = 0, n_match;
        int name_count = 0;
        int name_size = 0;

        pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &m_count);
        pcre_fullinfo(re, NULL, PCRE_INFO_NAMECOUNT, &name_count);
        if (name_count > 0) {
            pcre_fullinfo(re, NULL, PCRE_INFO_NAMEENTRYSIZE, &name_size);
        }

        m_size = (m_count + 1) * 3;
        r = buf_new(cls_matches, sizeof(RefMatches) + sizeof(int) * m_size + name_count * name_size);
        *vret = vp_Value(r);
        r->matches = (int*)r->buf;
        r->name_entry = r->buf + sizeof(int) * m_size;

        n_match = pcre_exec(re, NULL, src.p, src.size, offset, 0, r->matches, m_size);
        if (n_match >= 0) {
            r->name_count = name_count;
            pcre_fullinfo(re, NULL, PCRE_INFO_NAMEENTRYSIZE, &r->name_size);
            if (name_count > 0) {
                char *entry;
                pcre_fullinfo(re, NULL, PCRE_INFO_NAMETABLE, &entry);
                memcpy(r->name_entry, entry, name_count * name_size);
            }

            r->bin = (v0_type == fs->cls_bytes);
            r->count = n_match;
        } else {
            free(Value_vp(*vret));
            *vret = VALUE_NULL;
            r = NULL;
        }
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, v0_type, fs->cls_regex, v1_type, 1);
        return FALSE;
    }

    if (r != NULL) {
        r->source = *v;
        *v = VALUE_NULL;
    }
    return TRUE;
}

static int sequence_replace_sub(StrBuf *buf, RefNode *v_type)
{
    RefNode *ret_type;

    if (!call_function_obj(1)) {
        return FALSE;
    }
    ret_type = Value_type(fg->stk_top[-1]);
    if (ret_type == v_type) {
        const RefStr *rs = Value_vp(fg->stk_top[-1]);
        if (!StrBuf_add(buf, rs->c, rs->size)) {
            return FALSE;
        }
        Value_pop();
    } else {
        throw_error_select(THROW_RETURN_TYPE__NODE_NODE, v_type, ret_type);
        return FALSE;
    }
    return TRUE;
}
/**
 * 第1引数はStr, Bytes, Regex
 * 第2引数はthisと同じ型
 *
 * str.replace("pattern", "replace_to")
 * str.replace(/pattern/, "replace_to")
 * str.replace("pattern", function)
 * str.replace(/pattern/, function)
 */
static int sequence_replace(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_matches = FUNC_VP(node);
    Value v1 = v[1];
    Value v2 = v[2];
    RefNode *v_type = Value_type(*v);
    RefNode *v1_type = Value_type(v1);
    RefNode *v2_type = Value_type(v2);
    int is_str = (v_type == fs->cls_str);

    Str src = Value_str(*v);
    Str repl_to;
    StrBuf buf;
    int callback;
    int offset = 0;

    if (v2_type == v_type) {
        repl_to = Value_str(v2);
        callback = FALSE;
    } else if (v2_type == fs->cls_fn) {
        repl_to = fs->Str_EMPTY;
        callback = TRUE;
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, v_type, fs->cls_fn, v2_type, 1);
        return FALSE;
    }

    StrBuf_init_refstr(&buf, src.size);
    if (v1_type == v_type) {
        Str s1 = Value_str(v1);
        int prev = 0;

        for (;;) {
            Str result;
            int idx = sequence_find_sub(src, s1, offset);
            if (idx < 0) {
                result = Str_new(src.p + prev, src.size - prev);
                if (is_str) {
                    result = fix_utf8_part(result);
                }
                if (!StrBuf_add(&buf, result.p, result.size)) {
                    return FALSE;
                }
                break;
            }

            result = Str_new(src.p + prev, idx - prev);
            if (is_str) {
                result = fix_utf8_part(result);
            }
            if (!StrBuf_add(&buf, result.p, result.size)) {
                return FALSE;
            }

            prev = idx + s1.size;
            offset = prev;
            if (s1.size == 0) {
                offset++;
            }

            if (callback) {
                RefMatches *r;

                *fg->stk_top++ = v2;
                Value_inc(v2);

                r = buf_new(cls_matches, sizeof(RefMatches) + sizeof(int) * 2);
                *fg->stk_top++ = vp_Value(r);
                r->count = 1;
                r->matches = (int*)r->buf;
                r->matches[0] = idx;
                r->matches[1] = idx + s1.size;
                r->bin = (v_type == fs->cls_bytes);
                r->source = Value_cp(*v);

                if (!sequence_replace_sub(&buf, v_type)) {
                    return FALSE;
                }
            } else {
                if (!StrBuf_add(&buf, repl_to.p, repl_to.size)) {
                    return FALSE;
                }
            }
        }
    } else if (v1_type == fs->cls_regex) {
        pcre *re = Value_pcre_ptr(v1);
        int prev = 0;
        int m_count = 0, m_size = 0;
        int *matches = NULL;

        pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &m_count);
        m_size = (m_count + 1) * 3;
        matches = malloc(sizeof(int) * m_size);

        for (;;) {
            Str result;
            int n_match = pcre_exec(re, NULL, src.p, src.size, offset, 0, matches, m_size);

            if (n_match < 0) {
                result = Str_new(src.p + prev, src.size - prev);
                if (is_str) {
                    result = fix_utf8_part(result);
                }
                if (!StrBuf_add(&buf, result.p, result.size)) {
                    free(matches);
                    return FALSE;
                }
                break;
            }
            result = Str_new(src.p + prev, matches[0] - prev);
            if (is_str) {
                result = fix_utf8_part(result);
            }
            if (!StrBuf_add(&buf, result.p, result.size)) {
                free(matches);
                return FALSE;
            }

            prev = matches[1];
            offset = prev;
            if (matches[0] == matches[1]) {
                offset++;
            }

            if (callback) {
                // 引数に渡すmatchesオブジェクトを作成
                RefMatches *r;
                int name_count = 0;
                int name_size = 0;

                *fg->stk_top++ = v2;
                Value_inc(v2);

                pcre_fullinfo(re, NULL, PCRE_INFO_NAMECOUNT, &name_count);
                if (name_count > 0) {
                    pcre_fullinfo(re, NULL, PCRE_INFO_NAMEENTRYSIZE, &name_size);
                }

                r = buf_new(cls_matches, sizeof(RefMatches) + sizeof(int) * n_match * 2 + name_count * name_size);
                *fg->stk_top++ = vp_Value(r);
                r->count = n_match;
                r->matches = (int*)r->buf;
                r->name_entry = r->buf + sizeof(int) * n_match * 2;
                r->name_count = name_count;
                r->name_size = name_size;

                memcpy(r->matches, matches, sizeof(int) * n_match * 2);
                if (name_count > 0) {
                    char *entry;
                    pcre_fullinfo(re, NULL, PCRE_INFO_NAMETABLE, &entry);
                    memcpy(r->name_entry, entry, name_count * name_size);
                }

                r->bin = (v_type == fs->cls_bytes);
                r->source = Value_cp(*v);

                if (!sequence_replace_sub(&buf, v_type)) {
                    free(matches);
                    return FALSE;
                }

                matches = malloc(sizeof(int) * m_size);
            } else {
                if (!StrBuf_add(&buf, repl_to.p, repl_to.size)) {
                    free(matches);
                    return FALSE;
                }
            }
        }
        free(matches);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, v_type, fs->cls_regex, v1_type, 1);
        return FALSE;
    }
    *vret = StrBuf_str_Value(&buf, v_type);

    return TRUE;
}
void calc_splice_position(int *pstart, int *plen, int size, Value *v)
{
    int start, len;

    if (fg->stk_top > v + 1) {
        start = Value_int64(v[1], NULL);
    } else {
        start = 0;
    }
    if (start < 0) {
        start += size;
    }
    if (start > size) {
        start = size;
        len = 0;
    } else if (fg->stk_top > v + 2) {
        len = Value_int64(v[2], NULL);
        if (len < 0) {
            len = 0;
        }
    } else {
        len = size - start;
    }
    if (start < 0) {
        len += start;
        start = 0;
        if (len < 0) {
            len = 0;
        }
    }
    if (len > size - start) {
        len = size - start;
    }

    *pstart = start;
    *plen = len;
}
static int sequence_splice(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(*v);
    RefNode *v_type = Value_type(*v);
    Str append;
    int start, len;
    int size;
    RefStr *rs;

    if (fg->stk_top > v + 3) {
        RefNode *v3_type = Value_type(v[3]);
        if (v3_type != v_type) {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, v_type, v3_type, 3);
            return FALSE;
        }
        append = Value_str(v[3]);
    } else {
        append = fs->Str_EMPTY;
    }

    if (v_type == fs->cls_str) {
        size = strlen_utf8(src.p, src.size);
    } else {
        size = src.size;
    }
    calc_splice_position(&start, &len, size, v);
    if (v_type == fs->cls_str) {
        len = utf8_position(src.p + start, src.size - start, len);
        start = utf8_position(src.p, src.size, start);
    }
    rs = refstr_new_n(v_type, src.size + append.size - len);
    *vret = vp_Value(rs);
    memcpy(rs->c, src.p, start);
    if (append.size > 0) {
        memcpy(rs->c + start, append.p, append.size);
    }
    memcpy(rs->c + start + append.size, src.p + start + len, src.size - start - len);
    rs->c[rs->size] = '\0';

    return TRUE;
}

/**
 * 第1引数はStr, Bytes, Regex
 * thisがStrならStrの配列、BytesならBytesの配列を返す
 */
static int sequence_split(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefNode *v1_type = Value_type(v1);
    Str src = Value_str(*v);
    RefNode *v_type = Value_type(*v);
    int offset = 0;

    RefArray *arr = refarray_new(0);
    *vret = vp_Value(arr);

    if (v1_type == v_type) {
        Str s1 = Value_str(v1);
        int prev = 0;

        for (;;) {
            Str result;
            int idx = sequence_find_sub(src, s1, offset);
            if (idx < 0) {
                result = Str_new(src.p + prev, src.size - prev);
                refarray_push_str(arr, result.p, result.size, v_type);
                break;
            }
            result = Str_new(src.p + prev, idx - prev);
            refarray_push_str(arr, result.p, result.size, v_type);

            prev = idx + s1.size;
            offset = prev;
            if (s1.size == 0) {
                // マッチした幅が0の場合は1進めないと、無限ループになってしまう
                offset++;
            }
        }
    } else if (v1_type == fs->cls_regex) {
        pcre *re = Value_pcre_ptr(v1);
        int prev = 0;
        int m_count = 0, m_size = 0;
        int *matches = NULL;

        pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &m_count);
        m_size = (m_count + 1) * 3;
        matches = malloc(sizeof(int) * m_size);

        for (;;) {
            Str result;
            int n_match = pcre_exec(re, NULL, src.p, src.size, offset, 0, matches, m_size);

            if (n_match < 0) {
                result = Str_new(src.p + prev, src.size - prev);
                if (v_type == fs->cls_str) {
                    result = fix_utf8_part(result);
                }
                refarray_push_str(arr, result.p, result.size, v_type);
                break;
            }
            result = Str_new(src.p + prev, matches[0] - prev);
            if (v_type == fs->cls_str) {
                result = fix_utf8_part(result);
            }
            refarray_push_str(arr, result.p, result.size, v_type);

            prev = matches[1];
            offset = prev;
            if (matches[0] == matches[1]) {
                // マッチした幅が0の場合は1進めないと、無限ループになってしまう
                offset++;
            }
        }
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_sequence, fs->cls_regex, v1_type, 1);
        return FALSE;
    }

    return TRUE;
}
/**
 * 大文字小文字を変換します。
 */
static int sequence_caseconv(Value *vret, Value *v, RefNode *node)
{
    int i;
    int lower = FUNC_INT(node);
    Str src = Value_str(*v);
    RefNode *v_type = Value_type(*v);
    RefStr *rs = refstr_new_n(v_type, src.size);
    *vret = vp_Value(rs);

    if (lower) {
        for (i = 0; i < src.size; i++) {
            int ch = src.p[i] & 0xFF;
            rs->c[i] = (ch >= 'A' && ch <= 'Z') ? ch + ('a' - 'A') : ch;
        }
    } else {
        for (i = 0; i < src.size; i++) {
            int ch = src.p[i] & 0xFF;
            rs->c[i] = (ch >= 'a' && ch <= 'z') ? ch - ('a' - 'A') : ch;
        }
    }
    rs->c[i] = '\0';

    return TRUE;
}
static int sequence_trim(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(*v);
    RefNode *v_type = Value_type(*v);
    int i, j;
    // TRUE  : 前後の空白・制御文字を除去し、中間の空白・制御文字の連続を1個にまとめる
    // FALSE : 前後の空白のみ除去
    int collapse = FUNC_INT(node);

    // 左の空白を除去
    for (i = 0; i < src.size; i++) {
        if (collapse) {
            if ((src.p[i] & 0xFF) > ' ') {
                break;
            }
        } else {
            if (!isspace_fox(src.p[i])) {
                break;
            }
        }
    }
    // 右の空白を除去
    for (j = src.size; j > i; j--) {
        if (collapse) {
            if ((src.p[j - 1] & 0xFF) > ' ') {
                break;
            }
        } else {
            if (!isspace_fox(src.p[j - 1])) {
                break;
            }
        }
    }

    if (collapse) {
        Str ret = Str_new(src.p + i, j - i);
        int k, l;

        // スペースの連続または制御文字が存在するか調べる
        for (k = 0; k < ret.size - 1; k++) {
            if ((ret.p[k] & 0xFF) < ' ') {
                break;
            }
            if ((ret.p[k] & 0xFF) == ' ' && (ret.p[k + 1] & 0xFF) == ' ') {
                break;
            }
        }
        // 中間のスペースの連続を1個にまとめる
        if (k < ret.size - 1) {
            int pre = FALSE;
            RefStr *rs = refstr_new_n(v_type, ret.size);
            *vret = vp_Value(rs);

            l = 0;
            for (k = 0; k < ret.size; k++) {
                int ch = ret.p[k] & 0xFF;
                if (pre) {
                    if (ch > ' ') {
                        rs->c[l++] = ch;
                        pre = FALSE;
                    }
                } else {
                    if (ch > ' ') {
                        rs->c[l++] = ch;
                    } else {
                        rs->c[l++] = ' ';
                        pre = TRUE;
                    }
                }
            }
            rs->c[l] = '\0';
            rs->size = l;
            return TRUE;
        }
    }

    if (collapse || i > 0 || j < src.size) {
        *vret = cstr_Value(v_type, src.p + i, j - i);
    } else {
        // 長さの変化がないため、そのまま
        *vret = *v;
        *v = VALUE_NULL;
    }
    return TRUE;
}
// 指定したパターンが出現する回数を返す
static int sequence_count(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefNode *v_type = Value_type(*v);
    RefNode *v1_type = Value_type(v1);
    int offset = 0;
    int count = 0;

    Str s1 = Value_str(*v);

    if (v1_type == v_type) {
        Str s2 = Value_str(v1);
        for (;;) {
            int idx = sequence_find_sub(s1, s2, offset);
            if (idx >= 0) {
                offset = idx + s2.size;
                count++;
            } else {
                break;
            }
        }
    } else if (v1_type == fs->cls_regex) {
        pcre *re = Value_pcre_ptr(v1);
        int *matches = NULL;
        int m_count = 0, m_size = 0;

        pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &m_count);
        m_size = (m_count + 1) * 3;
        matches = malloc(sizeof(int) * m_size);

        for (;;) {
            if (pcre_exec(re, NULL, s1.p, s1.size, offset, 0, matches, m_size) >= 1) {
                offset = matches[1];
                count++;
            } else {
                break;
            }
        }
        free(matches);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, v_type, fs->cls_regex, v1_type, 1);
        return FALSE;
    }
    *vret = int32_Value(count);

    return TRUE;
}

// 指定した文字コードでエンコードできるか調べる
static int sequence_is_valid_encoding(Value *vret, Value *v, RefNode *node)
{
    int str = FUNC_INT(node);
    RefCharset *cs = Value_vp(v[1]);
    const RefStr *rs = Value_vp(*v);
    int result;

    if (cs == fs->cs_utf8) {
        if (str) {
            result = TRUE;
        } else {
            result = (invalid_utf8_pos(rs->c, rs->size) < 0);
        }
    } else {
        IconvIO ic;
        RefCharset *cs_from;
        RefCharset *cs_to;

        if (str) {
            cs_from = fs->cs_utf8;
            cs_to = cs;
        } else {
            cs_from = cs;
            cs_to = fs->cs_utf8;
        }

        // エラーがないかどうか調べるだけ
        if (IconvIO_open(&ic, cs_from, cs_to, NULL)) {
            char *tmp_buf = malloc(BUFFER_SIZE);
            result = TRUE;

            ic.inbuf = rs->c;
            ic.inbytesleft = rs->size;
            ic.outbuf = tmp_buf;
            ic.outbytesleft = BUFFER_SIZE;

            for (;;) {
                switch (IconvIO_next(&ic)) {
                case ICONV_OK:
                    if (ic.inbuf != NULL) {
                        ic.inbuf = NULL;
                    } else {
                        goto BREAK;
                    }
                    break;
                case ICONV_OUTBUF:
                    ic.outbuf = tmp_buf;
                    ic.outbytesleft = BUFFER_SIZE;
                    break;
                case ICONV_INVALID:
                    result = FALSE;
                    goto BREAK;
                }
            }
        BREAK:
            free(tmp_buf);
            if (ic.inbytesleft > 0) {
                result = FALSE;
            }
            IconvIO_close(&ic);
        } else {
            return FALSE;
        }
    }
    *vret = bool_Value(result);

    return TRUE;
}

static int seqiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int i = Value_integral(r->v[INDEX_SEQITER_CUR]);
    Str s = Value_str(r->v[INDEX_SEQITER_SRC]);
    int ret;

    if (i >= s.size) {
        throw_stopiter();
        return FALSE;
    } else if (Value_bool(r->v[INDEX_SEQITER_IS_STR])) {
        const char *p = &s.p[i];
        ret = utf8_codepoint_at(p);
        utf8_next(&p, &s.p[s.size]);
        i = p - s.p;
    } else {
        ret = s.p[i];
        i++;
    }
    r->v[INDEX_SEQITER_CUR] = int32_Value(i);
    *vret = int32_Value(ret);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * 緩い文字列変換
 */
static int string_new(Value *vret, Value *v, RefNode *node)
{
    if (fg->stk_top > v + 2) {
        RefNode *type = Value_type(v[1]);
        if (type != fs->cls_bytes) {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_bytes, type, 1);
            return FALSE;
        }
        if (!convert_bin_to_str(vret, NULL, 1)) {
            return FALSE;
        }
    } else if (fg->stk_top > v + 1) {
        Value *v1 = v + 1;

        RefNode *type = Value_type(*v1);
        if (type == fs->cls_str) {
            // そのまま使う
            *vret = *v1;
            *v1 = VALUE_NULL;
        } else if (type == fs->cls_bytes) {
            Str src = Value_str(*v1);
            *vret = cstr_Value_conv(src.p, src.size, NULL);
        } else {
            // 引数に積んだ値をthisとして、to_strを呼び出す
            if (!call_member_func(fs->str_tostr, 0, TRUE)) {
                return FALSE;
            }
            *vret = fg->stk_top[-1];
            fg->stk_top--;
        }
    } else {
        *vret = vp_Value(fs->str_0);
    }

    return TRUE;
}

/**
 * コードポイントの列から文字列を生成
 */
static int string_new_seq(Value *vret, Value *v, RefNode *node)
{
    Value *v1 = v + 1;
    Value *top = fg->stk_top;
    StrBuf buf;

    if (top == v1 + 1) {
        RefNode *v1_type = Value_type(*v1);
        if (v1_type == fs->cls_list) {
            RefArray *ra = Value_vp(*v1);
            v1 = ra->p;
            top = v1 + ra->size;
        } else if (v1_type != fs->cls_int) {
            if (!sequence_from_iterator(vret, v1, TRUE)) {
                return FALSE;
            }
            return TRUE;
        }
    }

    StrBuf_init(&buf, top - v1);
    while (v1 < top) {
        int32_t ch;
        RefNode *type = Value_type(*v1);

        if (type != fs->cls_int) {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_int, type, v1 - fg->stk_base + 1);
            return FALSE;
        }
        ch = Value_int64(*v1, NULL);
        if (!StrBuf_add_codepoint(&buf, ch)) {
            StrBuf_close(&buf);
            return FALSE;
        }
        v1++;
    }
    *vret = cstr_Value(fs->cls_str, buf.p, buf.size);
    StrBuf_close(&buf);

    return TRUE;
}

static int string_tostr(Value *vret, Value *v, RefNode *node)
{
    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);
        if (!string_format_sub(vret, Value_str(v[0]), fmt->c, fmt->size, TRUE)) {
            return FALSE;
        }
    } else {
        // そのまま返す
        *vret = *v;
        *v = VALUE_NULL;
    }
    return TRUE;
}

/**
 * 文字列のコードポイント数
 */
static int string_size_utf8(Value *vret, Value *v, RefNode *node)
{
    Str s1 = Value_str(*v);
    int size = strlen_utf8(s1.p, s1.size);
    *vret = int32_Value(size);
    return TRUE;
}

static int string_index(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefStr *src = Value_vp(*v);
    int idx = utf8_position(src->c, src->size, Value_int64(v1, NULL));

    if (idx >= 0 && idx < src->size) {
        int code = utf8_codepoint_at(&src->c[idx]);
        *vret = int32_Value(code);
    } else {
        throw_error_select(THROW_INVALID_INDEX__VAL_INT, v1, src->size);
        return FALSE;
    }

    return TRUE;
}
void string_substr_position(int *pbegin, int *pend, Str src, Value *v)
{
    int begin = 0;
    int end = src.size;

    if (fg->stk_top > v + 2) {
        int32_t begin_i = Value_int64(v[1], NULL);
        int32_t end_i = Value_int64(v[2], NULL);

        if (begin_i >= 0 && end_i >= 0) {
            if (end_i > begin_i) {
                begin = utf8_position(src.p, src.size, begin_i);
                end = begin + utf8_position(src.p + begin, src.size - begin, end_i - begin_i);
            } else {
                begin = 0;
                end = 0;
            }
        } else if (begin_i < 0 && end_i < 0) {
            if (end_i > begin_i) {
                end = utf8_position(src.p, src.size, end_i);
                if (end > 0) {
                    begin = utf8_position(src.p, end, begin_i - end_i);
                } else {
                    begin = 0;
                    end = 0;
                }
            } else {
                begin = 0;
                end = 0;
            }
        } else {
            begin = utf8_position(src.p, src.size, begin_i);
            if (begin < 0) {
                begin = 0;
            }
            end = utf8_position(src.p, src.size, end_i);
            if (end < 0) {
                end = 0;
            }
            if (end < begin) {
                end = begin;
            }
        }
    } else if (fg->stk_top > v + 1) {
        int32_t begin_i = Value_int64(v[1], NULL);
        begin = utf8_position(src.p, src.size, begin_i);
        if (begin < 0) {
            begin = 0;
        }
        end = src.size;
    }
    *pbegin = begin;
    *pend = end;
}
/**
 * "abc".sub(1) => "bc"
 * "abc".sub(1,2) => "b"
 * "abc".sub(-2,-1) => "b"
 */
static int string_substr(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(*v);
    int begin;
    int end;
    int len;

    string_substr_position(&begin, &end, src, v);
    len = end - begin;
    *vret = cstr_Value(fs->cls_str, src.p + begin, len);

    return TRUE;
}
static int string_tobytes(Value *vret, Value *v, RefNode *node)
{
    if (!convert_str_to_bin(vret, NULL, 0)) {
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * バイト値の列から文字列を生成
 */
static int bytes_new_seq(Value *vret, Value *v, RefNode *node)
{
    Value *v1 = v + 1;
    Value *top = fg->stk_top;
    char *dst;
    RefStr *rs;

    if (top == v1 + 1) {
        RefNode *v1_type = Value_type(*v1);
        if (v1_type == fs->cls_list) {
            RefArray *ra = Value_vp(*v1);
            v1 = ra->p;
            top = v1 + ra->size;
        } else if (v1_type != fs->cls_int) {
            if (!sequence_from_iterator(vret, v1, FALSE)) {
                return FALSE;
            }
            return TRUE;
        }
    }

    rs = refstr_new_n(fs->cls_bytes, top - v1);
    *vret = vp_Value(rs);
    dst = rs->c;
    rs->c[rs->size] = '\0';
    while (v1 < top) {
        int32_t ch;
        RefNode *type = Value_type(*v1);

        if (type != fs->cls_int) {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_int, type, v1 - fg->stk_base + 1);
            return FALSE;
        }
        ch = Value_int64(*v1, NULL);
        if (ch < 0 || ch > 255) {
            throw_errorf(fs->mod_lang, "ValueError", "Invalid value (0 - 255)");
            return FALSE;
        }

        *dst++ = ch;
        v1++;
    }

    return TRUE;
}
static int bytes_tostr(Value *vret, Value *v, RefNode *node)
{
    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);
        if (!string_format_sub(vret, Value_str(v[0]), fmt->c, fmt->size, FALSE)) {
            return FALSE;
        }
    } else {
        RefStr *rs = Value_vp(*v);
        StrBuf buf;

        StrBuf_init_refstr(&buf, rs->size);
        StrBuf_add(&buf, "b\"", 2);
        add_backslashes_bin(&buf, rs->c, rs->size);
        StrBuf_add_c(&buf, '"');

        *vret = StrBuf_str_Value(&buf, fs->cls_str);
    }
    return TRUE;
}
static int bytes_size(Value *vret, Value *v, RefNode *node)
{
    Str s1 = Value_str(*v);
    *vret = int32_Value(s1.size);

    return TRUE;
}
static int bytes_index(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    Str src = Value_str(*v);
    int32_t idx = Value_int64(v1, NULL);

    if (idx < 0) {
        idx += src.size;
    }
    if (idx >= 0 && idx < src.size) {
        char c = src.p[idx];
        *vret = int32_Value(c & 0xFF);
    } else {
        throw_error_select(THROW_INVALID_INDEX__VAL_INT, v1, src.size);
        return FALSE;
    }

    return TRUE;
}

/**
 * "abc".sub(1) => "bc"
 * "abc".sub(1,2) => "b"
 * "abc".sub(-2,-1) => "b"
 */
static int bytes_substr(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(*v);
    int32_t begin = Value_int64(v[1], NULL);
    int32_t end;
    int32_t len;

    if (begin < 0) {
        begin += src.size;
        if (begin < 0) {
            begin = 0;
        }
    } else {
        if (begin > src.size) {
            begin = src.size;
        }
    }

    if (fg->stk_top > v + 2) {
        end = Value_int64(v[2], NULL);
        if (end < 0) {
            end += src.size;
        } else {
            if (end > src.size) {
                end = src.size;
            }
        }
        if (end < begin) {
            end = begin;
        }
    } else {
        end = src.size;
    }
    len = end - begin;
    *vret = cstr_Value(fs->cls_bytes, src.p + begin, len);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static int regex_new(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    Ref *r = ref_new(fs->cls_regex);
    *vret = vp_Value(r);

    RefStr *src = Value_vp(v1);
    RefNode *v1_type = Value_type(v1);
    pcre *re;
    const char *errptr = NULL;
    int options = 0;
    int erroffset;

    // 第2引数をオプションとして解析
    if (fg->stk_top > v + 2) {
        Str opt = Value_str(v[2]);
        int i;

        for (i = 0; i < opt.size; i++) {
            switch (opt.p[i]) {
            case 'i':
                options |= PCRE_CASELESS;
                break;
            case 's':
                options |= PCRE_DOTALL;
                break;
            case 'm':
                options |= PCRE_MULTILINE;
                break;
            case 'u':
                if (v1_type == fs->cls_bytes) {
                    throw_errorf(fs->mod_lang, "RegexError", "Cannot use 'u' flag when Bytes given");
                    return FALSE;
                }
                options |= PCRE_UTF8;
                break;
            default:
                throw_errorf(fs->mod_lang, "RegexError", "Unknown option flag %c", opt.p[i]);
                return FALSE;
            }
        }
    }
    re = pcre_compile(src->c, options, &errptr, &erroffset, NULL);

    if (re != NULL) {
        r->v[INDEX_REGEX_PTR] = ptr_Value(re);
        r->v[INDEX_REGEX_SRC] = v1;
        v[1] = VALUE_NULL;
        r->v[INDEX_REGEX_OPT] = int32_Value(options);
    } else {
        throw_errorf(fs->mod_lang, "RegexError", "%s", errptr);
        return FALSE;
    }

    return TRUE;
}
static int regex_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    free(Value_ptr(r->v[INDEX_REGEX_PTR]));
    r->v[INDEX_REGEX_PTR] = VALUE_NULL;

    return TRUE;
}

static int regex_marshal_read(Value *vret, Value *v, RefNode *node)
{
    char is_str;
    uint32_t size;
    uint32_t options;
    int rd_size;
    RefStr *rs;
    pcre *re;
    const char *errptr = NULL;
    int erroffset;

    Value rd = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    Ref *r = ref_new(fs->cls_regex);
    *vret = vp_Value(r);

    rd_size = 1;
    if (!stream_read_data(rd, NULL, &is_str, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    if (!stream_read_uint32(&size, rd)) {
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
    rs = refstr_new_n(is_str ? fs->cls_str : fs->cls_bytes, size);
    r->v[INDEX_REGEX_SRC] = vp_Value(rs);
    rd_size = size;
    if (!stream_read_data(rd, NULL, rs->c, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    rs->c[size] = '\0';
    if (!stream_read_uint32(&options, rd)) {
        return FALSE;
    }
    re = pcre_compile(rs->c, options, &errptr, &erroffset, NULL);

    if (re != NULL) {
        r->v[INDEX_REGEX_PTR] = ptr_Value(re);
        r->v[INDEX_REGEX_OPT] = int32_Value(options);
    } else {
        throw_errorf(fs->mod_lang, "RegexError", "%s", errptr);
        return FALSE;
    }

    return TRUE;
}
/**
 * 0 : srcがStrなら1, Bytesなら0
 * 1 : srcの長さ(bytes)
 * 5 : src
 * 5 + src.size : オプション
 */
static int regex_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    Value src = r->v[INDEX_REGEX_SRC];
    RefStr *src_s = Value_vp(src);
    int opt = Value_integral(r->v[INDEX_REGEX_OPT]);
    char is_str = (Value_type(src) == fs->cls_str) ? 1 : 0;

    if (!stream_write_data(w, &is_str, 1)) {
        return FALSE;
    }
    if (!stream_write_uint32(src_s->size, w)) {
        return FALSE;
    }
    if (!stream_write_data(w, src_s->c, src_s->size)) {
        return FALSE;
    }
    if (!stream_write_uint32(opt, w)) {
        return FALSE;
    }
    return TRUE;
}
static int regex_source(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = Value_cp(r->v[INDEX_REGEX_SRC]);
    return TRUE;
}
static int regex_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefStr *source = Value_vp(r->v[INDEX_REGEX_SRC]);
    int flags = Value_integral(r->v[INDEX_REGEX_OPT]);
    StrBuf buf;

    StrBuf_init_refstr(&buf, source->size + 2);
    StrBuf_add_c(&buf, '/');
    add_backslashes_bin(&buf, source->c, source->size);
    StrBuf_add_c(&buf, '/');

    if ((flags & PCRE_CASELESS) != 0) {
        StrBuf_add_c(&buf, 'i');
    }
    if ((flags & PCRE_DOTALL) != 0) {
        StrBuf_add_c(&buf, 's');
    }
    if ((flags & PCRE_MULTILINE) != 0) {
        StrBuf_add_c(&buf, 'm');
    }
    if ((flags & PCRE_UTF8) != 0) {
        StrBuf_add_c(&buf, 'u');
    }
    *vret = StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}
static int regex_hash(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefStr *source = Value_vp(r->v[INDEX_REGEX_SRC]);
    int flags = Value_integral(r->v[INDEX_REGEX_OPT]);

    uint32_t hash = str_hash(source->c, source->size) ^ flags;
    hash &= INT32_MAX;
    *vret = int32_Value(hash);

    return TRUE;
}
static int regex_eq(Value *vret, Value *v, RefNode *node)
{
    Ref *r1 = Value_ref(*v);
    RefStr *source1 = Value_vp(r1->v[INDEX_REGEX_SRC]);
    int flags1 = Value_integral(r1->v[INDEX_REGEX_OPT]);

    Ref *r2 = Value_ref(v[1]);
    RefStr *source2 = Value_vp(r2->v[INDEX_REGEX_SRC]);
    int flags2 = Value_integral(r2->v[INDEX_REGEX_OPT]);

    if (refstr_cmp(source1, source2) == 0 && flags1 == flags2) {
        *vret = VALUE_TRUE;
    } else {
        *vret = VALUE_FALSE;
    }

    return TRUE;
}
static int regex_capture_count(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    pcre *re = Value_ptr(r->v[INDEX_REGEX_PTR]);
    int count = 0;

    pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &count);
    *vret = int32_Value(count);

    return TRUE;
}
static int pcre_capture_index_from_name(int count, int entry_size, const char *entry, const char *name)
{
    int i;

    for (i = 0; i < count; i++) {
        const char *p = entry + entry_size * i;
        if (strcmp(name, p + 2) == 0) {
            return ((p[0] & 0xFF) << 8) | (p[1] & 0xFF);
        }
    }
    return -1;
}
static int regex_capture_index(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    pcre *re = Value_ptr(r->v[INDEX_REGEX_PTR]);
    int count = 0;
    int entry_size = 0;
    int idx = -1;

    pcre_fullinfo(re, NULL, PCRE_INFO_NAMECOUNT, &count);
    if (count > 0) {
        pcre_fullinfo(re, NULL, PCRE_INFO_NAMEENTRYSIZE, &entry_size);
        if (entry_size > 0) {
            char *entry;
            pcre_fullinfo(re, NULL, PCRE_INFO_NAMETABLE, &entry);
            idx = pcre_capture_index_from_name(count, entry_size, entry, Value_cstr(v[1]));
        }
    }
    if (idx >= 0) {
        *vret = int32_Value(idx);
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static int matches_dispose(Value *vret, Value *v, RefNode *node)
{
    RefMatches *r = Value_vp(*v);
    Value_dec(r->source);
    r->source = VALUE_NULL;
    return TRUE;
}
static int matches_size(Value *vret, Value *v, RefNode *node)
{
    RefMatches *r = Value_vp(*v);
    *vret = int32_Value(r->count);
    return TRUE;
}
static int matches_index(Value *vret, Value *v, RefNode *node)
{
    RefMatches *r = Value_vp(*v);
    int32_t idx = 0;

    if (fg->stk_top > v + 1) {
        RefNode *v1_type = Value_type(v[1]);

        if (v1_type == fs->cls_int) {
            idx = Value_int64(v[1], NULL);
            if (idx < 0 || idx >= r->count) {
                throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], r->count + 1);
                return FALSE;
            }
        } else {
            idx = pcre_capture_index_from_name(r->name_count, r->name_size, r->name_entry, Value_cstr(v[1]));
            if (idx < 0) {
                throw_errorf(fs->mod_lang, "IndexError", "Named capture %v not found", v[1]);
                return FALSE;
            }
        }
    }

    {
        int begin = r->matches[idx * 2 + 0];
        int end = r->matches[idx * 2 + 1];
        Str src = Value_str(r->source);
        Str s = Str_new(src.p + begin, end - begin);

        if (r->bin) {
            *vret = cstr_Value(fs->cls_bytes, s.p, s.size);
        } else {
            s = fix_utf8_part(s);
            *vret = cstr_Value(fs->cls_str, s.p, s.size);
        }
    }

    return TRUE;
}
static int matches_position(Value *vret, Value *v, RefNode *node)
{
    RefMatches *r = Value_vp(*v);
    int end = FUNC_INT(node);
    int32_t idx = 0;

    if (fg->stk_top > v + 1) {
        idx = Value_int64(v[1], NULL);

        if (idx < 0 || idx >= r->count) {
            throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], r->count + 1);
            return FALSE;
        }
    }

    {
        int pos = r->matches[idx * 2 + (end ? 1 : 0)];
        if (!r->bin) {
            Str src = Value_str(r->source);
            pos = strlen_utf8(src.p, pos);
        }
        *vret = int32_Value(pos);
    }

    return TRUE;
}
static int matches_source(Value *vret, Value *v, RefNode *node)
{
    RefMatches *r = Value_vp(*v);
    *vret = Value_cp(r->source);
    return TRUE;
}

static int str_base64encode(Value *vret, Value *v, RefNode *node)
{
    int url = FUNC_INT(node);
    StrBuf buf;
    RefStr *rs;
    int size;

    StrBuf_init(&buf, 0);
    if (!convert_str_to_bin(NULL, &buf, 1)) {
        StrBuf_close(&buf);
        return FALSE;
    }

    rs = refstr_new_n(fs->cls_str, buf.size * 4 / 3 + 4);
    *vret = vp_Value(rs);
    size = encode_b64_sub(rs->c, buf.p, buf.size, url);
    rs->c[size] = '\0';
    rs->size = size;
    StrBuf_close(&buf);

    return TRUE;
}
static int str_base64decode(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(v[1]);
    RefStr *rs;
    int size;

    rs = refstr_new_n(fs->cls_bytes, src->size * 3 / 4 + 4);
    size = decode_b64_sub(rs->c, src->c, src->size, TRUE);
    if (size < 0) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid base64 sequence");
        return FALSE;
    }
    rs->c[size] = '\0';
    rs->size = size;

    Value_dec(v[1]);
    v[1] = vp_Value(rs);

    if (!convert_bin_to_str(vret, NULL, 1)) {
        return FALSE;
    }

    return TRUE;
}

static int str_hexencode(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;
    RefStr *rs;

    StrBuf_init(&buf, 0);
    if (!convert_str_to_bin(NULL, &buf, 1)) {
        StrBuf_close(&buf);
        return FALSE;
    }

    rs = refstr_new_n(fs->cls_str, buf.size * 2);
    *vret = vp_Value(rs);
    encode_hex_sub(rs->c, buf.p, buf.size, FALSE);
    rs->c[rs->size] = '\0';
    StrBuf_close(&buf);

    return TRUE;
}

static int str_hexdecode(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(v[1]);
    int isrc = 0, idst = 0;
    RefStr *rs;

    if (src->size % 2 != 0) {
        goto ERROR_END;
    }
    rs = refstr_new_n(fs->cls_bytes, src->size / 2);

    while (isrc + 1 < src->size) {
        int i = char2hex(src->c[isrc]);
        int j = char2hex(src->c[isrc + 1]);
        if (i < 0 || j < 0) {
            goto ERROR_END;
        }
        rs->c[idst] = (i << 4) | j;
        isrc += 2;
        idst++;
    }
    rs->c[rs->size] = '\0';

    Value_dec(v[1]);
    v[1] = vp_Value(rs);
    if (!convert_bin_to_str(vret, NULL, 1)) {
        return FALSE;
    }

    return TRUE;

ERROR_END:
    throw_errorf(fs->mod_lang, "ValueError", "Illigal hexdecimal format");
    return FALSE;
}

static int str_urlencode(Value *vret, Value *v, RefNode *node)
{
    int plus = FUNC_INT(node);
    StrBuf buf;
    StrBuf buf2;

    StrBuf_init(&buf, 0);
    if (!convert_str_to_bin(NULL, &buf, 1)) {
        StrBuf_close(&buf);
        return FALSE;
    }

    StrBuf_init_refstr(&buf2, buf.size);
    urlencode_sub(&buf2, buf.p, buf.size, plus);
    *vret = StrBuf_str_Value(&buf2, fs->cls_str);
    StrBuf_close(&buf);

    return TRUE;
}

static int str_urldecode(Value *vret, Value *v, RefNode *node)
{
    Str src = Value_str(v[1]);
    Str str;
    StrBuf buf;

    StrBuf_init(&buf, src.size);
    urldecode_sub(&buf, src.p, src.size);
    str = Str_new(buf.p, buf.size);

    if (!convert_bin_to_str(vret, &str, 1)) {
        StrBuf_close(&buf);
        return FALSE;
    }
    StrBuf_close(&buf);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

void define_lang_str_func(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "strcat", NODE_FUNC_N, 0);
    define_native_func_a(n, lang_strcat, 0, -1, NULL);
    fv->func_strcat = n;


    n = define_identifier(m, m, "hexencode", NODE_FUNC_N, 0);
    define_native_func_a(n, str_hexencode, 1, 3, NULL, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "hexdecode", NODE_FUNC_N, 0);
    define_native_func_a(n, str_hexdecode, 1, 3, NULL, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "b64encode", NODE_FUNC_N, 0);
    define_native_func_a(n, str_base64encode, 1, 3, (void*) FALSE, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "b64encode_url", NODE_FUNC_N, 0);
    define_native_func_a(n, str_base64encode, 1, 3, (void*) TRUE, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "b64decode", NODE_FUNC_N, 0);
    define_native_func_a(n, str_base64decode, 1, 3, NULL, fs->cls_sequence, NULL, fs->cls_bool);

    n = define_identifier(m, m, "urlencode", NODE_FUNC_N, 0);
    define_native_func_a(n, str_urlencode, 1, 3, (void*) FALSE, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "urlencode_form", NODE_FUNC_N, 0);
    define_native_func_a(n, str_urlencode, 1, 3, (void*) TRUE, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "urldecode", NODE_FUNC_N, 0);
    define_native_func_a(n, str_urldecode, 1, 3, NULL, fs->cls_sequence, NULL, fs->cls_bool);
}
void define_lang_str_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    RefNode *cls_matches = define_identifier(m, m, "Matches", NODE_CLASS, 0);
    RefNode *cls_seqiter = define_identifier(m, m, "SequenceIter", NODE_CLASS, 0);

    // Sequence
    cls = fs->cls_sequence;
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, sequence_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, sequence_hash, 0, 0, NULL);
    // NODEOPT_STRCLASSのデフォルト値として使用
    fv->refnode_sequence_hash = n;

    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_iter, 0, 0, cls_seqiter);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_mul, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier(m, cls, "find", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_find, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_find, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "match", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_match, 1, 1, cls_matches, NULL);
    n = define_identifier(m, cls, "replace", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_replace, 2, 2, cls_matches, NULL, NULL);
    n = define_identifier(m, cls, "split", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_split, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "splice", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_splice, 0, 3, NULL, fs->cls_int, fs->cls_int, NULL);
    n = define_identifier(m, cls, "toupper", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_caseconv, 0, 0, (void*)FALSE);
    n = define_identifier(m, cls, "tolower", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_caseconv, 0, 0, (void*)TRUE);
    n = define_identifier(m, cls, "trim", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_trim, 0, 0, (void*) FALSE);
    n = define_identifier(m, cls, "collapse", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_trim, 0, 0, (void*) TRUE);
    n = define_identifier(m, cls, "count", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_count, 1, 1, NULL, NULL);
    extends_method(cls, fs->cls_iterable);

    // Bytes
    cls = fs->cls_bytes;
    n = define_identifier(m, cls, "chr", NODE_NEW_N, 0);
    define_native_func_a(n, bytes_new_seq, 0, -1, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, sequence_marshal_read, 1, 1, (void*) FALSE, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, bytes_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, bytes_index, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_cmp, 1, 1, NULL, fs->cls_bytes);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, bytes_size, 0, 0, NULL);
    n = define_identifier(m, cls, "sub", NODE_FUNC_N, 0);
    define_native_func_a(n, bytes_substr, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "is_valid_encoding", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_is_valid_encoding, 1, 1, (void*) FALSE, fs->cls_charset);
    extends_method(cls, fs->cls_sequence);

    // Str
    cls = fs->cls_str;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, string_new, 0, 3, NULL, NULL, fs->cls_charset, fs->cls_bool);
    n = define_identifier(m, cls, "chr", NODE_NEW_N, 0);
    define_native_func_a(n, string_new_seq, 0, -1, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, sequence_marshal_read, 1, 1, (void*) TRUE, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, string_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, string_index, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_cmp, 1, 1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, string_size_utf8, 0, 0, NULL);
    n = define_identifier(m, cls, "sub", NODE_FUNC_N, 0);
    define_native_func_a(n, string_substr, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "to_bytes", NODE_FUNC_N, 0);
    define_native_func_a(n, string_tobytes, 1, 2, NULL, fs->cls_charset, fs->cls_bool);
    n = define_identifier(m, cls, "representable", NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_is_valid_encoding, 1, 1, (void*) TRUE, fs->cls_charset);
    extends_method(cls, fs->cls_sequence);

    // SequenceIter
    cls = cls_seqiter;
    cls->u.c.n_memb = INDEX_SEQITER_NUM;
    n = define_identifier_p(m, cls, fs->str_next, NODE_FUNC_N, 0);
    define_native_func_a(n, seqiter_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);

    // Regex
    cls = fs->cls_regex;
    cls->u.c.n_memb = INDEX_REGEX_NUM;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, regex_new, 1, 2, NULL, fs->cls_sequence, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, regex_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, regex_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, regex_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, regex_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, regex_eq, 1, 1, NULL, fs->cls_regex);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, regex_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier(m, cls, "source", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, regex_source, 0, 0, NULL);
    n = define_identifier(m, cls, "capture_count", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, regex_capture_count, 0, 0, NULL);
    n = define_identifier(m, cls, "capture_index", NODE_FUNC_N, 0);
    define_native_func_a(n, regex_capture_index, 1, 1, NULL, fs->cls_str);
    extends_method(cls, fs->cls_obj);

    // Matches
    cls = cls_matches;
    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, matches_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, matches_index, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    define_native_func_a(n, matches_index, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "begin", NODE_FUNC_N, 0);
    define_native_func_a(n, matches_position, 1, 1, (void*) FALSE, fs->cls_int);
    n = define_identifier(m, cls, "end", NODE_FUNC_N, 0);
    define_native_func_a(n, matches_position, 1, 1, (void*) TRUE, fs->cls_int);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, matches_size, 0, 0, NULL);
    n = define_identifier(m, cls, "source", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, matches_source, 0, 0, NULL);
    n = define_identifier(m, cls, "all", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, matches_index, 0, 0, NULL);
    extends_method(cls, fs->cls_obj);


    cls = define_identifier(m, m, "RegexError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
}


