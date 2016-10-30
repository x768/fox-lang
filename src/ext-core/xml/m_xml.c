#define DEFINE_GLOBALS
#include "fox_xml.h"
#include "m_codecvt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>


enum {
    LOAD_XML = 1,
    LOAD_FILE = 2,
};

typedef struct {
    int html;
    int ascii;
    int pretty;
    int keep_space;
    RefCharset *cs;
    CodeCVT ic;
} XMLOutputFormat;

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

static int xml_elem_attr(Value *vret, Value *v, RefNode *node);

////////////////////////////////////////////////////////////////////////////////////

int ch_get_category(int ch)
{
    static const uint8_t cate[] = {
        // 0x20
        CATE_Zs, CATE_Po, CATE_Po, CATE_Po,
        CATE_Sc, CATE_Po, CATE_Po, CATE_Po,
        CATE_Ps, CATE_Pe, CATE_Po, CATE_Sm,
        CATE_Po, CATE_Pd, CATE_Po, CATE_Po,
        // 0x30
        CATE_Nd, CATE_Nd, CATE_Nd, CATE_Nd,
        CATE_Nd, CATE_Nd, CATE_Nd, CATE_Nd,
        CATE_Nd, CATE_Nd, CATE_Po, CATE_Po,
        CATE_Sm, CATE_Sm, CATE_Sm, CATE_Po,
        // 0x40
        CATE_Po, CATE_Lu, CATE_Lu, CATE_Lu,
        CATE_Lu, CATE_Lu, CATE_Lu, CATE_Lu,
        CATE_Lu, CATE_Lu, CATE_Lu, CATE_Lu,
        CATE_Lu, CATE_Lu, CATE_Lu, CATE_Lu,
        // 0x50
        CATE_Lu, CATE_Lu, CATE_Lu, CATE_Lu,
        CATE_Lu, CATE_Lu, CATE_Lu, CATE_Lu,
        CATE_Lu, CATE_Lu, CATE_Lu, CATE_Ps,
        CATE_Po, CATE_Pe, CATE_Sk, CATE_Pc,
        // 0x60
        CATE_Sk, CATE_Ll, CATE_Ll, CATE_Ll,
        CATE_Ll, CATE_Ll, CATE_Ll, CATE_Ll,
        CATE_Ll, CATE_Ll, CATE_Ll, CATE_Ll,
        CATE_Ll, CATE_Ll, CATE_Ll, CATE_Ll,
        // 0x70
        CATE_Ll, CATE_Ll, CATE_Ll, CATE_Ll,
        CATE_Ll, CATE_Ll, CATE_Ll, CATE_Ll,
        CATE_Ll, CATE_Ll, CATE_Ll, CATE_Ps,
        CATE_Sm, CATE_Pe, CATE_Sm, CATE_Cc,
    };
    static int (*fn)(int ch) = NULL;

    if (ch < 0x20) {
        return CATE_Cc;
    } else if (ch < 0x80) {
        return cate[ch - 0x20];
    }
    if (fn == NULL) {
        RefNode *mod_unicode = fs->get_module_by_name("text.unicode", -1, TRUE, FALSE);
        const UnicodeStatic *uni = mod_unicode->u.m.ext;
        fn = uni->ch_get_category;
    }
    return fn(ch);
}

static int Str_isascii(const char *p, int size)
{
    int i;

    for (i = 0; i < size; i++) {
        if ((p[i] & 0x80) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}
static int Str_isspace(const char *p, int size)
{
    int i;

    for (i = 0; i < size; i++) {
        if (!isspace_fox(p[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

static Str Str_trim(Str src)
{
    int i, j;
    Str s;

    // 左の空白を除去
    for (i = 0; i < src.size; i++) {
        if (!isspace_fox(src.p[i])) {
            break;
        }
    }
    // 右の空白を除去
    for (j = src.size; j > i; j--) {
        if (!isspace_fox(src.p[j - 1])) {
            break;
        }
    }
    s.p = src.p + i;
    s.size = j - i;
    return s;
}
static Str Value_str(Value v)
{
    RefStr *rs = Value_vp(v);
    if (rs != NULL) {
        return Str_new(rs->c, rs->size);
    } else {
        return fs->Str_EMPTY;
    }
}

static NativeFunc get_function_ptr(RefNode *node, RefStr *name)
{
    RefNode *n = fs->get_node_member(fs->cls_list, name, NULL);
    if (n == NULL || n->type != NODE_FUNC_N) {
        fs->fatal_errorf(NULL, "Initialize module 'xml' failed at get_function_ptr(%r)", name);
    }
    return n->u.f.u.fn;
}
static int stream_write_xmldata(Value dst, StrBuf *buf, XMLOutputFormat *xof)
{
    if (buf->size == 0) {
        return TRUE;
    }
    if (xof->cs == NULL || xof->cs == fs->cs_utf8) {
        if (!fs->stream_write_data(dst, buf->p, buf->size)) {
            return FALSE;
        }
    } else {
        StrBuf sb;
        fs->StrBuf_init(&sb, buf->size);
        CodeCVTStatic_init();
        if (!codecvt->CodeCVT_conv(&xof->ic, &sb, buf->p, buf->size, TRUE, FALSE)) {
            return FALSE;
        }
        if (!fs->stream_write_data(dst, sb.p, sb.size)) {
            return FALSE;
        }
        StrBuf_close(&sb);
    }
    buf->size = 0;

    return TRUE;
}
static int StrBuf_add_spaces(StrBuf *buf, int num)
{
    int prev_size = buf->size;
    if (prev_size + num > fs->max_alloc) {
        fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    fs->StrBuf_alloc(buf, prev_size + num);
    memset(buf->p + prev_size, ' ', num);
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

/**
 *  vの子要素にvbaseがあればTRUE
 */
static int xml_elem_has_elem_sub(Value vbase, RefArray *ra)
{
    int i;
    for (i = 0; i < ra->size; i++) {
        Value pv = ra->p[i];
        if (fs->Value_type(pv) == cls_elem) {
            Ref *r2 = Value_ref(pv);
            RefArray *ra2 = Value_vp(r2->v[INDEX_ELEM_CHILDREN]);
            if (xml_elem_has_elem_sub(vbase, ra2)) {
                return TRUE;
            }
        }
    }
    return FALSE;
}
static int xml_elem_has_elem(Value vbase, Value v)
{
    if (vbase == v) {
        return TRUE;
    }
    if (fs->Value_type(v) == cls_elem) {
        Ref *r = Value_ref(v);
        RefArray *ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
        if (xml_elem_has_elem_sub(vbase, ra)) {
            return TRUE;
        }
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

static int stream_read_all(StrBuf *sb, Value v)
{
    int size = BUFFER_SIZE;
    char *buf = malloc(BUFFER_SIZE);

    for (;;) {
        if (!fs->stream_read_data(v, NULL, buf, &size, FALSE, FALSE)) {
            return FALSE;
        }
        if (size <= 0) {
            break;
        }
        if (!fs->StrBuf_add(sb, buf, size)) {
            return FALSE;
        }
    }
    if (!fs->StrBuf_add_c(sb, '\0')) {
        return FALSE;
    }
    sb->size--;
    return TRUE;
}
static int file_read_all(StrBuf *sb, int fh)
{
    char *buf = malloc(BUFFER_SIZE);

    for (;;) {
        int size = read_fox(fh, buf, BUFFER_SIZE);
        if (size <= 0) {
            break;
        }
        if (!fs->StrBuf_add(sb, buf, size)) {
            return FALSE;
        }
    }
    if (!fs->StrBuf_add_c(sb, '\0')) {
        return FALSE;
    }
    sb->size--;
    return TRUE;
}
static int parse_xml_from_str(Value *vret, const char *src_p, int src_size)
{
    XMLTok tk;
    char *buf = fs->str_dup_p(src_p, src_size, NULL);

    // XML Strict
    XMLTok_init(&tk, buf, buf + src_size, TRUE, FALSE);
    XMLTok_skip_next(&tk);  // 最初のスペースは無視

    if (!parse_xml_body(vret, &tk)) {
        free(buf);
        return FALSE;
    }
    free(buf);

    return TRUE;
}
static RefCharset *detect_bom(const char *p, int size)
{
    if (size >= 3) {
        if (memcmp(p, "\xEF\xBB\xBF", 3) == 0) {
            return fs->cs_utf8;
        }
    }
    if (size >= 2) {
        if (memcmp(p, "\xFE\xFF", 2) == 0) {
            // UTF-16BE
            return fs->get_charset_from_name("UTF-16", -1);
        } else if (memcmp(p, "\xFF\xFE", 2) == 0) {
            // UTF-16LE
            return fs->get_charset_from_name("UTF-16LE", -1);
        }
    }
    return NULL;
}
static RefCharset *detect_charset(const char *p, int size)
{
    // (charset|encoding)
    // =
    // ("|')?
    // [0-9A-Za-z\-_]+
    int i;

    if (size > 512) {
        size = 512;
    }

    for (i = 0; i < size - 10; i++) {
        if (memcmp(p + i, "charset", 7) == 0) {
            i += 7;
            goto FOUND;
        } else if (memcmp(p + i, "encoding", 8) == 0) {
            i += 8;
            goto FOUND;
        }
    }
    return fs->cs_utf8;

FOUND:
    while (i < size) {
        if (p[i] == '=') {
            RefCharset *cs;
            const char *begin;
            const char *end;

            i++;
            while (i < size && isspace_fox(p[i])) {
                i++;
            }
            if (i < size && (p[i] == '"' || p[i] == '\'')) {
                i++;
            }
            begin = p + i;
            while (i < size) {
                int ch = p[i];
                if (!isalnumu_fox(ch) && ch != '-') {
                    break;
                }
                i++;
            }
            end = p + i;
            cs = fs->get_charset_from_name(begin, end - begin);
            if (cs != NULL) {
                return cs;
            } else {
                return fs->cs_utf8;
            }
        }
        i++;
    }
    return fs->cs_utf8;
}
static void convert_encoding_sub(StrBuf *sb, const char *p, int size, RefCharset *cs)
{
    CodeCVT ic;

    CodeCVTStatic_init();
    codecvt->CodeCVT_open(&ic, cs, fs->cs_utf8, UTF8_ALTER_CHAR);
    codecvt->CodeCVT_conv(&ic, sb, p, size, FALSE, FALSE);
    fs->StrBuf_add_c(sb, '\0');
    codecvt->CodeCVT_close(&ic);
}
static int parse_xml(Value *vret, Value *v, RefNode *node)
{
    int mode = FUNC_INT(node);
    int xml = (mode & LOAD_XML) != 0;
    XMLTok tk;
    char *buf = NULL;
    int buf_size = 0;
    Value v1 = v[1];
    RefNode *v1_type = fs->Value_type(v1);
    int loose = !xml;
    RefCharset *cs = NULL;
    StrBuf sb_conv;

    if (fg->stk_top > v + 2) {
        if (xml) {
            if (Value_bool(v[2])) {
                loose = TRUE;
            }
        } else {
            cs = Value_vp(v[2]);
        }
    }

    if ((mode & LOAD_FILE) != 0) {
        StrBuf sb;
        Str path_s;
        int fh = -1;
        char *path = fs->file_value_to_path(&path_s, v1, 0);

        if (path == NULL) {
            return FALSE;
        }
        fh = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
        if (fh == -1) {
            fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
            free(path);
            return FALSE;
        }
        fs->StrBuf_init(&sb, 0);
        if (!file_read_all(&sb, fh)) {
            close_fox(fh);
            free(path);
            return FALSE;
        }
        close_fox(fh);
        free(path);

        buf = sb.p;
        buf_size = sb.size;
    } else {
        if (v1_type == fs->cls_str || v1_type == fs->cls_bytes) {
            RefStr *src = Value_vp(v1);
            buf = fs->str_dup_p(src->c, src->size, NULL);
            buf_size = src->size;
            if (v1_type == fs->cls_str) {
                cs = fs->cs_utf8;
            }
        } else if (v1_type == fs->cls_bytesio) {
            RefBytesIO *mb = Value_vp(v1);
            buf = fs->str_dup_p(mb->buf.p, mb->buf.size, NULL);
            buf_size = mb->buf.size;
        } else if (fs->is_subclass(v1_type, fs->cls_streamio)) {
            StrBuf sb;
            fs->StrBuf_init(&sb, 0);
            if (!stream_read_all(&sb, v1)) {
                return FALSE;
            }
            buf = sb.p;
            buf_size = sb.size;
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_sequence, fs->cls_streamio, v1_type, 1);
            return FALSE;
        }
    }

    {
        RefCharset *cs_bom = detect_bom(buf, buf_size);

        fs->StrBuf_init(&sb_conv, 0);
        if (cs_bom == fs->cs_utf8) {
            // UTF-8
            XMLTok_init(&tk, buf + 3, buf + buf_size, xml, loose);
        } else if (cs_bom != NULL) {
            // UTF-16
            convert_encoding_sub(&sb_conv, buf + 2, buf_size - 2, cs_bom);
            free(buf);
            buf = NULL;
            XMLTok_init(&tk, sb_conv.p, sb_conv.p + sb_conv.size, xml, loose);
        } else {
            if (cs == NULL) {
                cs = detect_charset(buf, buf_size);
            }
            if (cs == fs->cs_utf8) {
                XMLTok_init(&tk, buf, buf + buf_size, xml, loose);
            } else {
                convert_encoding_sub(&sb_conv, buf, buf_size, cs);
                free(buf);
                buf = NULL;
                XMLTok_init(&tk, sb_conv.p, sb_conv.p + sb_conv.size, xml, loose);
            }
        }
    }

    XMLTok_skip_next(&tk);  // 最初のスペースは無視

    if (tk.xml) {
        Ref *r = fs->ref_new(cls_document);
        RefArray *ra = fs->refarray_new(0);

        r->v[INDEX_DOCUMENT_HAS_DTD] = VALUE_FALSE;
        r->v[INDEX_DOCUMENT_LIST] = vp_Value(ra);
        *vret = vp_Value(r);

        if (!parse_xml_begin(r, &tk)) {
            goto ERROR_END;
        }
        while (tk.type != TK_EOS && tk.type != TK_ERROR) {
            // XMLElemは1つ(DOCUMENT_ROOT)
            // XMLElem以外はDOCUMENT_LISTに追加
            Value tmp;
            RefNode *tmp_type;
            if (!parse_xml_body(&tmp, &tk)) {
                goto ERROR_END;
            }

            tmp_type = fs->Value_type(tmp);
            if (tmp_type == cls_text) {
                RefStr *rs = Value_vp(tmp);
                if (!loose || Str_isspace(rs->c, rs->size)) {
                    fs->unref(tmp);
                } else {
                    fs->throw_errorf(mod_xml, "XMLParseError", "Illigal text found (%d)", tk.line);
                    fs->unref(tmp);
                    goto ERROR_END;
                }
            } else {
                *fs->refarray_push(ra) = tmp;
            }

            if (tmp_type == cls_elem) {
                if (r->v[INDEX_DOCUMENT_ROOT] == VALUE_NULL) {
                    r->v[INDEX_DOCUMENT_ROOT] = tmp;
                    fs->addref(tmp);
                } else if (!loose) {
                    fs->throw_errorf(mod_xml, "XMLParseError", "Additional root node found (%d)", tk.line);
                    goto ERROR_END;
                }
            }
        }
        if (!Value_isref(r->v[INDEX_DOCUMENT_ROOT])) {
            fs->throw_errorf(mod_xml, "XMLError", "root element not exists");
            return FALSE;
        }
    } else {
        Ref *r = fs->ref_new(cls_document);
        RefArray *ra = fs->refarray_new(0);

        r->v[INDEX_DOCUMENT_HAS_DTD] = VALUE_FALSE;
        r->v[INDEX_DOCUMENT_LIST] = vp_Value(ra);
        *vret = vp_Value(r);

        if (tk.type == TK_DOCTYPE && str_eqi(tk.val.p, tk.val.size, "DOCTYPE", -1)) {
            if (!parse_doctype_declaration(r, &tk)) {
                goto ERROR_END;
            }
        }
        if (!parse_html_body(&r->v[INDEX_DOCUMENT_ROOT], ra, &tk)) {
            goto ERROR_END;
        }
    }
    free(buf);
    StrBuf_close(&sb_conv);

    return TRUE;

ERROR_END:
    free(buf);
    StrBuf_close(&sb_conv);
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int text_convert_html(StrBuf *buf, Str src, int quot, XMLOutputFormat *xof)
{
    const char *p = src.p;
    const char *end = p + src.size;
    int prev_space = FALSE;

    while (p < end) {
        switch (*p) {
        case '<':
            if (!fs->StrBuf_add(buf, "&lt;", 4)) {
                return FALSE;
            }
            prev_space = FALSE;
            p++;
            break;
        case '>':
            if (!fs->StrBuf_add(buf, "&gt;", 4)) {
                return FALSE;
            }
            prev_space = FALSE;
            p++;
            break;
        case '&':
            if (!fs->StrBuf_add(buf, "&amp;", 5)) {
                return FALSE;
            }
            prev_space = FALSE;
            p++;
            break;
        case '"':
            if (quot) {
                if (!fs->StrBuf_add(buf, "&quot;", 6)) {
                    return FALSE;
                }
            } else {
                if (!fs->StrBuf_add_c(buf, '"')) {
                    return FALSE;
                }
            }
            prev_space = FALSE;
            p++;
            break;
        default:
            if (xof->ascii && (*p & 0x80) != 0) {
                char cbuf[16];
                int code = fs->utf8_codepoint_at(p);
                sprintf(cbuf, "&#%d;", code);
                if (!fs->StrBuf_add(buf, cbuf, -1)) {
                    return FALSE;
                }
                fs->utf8_next(&p, end);
                prev_space = FALSE;
            } else if ((*p & 0xFF) <= ' ' && !xof->keep_space) {
                if (!prev_space) {
                    if (!fs->StrBuf_add_c(buf, ' ')) {
                        return FALSE;
                    }
                    prev_space = TRUE;
                }
                p++;
            } else {
                if (!fs->StrBuf_add_c(buf, *p)) {
                    return FALSE;
                }
                prev_space = FALSE;
                p++;
            }
            break;
        }
    }
    return TRUE;
}
static int html_attr_no_value(Str name)
{
    static const char *list[] = {
        "checked", "disabled", "readonly", "multiple", "selected",
    };
    int i;

    for (i = 0; i < lengthof(list); i++) {
        if (str_eqi(name.p, name.size, list[i], -1)) {
            return TRUE;
        }
    }
    return FALSE;
}
/**
 * xmlを文字列化
 * dst != VALUE_NULLの場合、dstに出力し、bufは一時的に使用する
 */
static int node_tostr_xml(StrBuf *buf, Value dst, Value v, XMLOutputFormat *xof, int level)
{
    RefNode *v_type = fs->Value_type(v);

    if (v_type == cls_text) {
        Str s = Value_str(v);
        if (level >= 0 && !xof->pretty && !xof->keep_space) {
            s = Str_trim(s);
            if (s.size == 0) {
                s.p = " ";
                s.size = 1;
            }
        }
        if (!text_convert_html(buf, s, FALSE, xof)) {
            return FALSE;
        }
        if (dst != VALUE_NULL && !stream_write_xmldata(dst, buf, xof)) {
            return FALSE;
        }
    } else if (v_type == cls_comment) {
        Str s = Value_str(v);
        if (level >= 0 && !xof->pretty && !xof->keep_space) {
            s = Str_trim(s);
            if (s.size == 0) {
                s.p = " ";
                s.size = 1;
            }
        }
        fs->StrBuf_add(buf, "<!--", -1);
        // TODO &<>"の変換をしない
        if (!text_convert_html(buf, s, FALSE, xof)) {
            return FALSE;
        }
        fs->StrBuf_add(buf, "-->", -1);
        if (dst != VALUE_NULL && !stream_write_xmldata(dst, buf, xof)) {
            return FALSE;
        }
    } else if (v_type == cls_decl) {
        Ref *r = Value_ref(v);
        if (xof->html) {
            fs->StrBuf_add(buf, "<!--?", -1);
        } else {
            fs->StrBuf_add(buf, "<?", -1);
        }
        fs->StrBuf_add_r(buf, Value_vp(r->v[INDEX_DECL_NAME]));
        fs->StrBuf_add_c(buf, ' ');
        fs->StrBuf_add_r(buf, Value_vp(r->v[INDEX_DECL_CONTENT]));
        if (xof->html) {
            fs->StrBuf_add(buf, "?-->", -1);
        } else {
            fs->StrBuf_add(buf, "?>", -1);
        }
    } else {
        Ref *r = Value_ref(v);
        RefArray *ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
        RefMap *rm = Value_vp(r->v[INDEX_ELEM_ATTR]);
        RefStr *tag_name = Value_vp(r->v[INDEX_ELEM_NAME]);
        int i;

        if (xof->ascii && !Str_isascii(tag_name->c, tag_name->size)) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Tag name contains not ascii chars.");
            return FALSE;
        }

        if (!fs->StrBuf_add_c(buf, '<') || !fs->StrBuf_add(buf, tag_name->c, tag_name->size)) {
            return FALSE;
        }

        if (rm != NULL) {
            for (i = 0; i < rm->entry_num; i++) {
                HashValueEntry *h = rm->entry[i];
                for (; h != NULL; h = h->next) {
                    Str key = Value_str(h->key);
                    if (xof->html && html_attr_no_value(key)) {
                        if (!fs->StrBuf_add_c(buf, ' ') ||
                            !fs->StrBuf_add(buf, key.p, key.size)) {
                            return FALSE;
                        }
                    } else {
                        if (!fs->StrBuf_add_c(buf, ' ') ||
                            !fs->StrBuf_add(buf, key.p, key.size) ||
                            !fs->StrBuf_add(buf, "=\"", 2)) {
                            return FALSE;
                        }
                        if (!text_convert_html(buf, Value_str(h->val), TRUE, xof)) {
                            return FALSE;
                        }
                        if (!fs->StrBuf_add_c(buf, '"')) {
                            return FALSE;
                        }
                    }
                }
            }
        }
        if (xof->html) {
            int type = get_tag_type_icase(tag_name->c, tag_name->size);

            if ((type & TTYPE_NO_ENDTAG) != 0) {
                if (!fs->StrBuf_add_c(buf, '>')) {
                    return FALSE;
                }
                if (dst != VALUE_NULL && !stream_write_xmldata(dst, buf, xof)) {
                    return FALSE;
                }
                return TRUE;
            } else if ((type & TTYPE_RAW) != 0) {
                if (!fs->StrBuf_add_c(buf, '>')) {
                    return FALSE;
                }
                for (i = 0; i < ra->size; i++) {
                    if (fs->Value_type(ra->p[i]) == cls_text) {
                        if (!fs->StrBuf_add_r(buf, Value_vp(ra->p[i]))) {
                            return FALSE;
                        }
                    }
                }
                if (!fs->StrBuf_add(buf, "</", 2) || !fs->StrBuf_add(buf, tag_name->c, tag_name->size) || !fs->StrBuf_add_c(buf, '>')) {
                    return FALSE;
                }
                if (dst != VALUE_NULL && !stream_write_xmldata(dst, buf, xof)) {
                    return FALSE;
                }
                return TRUE;
            } else if ((type & TTYPE_KEEP_SPACE) != 0) {
                if (!fs->StrBuf_add_c(buf, '>')) {
                    return FALSE;
                }
                int keep_space = xof->keep_space;
                xof->keep_space = TRUE;
                for (i = 0; i < ra->size; i++) {
                    if (!node_tostr_xml(buf, dst, ra->p[i], xof, level)) {
                        return FALSE;
                    }
                }
                if (!fs->StrBuf_add(buf, "</", 2) || !fs->StrBuf_add(buf, tag_name->c, tag_name->size) || !fs->StrBuf_add_c(buf, '>')) {
                    return FALSE;
                }
                xof->keep_space = keep_space;
                return TRUE;
            }
        }

        if (ra->size > 0) {
            int prt;

            if (!fs->StrBuf_add_c(buf, '>')) {
                return FALSE;
            }
            if (dst != VALUE_NULL && !stream_write_xmldata(dst, buf, xof)) {
                return FALSE;
            }

            if (level >= 0 && !xof->keep_space) {
                // 子要素が、ブロック要素を含まず、スペース以外のテキストを含む場合、整形せず表示
                int has_block = FALSE;
                int space_only = TRUE;
                prt = TRUE;

                for (i = 0; i < ra->size; i++) {
                    Value vp = ra->p[i];
                    RefNode *vp_type = fs->Value_type(vp);
                    if (vp_type == cls_text) {
                        RefStr *rs = Value_vp(vp);
                        if (!Str_isspace(rs->c, rs->size)) {
                            space_only = FALSE;
                        }
                    } else if (vp_type == cls_comment) {
                        space_only = FALSE;
                        has_block = TRUE;
                    } else if (xof->html) {
                        Ref *rp = Value_ref(vp);
                        RefStr *rs = Value_vp(rp->v[INDEX_ELEM_NAME]);
                        int type2 = get_tag_type_icase(rs->c, rs->size);
                        if ((type2 & TTYPE_BLOCK) != 0) {
                            has_block = TRUE;
                        }
                    } else {
                        has_block = TRUE;
                    }
                }
                if (!has_block && !space_only) {
                    prt = FALSE;
                }
            } else {
                prt = FALSE;
            }
            if (prt) {
                for (i = 0; i < ra->size; i++) {
                    if (fs->Value_type(ra->p[i]) == cls_text) {
                        RefStr *rs = Value_vp(ra->p[i]);
                        if (Str_isspace(rs->c, rs->size)) {
                            continue;
                        }
                    }
                    if (!fs->StrBuf_add_c(buf, '\n')) {
                        return FALSE;
                    }
                    if (!StrBuf_add_spaces(buf, level + 2)) {
                        return FALSE;
                    }
                    if (!node_tostr_xml(buf, dst, ra->p[i], xof, level + 2)) {
                        return FALSE;
                    }
                }

                if (!fs->StrBuf_add_c(buf, '\n')) {
                    return FALSE;
                }
                if (!StrBuf_add_spaces(buf, level)) {
                    return FALSE;
                }
            } else {
                for (i = 0; i < ra->size; i++) {
                    if (!node_tostr_xml(buf, dst, ra->p[i], xof, level)) {
                        return FALSE;
                    }
                }
            }

            if (!fs->StrBuf_add(buf, "</", 2) ||
                !fs->StrBuf_add(buf, tag_name->c, tag_name->size) ||
                !fs->StrBuf_add_c(buf, '>'))
            {
                return FALSE;
            }
        } else if (xof->html) {
            if (!fs->StrBuf_add(buf, "></", 3) ||
                !fs->StrBuf_add(buf, tag_name->c, tag_name->size) ||
                !fs->StrBuf_add_c(buf, '>'))
            {
                return FALSE;
            }
        } else {
            if (!fs->StrBuf_add(buf, " />", 3)) {
                return FALSE;
            }
        }
        if (dst != VALUE_NULL && !stream_write_xmldata(dst, buf, xof)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int xml_parse_format(XMLOutputFormat *xof, const char *fmt_p, int fmt_size)
{
    const char *ptr = fmt_p;
    const char *pend = ptr + fmt_size;
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
            if (isspace_fox(c)) {
                break;
            }
            ptr++;
        }
        fm.size = ptr - fm.p;
        if (fm.size > 0) {
            switch (fm.p[0]) {
            case 'a':
                if (fm.size == 1 || str_eq(fm.p, fm.size, "ascii", -1)) {
                    xof->ascii = TRUE;
                    found = TRUE;
                }
                break;
            case 'h':
                if (fm.size == 1 || str_eq(fm.p, fm.size, "html", -1)) {
                    xof->html = TRUE;
                    found = TRUE;
                }
                break;
            case 'x':
                if (fm.size == 1 || str_eq(fm.p, fm.size, "xml", -1)) {
                    xof->html = FALSE;
                    found = TRUE;
                }
                break;
            case 'p':
                if (fm.size == 1 || str_eq(fm.p, fm.size, "pretty", -1)) {
                    xof->pretty = TRUE;
                    found = TRUE;
                }
                break;
            }
            if (!found) {
                fs->throw_errorf(fs->mod_lang, "FormatError", "Unknown format %S", fm);
                return FALSE;
            }
        }
    }
    xof->keep_space = !xof->pretty;
    return TRUE;
}
static int xml_node_tostr(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;
    XMLOutputFormat xof;

    memset(&xof, 0, sizeof(xof));
    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);
        if (!xml_parse_format(&xof, fmt->c, fmt->size)) {
            return FALSE;
        }
    } else {
        if (FUNC_INT(node)) {
            xof.html = TRUE;
        }
        xof.keep_space = TRUE;
    }

    fs->StrBuf_init_refstr(&buf, 0);
    if (fs->Value_type(*v) == cls_nodelist) {
        RefArray *ra = Value_vp(*v);
        int i;
        for (i = 0; i < ra->size; i++) {
            if (!node_tostr_xml(&buf, VALUE_NULL, ra->p[i], &xof, (xof.pretty ? 0 : -1))) {
                goto ERROR_END;
            }
            if (xof.pretty && !fs->StrBuf_add_c(&buf, '\n')) {
                goto ERROR_END;
            }
        }
    } else {
        if (!node_tostr_xml(&buf, VALUE_NULL, *v, &xof, (xof.pretty ? 0 : -1))) {
            goto ERROR_END;
        }
        if (xof.pretty && !fs->StrBuf_add_c(&buf, '\n')) {
            goto ERROR_END;
        }
    }
    *vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
ERROR_END:
    StrBuf_close(&buf);
    return FALSE;
}

static int parse_output_format_charset(XMLOutputFormat *xof, Value *v)
{
    memset(xof, 0, sizeof(*xof));

    if (fg->stk_top > v + 2) {
        RefStr *fmt = Value_vp(v[2]);
        if (!xml_parse_format(xof, fmt->c, fmt->size)) {
            return FALSE;
        }
    }
    // デフォルトでUTF-8
    if (fg->stk_top > v + 3) {
        xof->cs = Value_vp(v[3]);
        if (xof->cs != fs->cs_utf8) {
            if (!xof->cs->ascii) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Encoding %r is not supported", xof->cs->name);
                return FALSE;
            }
            CodeCVTStatic_init();
            if (!codecvt->CodeCVT_open(&xof->ic, fs->cs_utf8, xof->cs, "&#d;")) {
                return FALSE;
            }
        }
    } else {
        xof->cs = fs->cs_utf8;
    }
    return TRUE;
}
/**
 * node.save(dest, format, charcode)
 */
static int xml_node_save(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;    // temporaryとして使用
    XMLOutputFormat xof;
    Value writer;

    writer = VALUE_NULL;
    if (!fs->value_to_streamio(&writer, v[1], TRUE, 0)) {
        return FALSE;
    }

    if (!parse_output_format_charset(&xof, v)) {
        fs->unref(writer);
        return FALSE;
    }

    fs->StrBuf_init(&buf, 0);
    if (fs->Value_type(*v) == cls_nodelist) {
        RefArray *ra = Value_vp(*v);
        int i;
        for (i = 0; i < ra->size; i++) {
            if (!node_tostr_xml(&buf, writer, ra->p[i], &xof, (xof.pretty ? 0 : -1))) {
                goto ERROR_END;
            }
            if (xof.pretty && !fs->StrBuf_add_c(&buf, '\n')) {
                goto ERROR_END;
            }
        }
    } else {
        if (!node_tostr_xml(&buf, writer, *v, &xof, (xof.pretty ? 0 : -1))) {
            goto ERROR_END;
        }
        if (xof.pretty && !fs->StrBuf_add_c(&buf, '\n')) {
            goto ERROR_END;
        }
    }

    if (!stream_write_xmldata(writer, &buf, &xof)) {
        goto ERROR_END;
    }
    StrBuf_close(&buf);
    fs->unref(writer);
    if (xof.cs != fs->cs_utf8) {
        CodeCVTStatic_init();
        codecvt->CodeCVT_close(&xof.ic);
    }
    return TRUE;
ERROR_END:
    fs->unref(writer);
    StrBuf_close(&buf);
    if (xof.cs != fs->cs_utf8) {
        CodeCVTStatic_init();
        codecvt->CodeCVT_close(&xof.ic);
    }
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_elem_new(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra;
    int pos = 2;
    RefStr *rs_name = Value_vp(v[1]);
    Ref *r = fs->ref_new(cls_elem);
    *vret = vp_Value(r);

    if (!is_valid_elem_name(rs_name->c, rs_name->size)) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid element name");
        return FALSE;
    }

    r->v[INDEX_ELEM_NAME] = fs->Value_cp(v[1]);

    ra = fs->refarray_new(0);
    ra->rh.type = cls_nodelist;
    r->v[INDEX_ELEM_CHILDREN] = vp_Value(ra);

    if (fg->stk_top > v + 2 && fs->Value_type(v[2]) == fs->cls_map) {
        int i;
        Value pv = v[2];
        RefMap *map = Value_vp(pv);
        RefMap *rm_dst = fs->refmap_new(0);
        r->v[INDEX_ELEM_ATTR] = vp_Value(rm_dst);

        for (i = 0; i < map->entry_num; i++) {
            HashValueEntry *ep;
            for (ep = map->entry[i]; ep != NULL; ep = ep->next) {
                HashValueEntry *ve;
                if (fs->Value_type(ep->key) != fs->cls_str || fs->Value_type(ep->val) != fs->cls_str) {
                    fs->throw_errorf(fs->mod_lang, "TypeError", "Argument #2 must be map of {str:str, str:str ...}");
                    return FALSE;
                }
                ve = fs->refmap_add(rm_dst, ep->key, TRUE, FALSE);
                if (ve == NULL) {
                    return FALSE;
                }
                ve->val = fs->Value_cp(ep->val);
            }
        }
        pos = 3;
    }
    for (; v + pos < fg->stk_top; pos++) {
        RefNode *v_type = fs->Value_type(v[pos]);
        if (v_type == cls_elem || v_type == cls_text || v_type == cls_comment) {
            Value *pv = fs->refarray_push(ra);
            *pv = fs->Value_cp(v[pos]);
        } else if (v_type == fs->cls_str) {
            Value tmp;
            Value *pv;
            RefStr *rs = Value_vp(v[pos]);
            if (!parse_xml_from_str(&tmp, rs->c, rs->size)) {
                return FALSE;
            }
            pv = fs->refarray_push(ra);
            // moveなので参照カウンタは変化しない
            *pv = tmp;
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, cls_node, v_type, pos + 1);
            return FALSE;
        }
    }

    return TRUE;
}
static int xml_elem_parse(Value *vret, Value *v, RefNode *node)
{
    XMLTok tk;
    RefStr *src = Value_vp(v[1]);

    XMLTok_init(&tk, src->c, src->c + src->size, TRUE, FALSE);
    XMLTok_skip_next(&tk);
    if (!parse_xml_body(vret, &tk)) {
        return FALSE;
    }
    if (tk.type != TK_EOS) {
        goto PARSE_ERROR;
    }
    {
        RefNode *type = fs->Value_type(*vret);
        if (type != cls_elem) {
            goto PARSE_ERROR;
        }
    }

    return TRUE;

PARSE_ERROR:
    fs->throw_errorf(mod_xml, "XMLParseError", "Unexpected token at line %d", tk.line);
    return FALSE;
}
static int xml_elem_index_key(Value *vret, Value *v, RefNode *node)
{
    int i;
    Ref *r = Value_ref(*v);
    RefStr *key = Value_vp(v[1]);

    if (key->size >= 1 && key->c[0] == '@') {
        Value vkey = fs->cstr_Value(fs->cls_str, key->c + 1, key->size - 1);
        fs->unref(v[1]);
        v[1] = vkey;
        return xml_elem_attr(vret, v, node);
    } else {
        RefArray *ra = fs->refarray_new(0);
        RefArray *rch = Value_vp(r->v[INDEX_ELEM_CHILDREN]);

        *vret = vp_Value(ra);
        ra->rh.type = cls_nodelist;

        for (i = 0; i < rch->size; i++) {
            Value vk = rch->p[i];
            if (fs->Value_type(vk) == cls_elem) {
                Ref *rk = Value_ref(vk);
                RefStr *k = Value_vp(rk->v[INDEX_ELEM_NAME]);
                if (str_eq(key->c, key->size, k->c, k->size)) {
                    Value *ve = fs->refarray_push(ra);
                    *ve = fs->Value_cp(vk);
                }
            }
        }
    }

    return TRUE;
}
static int xml_elem_set_index_key(Value *vret, Value *v, RefNode *node)
{
    RefStr *key = Value_vp(v[1]);

    if (key->size >= 1 && key->c[0] == '@') {
        Value vkey = fs->cstr_Value(fs->cls_str, key->c + 1, key->size - 1);
        fs->unref(v[1]);
        v[1] = vkey;
        return xml_elem_attr(vret, v, node);
    } else {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Attribute name needs @");
        return FALSE;
    }

    return TRUE;
}

static int xml_attr_eq_sub(RefMap *r1, RefMap *r2)
{
    int i;
    for (i = 0; i < r1->entry_num; i++) {
        HashValueEntry *ve1 = r1->entry[i];
        for (; ve1 != NULL; ve1 = ve1->next) {
            HashValueEntry *ve2 = NULL;
            fs->refmap_get(&ve2, r2, ve1->key);

            if (ve2 == NULL) {
                return FALSE;
            }
            if (!refstr_eq(Value_vp(ve1->val), Value_vp(ve2->val))) {
                return FALSE;
            }
        }
    }
    return TRUE;
}
static int xml_node_eq_sub(Value *v1, int v1_size, Value *v2, int v2_size)
{
    int i;

    if (v1_size != v2_size) {
        return FALSE;
    }
    if (v1 == v2) {
        return TRUE;
    }

    for (i = 0; i < v1_size; i++) {
        RefNode *v1_type = fs->Value_type(v1[i]);
        RefNode *v2_type = fs->Value_type(v2[i]);

        if (v1_type != v2_type) {
            return FALSE;
        } else if (v1_type == cls_elem) {
            Ref *r1 = Value_vp(v1[i]);
            Ref *r2 = Value_vp(v2[i]);
            if (r1 != r2) {
                RefMap *rm1 = Value_vp(r1->v[INDEX_ELEM_ATTR]);
                RefMap *rm2 = Value_vp(r2->v[INDEX_ELEM_ATTR]);
                RefArray *ra1 = Value_vp(r1->v[INDEX_ELEM_CHILDREN]);
                RefArray *ra2 = Value_vp(r2->v[INDEX_ELEM_CHILDREN]);
                int rm1_count, rm2_count;

                if (!refstr_eq(Value_vp(r1->v[INDEX_ELEM_NAME]), Value_vp(r2->v[INDEX_ELEM_NAME]))) {
                    return FALSE;
                }
                if (rm1 != NULL) {
                    rm1_count = rm1->count;
                } else {
                    rm1_count = 0;
                }
                if (rm2 != NULL) {
                    rm2_count = rm2->count;
                } else {
                    rm2_count = 0;
                }
                if (rm1_count != rm2_count) {
                    return FALSE;
                }
                if (rm1_count > 0) {
                    if (!xml_attr_eq_sub(rm1, rm2)) {
                        return FALSE;
                    }
                }
                if (!xml_node_eq_sub(ra1->p, ra1->size, ra2->p, ra2->size)) {
                    return FALSE;
                }
            }
        } else {
            if (!refstr_eq(Value_vp(v1[i]), Value_vp(v2[i]))) {
                return FALSE;
            }
        }
    }
    return TRUE;
}
static int xml_elem_eq(Value *vret, Value *v, RefNode *node)
{
    if (xml_node_eq_sub(v, 1, v + 1, 1)) {
        *vret = VALUE_TRUE;
    } else {
        *vret = VALUE_FALSE;
    }
    return TRUE;
}
/**
 * 子要素を追加
 */
static int xml_elem_push(Value *vret, Value *v, RefNode *node)
{
    Value *vp;
    Value tmp;
    int vp_size;
    RefNode *v1_type = fs->Value_type(v[1]);
    
    if (v1_type == cls_elem || v1_type == cls_text) {
        vp = &v[1];
        vp_size = 1;
        if (xml_elem_has_elem(*v, *vp)) {
            fs->throw_error_select(THROW_LOOP_REFERENCE);
            return FALSE;
        }
    } else if (v1_type == cls_nodelist) {
        RefArray *ra = Value_vp(v[1]);
        vp = ra->p;
        vp_size = ra->size;
        // 循環参照のチェック
        int i;
        for (i = 0; i < vp_size; i++) {
            if (xml_elem_has_elem(*v, vp[i])) {
                fs->throw_error_select(THROW_LOOP_REFERENCE);
                return FALSE;
            }
        }
    } else if (v1_type == fs->cls_str) {
        RefStr *rs = Value_vp(v[1]);
        if (!parse_xml_from_str(&tmp, rs->c, rs->size)) {
            return FALSE;
        }
        vp = &tmp;
        vp_size = 1;
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "XMLRefNode, XMLRefNodeList or Str required but %n (argument #1)", v1_type);
        return FALSE;
    }

    {
        Ref *r = Value_ref(*v);
        RefArray *ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);

        if (fg->stk_top > v + 2) {
            // insert
        } else {
            if (FUNC_INT(node)) {
                // unshift
                int size = ra->size;
                int i;
                for (i = 0; i < vp_size; i++) {
                    fs->refarray_push(ra);
                }
                memmove(ra->p + vp_size, ra->p, size * sizeof(Value));
                for (i = 0; i < vp_size; i++) {
                    ra->p[i] = fs->Value_cp(vp[i]);
                }
            } else {
                // push
                int i;
                for (i = 0; i < vp_size; i++) {
                    Value *vd = fs->refarray_push(ra);
                    *vd = fs->Value_cp(vp[i]);
                }
            }
        }
    }
    if (v1_type == fs->cls_str) {
        fs->unref(tmp);
    }

    return TRUE;
}
static int xml_elem_index(Value *vret, Value *v, RefNode *node)
{
    RefNode *v1_type = fs->Value_type(v[1]);

    if (v1_type == fs->cls_str) {
        return xml_elem_index_key(vret, v, node);
    } else if (v1_type == fs->cls_int) {
        Ref *r = Value_ref(*v);
        RefArray *ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
        int64_t idx = fs->Value_int64(v[1], NULL);

        if (idx < 0) {
            idx += ra->size;
        }
        if (idx < 0 || idx >= ra->size) {
            fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, &v[1], ra->size);
            return FALSE;
        }
        *vret = fs->Value_cp(ra->p[idx]);
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_int, v1_type, 1);
        return FALSE;
    }

    return TRUE;
}
static int xml_elem_iterator(Value *vret, Value *v, RefNode *node)
{
    int (*array_iterator)(Value *, Value *, RefNode *) = get_function_ptr(fs->cls_list, fs->str_iterator);
    Ref *r = Value_ref(*v);

    array_iterator(vret, &r->v[INDEX_ELEM_CHILDREN], node);
    return TRUE;
}
static int xml_elem_children(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = fs->Value_cp(r->v[INDEX_ELEM_CHILDREN]);
    return TRUE;
}
static int xml_elem_tagname(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = fs->Value_cp(r->v[INDEX_ELEM_NAME]);
    return TRUE;
}
static int xml_elem_attr(Value *vret, Value *v, RefNode *node)
{
    Value *vp;
    int v_size;
    int i;

    if (FUNC_INT(node)) {
        RefArray *ra = Value_vp(*v);
        vp = ra->p;
        v_size = ra->size;
    } else {
        vp = v;
        v_size = 1;
    }

    if (fg->stk_top > v + 2) {
        RefNode *v_type = fs->Value_type(v[2]);
        if (v_type == fs->cls_str) {
            // 追加
            for (i = 0; i < v_size; i++) {
                Value p = vp[i];
                if (fs->Value_type(p) == cls_elem) {
                    HashValueEntry *ve;
                    Ref *pr = Value_ref(p);
                    Value *v_rm = &pr->v[INDEX_ELEM_ATTR];

                    if (!Value_isref(*v_rm)) {
                        *v_rm = vp_Value(fs->refmap_new(0));
                    }
                    ve = fs->refmap_add(Value_vp(*v_rm), v[1], TRUE, FALSE);
                    if (ve == NULL) {
                        return TRUE;
                    }
                    ve->val = fs->Value_cp(v[2]);
                }
            }
        } else if (v_type == fs->cls_null) {
            // 削除
            for (i = 0; i < v_size; i++) {
                Value p = vp[i];
                if (fs->Value_type(p) == cls_elem) {
                    Ref *pr = Value_ref(p);
                    Value v_rm = pr->v[INDEX_ELEM_ATTR];
                    if (Value_isref(v_rm)) {
                        RefMap *rm = Value_vp(v_rm);
                        if (!fs->refmap_del(vret, rm, v[1])) {
                            return FALSE;
                        }
                        if (rm->count == 0) {
                            fs->unref(pr->v[INDEX_ELEM_ATTR]);
                            pr->v[INDEX_ELEM_ATTR] = VALUE_NULL;
                        }
                    }
                }
            }
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_null, v_type, 2);
            return FALSE;
        }
    } else {
        // 取得
        HashValueEntry *he_found = NULL;

        for (i = 0; i < v_size; i++) {
            Value p = vp[i];
            if (fs->Value_type(p) == cls_elem) {
                Ref *pr = Value_ref(p);
                Value v_rm = pr->v[INDEX_ELEM_ATTR];

                if (Value_isref(v_rm)) {
                    HashValueEntry *he;
                    if (!fs->refmap_get(&he, Value_vp(v_rm), v[1])) {
                        return FALSE;
                    }
                    if (he != NULL) {
                        if (he_found != NULL) {
                            fs->throw_errorf(fs->mod_lang, "ValueError", "More than 2 elements matches");
                            return FALSE;
                        }
                        he_found = he;
                    }
                }
            }
        }
        if (he_found != NULL) {
            *vret = fs->Value_cp(he_found->val);
        }
    }

    return TRUE;
}
/**
 * xmlのテキスト部分を抽出
 */
static int node_text_xml(StrBuf *buf, Value v)
{
    RefNode *type = fs->Value_type(v);

    if (type == cls_text) {
        if (!fs->StrBuf_add_r(buf, Value_vp(v))) {
            return FALSE;
        }
    } else if (type == cls_elem) {
        Ref *r = Value_ref(v);
        RefArray *ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
        int i;
        for (i = 0; i < ra->size; i++) {
            if (!node_text_xml(buf, ra->p[i])) {
                return FALSE;
            }
        }
    }
    return TRUE;
}
static int xml_node_text(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;

    fs->StrBuf_init_refstr(&buf, 0);
    if (fs->Value_type(*v) == cls_nodelist) {
        RefArray *ra = Value_vp(*v);
        int i;
        for (i = 0; i < ra->size; i++) {
            if (!node_text_xml(&buf, ra->p[i])) {
                StrBuf_close(&buf);
                return FALSE;
            }
        }
    } else {
        if (!node_text_xml(&buf, *v)) {
            StrBuf_close(&buf);
            return FALSE;
        }
    }
    *vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
}

/**
 * CSSセレクタ互換
 */
static int xml_elem_css(Value *vret, Value *v, RefNode *node)
{
    if (!select_css(vret, v, 1, Value_str(v[1]))) {
        return FALSE;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int is_valid_instruction(const char *p, int size)
{
    const char *end = p + size;
    while (p < end) {
        if (p[1] == '>') {
            return FALSE;
        }
        p++;
    }
    return TRUE;
}
static int xml_decl_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = fs->ref_new(cls_decl);
    RefStr *name = Value_vp(v[1]);
    RefStr *inst = Value_vp(v[2]);

    if (!is_valid_elem_name(name->c, name->size) || str_eq(name->c, name->size, "xml", 3)) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid instruction name");
        return FALSE;
    }
    if (!is_valid_instruction(inst->c, inst->size)) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid instruction content");
        return FALSE;
    }
    
    r->v[INDEX_DECL_NAME] = fs->Value_cp(v[1]);
    r->v[INDEX_DECL_CONTENT] = fs->Value_cp(v[2]);
    *vret = vp_Value(r);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_text_new(Value *vret, Value *v, RefNode *node)
{
    const RefStr *rs = Value_vp(v[1]);
    *vret = fs->cstr_Value(cls_text, rs->c, rs->size);
    return TRUE;
}
static int xml_text_text(Value *vret, Value *v, RefNode *node)
{
    const RefStr *rs = Value_vp(*v);
    *vret = fs->cstr_Value(fs->cls_str, rs->c, rs->size);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_comment_new(Value *vret, Value *v, RefNode *node)
{
    const RefStr *rs = Value_vp(v[1]);
    int i;

    // -->が含まれていたらエラー
    for (i = 0; i < rs->size - 2; i++) {
        if (memcmp(rs->c + i, "-->", 3) == 0) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Parameter contains '-->'");
            return FALSE;
        }
    }
    *vret = fs->cstr_Value(cls_comment, rs->c, rs->size);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_node_list_css(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);

    if (!select_css(vret, ra->p, ra->size, Value_str(v[1]))) {
        return FALSE;
    }
    return TRUE;
}
static int xml_node_remove_css(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    RefNode *v1_type = fs->Value_type(v[1]);
    int result = FALSE;

    if (v1_type == fs->cls_str) {
        result = delete_css(vret, ra->p, ra->size, Value_str(v[1]));
    } else if (v1_type == cls_nodelist) {
        result = delete_nodelist(vret, ra->p, ra->size, Value_vp(v[1]));
    } else if (v1_type == cls_elem) {
        RefArray ra2;
        Value va = v[1];
        fs->addref(va);
        ra2.p = &va;
        ra2.size = 1;
        result = delete_nodelist(vret, ra->p, ra->size, &ra2);
        fs->unref(va);
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Str, XMLElem or XMLRefNodeList required but %n (argument #2)", v1_type);
        return FALSE;
    }
    if (!result) {
        return FALSE;
    }
    return TRUE;
}

static int xml_node_list_index_key(Value *vret, Value *v, RefNode *node)
{
    RefArray *rch = Value_vp(*v);
    RefNode *v1_type = fs->Value_type(v[1]);

    if (v1_type == fs->cls_str) {
        RefStr *key = Value_vp(v[1]);
        if (key->size >= 1 && key->c[0] == '@') {
            Value vkey = fs->cstr_Value(fs->cls_str, key->c + 1, key->size - 1);
            fs->unref(v[1]);
            v[1] = vkey;
            return xml_elem_attr(vret, v, node);
        } else {
            RefArray *ra = fs->refarray_new(0);
            int i, j;

            *vret = vp_Value(ra);
            ra->rh.type = cls_nodelist;
            for (i = 0; i < rch->size; i++) {
                Value vch2 = rch->p[i];
                RefArray *rch2 = Value_vp(Value_ref(vch2)->v[INDEX_ELEM_CHILDREN]);

                for (j = 0; j < rch2->size; j++) {
                    Value vk = rch2->p[j];
                    if (fs->Value_type(vk) == cls_elem) {
                        RefStr *k = Value_vp(Value_ref(vk)->v[INDEX_ELEM_NAME]);
                        if (str_eq(key->c, key->size, k->c, k->size)) {
                            Value *ve = fs->refarray_push(ra);
                            *ve = fs->Value_cp(vk);
                        }
                    }
                }
            }
        }
    } else if (v1_type == fs->cls_int) {
        int64_t idx = fs->Value_int64(v[1], NULL);

        if (idx < 0) {
            idx += rch->size;
        }
        if (idx < 0 || idx >= rch->size) {
            fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], rch->size);
            return FALSE;
        }
        *vret = fs->Value_cp(rch->p[idx]);
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_null, v1_type, 1);
        return FALSE;
    }

    return TRUE;
}
static int xml_node_list_eq(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra1 = Value_vp(*v);
    RefArray *ra2 = Value_vp(v[1]);

    if (xml_node_eq_sub(ra1->p, ra1->size, ra2->p, ra2->size)) {
        *vret = VALUE_TRUE;
    } else {
        *vret = VALUE_FALSE;
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int xml_document_new(Value *vret, Value *v, RefNode *node)
{
    XMLTok tk;
    Ref *r = fs->ref_new(cls_document);
    *vret = vp_Value(r);

    {
        RefStr *dtd_src = Value_vp(v[1]);
        XMLTok_init(&tk, dtd_src->c, dtd_src->c + dtd_src->size, TRUE, FALSE);
    }

    // default value
    r->v[INDEX_DOCUMENT_IS_PUBLIC] = VALUE_TRUE;
    if (!parse_doctype_declaration(r, &tk)) {
        fs->throw_errorf(mod_xml, "XMLParseError", "Illigal DOCTYPE");
        return FALSE;
    }
    if (tk.type != TK_EOS) {
        fs->throw_errorf(mod_xml, "XMLParseError", "Illigal DOCTYPE");
        return FALSE;
    }
    {
        RefArray *ra = fs->refarray_new(0);
        r->v[INDEX_DOCUMENT_LIST] = vp_Value(ra);
        if (fg->stk_top > v + 2) {
            r->v[INDEX_DOCUMENT_ROOT] = fs->Value_cp(v[2]);
            *fs->refarray_push(ra) = fs->Value_cp(v[2]);
        }
    }

    return TRUE;
}
static int xml_document_set_dtd(Value *vret, Value *v, RefNode *node)
{
    XMLTok tk;
    Ref *r = Value_vp(*v);

    {
        RefStr *dtd_src = Value_vp(v[1]);
        XMLTok_init(&tk, dtd_src->c, dtd_src->c + dtd_src->size, TRUE, FALSE);
    }

    // default value
    r->v[INDEX_DOCUMENT_IS_PUBLIC] = VALUE_TRUE;
    fs->unref(r->v[INDEX_DOCUMENT_FPI]);
    r->v[INDEX_DOCUMENT_FPI] = VALUE_NULL;
    fs->unref(r->v[INDEX_DOCUMENT_DTD_URI]);
    r->v[INDEX_DOCUMENT_DTD_URI] = VALUE_NULL;
    if (!parse_doctype_declaration(r, &tk)) {
        fs->throw_errorf(mod_xml, "XMLParseError", "Illigal DOCTYPE");
        return FALSE;
    }

    return TRUE;
}
static int xml_document_get_property(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    int idx = FUNC_INT(node);
    *vret = fs->Value_cp(r->v[idx]);
    return TRUE;
}
static int xml_document_set_property(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    int idx = FUNC_INT(node);
    r->v[idx] = fs->Value_cp(v[1]);
    return TRUE;
}
static int xml_document_index(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    RefArray *ra = Value_vp(r->v[INDEX_DOCUMENT_LIST]);
    int err;
    int64_t idx = fs->Value_int64(v[1], &err);

    if (err) {
        fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], ra->size);
        return FALSE;
    }
    if (idx < 0) {
        idx += ra->size;
    }
    if (idx < 0 || idx >= ra->size) {
        fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], ra->size);
        return FALSE;
    }
    *vret = fs->Value_cp(ra->p[idx]);

    return TRUE;
}
static int xml_document_print_header(StrBuf *buf, Ref *r, XMLOutputFormat *xof)
{
    if (!Value_isref(r->v[INDEX_DOCUMENT_ROOT])) {
        fs->throw_errorf(mod_xml, "XMLError", "root element not exists");
        return FALSE;
    }
    if (!xof->html) {
        RefCharset *cs = xof->cs;
        if (cs == NULL) {
            cs = fs->cs_utf8;
        }
        if (!fs->StrBuf_add(buf, "<?xml version=\"1.0\" encoding=\"", -1)) {
            return FALSE;
        }
        if (!fs->StrBuf_add_r(buf, cs->iana)) {
            return FALSE;
        }
        if (!fs->StrBuf_add(buf, "\"?>\n", -1)) {
            return FALSE;
        }
    }
    if (Value_bool(r->v[INDEX_DOCUMENT_HAS_DTD])) {
        Value root_name = Value_ref(r->v[INDEX_DOCUMENT_ROOT])->v[INDEX_ELEM_NAME];
        if (!fs->StrBuf_add(buf, "<!DOCTYPE ", -1) || !fs->StrBuf_add_r(buf, Value_vp(root_name))) {
            return FALSE;
        }
        if (Value_isref(r->v[INDEX_DOCUMENT_FPI]) || Value_isref(r->v[INDEX_DOCUMENT_DTD_URI])) {
            if (Value_bool(r->v[INDEX_DOCUMENT_IS_PUBLIC])) {
                if (!fs->StrBuf_add(buf, " PUBLIC", -1)) {
                    return FALSE;
                }
            } else {
                if (!fs->StrBuf_add(buf, " SYSTEM", -1)) {
                    return FALSE;
                }
            }
            if (Value_isref(r->v[INDEX_DOCUMENT_FPI])) {
                if (!fs->StrBuf_add(buf, " \"", -1)) {
                    return FALSE;
                }
                if (!text_convert_html(buf, Value_str(r->v[INDEX_DOCUMENT_FPI]), TRUE, xof)) {
                    return FALSE;
                }
                if (!fs->StrBuf_add_c(buf, '"')) {
                    return FALSE;
                }
            }
            if (Value_isref(r->v[INDEX_DOCUMENT_DTD_URI])) {
                if (!fs->StrBuf_add(buf, " \"", -1)) {
                    return FALSE;
                }
                if (!text_convert_html(buf, Value_str(r->v[INDEX_DOCUMENT_DTD_URI]), TRUE, xof)) {
                    return FALSE;
                }
                if (!fs->StrBuf_add_c(buf, '"')) {
                    return FALSE;
                }
            }
        }
        if (!fs->StrBuf_add(buf, ">\n", -1)) {
            return FALSE;
        }
    }
    return TRUE;
}
static int xml_document_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    StrBuf buf;
    XMLOutputFormat xof;

    memset(&xof, 0, sizeof(xof));
    if (fg->stk_top > v + 1) {
        RefStr *fmt = Value_vp(v[1]);
        if (!xml_parse_format(&xof, fmt->c, fmt->size)) {
            return FALSE;
        }
    } else {
        if (FUNC_INT(node)) {
            xof.html = TRUE;
        }
        xof.keep_space = TRUE;
    }

    fs->StrBuf_init_refstr(&buf, 0);
    if (!xml_document_print_header(&buf, r, &xof)) {
        goto ERROR_END;
    }
    {
        RefArray *ra = Value_vp(r->v[INDEX_DOCUMENT_LIST]);
        int i;
        for (i = 0; i < ra->size; i++) {
            if (!node_tostr_xml(&buf, VALUE_NULL, ra->p[i], &xof, (xof.pretty ? 0 : -1))) {
                goto ERROR_END;
            }
            if (xof.pretty) {
                if (!fs->StrBuf_add_c(&buf, '\n')) {
                    goto ERROR_END;
                }
            }
        }
    }
    *vret = fs->StrBuf_str_Value(&buf, fs->cls_str);

    return TRUE;
ERROR_END:
    StrBuf_close(&buf);
    return FALSE;
}
/**
 * node.save(dest, format, charcode)
 */
static int xml_document_save(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    StrBuf buf;    // temporaryとして使用
    XMLOutputFormat xof;
    Value writer;

    writer = VALUE_NULL;
    if (!fs->value_to_streamio(&writer, v[1], TRUE, 0)) {
        return FALSE;
    }

    if (!parse_output_format_charset(&xof, v)) {
        fs->unref(writer);
        return FALSE;
    }

    fs->StrBuf_init(&buf, 0);
    if (!xml_document_print_header(&buf, r, &xof)) {
        goto ERROR_END;
    }
    if (!stream_write_xmldata(writer, &buf, &xof)) {
        goto ERROR_END;
    }
    {
        RefArray *ra = Value_vp(r->v[INDEX_DOCUMENT_LIST]);
        int i;
        for (i = 0; i < ra->size; i++) {
            if (!node_tostr_xml(&buf, VALUE_NULL, ra->p[i], &xof, (xof.pretty ? 0 : -1))) {
                goto ERROR_END;
            }
            if (xof.pretty) {
                if (!fs->StrBuf_add_c(&buf, '\n')) {
                    goto ERROR_END;
                }
            }
            if (!stream_write_xmldata(writer, &buf, &xof)) {
                goto ERROR_END;
            }
        }
    }
    StrBuf_close(&buf);
    fs->unref(writer);
    if (xof.cs != fs->cs_utf8) {
        CodeCVTStatic_init();
        codecvt->CodeCVT_close(&xof.ic);
    }
    return TRUE;
ERROR_END:
    fs->unref(writer);
    StrBuf_close(&buf);
    if (xof.cs != fs->cs_utf8) {
        CodeCVTStatic_init();
        codecvt->CodeCVT_close(&xof.ic);
    }
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_const(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "DTD_HTML4", NODE_CONST, 0);
    n->u.k.val = fs->cstr_Value(fs->cls_str, "html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"", -1);

    n = fs->define_identifier(m, m, "DTD_HTML4TR", NODE_CONST, 0);
    n->u.k.val = fs->cstr_Value(fs->cls_str, "html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\"", -1);

    n = fs->define_identifier(m, m, "DTD_XHTML1", NODE_CONST, 0);
    n->u.k.val = fs->cstr_Value(fs->cls_str, "html PUBLIC PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"", -1);

    n = fs->define_identifier(m, m, "DTD_HTML5", NODE_CONST, 0);
    n->u.k.val = fs->cstr_Value(fs->cls_str, "html", -1);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *cls2;
    RefNode *n;
    RefStr *empty = fs->intern("empty", -1);
    RefStr *size = fs->intern("size", -1);

    cls_document = fs->define_identifier(m, m, "XMLDocument", NODE_CLASS, 0);
    cls_node = fs->define_identifier(m, m, "XMLNode", NODE_CLASS, NODEOPT_ABSTRACT);
    cls_nodelist = fs->define_identifier(m, m, "XMLNodeList", NODE_CLASS, 0);
    cls_elem = fs->define_identifier(m, m, "XMLElem", NODE_CLASS, 0);
    cls_decl = fs->define_identifier(m, m, "XMLDeclaration", NODE_CLASS, 0);
    cls_text = fs->define_identifier(m, m, "XMLText", NODE_CLASS, NODEOPT_STRCLASS);
    cls_comment = fs->define_identifier(m, m, "XMLComment", NODE_CLASS, NODEOPT_STRCLASS);

    // XMLDocument
    cls = cls_document;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, xml_document_new, 1, 2, NULL, fs->cls_str, cls_elem);
    n = fs->define_identifier(m, cls, "parse_xml", NODE_NEW_N, 0);
    fs->define_native_func_a(n, parse_xml, 1, 2, (void*) LOAD_XML, NULL, fs->cls_bool);
    n = fs->define_identifier(m, cls, "parse_html", NODE_NEW_N, 0);
    fs->define_native_func_a(n, parse_xml, 1, 2, (void*) 0, NULL, fs->cls_charset);
    n = fs->define_identifier(m, cls, "load_xml", NODE_NEW_N, 0);
    fs->define_native_func_a(n, parse_xml, 1, 2, (void*) (LOAD_XML|LOAD_FILE), NULL, fs->cls_bool);
    n = fs->define_identifier(m, cls, "load_html", NODE_NEW_N, 0);
    fs->define_native_func_a(n, parse_xml, 1, 2, (void*) LOAD_FILE, NULL, fs->cls_charset);

    n = fs->define_identifier(m, cls, "has_dtd", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_document_get_property, 0, 0, (void*)INDEX_DOCUMENT_HAS_DTD);
    n = fs->define_identifier(m, cls, "set_dtd", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_document_set_dtd, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "is_public", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_document_get_property, 0, 0, (void*)INDEX_DOCUMENT_IS_PUBLIC);
    n = fs->define_identifier(m, cls, "public_identifier", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_document_get_property, 0, 0, (void*)INDEX_DOCUMENT_FPI);
    n = fs->define_identifier(m, cls, "uri", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_document_get_property, 0, 0, (void*)INDEX_DOCUMENT_DTD_URI);
    n = fs->define_identifier(m, cls, "root", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_document_get_property, 0, 0, (void*)INDEX_DOCUMENT_ROOT);
    n = fs->define_identifier(m, cls, "root=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_document_set_property, 1, 1, (void*)INDEX_DOCUMENT_ROOT, cls_node);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_document_index, 1, 1, NULL, fs->cls_int);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_document_tostr, 0, 2, (void*)FALSE, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier(m, cls, "as_xml", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_document_tostr, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "as_html", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_document_tostr, 0, 0, (void*)TRUE);
    n = fs->define_identifier(m, cls, "save", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_document_save, 1, 3, NULL, NULL, fs->cls_str, fs->cls_charset);

    cls->u.c.n_memb = INDEX_DOCUMENT_NUM;
    fs->extends_method(cls, fs->cls_obj);


    // XMLRefNodeList
    cls = cls_nodelist;
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_dtor), 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_tostr, 0, 2, (void*)FALSE, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier(m, cls, "as_text", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_node_text, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "as_xml", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_node_tostr, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "as_html", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_node_tostr, 0, 0, (void*)TRUE);
    n = fs->define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, empty), 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, size, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, size), 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_list_index_key, 1, 1, NULL, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_set_index_key, 2, 2, (void*) TRUE, fs->cls_str, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_list_index_key, 1, 1, (void*) TRUE, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_missing_set, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_set_index_key, 2, 2, (void*) TRUE, NULL, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_list_eq, 1, 1, NULL, cls_nodelist);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->symbol_stock[T_LET_B]), 2, 2, NULL, fs->cls_int, cls_node);
    n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, get_function_ptr(fs->cls_list, fs->str_iterator), 0, 0, NULL);
    n = fs->define_identifier(m, cls, "attr", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_attr, 1, 2, (void*) TRUE, fs->cls_str, NULL);
    n = fs->define_identifier(m, cls, "select", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_list_css, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "remove", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_remove_css, 1, 1, NULL, NULL);
    n = fs->define_identifier(m, cls, "save", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_save, 1, 3, NULL, NULL, fs->cls_str, fs->cls_charset);
    fs->extends_method(cls, fs->cls_obj);

    // XMLNode (abstract)
    cls = cls_node;
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_tostr, 0, 2, (void*)FALSE, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier(m, cls, "as_text", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_node_text, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "as_xml", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_node_tostr, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "as_html", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_node_tostr, 0, 0, (void*)TRUE);
    n = fs->define_identifier(m, cls, "save", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_node_save, 1, 3, NULL, NULL, fs->cls_str, fs->cls_charset);
    fs->extends_method(cls, fs->cls_obj);

    // XMLElem
    cls = cls_elem;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, xml_elem_new, 1, -1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "parse", NODE_NEW_N, 0);
    fs->define_native_func_a(n, xml_elem_parse, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_index, 1, 1, NULL, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_set_index_key, 2, 2, (void*) FALSE, fs->cls_str, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_index_key, 1, 1, (void*) FALSE, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_missing_set, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_set_index_key, 2, 2, (void*) FALSE, NULL, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_eq, 1, 1, NULL, cls_elem);
    n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_iterator, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "push", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_push, 1, 1, (void*)FALSE, NULL);
    n = fs->define_identifier(m, cls, "unshift", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_push, 1, 1, (void*)TRUE, NULL);
    n = fs->define_identifier(m, cls, "children", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_elem_children, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "tagname", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_elem_tagname, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "attr", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_attr, 1, 2, (void*) FALSE, fs->cls_str, NULL);
    n = fs->define_identifier(m, cls, "select", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, xml_elem_css, 1, 1, NULL, fs->cls_str);
    cls->u.c.n_memb = INDEX_ELEM_NUM;
    fs->extends_method(cls, cls_node);


    // XMLDeclaration
    cls = cls_decl;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, xml_decl_new, 2, 2, NULL, fs->cls_str, fs->cls_str);

    cls->u.c.n_memb = INDEX_DECL_NUM;
    fs->extends_method(cls, cls_node);


    // XMLText
    cls = cls_text;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, xml_text_new, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "as_text", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, xml_text_text, 0, 0, NULL);
    fs->extends_method(cls, cls_node);


    // XMLComment
    cls = cls_comment;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, xml_comment_new, 1, 1, NULL, fs->cls_str);
    fs->extends_method(cls, cls_node);


    cls = fs->define_identifier(m, m, "XMLError", NODE_CLASS, NODEOPT_ABSTRACT);
    cls2 = cls;
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);

    cls = fs->define_identifier(m, m, "XMLParseError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, cls2);
}

static XMLStatic *XMLStatic_new(void)
{
    XMLStatic *f = fs->Mem_get(&fg->st_mem, sizeof(XMLStatic));

    f->resolve_entity = resolve_entity;
    f->is_valid_elem_name = is_valid_elem_name;

    return f;
}
void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_xml = m;

    m->u.m.ext = XMLStatic_new();

    define_class(m);
    define_const(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}
