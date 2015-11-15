#include "fox_vm.h"
#include <string.h>


static Value v_res;

static RefNode *mod_cgi;
static RefNode *cls_cookie;

static int cookie_has_expires;
static int64_t cookie_expires;

static Value ref_map_post;
static Value ref_map_cookie;
static Value ref_set_cookie;


static void for_cgi_mode_error(void)
{
    throw_errorf(fs->mod_lang, "NotSupportedError", "This function is only for cgi-mode");
}

void cgi_init_responce()
{
    Value key;
    HashValueEntry *ve;
    RefMap *rm = refmap_new(32);
    v_res = vp_Value(rm);
    rm->rh.type = fv->cls_mimeheader;

    key = cstr_Value(fs->cls_str, "Content-Type", -1);
    ve = refmap_add(rm, key, TRUE, FALSE);
    ve->val = printf_Value("text/plain; charset=UTF-8");
}

int is_content_type_html()
{
    Str type;
    int found = FALSE;

    if (!mimedata_get_header_sub(&found, &type, Value_vp(v_res), "Content-Type", -1, NULL, 0)) {
        return TRUE;
    }
    if (found) {
        if (str_eq(type.p, type.size, "text/html", -1)) {
            return TRUE;
        }
    }
    return FALSE;
}

static const char *get_cookie_cfg(const char *key)
{
    const char *p = Hash_get(&fs->envs, key, -1);
    if (p != NULL && is_string_only_ascii(p, -1, " =;,")) {
        return p;
    } else {
        return NULL;
    }
}
static int get_cookie_cfg_bool(const char *key)
{
    const char *p = Hash_get(&fs->envs, key, -1);
    if (p != NULL) {
        if (strcmp(p, "y") == 0 || strcmp(p, "yes") == 0 || strcmp(p, "t") == 0 || strcmp(p, "true") == 0) {
            return TRUE;
        }
    }
    return FALSE;
}
static void write_http_cookie(
    StrBuf *buf, HashValueEntry *et,
    const char *cookie_path, const char *cookie_domain,
    int cookie_secure, int cookie_httponly, int64_t now)
{
    StrBuf_add(buf, "Set-Cookie: ", -1);
    StrBuf_add_r(buf, Value_vp(et->key));
    StrBuf_add_c(buf, '=');

    if (et->val == VALUE_NULL) {
        char c_buf[48];

        // 期限を1日前に設定(削除する)
        timestamp_to_cookie_date(now - 86400000LL, c_buf);
        StrBuf_add(buf, " ;expires=", -1);
        StrBuf_add(buf, c_buf, -1);
    } else {
        RefStr *rval = Value_vp(et->val);
        urlencode_sub(buf, rval->c, rval->size, FALSE);

        if (cookie_has_expires) {
            char c_buf[48];
            timestamp_to_cookie_date(cookie_expires, c_buf);
            StrBuf_add(buf, " ;expires=", -1);
            StrBuf_add(buf, c_buf, -1);
        }
    }
    if (cookie_path != NULL) {
        StrBuf_add(buf, " ;path=", -1);
        StrBuf_add(buf, cookie_path, -1);
    }
    if (cookie_domain != NULL) {
        StrBuf_add(buf, " ;domain=", -1);
        StrBuf_add(buf, cookie_domain, -1);
    }
    if (cookie_secure) {
        StrBuf_add(buf, " ;secure", -1);
    }
    if (cookie_httponly) {
        StrBuf_add(buf, " ;HttpOnly", -1);
    }

    StrBuf_add_c(buf, '\n');
}

void send_headers()
{
    if (fs->cgi_mode && !fv->headers_sent) {
        RefMap *rm;
        int i;
        StrBuf buf;
        Value dst = fg->v_cio;

        fv->headers_sent = TRUE;
        StrBuf_init(&buf, 1024);
        rm = Value_vp(v_res);

        // Content-Typeが先頭にくるように
        {
            HashValueEntry *et = refmap_get_strkey(rm, "Content-Type", -1);
            if (et != NULL) {
                RefStr *val = Value_vp(et->val);
                StrBuf_add(&buf, "Content-Type: ", -1);
                StrBuf_add(&buf, val->c, val->size);
                StrBuf_add_c(&buf, '\n');
            } else {
                StrBuf_add(&buf, "Content-Type: text/plain; charset=UTF-8\n", -1);
            }
        }

        for (i = 0; i < rm->entry_num; i++) {
            HashValueEntry *et;
            for (et = rm->entry[i]; et != NULL; et = et->next) {
                RefStr *key = Value_vp(et->key);
                if (strcmp(key->c, "Content-Type") != 0) {
                    RefStr *val = Value_vp(et->val);

                    StrBuf_add(&buf, key->c, key->size);
                    StrBuf_add(&buf, ": ", 2);
                    StrBuf_add(&buf, val->c, val->size);
                    StrBuf_add_c(&buf, '\n');
                }
            }
        }
        if (ref_set_cookie != VALUE_NULL) {
            const char *cookie_path = get_cookie_cfg("FOX_COOKIE_PATH");
            const char *cookie_domain = get_cookie_cfg("FOX_COOKIE_DOMAIN");
            int cookie_secure = get_cookie_cfg_bool("FOX_COOKIE_SECURE");
            int cookie_httponly = get_cookie_cfg_bool("FOX_COOKIE_HTTPONLY");
            int64_t now = get_now_time();
            rm = Value_vp(ref_set_cookie);

            for (i = 0; i < rm->entry_num; i++) {
                HashValueEntry *et;
                for (et = rm->entry[i]; et != NULL; et = et->next) {
                    write_http_cookie(&buf, et, cookie_path, cookie_domain, cookie_secure, cookie_httponly, now);
                }
            }
        }
        StrBuf_add_c(&buf, '\n');
        stream_write_data(dst, buf.p, buf.size);
        StrBuf_close(&buf);
    }
}

static int check_header_already_send(void)
{
    if (fv->headers_sent) {
        throw_errorf(mod_cgi, "CgiError", "Headers were already sent.");
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

int param_string_init_hash(RefMap *v_map, Value keys)
{
    int i;
    RefMap *rm = Value_vp(keys);

    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ve = rm->entry[i];
        for (; ve != NULL; ve = ve->next) {
            HashValueEntry *ve2;
            RefNode *v_type;

            if (Value_type(ve->key) != fs->cls_str) {
                goto ERROR_END;
            }
            if (Value_type(ve->val) != fs->cls_class) {
                goto ERROR_END;
            }
            ve2 = refmap_add(v_map, ve->key, FALSE, FALSE);
            v_type = Value_vp(ve->val);
            if (v_type == fs->cls_str) {
                // ve->val = VALUE_NULL
            } else if (v_type == fs->cls_list) {
                ve2->val = vp_Value(refarray_new(0));
            } else if (v_type == fv->cls_mimedata) {
                ve2->val = VALUE_FALSE;
            } else {
                goto ERROR_END;
            }
        }
    }
    return TRUE;

ERROR_END:
    throw_errorf(fs->mod_lang, "TypeError", "{\"...\":Str|List, ...} required");
    return FALSE;
}
static int param_string_to_hash(RefMap *v_map, const char *p, Value keys, RefCharset *cs, const char *csep)
{
    StrBuf sb;

    if (p == NULL) {
        return TRUE;
    }
    if (!param_string_init_hash(v_map, keys)) {
        return FALSE;
    }

    StrBuf_init(&sb, 0);
    while (*p != '\0') {
        const char *top = p;
        while (*p != '\0' && strchr(csep, *p) == NULL) {
            p++;
        }

        if (p - top < fs->max_alloc) {
            const char *sep = top;
            while (sep < p) {
                if (*sep == '=') {
                    break;
                }
                sep++;
            }
            if (sep < p) {
                Value v1, v2;
                HashValueEntry *ve;

                sb.size = 0;
                urldecode_sub(&sb, top, sep - top);
                v1 = cstr_Value_conv(sb.p, sb.size, cs);

                if (!refmap_get(&ve, v_map, v1)) {
                    return FALSE;
                }
                // keysにない項目は無視する
                if (ve == NULL) {
                    continue;
                }

                sb.size = 0;
                urldecode_sub(&sb, sep + 1, (p - sep) - 1);
                v2 = cstr_Value_conv(sb.p, sb.size, cs);

                if (ve->val == VALUE_NULL) {
                    ve->val = v2;
                } else if (Value_type(ve->val) == fs->cls_list) {
                    Value *pv = refarray_push(Value_vp(ve->val));
                    *pv = v2;
                }
            }
        }
        if (*p != '\0') {
            p++;
        }
    }
    StrBuf_close(&sb);
    return TRUE;
}
static int http_header_string_to_hash(RefMap *v_map, const char *p, int keys)
{
    if (p == NULL) {
        return TRUE;
    }

    for (;;) {
        const char *top;
        Str key, val;

        while (isspace_fox(*p)) {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        top = p;
        while (*p != '\0' && !isspace_fox(*p) && *p != '=' && *p != ';' && *p != ',') {
            p++;
        }

        key = Str_new(top, p - top);
        while (isspace_fox(*p)) {
            p++;
        }

        if (*p == '=') {
            p++;
            while (isspace_fox(*p)) {
                p++;
            }
            top = p;
            while (*p != '\0' && !isspace_fox(*p) && *p != ';' && *p != ',') {
                p++;
            }
            val = Str_new(top, p - top);
            if (*p != '\0' && !isspace_fox(*p)) {
                p++;
            }
        } else {
            val.size = 0;
        }

        if (key.size > 0 && key.size < fs->max_alloc && val.size > 0 && val.size < fs->max_alloc) {
            Value vkey = cstr_Value_conv(key.p, key.size, NULL);
            if (keys) {
                HashValueEntry *ve;
                if (!refmap_get(&ve, v_map, vkey)) {
                    Value_dec(vkey);
                    return FALSE;
                }
                if (ve != NULL) {
                    Value vval = cstr_Value_conv(val.p, val.size, NULL);
                    if (ve->val == VALUE_NULL) {
                        ve->val = vval;
                    } else if (Value_type(ve->val) == fs->cls_list) {
                        Value *pv = refarray_push(Value_vp(ve->val));
                        *pv = vval;
                    }
                }
            } else {
                // 同じ名前なら先に来る要素を優先させる
                HashValueEntry *ve = refmap_add(v_map, vkey, FALSE, FALSE);
                ve->val = cstr_Value_conv(val.p, val.size, NULL);
            }
            Value_dec(vkey);
        }
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static int cgi_req_param(Value *vret, Value *v, RefNode *node)
{
    if (fs->cgi_mode) {
        int i;
        RefMap *rm = refmap_new(32);
        *vret = vp_Value(rm);
        rm->rh.type = fv->cls_mimeheader;

        for (i = 0; i < fs->envs.entry_num; i++) {
            HashEntry *he = fs->envs.entry[i];
            for (; he != NULL; he = he->next) {
                RefStr *key = he->key;
                if (key->size > 5 && memcmp(key->c, "HTTP_", 5) == 0) {
                    char *pkey = malloc(key->size - 5);
                    Value vkey;
                    HashValueEntry *ve;
                    int j;
                    int up = TRUE;
                    // HTTP_CONTENT_TYPE -> Content-Type
                    for (j = 5; j < key->size; j++) {
                        char c = key->c[j] & 0xFF;
                        if (c == '_') {
                            c = '-';
                            up = TRUE;
                        } else if (up) {
                            c = toupper(c);
                            up = FALSE;
                        } else {
                            c = tolower(c);
                        }
                        pkey[j - 5] = c;
                    }

                    vkey = cstr_Value_conv(pkey, key->size - 5, NULL);
                    free(pkey);

                    // 上書きしない
                    ve = refmap_add(rm, vkey, FALSE, FALSE);
                    ve->val = cstr_Value_conv(he->p, -1, NULL);
                    Value_dec(vkey);
                } else if (strcmp(key->c, "CONTENT_TYPE") == 0) {
                    HashValueEntry *ve;
                    Value vkey = cstr_Value(fs->cls_str, "Content-Type", -1);

                    // 上書きしない
                    ve = refmap_add(rm, vkey, FALSE, FALSE);
                    ve->val = cstr_Value_conv(he->p, -1, NULL);
                }
            }
        }
        return TRUE;
    } else {
        for_cgi_mode_error();
        return FALSE;
    }
}
static int cgi_res_param(Value *vret, Value *v, RefNode *node)
{
    if (fs->cgi_mode) {
        *vret = Value_cp(v_res);
        return TRUE;
    } else {
        for_cgi_mode_error();
        return FALSE;
    }
}

static int cgi_get_string(Value *vret, Value *v, RefNode *node)
{
    const char *ptr = Hash_get_p(&fs->envs, FUNC_VP(node));
    if (ptr != NULL) {
        *vret = cstr_Value_conv(ptr, -1, NULL);
    }
    return TRUE;
}
static int cgi_get_integer(Value *vret, Value *v, RefNode *node)
{
    const char *ptr = Hash_get_p(&fs->envs, FUNC_VP(node));
    if (ptr != NULL) {
        *vret = int64_Value(strtoll(ptr, NULL, 10));
    }
    return TRUE;
}
static int cgi_get_pathinfo(Value *vret, Value *v, RefNode *node)
{
    if (fv->path_info != NULL) {
        *vret = cstr_Value_conv(fv->path_info, -1, NULL);
    }
    return TRUE;
}
static int cgi_get_addr(Value *vret, Value *v, RefNode *node)
{
    RefStr *key = FUNC_VP(node);
    const char *hostname = Hash_get_p(&fs->envs, key);

    if (hostname != NULL) {
        static RefNode *ipaddr_new;
        if (ipaddr_new == NULL) {
            RefNode *ipaddr;
            RefNode *mod = get_module_by_name("io.net", -1, TRUE, TRUE);
            if (mod == NULL) {
                return FALSE;
            }
            ipaddr = Hash_get(&mod->u.m.h, "IPAddr", -1);
            if (ipaddr == NULL) {
                throw_errorf(fs->mod_lang, "NameError", "IPAddr is not defined.");
                return FALSE;
            }
            ipaddr_new = Hash_get_p(&ipaddr->u.c.h, fs->str_new);
            if (ipaddr_new == NULL) {
                throw_errorf(fs->mod_lang, "NameError", "IPAddr.new is not defined.");
                return FALSE;
            }
        }
        Value_push("Ns", hostname);
        if (!call_function(ipaddr_new, 1)) {
            return FALSE;
        }
        *vret = fg->stk_top[-1];
        fg->stk_top--;
    } else {
        *vret = VALUE_NULL;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static void map_false_to_null(RefMap *rm)
{
    int i;

    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ve = rm->entry[i];
        for (; ve != NULL; ve = ve->next) {
            if (ve->val == VALUE_FALSE) {
                ve->val = VALUE_NULL;
            }
        }
    }
}

static int cgi_stdin_param(Value keys, RefCharset *cs)
{
    int result = TRUE;
    RefMap *map_post;
    RefStr *content_type;
    const char *ctype_p = Hash_get(&fs->envs, "CONTENT_TYPE", -1);

    if (ref_map_post != VALUE_NULL) {
        return TRUE;
    }

    // def POST = {}
    map_post = refmap_new(32);
    ref_map_post = vp_Value(map_post);

    if (!param_string_init_hash(map_post, keys)) {
        return FALSE;
    }
    {
        Str ctype_s;
        if (ctype_p == NULL) {
            return TRUE;
        }
        if (!parse_header_sub(&ctype_s, NULL, 0, ctype_p, -1)) {
            return TRUE;
        }
        content_type = Value_vp(cstr_Value(fs->cls_mimetype, ctype_s.p, ctype_s.size));
        content_type = resolve_mimetype_alias(content_type);
    }

    if (strcmp(content_type->c, "multipart/form-data") == 0) {
        Str boundary;
        if (!parse_header_sub(&boundary, "boundary", -1, ctype_p, -1)) {
            return TRUE;
        }
        if (!mimerandomreader_sub(map_post, fg->v_cio, boundary.p, boundary.size, cs, "Content-Disposition", -1, "name", -1, TRUE)) {
            return FALSE;
        }
        map_false_to_null(map_post);
    } else if (strcmp(content_type->c, "application/x-www-form-urlencoded") == 0) {
        StrBuf buf;
        int size = fs->max_alloc;
        StrBuf_init(&buf, 4096);
        if (stream_read_data(fg->v_cio, &buf, NULL, &size, FALSE, FALSE)) {
            StrBuf_add_c(&buf, '\0');
            buf.size--;
            result = param_string_to_hash(map_post, buf.p, keys, cs, "&");
            map_false_to_null(map_post);
        } else {
            result = FALSE;
        }
        StrBuf_close(&buf);
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static int param_string_init_cookie(Value v, Value keys)
{
    int i;
    RefMap *rm = Value_vp(keys);
    RefMap *v_map = Value_vp(v);

    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ve = rm->entry[i];
        for (; ve != NULL; ve = ve->next) {
            HashValueEntry *ve2;
            RefNode *klass;
            if (Value_type(ve->key) != fs->cls_str) {
                goto ERROR_END;
            }
            if (Value_type(ve->val) != fs->cls_class) {
                goto ERROR_END;
            }
            ve2 = refmap_add(v_map, ve->key, FALSE, FALSE);
            klass = Value_vp(ve->val);
            if (klass == fs->cls_str) {
                ve2->val = VALUE_NULL;
            } else if (klass == fs->cls_list) {
                RefArray *ra = refarray_new(0);
                ve2->val = vp_Value(ra);
            } else {
                goto ERROR_END;
            }
        }
    }
    return TRUE;
ERROR_END:
    throw_errorf(fs->mod_lang, "TypeError", "{\"...\":Str, ...} required");
    return FALSE;
}
static int cookie_get_expires(Value *vret, Value *v, RefNode *node)
{
    if (cookie_has_expires) {
        RefInt64 *tm = buf_new(fs->cls_timestamp, sizeof(int64_t));
        *vret = vp_Value(tm);
        tm->u.i = cookie_expires;
    }
    return TRUE;
}
static int cookie_set_expires(Value *vret, Value *v, RefNode *node)
{
    RefInt64 *ri = Value_vp(v[1]);

    if (!check_header_already_send()) {
        return FALSE;
    }
    cookie_has_expires = TRUE;
    cookie_expires = ri->u.i;

    return TRUE;
}
static int cookie_set_value(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    Value v2 = v[2];
    RefStr *key = Value_vp(v1);
    RefStr *val = Value_vp(v2);

    if (!check_header_already_send()) {
        return FALSE;
    }
    if (!is_string_only_ascii(key->c, key->size, " =;,")) {
        throw_errorf(fs->mod_lang, "ValueError", "Illigal characters in cookie name.");
        return FALSE;
    }
    if (!is_string_only_ascii(val->c, val->size, " =;,")) {
        throw_errorf(fs->mod_lang, "ValueError", "Illigal characters in cookie value.");
        return FALSE;
    }

    {
        HashValueEntry *ve;
        if (!refmap_get(&ve, Value_vp(ref_map_cookie), v1)) {
            return FALSE;
        }
        if (ve != NULL) {
            HashValueEntry *ve2;
            if (ref_set_cookie == VALUE_NULL) {
                ref_set_cookie = vp_Value(refmap_new(0));
            }
            Value_dec(ve->val);
            ve->val = v2;

            ve2 = refmap_add(Value_vp(ref_set_cookie), v1, TRUE, FALSE);
            ve2->val = Value_cp(v2);
        } else {
            throw_errorf(fs->mod_lang, "IndexError", "Map key %Q not found", key);
            return FALSE;
        }
    }

    return TRUE;
}
static int cookie_delete_value(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    RefStr *key = Value_vp(v1);
    RefMap *set_cookie;

    if (!check_header_already_send()) {
        return FALSE;
    }
    if (!is_string_only_ascii(key->c, key->size, " =;,")) {
        throw_errorf(fs->mod_lang, "ValueError", "Illigal characters in cookie name.");
        return FALSE;
    }

    if (ref_set_cookie != VALUE_NULL) {
        set_cookie = Value_vp(ref_set_cookie);
    } else {
        set_cookie = refmap_new(0);
        ref_set_cookie = vp_Value(set_cookie);
    }
    // add VALUE_NULL
    refmap_add(Value_vp(*v), v1, TRUE, FALSE);
    refmap_add(set_cookie, v1, TRUE, FALSE);

    return TRUE;
}

static int cgi_session_id(Value *vret, Value *v, RefNode *node)
{
    enum {
        RAND_LEN = 64,
    };
    static RefNode *func_hash;
    RefStr *rs;

    if (func_hash == NULL) {
        RefNode *mod = get_module_by_name("crypt", -1, TRUE, TRUE);
        if (mod == NULL) {
            return FALSE;
        }
        func_hash = get_node_member(mod, intern("get_hash", -1), NULL);
        if (func_hash == NULL) {
            return FALSE;
        }
    }

    rs = refstr_new_n(fs->cls_bytes, RAND_LEN);
    get_random(rs->c, RAND_LEN);

    Value_push("Nrs", rs, "sha256");
    Value_dec(vp_Value(rs));
    if (!call_function(func_hash, 2)) {
        return FALSE;
    }
    fg->stk_top--;
    *vret = *fg->stk_top;

    return TRUE;
}

static int cgi_get_param(Value *vret, Value *v, RefNode *node)
{
    if (fs->cgi_mode) {
        const char *src = Hash_get(&fs->envs, "QUERY_STRING", -1);
        RefMap *rm = refmap_new(32);
        RefCharset *cs = (fg->stk_top > v + 2 ? Value_vp(v[2]) : fs->cs_utf8);
        const char *csep = "&";
        if (fg->stk_top > v + 3) {
            RefStr *rs = Value_vp(v[3]);
            int i;
            for (i = 0; i < rs->size; i++) {
                if (rs->c[i] == 0 || (rs->c[i] & 0x80) != 0) {
                    throw_errorf(fs->mod_lang, "ValueError", "ASCII characters required (argument #3)");
                    return FALSE;
                }
            }
            csep = rs->c;
        }
        *vret = vp_Value(rm);
        return param_string_to_hash(rm, src, v[1], cs, csep);
    } else {
        for_cgi_mode_error();
        return FALSE;
    }
}
static int cgi_post_param(Value *vret, Value *v, RefNode *node)
{
    if (fs->cgi_mode) {
        RefCharset *cs = (fg->stk_top > v + 2 ? Value_vp(v[2]) : fs->cs_utf8);
        if (!cgi_stdin_param(v[1], cs)) {
            return FALSE;
        }
        *vret = Value_cp(ref_map_post);
        return TRUE;
    } else {
        for_cgi_mode_error();
        return FALSE;
    }
}
static int cgi_cookie_param(Value *vret, Value *v, RefNode *node)
{
    if (fs->cgi_mode) {
        if (ref_map_cookie == VALUE_NULL) {
            const char *p = Hash_get(&fs->envs, "HTTP_COOKIE", -1);
            RefMap *rm = refmap_new(0);
            ref_map_cookie = vp_Value(rm);
            rm->rh.type = cls_cookie;

            if (!param_string_init_cookie(ref_map_cookie, v[1])) {
                return FALSE;
            }
            if (p != NULL) {
                http_header_string_to_hash(rm, p, TRUE);
            }
        }
        *vret = Value_cp(ref_map_cookie);
        return TRUE;
    } else {
        for_cgi_mode_error();
        return FALSE;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_lang_cgi_const(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "RES", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_res_param, 0, 0, NULL);

    n = define_identifier(m, m, "REQ", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_req_param, 0, 0, NULL);

    n = define_identifier(m, m, "QUERY_STRING", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_string, 0, 0, intern("QUERY_STRING", -1));
    n = define_identifier(m, m, "REQUEST_METHOD", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_string, 0, 0, intern("REQUEST_METHOD", -1));
    n = define_identifier(m, m, "SCRIPT_NAME", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_string, 0, 0, intern("SCRIPT_NAME", -1));
    n = define_identifier(m, m, "REQUEST_URI", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_string, 0, 0, intern("REQUEST_URI", -1));
    n = define_identifier(m, m, "PATH_INFO", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_pathinfo, 0, 0, NULL);
    n = define_identifier(m, m, "SERVER_ADDR", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_addr, 0, 0, intern("SERVER_ADDR", -1));
    n = define_identifier(m, m, "SERVER_PORT", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_integer, 0, 0, intern("SERVER_PORT", -1));
    n = define_identifier(m, m, "REMOTE_ADDR", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_addr, 0, 0, intern("REMOTE_ADDR", -1));
    n = define_identifier(m, m, "REMOTE_PORT", NODE_CONST_U_N, 0);
    define_native_func_a(n, cgi_get_integer, 0, 0, intern("REMOTE_PORT", -1));
}

static void define_lang_cgi_func(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "session_id", NODE_FUNC_N, 0);
    define_native_func_a(n, cgi_session_id, 0, 0, NULL);

    n = define_identifier(m, m, "get_param", NODE_FUNC_N, 0);
    define_native_func_a(n, cgi_get_param, 1, 3, NULL, fs->cls_map, fs->cls_charset, fs->cls_str);

    n = define_identifier(m, m, "post_param", NODE_FUNC_N, 0);
    define_native_func_a(n, cgi_post_param, 1, 2, NULL, fs->cls_map, fs->cls_charset);

    n = define_identifier(m, m, "cookie_param", NODE_FUNC_N, 0);
    define_native_func_a(n, cgi_cookie_param, 1, 1, NULL, fs->cls_map);
}

static void define_map_methods(RefNode *m, RefNode *cls)
{
    RefNode *n;

    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, map_index, 1, 1, (void*) TRUE, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    define_native_func_a(n, map_index, 1, 1, (void*) FALSE, NULL);
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_empty, 0, 0, NULL);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_size, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_BOTH);
    n = define_identifier(m, cls, "pairs", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_BOTH);
    n = define_identifier(m, cls, "keys", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_KEY);
    n = define_identifier(m, cls, "values", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_VAL);
    n = define_identifier(m, cls, "has_key", NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, fs->cls_str);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, fs->cls_str);
}

static void define_lang_cgi_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    // Cookie
    cls = define_identifier(m, m, "_Cookie", NODE_CLASS, 0);
    cls_cookie = cls;
    define_map_methods(m, cls);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    define_native_func_a(n, cookie_set_value, 2, 2, NULL, fs->cls_str, fs->cls_str);
    n = define_identifier_p(m, cls, fs->str_missing_set, NODE_FUNC_N, 0);
    define_native_func_a(n, cookie_set_value, 2, 2, NULL, NULL, fs->cls_str);
    n = define_identifier(m, cls, "delete", NODE_FUNC_N, 0);
    define_native_func_a(n, cookie_delete_value, 1, 1, NULL, fs->cls_str);
    n = define_identifier(m, cls, "expires", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, cookie_get_expires, 0, 0, NULL);
    n = define_identifier(m, cls, "expires=", NODE_FUNC_N, 0);
    define_native_func_a(n, cookie_set_expires, 1, 1, NULL, fs->cls_timestamp);
    extends_method(cls, fs->cls_obj);


    cls = define_identifier(m, m, "CgiError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    extends_method(cls, fs->cls_error);
}
void init_cgi_module_1()
{
    RefNode *m = Module_new_sys("cgi");

    mod_cgi = m;
    define_lang_cgi_const(m);
    define_lang_cgi_func(m);
    define_lang_cgi_class(m);

    m->u.m.loaded = TRUE;
}
