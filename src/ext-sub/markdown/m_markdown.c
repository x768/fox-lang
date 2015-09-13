#define DEFINE_GLOBALS
#include "markdown.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


static uint32_t str_hash(const char *p)
{
    uint32_t h = 0;
    for (; *p != '\0'; p++) {
        h = h * 31 + *p;
    }
    return h & (SIMPLE_HASH_MAX - 1);
}

MDNode *SimpleHash_get_node(SimpleHash **hash, const char *name)
{
    uint32_t ihash = str_hash(name);
    SimpleHash *h = hash[ihash];
    for (; h != NULL; h = h->next) {
        if (ihash == h->hash && strcmp(name, h->name) == 0) {
            return h->node;
        }
    }
    return NULL;
}

void SimpleHash_add_node(SimpleHash **hash, Mem *mem, const char *name, MDNode *node)
{
    uint32_t ihash = str_hash(name);
    SimpleHash **pp = &hash[ihash];
    SimpleHash *h = *pp;

    while (h != NULL) {
        if (ihash == h->hash && strcmp(name, h->name) == 0) {
            return;
        }
        pp = &h->next;
        h = *pp;
    }
    h = fs->Mem_get(mem, sizeof(SimpleHash));
    h->hash = ihash;
    h->name = fs->str_dup_p(name, -1, mem);
    h->node = node;
    *pp = h;
}
static void MDNode_tostr_sub(StrBuf *sb, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        if (node->type == MD_TEXT || node->type == MD_CODE) {
            fs->StrBuf_add(sb, node->cstr, -1);
        }
        if (node->child != NULL) {
            MDNode_tostr_sub(sb, node->child);
        }
    }
}
static void MDNode_tostr(StrBuf *sb, MDNode *node)
{
    if (node->type == MD_TEXT || node->type == MD_CODE) {
        fs->StrBuf_add(sb, node->cstr, -1);
    } else if (node->child != NULL) {
        MDNode_tostr_sub(sb, node->child);
    }
}
static void cstr_to_heading_id(StrBuf *sb, const char *p, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        int ch = p[i] & 0xFF;
        if (isalnumu_fox(ch) || ch == '-') {
            fs->StrBuf_add_c(sb, ch);
        } else {
            char cbuf[8];
            sprintf(cbuf, ".%02X", ch);
            fs->StrBuf_add(sb, cbuf, 3);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

MDNode *MDNode_new(int type, Markdown *r)
{
    MDNode *n = fs->Mem_get(&r->mem, sizeof(MDNode));
    memset(n, 0, sizeof(MDNode));
    n->type = type;
    n->indent = 0;
    n->cstr = NULL;
    n->href = NULL;
    return n;
}

static Ref *xmlelem_new(const char *name)
{
    RefArray *ra;
    Ref *r = fs->ref_new(cls_xmlelem);
    r->v[INDEX_ELEM_NAME] = vp_Value(fs->intern(name, -1));

    ra = fs->refarray_new(0);
    ra->rh.type = cls_nodelist;
    r->v[INDEX_ELEM_CHILDREN] = vp_Value(ra);

    return r;
}
static void xmlelem_add(Ref *r, Ref *elem)
{
    RefArray *ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
    Value *v = fs->refarray_push(ra);
    *v = fs->Value_cp(vp_Value(elem));
}

static void xmlelem_add_attr(Ref *r, const char *ckey, const char *cval)
{
    RefMap *rm;
    Value key;
    HashValueEntry *ve;

    if (Value_isref(r->v[INDEX_ELEM_ATTR])) {
        rm = Value_vp(r->v[INDEX_ELEM_ATTR]);
    } else {
        rm = fs->refmap_new(0);
        r->v[INDEX_ELEM_ATTR] = vp_Value(rm);
    }
    key = fs->cstr_Value(fs->cls_str, ckey, -1);
    ve = fs->refmap_add(rm, key, TRUE, FALSE);
    fs->Value_dec(key);
    fs->Value_dec(ve->val);
    ve->val = fs->cstr_Value(fs->cls_str, cval, -1);
}

//////////////////////////////////////////////////////////////////////////////////////////

static int xml_convert_line(Ref *r, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_TEXT:
            xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            break;
        case MD_LINK: {
            Ref *child = xmlelem_new("a");
            xmlelem_add(r, child);
            if (node->href != NULL) {
                xmlelem_add_attr(child, "href", node->href);
            }
            xmlelem_add(child, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            break;
        }
        case MD_LINK_FOOTNOTE: {
            Ref *sup = xmlelem_new("sup");
            Ref *link = xmlelem_new("a");
            char ctext[16];
            char clink[16];
            sprintf(ctext, "[%d]", node->opt);
            sprintf(clink, "#note.%d", node->opt);
            xmlelem_add_attr(link, "href", clink);
            xmlelem_add(link, Value_vp(fs->cstr_Value(cls_xmltext, ctext, -1)));
            xmlelem_add(r, sup);
            xmlelem_add(sup, link);
            break;
        }
        case MD_EM:
        case MD_STRONG:
        case MD_STRIKE: {
            Ref *child;
            switch (node->type) {
            case MD_EM:
                child = xmlelem_new("em");
                break;
            case MD_STRONG:
                child = xmlelem_new("strong");
                break;
            case MD_STRIKE:
                child = xmlelem_new("strike");
                break;
            default:
                return FALSE;
            }
            xmlelem_add(r, child);
            if (!xml_convert_line(child, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_CODE_INLINE: {
            Ref *code;
            code = xmlelem_new("code");
            xmlelem_add(r, code);
            xmlelem_add(code, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            break;
        }
        default:
            break;
        }
    }
    return TRUE;
}
static int xml_source_code(Ref *r, const char *code, const char *type)
{
    // TODO 色分け
    xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, code, -1)));
    return TRUE;
}
static int xml_convert_listitem(Ref *r, MDNode *node, int gen_id)
{
    Ref *prev_li = NULL;
    int footnote_id = 1;

    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_LIST_ITEM: {
            Ref *li = xmlelem_new("li");
            xmlelem_add(r, li);
            if (gen_id) {
                char cbuf[16];
                sprintf(cbuf, "note.%d", footnote_id);
                xmlelem_add_attr(li, "id", cbuf);
                footnote_id++;
            }
            if (!xml_convert_line(li, node->child)) {
                return FALSE;
            }
            prev_li = li;
            break;
        }
        case MD_UNORDERD_LIST:
        case MD_ORDERD_LIST:
            if (prev_li != NULL) {
                Ref *list = xmlelem_new(node->type == MD_UNORDERD_LIST ? "ul" : "ol");
                xmlelem_add(prev_li, list);
                if (!xml_convert_listitem(list, node->child, FALSE)) {
                    return FALSE;
                }
            }
            break;
        }
    }
    return TRUE;
}
static int xml_from_markdown(Ref *root, Markdown *r, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_HEADING: {
            char tag[4];
            int head = node->opt + r->heading_level - 1;
            Ref *heading;
            tag[0] = 'h';
            tag[1] = (head <= 6 ? '0' + head : '6');
            tag[2] = '\0';
            heading = xmlelem_new(tag);
            xmlelem_add(root, heading);
            if (!xml_convert_line(heading, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_HORIZONTAL: {
            Ref *hr = xmlelem_new("hr");
            xmlelem_add(root, hr);
            break;
        }
        case MD_UNORDERD_LIST:
        case MD_ORDERD_LIST: {
            Ref *list = xmlelem_new(node->type == MD_UNORDERD_LIST ? "ul" : "ol");
            xmlelem_add(root, list);
            if (!xml_convert_listitem(list, node->child, FALSE)) {
                return FALSE;
            }
            break;
        }
        case MD_FOOTNOTE_LIST: {
            Ref *div = xmlelem_new("div");
            Ref *list;
            xmlelem_add_attr(div, "class", "footnote");
            list = xmlelem_new("ol");
            xmlelem_add(root, div);
            xmlelem_add(div, list);
            if (!xml_convert_listitem(list, node->child, TRUE)) {
                return FALSE;
            }
            break;
        }
        case MD_PARAGRAPH: {
            Ref *para = xmlelem_new("p");
            xmlelem_add(root, para);
            if (!xml_convert_line(para, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_CODE: {
            Ref *pre = xmlelem_new("pre");
            Ref *code = xmlelem_new("code");
            xmlelem_add(root, pre);
            xmlelem_add(pre, code);
            if (!xml_source_code(code, node->child->cstr, node->cstr)) {
                return FALSE;
            }
            break;
        }
        case MD_BLOCKQUOTE: {
            Ref *blockquote = xmlelem_new("blockquote");
            xmlelem_add(root, blockquote);
            if (!xml_from_markdown(blockquote, r, node->child)) {
                return FALSE;
            }
            break;
        }
        case MD_TABLE: {
            MDNode *nrow;
            Ref *table = xmlelem_new("table");
            xmlelem_add(root, table);
            for (nrow = node->child; nrow != NULL; nrow = nrow->next) {
                MDNode *ncell;
                Ref *row = xmlelem_new("tr");
                xmlelem_add(table, row);
                for (ncell = nrow->child; ncell != NULL; ncell = ncell->next) {
                    Ref *cell = NULL;
                    switch (ncell->type) {
                    case MD_TABLE_HEADER:
                        cell = xmlelem_new("th");
                        break;
                    case MD_TABLE_CELL:
                        cell = xmlelem_new("td");
                        break;
                    }
                    if (cell != NULL) {
                        xmlelem_add(row, cell);
                        if (ncell->opt > 1) {
                            char str[16];
                            sprintf(str, "%d", ncell->opt);
                            xmlelem_add_attr(cell, "colspan", str);
                        }
                        if (!xml_convert_line(cell, ncell->child)) {
                            return FALSE;
                        }
                    }
                }
            }
        }
        case MD_IGNORE:
            xml_from_markdown(root, r, node->child);
            break;
        default:
            break;
        }
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int markdown_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_markdown = FUNC_VP(node);
    Ref *ref;
    Markdown *md;
    RefNode *type = fs->Value_type(*v);

    // 継承可能なクラス
    if (type == fs->cls_fn) {
        ref = fs->ref_new(cls_markdown);
        *vret = vp_Value(ref);
    } else {
        ref = Value_vp(*v);
        *vret = fs->Value_cp(*v);
    }

    md = malloc(sizeof(Markdown));
    memset(md, 0, sizeof(Markdown));
    ref->v[INDEX_MARKDOWN_MD] = ptr_Value(md);

    fs->Mem_init(&md->mem, 1024);
    md->tabstop = 4;
    md->heading_level = 1;
    md->footnote_id = 1;

    md->heading_p = &md->heading;
    md->footnote_p = &md->footnote;

    return TRUE;
}
static int markdown_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Markdown *r = Value_ptr(ref->v[INDEX_MARKDOWN_MD]);
    fs->Mem_close(&r->mem);
    free(r);
    ref->v[INDEX_MARKDOWN_MD] = VALUE_NULL;

    return TRUE;
}
static int markdown_enable_semantic(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        md->enable_semantic = Value_bool(v[1]);
    } else {
        *vret = bool_Value(md->enable_semantic);
    }
    return TRUE;
}
static int markdown_quiet_error(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        md->quiet_error = Value_bool(v[1]);
    } else {
        *vret = bool_Value(md->quiet_error);
    }
    return TRUE;
}
static int markdown_enable_tex(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        md->enable_tex = Value_bool(v[1]);
    } else {
        *vret = bool_Value(md->enable_tex);
    }
    return TRUE;
}
static int markdown_tabstop(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        int64_t i64 = fs->Value_int64(v[1], NULL);
        if (i64 < 1 || i64 > 16) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Out of range (1 - 16)");
            return FALSE;
        }
        md->tabstop = (int)i64;
    } else {
        *vret = int32_Value(md->tabstop);
    }
    return TRUE;
}
static int markdown_heading_level(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    if (fg->stk_top > v + 1) {
        int64_t i64 = fs->Value_int64(v[1], NULL);
        if (i64 < 1 || i64 > 6) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Out of range (1 - 6)");
            return FALSE;
        }
        md->heading_level = (int)i64;
    } else {
        *vret = int32_Value(md->heading_level);
    }
    return TRUE;
}
static int markdown_heading_list(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    RefArray *ra = fs->refarray_new(0);
    MDNodeLink *link;

    *vret = vp_Value(ra);
    for (link = md->heading; link != NULL; link = link->next) {
        StrBuf sb;
        fs->StrBuf_init_refstr(&sb, 0);
        MDNode_tostr(&sb, link->node);
        *fs->refarray_push(ra) = fs->StrBuf_str_Value(&sb, fs->cls_str);
    }

    return TRUE;
}

static int markdown_compile(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);

    if (!parse_markdown(md, Value_cstr(v[1]))) {
        return FALSE;
    }
    if (!link_markdown(r)) {
        return FALSE;
    }

    return TRUE;
}
static int markdown_add_link(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    RefStr *key = Value_vp(v[1]);
    RefStr *href = Value_vp(v[2]);

    MDNode *nd = MDNode_new(MD_TEXT, md);
    nd->cstr = fs->str_dup_p(href->c, href->size, &md->mem);
    SimpleHash_add_node(md->link_map, &md->mem, key->c, nd);

    return TRUE;
}
static int markdown_make_xml(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    RefStr *root_name = Value_vp(v[1]);
    Ref *root;

    if (!xst->is_valid_elem_name(root_name->c, root_name->size)) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid element name");
        return FALSE;
    }

    root = xmlelem_new(root_name->c);
    *vret = vp_Value(root);
    if (!xml_from_markdown(root, md, md->root)) {
        return FALSE;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int md_escape(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;
    RefStr *src = Value_vp(v[1]);
    int i;

    fs->StrBuf_init(&buf, src->size);
    for (i = 0; i < src->size; i++) {
        int ch = src->c[i];
        switch (ch) {
        case '[': case ']':
        case '(': case ')':
        case '`': case '~': case '_': case '*':
        case '&': case '|': case ':':
        case '\\':
            fs->StrBuf_add_c(&buf, '\\');
            fs->StrBuf_add_c(&buf, ch);
            break;
        default:
            fs->StrBuf_add_c(&buf, ch);
            break;
        }
    }
    *vret = fs->cstr_Value(fs->cls_str, buf.p, buf.size);
    StrBuf_close(&buf);

    return TRUE;
}

static int md_heading_id(Value *vret, Value *v, RefNode *node)
{
    StrBuf buf;
    RefStr *src = Value_vp(v[1]);

    fs->StrBuf_init(&buf, src->size);
    cstr_to_heading_id(&buf, src->c, src->size);
    *vret = fs->cstr_Value(fs->cls_str, buf.p, buf.size);
    StrBuf_close(&buf);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_function(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "md_escape", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, md_escape, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier(m, m, "md_heading_id", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, md_heading_id, 1, 1, NULL, fs->cls_str);
}
static void define_class(RefNode *m)
{
    RefNode *n;
    RefNode *cls;

    RefNode *cls_markdown = fs->define_identifier(m, m, "Markdown", NODE_CLASS, NODEOPT_ABSTRACT);


    cls = cls_markdown;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, markdown_new, 0, 0, cls);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_NEW_N, 0);
    fs->define_native_func_a(n, markdown_dispose, 0, 0, cls);
    n = fs->define_identifier(m, cls, "enable_semantic", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_enable_semantic, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "enable_semantic=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_enable_semantic, 1, 1, NULL, fs->cls_bool);
    n = fs->define_identifier(m, cls, "quiet_error", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_quiet_error, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "quiet_error=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_quiet_error, 1, 1, NULL, fs->cls_bool);
    n = fs->define_identifier(m, cls, "enable_tex", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_enable_tex, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "enable_tex=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_enable_tex, 1, 1, NULL, fs->cls_bool);
    n = fs->define_identifier(m, cls, "tabstop", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_tabstop, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "tabstop=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_tabstop, 1, 1, NULL, fs->cls_int);
    n = fs->define_identifier(m, cls, "heading_level", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_heading_level, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "heading_level=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_heading_level, 1, 1, NULL, fs->cls_int);
    n = fs->define_identifier(m, cls, "heading_list", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_heading_list, 0, 0, NULL);


    n = fs->define_identifier(m, cls, "compile", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_compile, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "add_link", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_add_link, 2, 2, NULL, fs->cls_str, fs->cls_str);
    n = fs->define_identifier(m, cls, "make_xml", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_make_xml, 1, 1, NULL, fs->cls_str);

    cls->u.c.n_memb = INDEX_MARKDOWN_NUM;
    fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_markdown = m;
    mod_xml = fs->get_module_by_name("marshal.xml", -1, TRUE, FALSE);
    xst = mod_xml->u.m.ext;

    fs->add_unresolved_ptr(m, mod_xml, "XMLElem", &cls_xmlelem);
    fs->add_unresolved_ptr(m, mod_xml, "XMLText", &cls_xmltext);
    fs->add_unresolved_ptr(m, mod_xml, "XMLNodeList", &cls_nodelist);

    str_link_callback = fs->intern("link_callback", -1);
    str_inline_plugin = fs->intern("inline_plugin", -1);
    str_block_plugin = fs->intern("block_plugin", -1);

    define_class(m);
    define_function(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\n";
}
