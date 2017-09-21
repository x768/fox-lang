#define DEFINE_GLOBALS
#include "markdown.h"
#include "genml.h"
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
    h->next = NULL;
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

MDNode *MDNode_new(int type, Markdown *md)
{
    MDNode *n = fs->Mem_get(&md->mem, sizeof(MDNode));
    memset(n, 0, sizeof(MDNode));
    n->type = type;
    return n;
}

static int markdown_new(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_markdown = FUNC_VP(node);
    Ref *ref;
    Markdown *md;
    RefNode *type = fs->Value_type(*v);
    Mem mem;

    // 継承可能なクラス
    if (type == fs->cls_fn) {
        ref = fs->ref_new(cls_markdown);
        *vret = vp_Value(ref);
    } else {
        ref = Value_vp(*v);
        *vret = fs->Value_cp(*v);
    }

    fs->Mem_init(&mem, 1024);
    md = fs->Mem_get(&mem, sizeof(Markdown));
    memset(md, 0, sizeof(Markdown));
    ref->v[INDEX_MARKDOWN_MD] = ptr_Value(md);

    md->mem = mem;
    md->tabstop = 4;
    md->heading_level = 1;
    md->footnote_id = 1;
    md->link_done = FALSE;

    md->heading_p = &md->heading;
    md->footnote_p = &md->footnote;
    fs->Hash_init(&md->hilight, &md->mem, 16);

    if (fg->stk_top > v + 2) {
        RefMap *map = Value_vp(v[2]);
        HashValueEntry *tabstop = fs->refmap_get_strkey(map, "tabstop", -1);
        HashValueEntry *heading = fs->refmap_get_strkey(map, "heading_level", -1);
        if (tabstop != NULL) {
            RefNode *type2 = fs->Value_type(tabstop->val);
            if (type2 == fs->cls_int) {
                int32_t ts = fs->Value_int32(tabstop->val);
                if (ts >= 1 && ts <= 32) {
                    md->tabstop = ts;
                } else {
                    fs->throw_errorf(fs->mod_lang, "ValueError", "tabstop out of range (1...32)");
                    return FALSE;
                }
            } else {
                fs->throw_errorf(fs->mod_lang, "TypeError", "Int required but ({tabstop = %n})", type2);
                return FALSE;
            }
        }
        if (heading != NULL) {
            RefNode *type2 = fs->Value_type(heading->val);
            if (type2 == fs->cls_int) {
                int32_t hd = fs->Value_int32(heading->val);
                if (hd >= 1 && hd <= 6) {
                    md->heading_level = hd;
                } else {
                    fs->throw_errorf(fs->mod_lang, "ValueError", "heading_level out of range (1...6)");
                    return FALSE;
                }
            } else {
                fs->throw_errorf(fs->mod_lang, "TypeError", "Int required but ({heading_level = %n})", type2);
                return FALSE;
            }
        }
    }
    if (!parse_markdown(md, Value_cstr(v[1]))) {
        return FALSE;
    }

    return TRUE;
}
static int markdown_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *ref = Value_ref(*v);
    Markdown *r = Value_ptr(ref->v[INDEX_MARKDOWN_MD]);
    fs->Mem_close(&r->mem);
    ref->v[INDEX_MARKDOWN_MD] = VALUE_NULL;

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

static int markdown_make(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);

    if (!md->link_done) {
        if (!link_markdown(r)) {
            return FALSE;
        }
        md->link_done = TRUE;
    }

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

    if (!md->link_done) {
        if (!link_markdown(r)) {
            return FALSE;
        }
        md->link_done = TRUE;
    }

    root = xmlelem_new(root_name->c);
    *vret = vp_Value(root);
    if (!xml_from_markdown(root, md, md->root)) {
        return FALSE;
    }

    return TRUE;
}
static int markdown_make_text(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    Markdown *md = Value_ptr(r->v[INDEX_MARKDOWN_MD]);
    StrBuf sb;

    if (!md->link_done) {
        if (!link_markdown(r)) {
            return FALSE;
        }
        md->link_done = TRUE;
    }

    fs->StrBuf_init_refstr(&sb, 256);
    if (!text_from_markdown(&sb, md, md->root)) {
        StrBuf_close(&sb);
        return FALSE;
    }
    *vret = fs->StrBuf_str_Value(&sb, fs->cls_str);

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
        case '<': case '>':
        case '`': case '~': case '*':
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

static int genml_new(Value *vret, Value *v, RefNode *node)
{
    const char *src = Value_cstr(v[1]);
    RefMap *rm = NULL;
    GenML *gm;

    if (fg->stk_top > v + 2) {
        rm = Value_vp(v[2]);
    }

    gm = GenML_new(rm);
    if (gm == NULL) {
        return FALSE;
    }
    *vret = vp_Value(gm);

    if (!parse_genml(gm, src)) {
        return FALSE;
    }

    return TRUE;
}

static int genml_dispose(Value *vret, Value *v, RefNode *node)
{
    GenML *gm = Value_vp(*v);
    fs->Mem_close(&gm->mem);
    return TRUE;
}

static int genml_make(Value *vret, Value *v, RefNode *node)
{
    GenML *gm = Value_vp(*v);

    if (v + 2 < fg->stk_top) {
        if (!make_genml_sub(v[1], gm, Value_cstr(v[2]))) {
            return FALSE;
        }
    } else {
        if (!make_genml_root(vret, v[1], gm)) {
            return FALSE;
        }
    }
    return TRUE;
}

static int genml_escape(Value *vret, Value *v, RefNode *node)
{
    GenML *gm = Value_vp(*v);
    RefStr *src = Value_vp(v[1]);
    StrBuf buf;
    int i;

    fs->StrBuf_init(&buf, src->size);
    for (i = 0; i < src->size; i++) {
        int ch = src->c[i];
        switch (ch) {
        case '{': case '}':
        case '\\':
            fs->StrBuf_add_c(&buf, '\\');
            fs->StrBuf_add_c(&buf, ch);
            break;
        default:
            if ((ch & 0x80) == 0 && gm->punct[ch] != NULL) {
                fs->StrBuf_add_c(&buf, '\\');
            }
            fs->StrBuf_add_c(&buf, ch);
            break;
        }
    }
    *vret = fs->cstr_Value(fs->cls_str, buf.p, buf.size);
    StrBuf_close(&buf);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int gmlargs_count(GenMLNode *node)
{
    int n = 0;
    for (; node != NULL; node = node->next) {
        n++;
    }
    return n;
}
static GenMLNode *gmlargs_get_index(GenMLNode *node, int idx)
{
    if (idx < 0) {
        idx += gmlargs_count(node);
    }
    if (idx < 0) {
        return NULL;
    }

    while (idx > 0 && node != NULL) {
        node = node->next;
        idx--;
    }
    return node;
}
static int gmlargs_rows(Value *vret, Value *v, RefNode *node)
{
    GenMLArgs *arg = Value_ptr(*v);
    *vret = int32_Value(gmlargs_count(arg->node));
    return TRUE;
}
static int gmlargs_cols(Value *vret, Value *v, RefNode *node)
{
    GenMLArgs *arg = Value_ptr(*v);
    int idx = Value_integral(v[1]);
    GenMLNode *row = gmlargs_get_index(arg->node, idx);

    if (row == NULL) {
        fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], gmlargs_count(arg->node));
        return FALSE;
    }
    *vret = int32_Value(gmlargs_count(row->child));
    return TRUE;
}
static int gmlargs_dim(Value *vret, Value *v, RefNode *node)
{
    GenMLArgs *arg = Value_ptr(*v);
    GenMLNode *n = arg->node;

    if (n == NULL) {
        *vret = int32_Value(0);
    } else if (n->child != NULL && n->child->type == GMLTYPE_ARG) {
        *vret = int32_Value(2);
    } else {
        *vret = int32_Value(1);
    }
    return TRUE;
}
static int gmlargs_index(Value *vret, Value *v, RefNode *node)
{
    GenMLArgs *arg = Value_ptr(*v);
    int idx = Value_integral(v[1]);
    GenMLNode *n = gmlargs_get_index(arg->node, idx);

    if (n == NULL) {
        fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], gmlargs_count(arg->node));
        return FALSE;
    }

    if (v + 2 < fg->stk_top) {
        int idx2 = Value_integral(v[2]);
        GenMLNode *n2 = gmlargs_get_index(n->child, idx2);
        if (n2 == NULL) {
            fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[2], gmlargs_count(n));
            return FALSE;
        }
        n = n2;
    }
    if (!make_genml_elem(vret, arg->cvt, arg->gm, n)) {
        return FALSE;
    }

    return TRUE;
}
static int gmlargs_iterator(Value *vret, Value *v, RefNode *node)
{
    GenMLArgs *arg = Value_ptr(*v);
    Ref *iter = fs->ref_new(cls_gmlargsiter);
    GenMLNode *n = arg->node;

    if (v + 1 < fg->stk_top) {
        int idx = Value_integral(v[1]);
        GenMLNode *n2 = gmlargs_get_index(n, idx);
        if (n2 == NULL) {
            fs->throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], gmlargs_count(n));
            return FALSE;
        }
        n = n2;
    }

    iter->v[INDEX_GENMLARGSITER_ARG] = fs->Value_cp(*v);
    iter->v[INDEX_GENMLARGSITER_NODE] = ptr_Value(n);
    *vret = vp_Value(iter);
    return TRUE;
}
static int gmlargsiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *iter = Value_vp(*v);
    GenMLNode *n = Value_ptr(iter->v[INDEX_GENMLARGSITER_NODE]);
    GenMLArgs *arg = Value_vp(iter->v[INDEX_GENMLARGSITER_ARG]);

    // GenMLArgsが無効化されていないかチェック
    if (arg->node != NULL && n != NULL) {
        iter->v[INDEX_GENMLARGSITER_NODE] = ptr_Value(n->next);
        if (!make_genml_elem(vret, arg->cvt, arg->gm, n)) {
            return FALSE;
        }
    } else {
        fs->throw_stopiter();
        return FALSE;
    }
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
    cls_genml = fs->define_identifier(m, m, "GenML", NODE_CLASS, 0);
    cls_gmlargs = fs->define_identifier(m, m, "GenMLArgs", NODE_CLASS, 0);
    cls_gmlargsiter = fs->define_identifier(m, m, "GenMLArgsIter", NODE_CLASS, 0);


    cls = cls_markdown;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, markdown_new, 1, 2, cls, fs->cls_str, fs->cls_map);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_NEW_N, 0);
    fs->define_native_func_a(n, markdown_dispose, 0, 0, cls);


    n = fs->define_identifier(m, cls, "make", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_make, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "heading_list", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, markdown_heading_list, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "make_xml", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_make_xml, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "make_text", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, markdown_make_text, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_MARKDOWN_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_genml;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, genml_new, 1, 2, cls, fs->cls_str, fs->cls_map);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_NEW_N, 0);
    fs->define_native_func_a(n, genml_dispose, 0, 0, cls);

    n = fs->define_identifier(m, cls, "make", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, genml_make, 1, 2, NULL, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "escape", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, genml_escape, 1, 1, NULL, fs->cls_str);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_gmlargs;
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gmlargs_index, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gmlargs_iterator, 0, 1, NULL, fs->cls_int);
    n = fs->define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, gmlargs_rows, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "get_rows", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gmlargs_rows, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "get_cols", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gmlargs_cols, 1, 1, NULL, fs->cls_int);
    n = fs->define_identifier(m, cls, "dim", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, gmlargs_dim, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_iterable);


    cls = cls_gmlargsiter;
    n = fs->define_identifier_p(m, cls, fs->str_next, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, gmlargsiter_next, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_GENMLARGSITER_NUM;
    fs->extends_method(cls, fs->cls_iterator);


    cls = fs->define_identifier(m, m, "GenMLError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_markup = m;
    mod_xml = fs->get_module_by_name("marshal.xml", -1, TRUE, FALSE);
    xst = mod_xml->u.m.ext;

    fs->add_unresolved_ptr(m, mod_xml, "XMLElem", &cls_xmlelem);
    fs->add_unresolved_ptr(m, mod_xml, "XMLText", &cls_xmltext);
    fs->add_unresolved_ptr(m, mod_xml, "XMLNodeList", &cls_nodelist);

    str_link_callback = fs->intern("link_callback", -1);
    str_image_callback = fs->intern("image_callback", -1);
    str_inline_plugin = fs->intern("inline_plugin", -1);
    str_block_plugin = fs->intern("block_plugin", -1);

    define_class(m);
    define_function(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}
