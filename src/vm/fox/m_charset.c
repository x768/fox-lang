#include "fox_parse.h"
#include "m_codecvt.h"


static Hash charset_entries;


void CodeCVTStatic_init()
{
    if (codecvt == NULL) {
        RefNode *mod = get_module_by_name("text.codecvt", -1, TRUE, TRUE);
        if (mod == NULL) {
            fatal_errorf("Cannot load module 'text.codecvt'");
        }
        codecvt = mod->u.m.ext;
    }
}

static void load_charset_alias_file(const char *filename)
{
    Tok tk;
    char *path = path_normalize(NULL, fs->fox_home, filename, -1, NULL);
    char *buf = read_from_file(NULL, path, NULL);

    if (buf == NULL) {
        fatal_errorf("Cannot load file %q", path);
    }

    Tok_simple_init(&tk, buf);
    Tok_simple_next(&tk);

    for (;;) {
        switch (tk.v.type) {
        case TL_STR: {
            RefCharset *cs;
            RefStr *name_r = intern(tk.str_val.p, tk.str_val.size);
            Tok_simple_next(&tk);
            if (tk.v.type != T_LET) {
                goto ERROR_END;
            }
            Tok_simple_next(&tk);

            cs = Mem_get(&fg->st_mem, sizeof(RefCharset));
            cs->rh.type = fs->cls_charset;
            cs->rh.nref = -1;
            cs->rh.n_memb = 0;
            cs->rh.weak_ref = NULL;

            cs->name = name_r;
            cs->iana = name_r;
            cs->cs = NULL;
            cs->type = FCHARSET_NONE;
            cs->files = NULL;

            for (;;) {
                if (tk.v.type != TL_STR) {
                    goto ERROR_END;
                }
                Hash_add_p(&charset_entries, &fg->st_mem, intern(tk.str_val.p, tk.str_val.size), cs);

                Tok_simple_next(&tk);
                if (tk.v.type != T_COMMA) {
                    break;
                }
                Tok_simple_next(&tk);
            }
            if (tk.v.type != T_NL && tk.v.type != T_EOF) {
                goto ERROR_END;
            }
            break;
        }
        case T_NL:
        case T_SEMICL:
            Tok_simple_next(&tk);
            break;
        case T_EOF:
            free(buf);
            free(path);
            return;
        default:
            goto ERROR_END;
        }
    }

ERROR_END:
    fatal_errorf("Error at line %d (%s)", tk.v.line, path);
    free(path);
}
static void load_charset_info_file(const char *filename)
{
    enum {
        MAX_CHARSET_FILES = 16,
    };
    
    Tok tk;
    char *path = path_normalize(NULL, fs->fox_home, filename, -1, NULL);
    char *buf = read_from_file(NULL, path, NULL);
    Str def_path;

    if (buf == NULL) {
        fatal_errorf("Cannot load file %q", path);
    }

    Tok_simple_init(&tk, buf);
    Tok_simple_next(&tk);

    path_normalize(&def_path, fs->fox_home, "charset" SEP_S, -1, &fg->st_mem);

    for (;;) {
    LINE_BEGIN:
        switch (tk.v.type) {
        case TL_STR: {
            RefCharset *cs = get_charset_from_name(tk.str_val.p, tk.str_val.size);
            if (cs == NULL) {
                fatal_errorf("Unknown charset %Q at line %d (%s)", tk.str_val, tk.v.line, path);
            }
            Tok_simple_next(&tk);
            if (tk.v.type != T_COLON) {
                goto ERROR_END;
            }
            Tok_simple_next(&tk);
            for (;;) {
                Str key;
                if (tk.v.type != TL_VAR) {
                    goto ERROR_END;
                }
                key = tk.str_val;
                Tok_simple_next(&tk);
                if (tk.v.type != T_LET) {
                    goto ERROR_END;
                }
                Tok_simple_next(&tk);
                if (str_eq(key.p, key.size, "iana", -1)) {
                    if (tk.v.type != TL_STR) {
                        goto ERROR_END;
                    }
                    cs->iana = intern(tk.str_val.p, tk.str_val.size);
                } else if (str_eq(key.p, key.size, "type", -1)) {
                    if (tk.v.type == TL_VAR) {
                        if (str_eq(tk.str_val.p, tk.str_val.size, "ascii", -1)) {
                            cs->type = FCHARSET_ASCII;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "utf8", -1)) {
                            cs->type = FCHARSET_UTF8;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "utf8loose", -1)) {
                            cs->type = FCHARSET_UTF8_LOOSE;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "cesu8", -1)) {
                            cs->type = FCHARSET_CESU8;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "singlebyte", -1)) {
                            cs->type = FCHARSET_SINGLE_BYTE;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "euc", -1)) {
                            cs->type = FCHARSET_EUC;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "shiftjis", -1)) {
                            cs->type = FCHARSET_SHIFTJIS;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "big5", -1)) {
                            cs->type = FCHARSET_BIG5;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "gb18030", -1)) {
                            cs->type = FCHARSET_GB18030;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "iso2022", -1)) {
                            cs->type = FCHARSET_ISO_2022;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "utf16le", -1)) {
                            cs->type = FCHARSET_UTF16LE;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "utf16be", -1)) {
                            cs->type = FCHARSET_UTF16BE;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "utf32le", -1)) {
                            cs->type = FCHARSET_UTF32LE;
                        } else if (str_eq(tk.str_val.p, tk.str_val.size, "utf32be", -1)) {
                            cs->type = FCHARSET_UTF32BE;
                        } else {
                            goto ERROR_END;
                        }
                    } else {
                        goto ERROR_END;
                    }
                } else if (str_eq(key.p, key.size, "files", -1)) {
                    const char *list[MAX_CHARSET_FILES];
                    int num = 0;

                    if (tk.v.type != T_LB && tk.v.type != T_LB_C) {
                        goto ERROR_END;
                    }
                    Tok_simple_next(&tk);
                    while (tk.v.type != T_RB) {
                        if (num >= MAX_CHARSET_FILES - 1) {
                            fatal_errorf("Too many files at line %d (%s)", tk.v.line, path);
                        }
                        if (tk.v.type != TL_STR) {
                            goto ERROR_END;
                        }
                        {
                            char *p = Mem_get(&fg->st_mem, def_path.size + tk.str_val.size + 1);
                            memcpy(p, def_path.p, def_path.size);
                            memcpy(p + def_path.size, tk.str_val.p, tk.str_val.size);
                            p[def_path.size + tk.str_val.size] = '\0';
                            list[num++] = p;
                        }

                        Tok_simple_next(&tk);
                        if (tk.v.type == T_RB) {
                            break;
                        }
                        if (tk.v.type != T_COMMA) {
                            goto ERROR_END;
                        }
                        Tok_simple_next(&tk);
                    }
                    if (num > 0) {
                        list[num++] = NULL;
                        cs->files = Mem_get(&fg->st_mem, num * sizeof(const char*));
                        memcpy(cs->files, list, num * sizeof(const char*));
                    }
                } else {
                    fatal_errorf("Unknown key %Q at line %d (%s)", key, tk.v.line, path);
                }
                Tok_simple_next(&tk);
                if (tk.v.type != T_COMMA) {
                    break;
                }
                Tok_simple_next(&tk);
            }
            if (cs->type >= FCHARSET_SINGLE_BYTE && cs->type < FCHARSET_UTF16LE) {
                if (cs->files == NULL) {
//                    fatal_errorf("'files' required at line %d (%s)", tk.v.line, path);
                    cs->type = FCHARSET_NONE;
                }
            }
            break;
        }
        case T_NL:
        case T_SEMICL:
            Tok_simple_next(&tk);
            goto LINE_BEGIN;
        case T_EOF:
            free(path);
            return;
        default:
            goto ERROR_END;
        }
        if (tk.v.type != T_NL && tk.v.type != T_EOF) {
            goto ERROR_END;
        }
    }

ERROR_END:
    fatal_errorf("Error at line %d (%s)", tk.v.line, path);
    free(path);
}

RefCharset *get_charset_from_name(const char *src_p, int src_size)
{
    char buf[32];
    int i, j = 0;

    if (src_size < 0) {
        src_size = strlen(src_p);
    }

    for (i = 0; i < src_size; i++) {
        char c = src_p[i];
        if (isalnum(c)) {
            buf[j++] = toupper(c);
            if (j >= sizeof(buf) - 1) {
                break;
            }
        }
    }
    buf[j] = '\0';
    return Hash_get(&charset_entries, buf, j);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int convert_str_to_bin_sub(StrBuf *dst_buf, const char *src_p, int src_size, RefCharset *cs, const char *alt_s)
{
    if (src_size < 0) {
        src_size = strlen(src_p);
    }

    if (cs == NULL || cs->type == FCHARSET_UTF8 || cs->type == FCHARSET_UTF8_LOOSE) {
        // そのまま
        if (!StrBuf_add(dst_buf, src_p, src_size)) {
            return FALSE;
        }
        return TRUE;
    } else {
        // UTF-8 -> 任意の文字コード変換
        FConv cv;
        FCharset *fc;

        CodeCVTStatic_init();
        fc = codecvt->RefCharset_get_fcharset(cs, TRUE);
        if (fc == NULL) {
            return FALSE;
        }
        codecvt->FConv_init(&cv, fc, FALSE, alt_s);
        if (!codecvt->FConv_conv_strbuf(&cv, dst_buf, src_p, src_size, TRUE)) {
            return FALSE;
        }

        return TRUE;
    }
}

/**
 * dst     : optional
 * dst_buf : optional
 * arg     : Str
 * arg+1   : Charset (default: UTF-8)
 * arg+2   : Bool(optional)
 */
int convert_str_to_bin(Value *dst, StrBuf *dst_buf, int arg)
{
    Value *v = fg->stk_base;
    RefCharset *cs;
    RefStr *rs;
    int alt_b;

    if (fg->stk_top > v + arg + 1) {
        cs = Value_vp(v[arg + 1]);
    } else {
        cs = fs->cs_utf8;
    }
    if (fg->stk_top > v + arg + 2) {
        alt_b = Value_bool(v[arg + 2]);
    } else {
        alt_b = FALSE;
    }
    rs = Value_vp(v[arg]);

    if (cs == fs->cs_utf8) {
        if (dst_buf != NULL) {
            if (!StrBuf_add(dst_buf, rs->c, rs->size)) {
                return FALSE;
            }
        } else {
            *dst = cstr_Value(fs->cls_bytes, rs->c, rs->size);
        }
    } else {
        if (dst_buf != NULL) {
            if (!convert_str_to_bin_sub(dst_buf, rs->c, rs->size, cs, alt_b ? "?" : NULL)) {
                return FALSE;
            }
        } else {
            StrBuf sb_tmp;
            StrBuf_init_refstr(&sb_tmp, 64);
            if (!convert_str_to_bin_sub(&sb_tmp, rs->c, rs->size, cs, alt_b ? "?" : NULL)) {
                StrBuf_close(&sb_tmp);
                return FALSE;
            }
            *dst = StrBuf_str_Value(&sb_tmp, fs->cls_bytes);
        }
    }
    return TRUE;
}

/*
 * 出力先にdstとdst_bufどちらか一方を指定
 */
int convert_bin_to_str_sub(StrBuf *dst_buf, const char *src_p, int src_size, RefCharset *cs, int alt_b)
{
    if (src_size < 0) {
        src_size = strlen(src_p);
    }
    if (cs == NULL || cs->type == FCHARSET_UTF8 || cs->type == FCHARSET_UTF8_LOOSE) {
        // なるべくそのまま
        if (invalid_utf8_pos(src_p, src_size) >= 0) {
            if (alt_b) {
                alt_utf8_string(dst_buf, src_p, src_size);
            } else {
                throw_error_select(THROW_INVALID_UTF8);
                return FALSE;
            }
        } else {
            StrBuf_add(dst_buf, src_p, src_size);
        }
        return TRUE;
    } else {
        FConv cv;
        FCharset *fc;

        CodeCVTStatic_init();
        fc = codecvt->RefCharset_get_fcharset(cs, TRUE);
        if (fc == NULL) {
            return FALSE;
        }
        codecvt->FConv_init(&cv, fc, TRUE, alt_b ? UTF8_ALTER_CHAR : NULL);
        if (!codecvt->FConv_conv_strbuf(&cv, dst_buf, src_p, src_size, TRUE)) {
            return FALSE;
        }

        return TRUE;
    }
}

/**
 * dst     : needed
 * src_str : optional
 * arg     : Str needed if src_str == NULL
 * arg+1   : Charset (省略時はBytesのまま)
 * arg+1   : Bool (optional)
 */
int convert_bin_to_str(Value *dst, const Str *src_str, int arg)
{
    Value *v = fg->stk_base;
    RefCharset *cs;
    int alt_b;
    const char *src_p;
    int src_size;
    StrBuf sbuf;

    if (fg->stk_top > v + arg + 1) {
        cs = Value_vp(v[arg + 1]);
    } else {
        // そのまま返す
        if (src_str != NULL) {
            *dst = cstr_Value(fs->cls_bytes, src_str->p, src_str->size);
        } else {
            *dst = Value_cp(v[arg]);
        }
        return TRUE;
    }
    if (fg->stk_top > v + arg + 2) {
        alt_b = Value_bool(v[arg + 2]);
    } else {
        alt_b = FALSE;
    }
    if (src_str != NULL) {
        src_p = src_str->p;
        src_size = src_str->size;
    } else {
        RefStr *rs = Value_vp(v[arg]);
        src_p = rs->c;
        src_size = rs->size;
    }

    StrBuf_init_refstr(&sbuf, src_size);
    if (!convert_bin_to_str_sub(&sbuf, src_p, src_size, cs, alt_b)) {
        StrBuf_close(&sbuf);
        return FALSE;
    }
    *dst = StrBuf_str_Value(&sbuf, fs->cls_str);
    return TRUE;
}

static int charset_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *v_type = Value_type(v[1]);
    RefCharset *cs;

    if (v_type == fs->cls_charset) {
        cs = Value_vp(v[1]);
    } else if (v_type == fs->cls_str) {
        RefStr *rs = Value_vp(v[1]);
        cs = get_charset_from_name(rs->c, rs->size);
        if (cs == NULL) {
            throw_errorf(fs->mod_lang, "ValueError", "Unknown encoding name %q", rs->c);
            return FALSE;
        }
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_charset, fs->cls_str, v_type, 1);
        return FALSE;
    }
    *vret = vp_Value(cs);
    return TRUE;
}
static int charset_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefCharset *cs;
    uint32_t size;
    int rd_size;
    char cbuf[64];

    if (!stream_read_uint32(r, &size)) {
        goto ERROR_END;
    }
    if (size > 63) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        goto ERROR_END;
    }
    rd_size = size;
    if (!stream_read_data(r, NULL, cbuf, &rd_size, FALSE, TRUE)) {
        goto ERROR_END;
    }
    cbuf[rd_size] = '\0';

    cs = get_charset_from_name(cbuf, -1);
    if (cs == NULL) {
        throw_errorf(fs->mod_lang, "ValueError", "Unknown encoding name %q", cbuf);
        goto ERROR_END;
    }
    *vret = vp_Value(cs);

    return TRUE;

ERROR_END:
    return FALSE;
}
static int charset_marshal_write(Value *vret, Value *v, RefNode *node)
{
    RefCharset *cs = Value_vp(*v);
    RefStr *name = cs->name;
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];

    if (!stream_write_uint32(w, name->size)) {
        return FALSE;
    }
    if (!stream_write_data(w, name->c, name->size)) {
        return FALSE;
    }
    return TRUE;
}
static int charset_tostr(Value *vret, Value *v, RefNode *node)
{
    RefCharset *cs = Value_vp(*v);
    RefStr *name = cs->name;
    *vret = printf_Value("Charset(%r)", name);
    return TRUE;
}
static int charset_supported(Value *vret, Value *v, RefNode *node)
{
    RefCharset *cs = Value_vp(*v);
    *vret = bool_Value(cs->type != FCHARSET_NONE);
    return TRUE;
}
static int charset_name(Value *vret, Value *v, RefNode *node)
{
    RefCharset *cs = Value_vp(*v);
    *vret = vp_Value(cs->name);
    return TRUE;
}
static int charset_iana(Value *vret, Value *v, RefNode *node)
{
    RefCharset *cs = Value_vp(*v);
    *vret = vp_Value(cs->iana);
    return TRUE;
}

// 指定した文字コードでエンコードできるか調べる
static int charset_is_valid_encoding(Value *vret, Value *v, RefNode *node)
{
    int str = FUNC_INT(node);
    RefCharset *cs = Value_vp(*v);
    const RefStr *rs = Value_vp(v[1]);
    int result = FALSE;

    if (cs == fs->cs_utf8) {
        if (str) {
            result = TRUE;
        } else {
            result = (invalid_utf8_pos(rs->c, rs->size) < 0);
        }
    } else {
        FConv cv;
        FCharset *fc;

        CodeCVTStatic_init();
        fc = codecvt->RefCharset_get_fcharset(cs, TRUE);
        if (fc == NULL) {
            return FALSE;
        }
        codecvt->FConv_init(&cv, fc, FALSE, NULL);
        codecvt->FConv_set_src(&cv, rs->c, rs->size, TRUE);

        // エラーがないかどうか調べるだけ
        {
            char *tmp_buf = malloc(BUFFER_SIZE);
            codecvt->FConv_set_dst(&cv, tmp_buf, BUFFER_SIZE);
            for (;;) {
                codecvt->FConv_next(&cv, FALSE);
                switch (cv.status) {
                case FCONV_OK:
                    result = TRUE;
                    goto BREAK;
                case FCONV_OUTPUT_REQUIRED:
                    codecvt->FConv_set_dst(&cv, tmp_buf, BUFFER_SIZE);
                    break;
                case FCONV_ERROR:
                    goto BREAK;
                }
            }
        BREAK:
            free(tmp_buf);
        }
    }
    *vret = bool_Value(result);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void define_charset_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    Hash_init(&charset_entries, &fg->st_mem, 64);
    load_charset_alias_file("data" SEP_S "charset-alias.txt");
    load_charset_info_file("data" SEP_S "charset-info.txt");

    fs->cs_utf8 = get_charset_from_name("UTF8", 4);
    fs->cs_utf8->type = FCHARSET_UTF8;

    // Charset
    cls = fs->cls_charset;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, charset_new, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, charset_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, charset_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, object_eq, 1, 1, NULL, fs->cls_charset);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, charset_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier(m, cls, "supported", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, charset_supported, 0, 0, NULL);
    n = define_identifier(m, cls, "name", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, charset_name, 0, 0, NULL);
    n = define_identifier(m, cls, "iana", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, charset_iana, 0, 0, NULL);
    n = define_identifier(m, cls, "representable", NODE_FUNC_N, 0);
    define_native_func_a(n, charset_is_valid_encoding, 1, 1, (void*) TRUE, fs->cls_str);
    n = define_identifier(m, cls, "is_valid_encoding", NODE_FUNC_N, 0);
    define_native_func_a(n, charset_is_valid_encoding, 1, 1, (void*) FALSE, fs->cls_bytes);

    n = define_identifier(m, cls, "UTF8", NODE_CONST, 0);
    n->u.k.val = vp_Value(fs->cs_utf8);

    extends_method(cls, fs->cls_obj);


    cls = define_identifier(m, m, "CharsetError", NODE_CLASS, 0);
    define_error_class(cls, fs->cls_error, m);
}
