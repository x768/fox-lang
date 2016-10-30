#include "fox_parse.h"
#include <stdio.h>
#include <string.h>


enum {
    ACCEPT_LANG_MAX = 32,
    LOCALE_LEN = 64,
};

enum {
    ADDSTR_CAPITALIZE,
    ADDSTR_LOWER,
    ADDSTR_UPPER,
};

typedef struct {
    RefHeader rh;

    Mem mem;
    const char *locale_tag;
    Value locale;
    Hash h;
} RefResource;

////////////////////////////////////////////////////////////////////////////////

static void make_locale_name(char *dst, int size, const char *name_p, int name_size)
{
    int i;
    int len = name_size;

    if (len == 0) {
        strcpy(dst, "!");
        return;
    }
    if (len > size - 1) {
        len = size - 1;
    }

    for (i = 0; i < len; i++) {
        char c = tolower(name_p[i]);
        if (c == '-' || c == '_') {
            dst[i] = '-';
        } else if (isalnum(c)) {
            dst[i] = c;
        } else {
            break;
        }
    }
    dst[len] = '\0';
}
static RefStr *locale_alias(const char *name_p, int name_size)
{
    static Hash locales;

    if (locales.entry == NULL) {
        Hash_init(&locales, &fg->st_mem, 64);
        load_aliases_file(&locales, "data" SEP_S "locale-alias.txt");
    }
    return Hash_get(&locales, name_p, name_size);
}
static void locale_filename_trim(char *fname, const char *suffix)
{
    char *p;
    for (p = fname + strlen(fname) - 1; p >= fname; p--) {
        if (*p == '-') {
            if (suffix != NULL) {
                strcpy(p, suffix);
            } else {
                *p = '\0';
            }
            return;
        }
    }
    fname[0] = '!';
    if (suffix != NULL) {
        strcpy(&fname[1], suffix);
    } else {
        fname[1] = '\0';
    }
}


static RefStr **get_best_locale_cgi(const char *p)
{
    int size = 0;
    RefStr **list = Mem_get(&fg->st_mem, ACCEPT_LANG_MAX * sizeof(RefStr*));
    double *qa = malloc(ACCEPT_LANG_MAX * sizeof(double));
    char cbuf[LOCALE_LEN];

    while (*p != '\0') {
        const char *top;
        const char *locale_raw_p;
        int locale_raw_size;
        double qv = 1.0;

        // ja ; q=1.0,
        while (*p != '\0' && isspace(*p)) {
            p++;
        }
        top = p;
        while (*p != '\0' && *p != ',' && *p != ';') {
            p++;
        }
        locale_raw_p = top;
        locale_raw_size = p - top;

        // タグ名以降を切り出す
        if (*p == ';') {
            p++;
            while (*p != '\0' && isspace(*p)) {
                p++;
            }
            if (p[0] == 'q' && p[1] == '=') {
                p += 2;
                qv = strtod(p, NULL);
            }
            while (*p != '\0' && *p != ',') {
                p++;
            }
        }
        if (*p != '\0') {
            p++;
        }

        make_locale_name(cbuf, LOCALE_LEN, locale_raw_p, locale_raw_size);
        // localeのエイリアスの解決
        {
            RefStr *alias = locale_alias(cbuf, -1);
            if (alias != NULL) {
                memcpy(cbuf, alias->c, alias->size);
                cbuf[alias->size] = '\0';
            }
        }
        list[size] = intern(cbuf, -1);
        qa[size] = qv;
        size++;
        if (size >= ACCEPT_LANG_MAX - 1) {
            break;
        }
    }

    // 降順ソート
    {
        int i, j;
        for (i = 0; i < size - 1; i++) {
            for (j = i + 1; j < size; j++) {
                if (qa[i] < qa[j]) {
                    RefStr *tmp = list[i];
                    double dtmp = qa[i];
                    list[i] = list[j];
                    list[j] = tmp;
                    qa[i] = qa[j];
                    qa[j] = dtmp;
                }
            }
        }
    }
    list[size] = NULL;
    free(qa);

    return list;
}

/**
 * 優先度の高いlocaleから順番に入っているので
 * 呼び出し元で利用可能なリソースと一致するか調べる
 */
RefStr **get_best_locale_list()
{
    static RefStr **list;

    if (list != NULL) {
        return list;
    }

    if (fs->running_mode == RUNNING_MODE_CGI) {
        const char *lang = Hash_get(&fs->envs, "HTTP_ACCEPT_LANGUAGE", -1);
        if (lang != NULL) {
            return get_best_locale_cgi(lang);
        } else {
            list = Mem_get(&fg->st_mem, sizeof(RefStr*) * 2);
            list[0] = fs->str_0;
            list[1] = NULL;
        }
    } else {
        const char *lang = Hash_get(&fs->envs, "FOX_LANG", -1);
        if (lang == NULL) {
            lang = get_default_locale();
        }
        if (lang != NULL) {
            int i;
            char cbuf[LOCALE_LEN];
            int lang_size = strlen(lang);
            int hyphen_count = 0;

            // .以降を除去
            for (i = 0; i < lang_size; i++) {
                if (lang[i] == '.') {
                    break;
                }
            }
            lang_size = i;
            make_locale_name(cbuf, LOCALE_LEN, lang, lang_size);
            // localeのエイリアスの解決
            {
                RefStr *alias = locale_alias(cbuf, -1);
                if (alias != NULL) {
                    memcpy(cbuf, alias->c, alias->size);
                    cbuf[alias->size] = '\0';
                }
            }
            for (i = 0; cbuf[i] != '\0'; i++) {
                if (cbuf[i] == '-') {
                    hyphen_count++;
                }
            }
            // -を除去

            list = Mem_get(&fg->st_mem, sizeof(RefStr*) * (hyphen_count + 2));
            for (i = 0; i < hyphen_count; i++) {
                list[0] = intern(cbuf, -1);
                locale_filename_trim(cbuf, NULL);
            }
            list[i++] = intern(cbuf, -1);
            list[i] = NULL;
        } else {
            // ニュートラル言語
            list = Mem_get(&fg->st_mem, sizeof(RefStr*) * 2);
            list[0] = fs->str_0;
            list[1] = NULL;
        }
    }
    return list;
}

LocaleData *Value_locale_data(Value v)
{
    Ref *r = Value_ref(v);
    return Value_ptr(r->v[INDEX_LOCALE_LOCALE]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// 過剰の場合エラー
static int parse_ini_array(const char **ret, int num, Tok *tk, Mem *mem)
{
    int i = 0;
    for (;;) {
        if (tk->v.type != TL_STR) {
            return FALSE;
        }
        if (i < num && ret[i] == NULL) {
            ret[i] = str_dup_p(tk->str_val.p, tk->str_val.size, mem);
        }
        i++;
        Tok_simple_next(tk);
        if (tk->v.type != T_COMMA) {
            return i <= num;
        }
        Tok_simple_next(tk);
    }
}

static int parse_locale_word(const char *src, const char *end)
{
    const char *p = src;

    while (p < end && isalpha(*p & 0xFF)) {
        p++;
    }
    return p - src;
}
static int parse_locale_digit(const char *src, const char *end)
{
    const char *p = src;

    while (p < end && isdigit_fox(*p)) {
        p++;
    }
    return p - src;
}
static void parse_locale_addstr(Value *v, const char *p, int size, int mode)
{
    int i = 0;
    RefStr *rs = refstr_new_n(fs->cls_str, size);

    if (mode == ADDSTR_CAPITALIZE) {
        rs->c[i] = toupper(p[i]);
        i++;
    }
    for (; i < size; i++) {
        if (mode == ADDSTR_UPPER) {
            rs->c[i] = toupper(p[i]);
        } else {
            rs->c[i] = tolower(p[i]);
        }
    }
    rs->c[i] = '\0';
    *v = vp_Value(rs);
}
static int parse_separator(const char **pp, const char *end, int loose)
{
    const char *p = *pp;

    if (loose) {
        while (p < end && !isalnum(*p & 0xFF)) {
            p++;
        }
        *pp = p;
        return TRUE;
    } else {
        if (p >= end) {
            return TRUE;
        }
        if (*p == '-' || *p == '_') {
            p++;
            *pp = p;
            return TRUE;
        }
        return FALSE;
    }
}

static int parse_locale_tag(Ref *r, const char *src, int src_size, int loose)
{
    const char *end = src + src_size;
    int n;
    int subtag = '\0';

    // (?<language>[a-z]{2,3}) 必須
    n = parse_locale_word(src, end);
    if (n != 2 && n != 3) {
        if (loose) {
            // NEUTRAL
            return TRUE;
        } else {
            return FALSE;
        }
    }
    parse_locale_addstr(&r->v[INDEX_LOCALE_LANGUAGE], src, n, ADDSTR_LOWER);
    src += n;

    while (src < end) {
        if (!parse_separator(&src, end, loose)) {
            return FALSE;
        }
        if (src >= end) {
            break;
        }
        if (isdigit_fox(*src)) {
            // (?<territory>[0-9]{3})
            // (?<variant>[0-9]{4,})
            n = parse_locale_digit(src, end);
            if (n == 3) {
                if (r->v[INDEX_LOCALE_TERRITORY] == VALUE_NULL) {
                    r->v[INDEX_LOCALE_TERRITORY] = cstr_Value(fs->cls_str, src, n);
                } else if (!loose) {
                    return FALSE;
                }
            } else if (n >= 4) {
                if (r->v[INDEX_LOCALE_VARIANT] == VALUE_NULL) {
                    r->v[INDEX_LOCALE_VARIANT] = cstr_Value(fs->cls_str, src, n);
                } else if (!loose) {
                    return FALSE;
                }
            } else if (!loose) {
                return FALSE;
            }
        } else {
            // (?<extention>[a-z]-[a-z]{2,8})*
            // (?<extended>[a-z]{3})
            // (?<script>[a-z]{4})
            // (?<territory>[a-z]{2})
            // (?<variant>[a-z]{5,})
            n = parse_locale_word(src, end);
            if (subtag != '\0') {
                
                subtag = '\0';
            } else {
                if (n == 1) {
                    subtag = tolower_fox(*src);
                    src++;
                } else if (n == 2) {
                    if (r->v[INDEX_LOCALE_TERRITORY] == VALUE_NULL) {
                        parse_locale_addstr(&r->v[INDEX_LOCALE_TERRITORY], src, n, ADDSTR_UPPER);
                    } else if (!loose) {
                        return FALSE;
                    }
                } else if (n == 3) {
                    if (r->v[INDEX_LOCALE_EXTENDED] == VALUE_NULL) {
                        parse_locale_addstr(&r->v[INDEX_LOCALE_EXTENDED], src, n, ADDSTR_LOWER);
                    } else if (!loose) {
                        return FALSE;
                    }
                } else if (n == 4) {
                    if (r->v[INDEX_LOCALE_SCRIPT] == VALUE_NULL) {
                        parse_locale_addstr(&r->v[INDEX_LOCALE_SCRIPT], src, n, ADDSTR_CAPITALIZE);
                    } else if (!loose) {
                        return FALSE;
                    }
                } else {
                    if (r->v[INDEX_LOCALE_VARIANT] == VALUE_NULL) {
                        parse_locale_addstr(&r->v[INDEX_LOCALE_VARIANT], src, n, ADDSTR_LOWER);
                    } else if (!loose) {
                        return FALSE;
                    }
                }
            }
        }
        src += n;
    }
    
    return TRUE;
}

static char *locale_ref_to_filename(Ref *r)
{
    if (r->v[INDEX_LOCALE_LANGUAGE] == VALUE_NULL) {
        char *p_str = malloc(2);
        strcpy(p_str, "!");
        return p_str;
    } else {
        int i, j;
        int first = TRUE;
        StrBuf sb;

        StrBuf_init(&sb, 16);
        for (i = INDEX_LOCALE_LANGUAGE; i < INDEX_LOCALE_EXTENTIONS; i++) {
            RefStr *rs = Value_vp(r->v[i]);
            if (rs != NULL) {
                if (first) {
                    first = FALSE;
                } else {
                    StrBuf_add_c(&sb, '-');
                }
                for (j = 0; j < rs->size; j++) {
                    StrBuf_add_c(&sb, tolower_fox(rs->c[j]));
                }
            }
        }
        StrBuf_add_c(&sb, '\0');
        return sb.p;
    }
}
static int load_locale_from_file(LocaleData *loc, const char *fname)
{
    Tok tk;
    char *buf = read_from_file(NULL, fname, NULL);

    if (buf == NULL) {
        return FALSE;
    }
    Tok_simple_init(&tk, buf);

    Tok_simple_next(&tk);
    for (;;) {
    LINE_BEGIN:
        switch (tk.v.type) {
        case TL_VAR:
        case TL_CONST:
        case TL_CLASS: {
            Str key = tk.str_val;
            Tok_simple_next(&tk);
            if (tk.v.type != T_LET) {
                goto ERROR_END;
            }
            Tok_simple_next(&tk);
            switch (key.p[0]) {
            case 'a':
                if (str_eq(key.p, key.size, "am_pm", -1)) {
                    if (!parse_ini_array(loc->am_pm, lengthof(loc->am_pm), &tk, &loc->mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                }
                break;
            case 'b':
                if (str_eq(key.p, key.size, "bidi", -1)) {
                    if (tk.v.type != TL_VAR) {
                        goto ERROR_END;
                    }
                    if (loc->bidi == '\0') {
                        if (str_eq(tk.str_val.p, tk.str_val.size, "ltr", -1)) {
                            loc->bidi = 'L';
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "rtl", -1)) {
                            loc->bidi = 'R';
                        } else {
                            goto ERROR_END;
                        }
                    }
                    Tok_simple_next(&tk);
                    goto LINE_END;
                }
                break;
            case 'd':
                if (str_eq(key.p, key.size, "date", -1)) {
                    if (!parse_ini_array(loc->date, lengthof(loc->date), &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                } else if (str_eq(key.p, key.size, "decimal", -1)) {
                    if (!parse_ini_array(&loc->decimal, 1, &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                } else if (str_eq(key.p, key.size, "days", -1)) {
                    if (!parse_ini_array(loc->week, lengthof(loc->week), &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                } else if (str_eq(key.p, key.size, "days_w", -1)) {
                    if (!parse_ini_array(loc->week_w, lengthof(loc->week_w), &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                }
                break;
            case 'g':
                if (str_eq(key.p, key.size, "group", -1)) {
                    if (!parse_ini_array(&loc->group, 1, &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                } else if (str_eq(key.p, key.size, "group_n", -1)) {
                    if (tk.v.type != TL_INT) {
                        goto ERROR_END;
                    }
                    if (loc->group_n == 0) {
                        loc->group_n = tk.int_val;
                    }
                    Tok_simple_next(&tk);
                    goto LINE_END;
                }
                break;
            case 'l':
                if (str_eq(key.p, key.size, "langtag", -1)) {
                    if (tk.v.type != TL_STR) {
                        goto ERROR_END;
                    }
                    if (loc->langtag == NULL) {
                        loc->langtag = intern(tk.str_val.p, tk.str_val.size);
                    }
                    Tok_simple_next(&tk);
                    goto LINE_END;
                }
                break;
            case 'm':
                if (str_eq(key.p, key.size, "month", -1)) {
                    if (!parse_ini_array(loc->month, lengthof(loc->month), &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                } else if (str_eq(key.p, key.size, "month_w", -1)) {
                    if (!parse_ini_array(loc->month_w, lengthof(loc->month_w), &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                }
                break;
            case 't':
                if (str_eq(key.p, key.size, "time", -1)) {
                    if (!parse_ini_array(loc->time, lengthof(loc->time), &tk, &fg->st_mem)) {
                        goto ERROR_END;
                    }
                    goto LINE_END;
                }
                break;
            }
            throw_errorf(fs->mod_locale, "ResourceError", "Unknown key %Q at line %d (%s)", key, tk.v.line, fname);
            free(buf);
            return FALSE;
        }
        case T_NL:
            Tok_simple_next(&tk);
            goto LINE_BEGIN;
        case T_EOF:
            free(buf);
            return TRUE;
        case T_ERR:
            free(buf);
            return FALSE;
        default:
            goto ERROR_END;
        }
    LINE_END:
        if (tk.v.type != T_NL && tk.v.type != T_EOF) {
            goto ERROR_END;
        }
    }

ERROR_END:
    throw_errorf(fs->mod_locale, "ResourceError", "Error at line %d (%s)", tk.v.line, fname);
    free(buf);
    return FALSE;
}
static LocaleData *get_locale_from_ref(Ref *r)
{
    enum {
        INDEX_LOCALE_REFSTR_NUM = INDEX_LOCALE_EXTENTIONS - INDEX_LOCALE_LANGUAGE,
    };
    Mem mem;
    LocaleData *loc;
    char *filename = locale_ref_to_filename(r);

    Mem_init(&mem, 256);
    loc = Mem_get(&mem, sizeof(LocaleData));
    memset(loc, 0, sizeof(LocaleData));
    loc->mem = mem;

    for (;;) {
        // resource_pathを順に探索
        PtrList *res_path;

        for (res_path = fs->resource_path; res_path != NULL; res_path = res_path->next) {
            char *fname = str_printf("%s" SEP_S "locale" SEP_S "%s%s", res_path->u.c, filename, ".txt");
            if (exists_file(fname) == EXISTS_FILE) {
                if (!load_locale_from_file(loc, fname)) {
                    Mem_close(&loc->mem);
                    free(fname);
                    return NULL;
                }
            }
            free(fname);
        }
        if (filename[0] == '!') {
            break;
        }
        locale_filename_trim(filename, NULL);
    }

    free(filename);
    return loc;
}
static void locale_tag_strbuf(StrBuf *sb, Ref *r)
{
    if (r->v[INDEX_LOCALE_LANGUAGE] != VALUE_NULL) {
        int i;
        StrBuf_add_r(sb, Value_vp(r->v[INDEX_LOCALE_LANGUAGE]));
        for (i = INDEX_LOCALE_LANGUAGE + 1; i <= INDEX_LOCALE_VARIANT; i++) {
            if (r->v[i] != VALUE_NULL) {
                StrBuf_add_c(sb, '_');
                StrBuf_add_r(sb, Value_vp(r->v[i]));
            }
        }
    }
}
static Ref *get_locale_from_name(const char *name, int name_size, int loose)
{
    Ref *r = ref_new(fs->cls_locale);
    LocaleData *loc;

    if (!parse_locale_tag(r, name, name_size, FALSE)) {
        throw_errorf(fs->mod_lang, "ValueError", "Illformed locale string");
        return NULL;
    }
    {
        StrBuf sb;
        StrBuf_init_refstr(&sb, 32);
        locale_tag_strbuf(&sb, r);
        r->v[INDEX_LOCALE_LANGTAG] = StrBuf_str_Value(&sb, fs->cls_str);
    }

    // 最も適合するLocaleを取得
    loc = get_locale_from_ref(r);
    if (loc == NULL) {
        unref(vp_Value(r));
        return NULL;
    }
    r->v[INDEX_LOCALE_LOCALE] = ptr_Value(loc);

    return r;
}
static Ref *get_locale_neutral(void)
{
    Ref *r = ref_new(fs->cls_locale);
    fv->loc_neutral = get_locale_from_ref(r);
    r->v[INDEX_LOCALE_LANGTAG] = vp_Value(fs->str_0);
    r->v[INDEX_LOCALE_LOCALE] = ptr_Value(fv->loc_neutral);
    return r;
}
static int locale_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);

    if (rs->size == 1 && rs->c[0] == '!') {
        *vret = vp_Value(get_locale_neutral());
    } else {
        Ref *r = get_locale_from_name(rs->c, rs->size, FALSE);
        if (r == NULL) {
            return FALSE;
        }
        *vret = vp_Value(r);
    }
    return TRUE;
}
static int locale_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint32_t size;
    int rd_size;

    if (!stream_read_uint32(r, &size)) {
        return FALSE;
    }
    if (size > 256) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        return FALSE;
    }
    rd_size = size;
    if (size == 0) {
        *vret = vp_Value(get_locale_neutral());
    } else {
        char *cbuf = malloc(size + 1);
        Ref *r_loc;
        if (!stream_read_data(r, NULL, cbuf, &rd_size, FALSE, TRUE)) {
            return FALSE;
        }
        cbuf[rd_size] = '\0';
        r_loc = get_locale_from_name(cbuf, rd_size, FALSE);
        if (r_loc == NULL) {
            return FALSE;
        }
        *vret = vp_Value(r_loc);
    }
    return TRUE;
}
static int locale_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefStr *langtag = Value_vp(r->v[INDEX_LOCALE_LANGTAG]);
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];

    if (langtag != NULL) {
        if (!stream_write_uint32(w, langtag->size)) {
            return FALSE;
        }
        if (!stream_write_data(w, langtag->c, langtag->size)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int locale_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    LocaleData *loc = Value_ptr(r->v[INDEX_LOCALE_LOCALE]);
    if (loc != NULL) {
        Mem_close(&loc->mem);
        r->v[INDEX_LOCALE_LOCALE] = VALUE_NULL;
    }
    return TRUE;
}
static int locale_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefStr *tagname = Value_vp(r->v[INDEX_LOCALE_LANGTAG]);
    StrBuf sb;

    StrBuf_init_refstr(&sb, 32);
    StrBuf_add(&sb, "Locale(", -1);
    if (tagname->size > 0) {
        StrBuf_add_r(&sb, tagname);
    } else {
        StrBuf_add(&sb, "NEUTRAL", -1);
    }
    StrBuf_add_c(&sb, ')');
    *vret = StrBuf_str_Value(&sb, fs->cls_str);

    return TRUE;
}
static int locale_hash(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefStr *rs = Value_vp(r->v[INDEX_LOCALE_LANGTAG]);
    uint32_t hash = str_hash(rs->c, rs->size);
    *vret = int32_Value(hash & INT32_MAX);
    return TRUE;
}
static int locale_eq(Value *vret, Value *v, RefNode *node)
{
    Ref *r1 = Value_ref(*v);
    Ref *r2 = Value_ref(v[1]);
    int result = refstr_eq(Value_vp(r1->v[INDEX_LOCALE_LANGTAG]), Value_vp(r2->v[INDEX_LOCALE_LANGTAG]));
    *vret = bool_Value(result);
    return TRUE;
}
static int locale_get_param(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int idx = FUNC_INT(node);

    *vret = Value_cp(r->v[idx]);

    return TRUE;
}
static int locale_data_langtag(Value *vret, Value *v, RefNode *node)
{
    LocaleData *loc = Value_locale_data(*v);
    if (loc != NULL && loc->langtag != NULL) {
        *vret = vp_Value(loc->langtag);
    }
    return TRUE;
}
static int locale_data_bidi(Value *vret, Value *v, RefNode *node)
{
    LocaleData *loc = Value_locale_data(*v);
    if (loc != NULL) {
        switch (loc->bidi) {
        case 'L':
            *vret = cstr_Value(fs->cls_str, "ltr", 3);
            break;
        case 'R':
            *vret = cstr_Value(fs->cls_str, "rtl", 3);
            break;
        }
    }
    return TRUE;
}

static void locale_put_array(StrBuf *buf, const char **arr, int size)
{
    enum {
        SEP_SIZE = 3,
    };
    const char *sep = "\",\"";
    int i;

    for (i = 0; i < size; i++) {
        if (i > 0) {
            StrBuf_add(buf, sep, SEP_SIZE);
        }
        if (arr[i] != NULL) {
            add_backslashes_sub(buf, arr[i], -1, ADD_BACKSLASH_U_UCS2);
        }
    }
}
/**
 * localeの情報をjson文字列で返す
 */
static int locale_data_json(Value *vret, Value *v, RefNode *node)
{
    LocaleData *loc = Value_locale_data(*v);
    if (loc != NULL) {
        StrBuf buf;

        StrBuf_init_refstr(&buf, 0);
        StrBuf_add(&buf, "{\"tag\":\"", -1);
        StrBuf_add_r(&buf, loc->langtag);

        StrBuf_add(&buf, "\",\"month\":[\"", -1);
        locale_put_array(&buf, loc->month, lengthof(loc->month));

        StrBuf_add(&buf, "\"],\"month_w\":[\"", -1);
        locale_put_array(&buf, loc->month_w, lengthof(loc->month_w));

        StrBuf_add(&buf, "\"],\"days\":[\"", -1);
        locale_put_array(&buf, loc->week, lengthof(loc->week));

        StrBuf_add(&buf, "\"],\"days_w\":[\"", -1);
        locale_put_array(&buf, loc->week_w, lengthof(loc->week_w));

        StrBuf_add(&buf, "\"],\"date\":[\"", -1);
        locale_put_array(&buf, loc->date, lengthof(loc->date));

        StrBuf_add(&buf, "\"],\"time\":[\"", -1);
        locale_put_array(&buf, loc->time, lengthof(loc->time));

        StrBuf_add(&buf, "\"],\"am\":\"", -1);
        add_backslashes_sub(&buf, loc->am_pm[0], -1, ADD_BACKSLASH_U_UCS2);
        StrBuf_add(&buf, "\",\"pm\":\"", -1);
        add_backslashes_sub(&buf, loc->am_pm[1], -1, ADD_BACKSLASH_U_UCS2);

        StrBuf_add(&buf, "\",\"bidi\":\"", -1);
        StrBuf_add(&buf, (loc->bidi == 'R' ? "rtl\"}" : "ltr\"}"), 5);

        *vret = StrBuf_str_Value(&buf, fs->cls_str);
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////

char *resource_to_path(Str name_s, const char *ext_p)
{
    char *name_p = str_dup_p(name_s.p, name_s.size, NULL);
    PtrList *lst;
    int i;

    if (ext_p == NULL) {
        ext_p = "";
    }

    // 識別子文字以外はエラー
    for (i = 0; i < name_s.size; i++) {
        if (name_p[i] == '.') {
            name_p[i] = SEP_C;
        } else if (!isalnumu_fox(name_p[i])) {
            goto ERROR_END;
        }
    }

    for (lst = fs->resource_path; lst != NULL; lst = lst->next) {
        char *path = str_printf("%s" SEP_S "%s%s", lst->u.c, name_p, ext_p);
        if (exists_file(path) == EXISTS_FILE) {
            free(name_p);
            return path;
        }
        free(path);
        path = NULL;
    }

ERROR_END:
    throw_errorf(fs->mod_file, "FileOpenError", "Cannot find file for %Q", name_s);
    free(name_p);
    return NULL;
}

static int load_resource_sub(RefResource *res, const char *fname)
{
    Tok tk;
    char *buf = read_from_file(NULL, fname, NULL);

    if (buf == NULL) {
        // ファイルが見つからない場合は何もしない
        return TRUE;
    }
    Tok_simple_init(&tk, buf);

    Tok_simple_next(&tk);
    for (;;) {
    LINE_BEGIN:
        switch (tk.v.type) {
        case TL_VAR:
        case TL_CONST:
        case TL_CLASS: {
            Str key = tk.str_val;
            Tok_simple_next(&tk);
            if (tk.v.type != T_LET) {
                goto ERROR_END;
            }
            Tok_simple_next(&tk);
            if (tk.v.type != TL_STR) {
                goto ERROR_END;
            }
            if (str_eq(key.p, key.size, "locale", -1)) {
                if (tk.v.type != TL_STR) {
                    goto ERROR_END;
                }
                if (res->locale_tag == NULL) {
                    res->locale_tag = str_dup_p(tk.str_val.p, tk.str_val.size, &res->mem);
                }
            } else if (Hash_get(&res->h, key.p, key.size) == NULL) {
                if (tk.v.type == TL_STR) {
                    Str *ps = Mem_get(&res->mem, sizeof(Str));
                    ps->p = str_dup_p(tk.str_val.p, tk.str_val.size, &res->mem);
                    ps->size = tk.str_val.size;
                    Hash_add_p(&res->h, &res->mem, intern(key.p, key.size), ps);
                } else {
                    goto ERROR_END;
                }
            } else {
            }
            Tok_simple_next(&tk);
            break;
        }
        case T_NL:
            Tok_simple_next(&tk);
            goto LINE_BEGIN;
        case T_EOF:
            free(buf);
            return TRUE;
        case T_ERR:
            free(buf);
            return FALSE;
        default:
            goto ERROR_END;
        }
        if (tk.v.type != T_NL && tk.v.type != T_EOF) {
            goto ERROR_END;
        }
    }

ERROR_END:
    throw_errorf(fs->mod_locale, "ResourceError", "Error at line %d (%s)", tk.v.line, fname);
    free(buf);
    return FALSE;
}
static char *fname_pos(char *fpath)
{
    char *p = fpath + strlen(fpath) - 1;
    while (p > fpath) {
        if (p[-1] == SEP_C) {
            return p;
        }
        p--;
    }
    return fpath;
}
static RefResource *load_resource_from_locale(RefStr *name_r, const char *loc_name)
{
    // resource_pathを順に探索
    PtrList *res_path;

    for (res_path = fs->resource_path; res_path != NULL; res_path = res_path->next) {
        char *fpath = str_printf("%s" SEP_S "%r" SEP_S "%s.txt", res_path->u.c, name_r, loc_name);
        char *fname = fname_pos(fpath);
        int found = FALSE;

        for (;;) {
            if (exists_file(fpath) == EXISTS_FILE) {
                found = TRUE;
                break;
            }
            if (fname[0] == '!') {
                break;
            }
            locale_filename_trim(fname, ".txt");
        }
        if (found) {
            //load_resource
            RefResource *res = buf_new(fs->cls_resource, sizeof(RefResource));
            Mem_init(&res->mem, 256);
            Hash_init(&res->h, &res->mem, 32);
            res->locale_tag = NULL;
            res->locale = VALUE_NULL;
            for (;;) {
                if (!load_resource_sub(res, fpath)) {
                    free(fpath);
                    unref(vp_Value(res));
                    return NULL;
                }
                if (fname[0] == '!') {
                    break;
                }
                locale_filename_trim(fname, ".txt");
            }
            free(fpath);
            return res;
        }
    }
    return NULL;
}
static int resource_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *name_r = Value_vp(v[1]);
    RefResource *res = NULL;

    if (fg->stk_top > v + 2) {
        char *name = locale_ref_to_filename(Value_ref(v[2]));
        res = load_resource_from_locale(name_r, name);
    } else {
        // load best resource
        RefStr **list = get_best_locale_list();
        int i;
        for (i = 0; list[i] != NULL; i++) {
            const char *name = list[i]->c;
            res = load_resource_from_locale(name_r, name);
            if (res != NULL) {
                break;
            }
        }
    }
    if (res != NULL) {
        *vret = vp_Value(res);
        return TRUE;
    } else {
        if (fg->error == VALUE_NULL) {
            throw_errorf(fs->mod_locale, "ResourceError", "Cannot open resource file");
        }
        return FALSE;
    }
}

static int resource_dispose(Value *vret, Value *v, RefNode *node)
{
    RefResource *res = Value_vp(*v);
    unref(res->locale);
    res->locale = VALUE_NULL;
    Mem_close(&res->mem);
    return TRUE;
}

/**
 * 文字列をそのまま返す
 */
static int resource_getstr(Value *vret, Value *v, RefNode *node)
{
    RefResource *res = Value_vp(*v);
    RefStr *key = Value_vp(v[1]);
    Str *ps = Hash_get(&res->h, key->c, key->size);

    if (ps != NULL) {
        *vret = cstr_Value(fs->cls_str, ps->p, ps->size);
        return TRUE;
    } else {
        throw_errorf(fs->mod_lang, "IndexError", "Resource key %r not found", key);
        return FALSE;
    }

    return TRUE;
}

static int resource_locale(Value *vret, Value *v, RefNode *node)
{
    RefResource *res = Value_vp(*v);

    if (res->locale == VALUE_NULL) {
        const char *tag = res->locale_tag;
        if (tag[0] == '!' && tag[1] == '\0') {
            res->locale = vp_Value(get_locale_neutral());
        } else {
            res->locale = vp_Value(get_locale_from_name(tag, strlen(tag), FALSE));
        }
    }
    *vret = Value_cp(res->locale);
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////

static int get_resource_sub(Value *vret, const char *path, RefNode *type)
{
    int64_t size;
    FileHandle fd = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);

    if (fd == -1) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
        return FALSE;
    }
    size = get_file_size(fd);
    if (size == -1 || size >= fs->max_alloc) {
        close_fox(fd);
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
        return FALSE;
    }

    if (type == fs->cls_streamio) {
        RefBytesIO *bio = bytesio_new_sub(NULL, 0);
        StrBuf_alloc(&bio->buf, size);
        *vret = vp_Value(bio);
        if (size > 0) {
            int rd = read_fox(fd, bio->buf.p, size);
            if (rd >= 0) {
                bio->buf.size = rd;
            }
        }
    } else {
        RefStr *rs = refstr_new_n(type, size);
        *vret = vp_Value(rs);
        if (size > 0) {
            int rd = read_fox(fd, rs->c, size);
            if (rd >= 0) {
                rs->c[rd] = '\0';
                rs->size = rd;
                if (type == fs->cls_str) {
                    if (invalid_utf8_pos(rs->c, rs->size) >= 0) {
                        throw_error_select(THROW_INVALID_UTF8);
                        return FALSE;
                    }
                }
            } else {
                rs->c[0] = '\0';
                rs->size = 0;
            }
        }
    }
    return TRUE;
}
static int get_resource_data(Value *vret, Value *v, RefNode *node)
{
    RefNode *type = FUNC_VP(node);
    char *path;
    RefStr *name_s = Value_vp(v[1]);
    RefStr *ext_s = Value_vp(v[2]);
    char *ext = malloc(ext_s->size + 4);

    ext[0] = '.';
    strcpy(ext + 1, ext_s->c);
    path = resource_to_path(Str_new(name_s->c, name_s->size), ext);
    free(ext);

    if (path == NULL) {
        return FALSE;
    }
    if (!get_resource_sub(vret, path, type)) {
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////

static int locale_get_best(Value *vret, Value *v, RefNode *node)
{
    int i;
    RefArray *ra = refarray_new(0);
    RefStr **list = get_best_locale_list();

    *vret = vp_Value(ra);

    for (i = 0; list[i] != NULL; i++) {
        Ref *loc = get_locale_from_name(list[i]->c, list[i]->size, FALSE);
        if (loc != NULL) {
            Value *vp = refarray_push(ra);
            *vp = vp_Value(loc);
        }
    }

    return TRUE;
}

// RESOURCE_PATHが設定された直後に呼び出す
void set_neutral_locale()
{
    RefNode *cls = fs->cls_locale;
    RefNode *n = define_identifier(cls->defined_module, cls, "NEUTRAL", NODE_CONST, 0);
    n->u.k.val = vp_Value(get_locale_neutral());
}

////////////////////////////////////////////////////////////////////////////

static void define_locale_const(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "BEST_LOCALE", NODE_CONST_U_N, 0);
    define_native_func_a(n, locale_get_best, 0, 0, NULL);
}

static void define_locale_func(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "resource_bytes", NODE_FUNC_N, 0);
    define_native_func_a(n, get_resource_data, 2, 2, fs->cls_bytes, fs->cls_str, fs->cls_str);

    n = define_identifier(m, m, "resource_str", NODE_FUNC_N, 0);
    define_native_func_a(n, get_resource_data, 2, 2, fs->cls_str, fs->cls_str, fs->cls_str);

    n = define_identifier(m, m, "resource_stream", NODE_FUNC_N, 0);
    define_native_func_a(n, get_resource_data, 2, 2, fs->cls_streamio, fs->cls_str, fs->cls_str);
}
static void define_locale_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    // Locale
    redefine_identifier(m, m, "Locale", NODE_CLASS, 0, fs->cls_locale);
    cls = fs->cls_locale;
    cls->u.c.n_memb = INDEX_LOCALE_NUM;

    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, locale_new, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, locale_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    define_native_func_a(n, locale_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, locale_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, locale_eq, 1, 1, NULL, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, locale_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier(m, cls, "langtag", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_get_param, 0, 0, (void*)INDEX_LOCALE_LANGTAG);
    n = define_identifier(m, cls, "language", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_get_param, 0, 0, (void*)INDEX_LOCALE_LANGUAGE);
    n = define_identifier(m, cls, "extended", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_get_param, 0, 0, (void*)INDEX_LOCALE_EXTENDED);
    n = define_identifier(m, cls, "script", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_get_param, 0, 0, (void*)INDEX_LOCALE_SCRIPT);
    n = define_identifier(m, cls, "territory", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_get_param, 0, 0, (void*)INDEX_LOCALE_TERRITORY);
    n = define_identifier(m, cls, "extentions", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_get_param, 0, 0, (void*)INDEX_LOCALE_EXTENTIONS);

    n = define_identifier(m, cls, "data_langtag", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_data_langtag, 0, 0, NULL);
    n = define_identifier(m, cls, "data_bidi", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_data_bidi, 0, 0, NULL);
    n = define_identifier(m, cls, "data_json", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, locale_data_json, 0, 0, NULL);

    extends_method(cls, fs->cls_obj);

    // Resource
    cls = define_identifier(m, m, "Resource", NODE_CLASS, 0);
    fs->cls_resource = cls;

    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, resource_new, 1, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    define_native_func_a(n, resource_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    define_native_func_a(n, resource_getstr, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, resource_getstr, 1, 1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "locale", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, resource_locale, 0, 0, NULL);

    extends_method(cls, fs->cls_obj);


    cls = define_identifier(m, m, "ResourceError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
}

void init_locale_module_1()
{
    RefNode *m = Module_new_sys("locale");

    define_locale_class(m);
    define_locale_func(m);
    define_locale_const(m);
    m->u.m.loaded = TRUE;
    fs->mod_locale = m;
}
