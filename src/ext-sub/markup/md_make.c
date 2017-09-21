#include "markdown.h"
#include <stdio.h>
#include <stdlib.h>



static int text_convert_line(StrBuf *sb, MDNode *node);


Ref *xmlelem_new(const char *name)
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
    fs->unref(key);
    fs->unref(ve->val);
    ve->val = fs->cstr_Value(fs->cls_str, cval, -1);
}

static Ref *xmlelem_image(const char *alt, char *p)
{
    const char *src = p;
    const char *href = NULL;
    Ref *r;

    while (*p != '\0') {
        if (*p == ',') {
            *p++ = '\0';
            break;
        }
        p++;
    }
    if (*p != '\0') {
        href = p;
    }

    r = xmlelem_new("img");
    xmlelem_add_attr(r, "src", src);
    xmlelem_add_attr(r, "alt", alt);

    if (href != NULL) {
        Ref *anchor = xmlelem_new("a");
        xmlelem_add_attr(anchor, "href", href);
        xmlelem_add(anchor, r);
        r = anchor;
    }
    return r;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int xml_set_footnote_title(Ref *sup, Markdown *md, MDNode *node)
{
    int idx = node->opt;
    MDNodeLink *foot;
    
    for (foot = md->footnote; foot != NULL; foot = foot->next) {
        idx--;
        if (idx <= 0) {
            break;
        }
    }
    if (foot != NULL) {
        StrBuf sb;
        fs->StrBuf_init(&sb, 64);
        text_convert_line(&sb, foot->node);
        fs->StrBuf_add_c(&sb, '\0');
        xmlelem_add_attr(sup, "title", sb.p);
        StrBuf_close(&sb);
        return TRUE;
    } else {
        return FALSE;
    }
}

static int xml_convert_line(Ref *r, Markdown *md, MDNode *node, int convert_br)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_TEXT:
            if (node->href != NULL) {
                Ref *span = xmlelem_new("span");
                xmlelem_add(r, span);
                xmlelem_add_attr(span, "class", node->href);
                xmlelem_add(span, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            } else if (convert_br) {
                const char *p = node->cstr;
                const char *top = p;
                while (*p != '\0') {
                    while (*p != '\0' && *p != '\n') {
                        p++;
                    }
                    if (top < p) {
                        xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, top, p - top)));
                    }
                    if (*p == '\n') {
                        xmlelem_add(r, xmlelem_new("br"));
                        p++;
                    }
                    top = p;
                }
                if (top < p) {
                    xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, top, p - top)));
                }
            } else {
                xmlelem_add(r, Value_vp(fs->cstr_Value(cls_xmltext, node->cstr, -1)));
            }
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
        case MD_LINK_FOOTNOTE:
            if (node->opt != 0xFFFF) {
                Ref *sup = xmlelem_new("sup");
                Ref *link = xmlelem_new("a");
                char ctext[16];
                char clink[16];
                sprintf(ctext, "[%d]", node->opt);
                sprintf(clink, "#note.%d", node->opt);
                xmlelem_add_attr(link, "href", clink);
                xml_set_footnote_title(sup, md, node);
                xmlelem_add(link, Value_vp(fs->cstr_Value(cls_xmltext, ctext, -1)));
                xmlelem_add(r, sup);
                xmlelem_add(sup, link);
            } else {
                Ref *sup = xmlelem_new("sup");
                xmlelem_add(sup, Value_vp(fs->cstr_Value(cls_xmltext, "###", -1)));
                xmlelem_add(r, sup);
            }
            break;
        case MD_IMAGE: {
            Ref *child = xmlelem_image(node->cstr, node->href);
            xmlelem_add(r, child);
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
            if (!xml_convert_line(child, md, node->child, convert_br)) {
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
        case MD_IGNORE:
            if (!xml_convert_line(r, md, node->child, convert_br)) {
                return FALSE;
            }
            break;
        default:
            break;
        }
    }
    return TRUE;
}
static int xml_convert_listitem(Ref *r, Markdown *md, MDNode *node, int gen_id)
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
            if (!xml_convert_line(li, md, node->child, TRUE)) {
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
                if (!xml_convert_listitem(list, md, node->child, FALSE)) {
                    return FALSE;
                }
            }
            break;
        }
    }
    return TRUE;
}
int xml_from_markdown(Ref *root, Markdown *md, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_HEADING: {
            char tag[4];
            int head = node->opt + md->heading_level - 1;
            Ref *heading;
            tag[0] = 'h';
            tag[1] = (head <= 6 ? '0' + head : '6');
            tag[2] = '\0';
            heading = xmlelem_new(tag);
            xmlelem_add(root, heading);
            if (!xml_convert_line(heading, md, node->child, TRUE)) {
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
            if (!xml_convert_listitem(list, md, node->child, FALSE)) {
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
            if (!xml_convert_listitem(list, md, node->child, TRUE)) {
                return FALSE;
            }
            break;
        }
        case MD_PARAGRAPH: {
            Ref *para = xmlelem_new("p");
            xmlelem_add(root, para);
            if (!xml_convert_line(para, md, node->child, TRUE)) {
                return FALSE;
            }
            break;
        }
        case MD_CODE: {
            Ref *pre = xmlelem_new("pre");
            Ref *code = xmlelem_new("code");
            xmlelem_add(root, pre);
            xmlelem_add(pre, code);
            if (!xml_convert_line(code, md, node->child, FALSE)) {
                return FALSE;
            }
            break;
        }
        case MD_BLOCKQUOTE: {
            Ref *blockquote = xmlelem_new("blockquote");
            xmlelem_add(root, blockquote);
            if (!xml_from_markdown(blockquote, md, node->child)) {
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
                const char *col_type = (node->cstr != NULL ? node->cstr : "");

                Ref *row = xmlelem_new("tr");
                xmlelem_add(table, row);
                for (ncell = nrow->child; ncell != NULL; ncell = ncell->next) {
                    Ref *cell = NULL;
                    switch (ncell->type) {
                    case MD_TABLE_HEADER:
                        cell = xmlelem_new("th");
                        break;
                    case MD_TABLE_CELL:
                        if (*col_type == TABLESEP_TH) {
                            cell = xmlelem_new("th");
                        } else {
                            cell = xmlelem_new("td");
                        }
                        switch (*col_type) {
                        case TABLESEP_LEFT:
                            xmlelem_add_attr(cell, "style", "text-align:left");
                            break;
                        case TABLESEP_CENTER:
                            xmlelem_add_attr(cell, "style", "text-align:center");
                            break;
                        case TABLESEP_RIGHT:
                            xmlelem_add_attr(cell, "style", "text-align:right");
                            break;
                        }
                        break;
                    }
                    if (cell != NULL) {
                        xmlelem_add(row, cell);
                        if (ncell->opt > 1) {
                            char str[16];
                            sprintf(str, "%d", ncell->opt);
                            xmlelem_add_attr(cell, "colspan", str);
                        }
                        if (!xml_convert_line(cell, md, ncell->child, TRUE)) {
                            return FALSE;
                        }
                        if (*col_type != TABLESEP_NONE) {
                            col_type++;
                        }
                    }
                }
            }
            break;
        }
        case MD_IGNORE:
            xml_from_markdown(root, md, node->child);
            break;
        default:
            break;
        }
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////

static int text_convert_line(StrBuf *sb, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_TEXT:
        case MD_LINK:
        case MD_CODE_INLINE:
            if (!fs->StrBuf_add(sb, node->cstr, -1)) {
                return FALSE;
            }
            break;
        case MD_EM:
        case MD_STRONG:
        case MD_STRIKE:
        case MD_IGNORE:
            if (!text_convert_line(sb, node->child)) {
                return FALSE;
            }
            break;
        default:
            break;
        }
    }
    return TRUE;
}
static int text_convert_listitem(StrBuf *sb, MDNode *node, int depth)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_LIST_ITEM: {
            int i;
            for (i = 0; i < depth; i++) {
                if (!fs->StrBuf_add_c(sb, ' ')) {
                    return FALSE;
                }
            }
            if (!fs->StrBuf_add_c(sb, '-')) {
                return FALSE;
            }
            if (!text_convert_line(sb, node->child)) {
                return FALSE;
            }
            if (!fs->StrBuf_add_c(sb, '\n')) {
                return FALSE;
            }
            break;
        }
        case MD_UNORDERD_LIST:
        case MD_ORDERD_LIST:
            if (!text_convert_listitem(sb, node->child, depth + 1)) {
                return FALSE;
            }
            break;
        }
    }
    return TRUE;
}
int text_from_markdown(StrBuf *sb, Markdown *r, MDNode *node)
{
    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case MD_HEADING:
        case MD_PARAGRAPH:
        case MD_CODE:
        case MD_BLOCKQUOTE:
            if (!text_convert_line(sb, node->child)) {
                return FALSE;
            }
            if (!fs->StrBuf_add_c(sb, '\n')) {
                return FALSE;
            }
            break;
        case MD_HORIZONTAL:
            if (!fs->StrBuf_add(sb, "-----\n", 6)) {
                return FALSE;
            }
            break;
        case MD_UNORDERD_LIST:
        case MD_ORDERD_LIST:
            if (!text_convert_listitem(sb, node->child, 0)) {
                return FALSE;
            }
            break;
        case MD_TABLE: {
            MDNode *nrow;
            for (nrow = node->child; nrow != NULL; nrow = nrow->next) {
                MDNode *ncell;
                for (ncell = nrow->child; ncell != NULL; ncell = ncell->next) {
                    if (!fs->StrBuf_add_c(sb, '|')) {
                        return FALSE;
                    }
                    if (!text_convert_line(sb, ncell->child)) {
                       return FALSE;
                    }
                }
                if (!fs->StrBuf_add(sb, "|\n", 2)) {
                    return FALSE;
                }
            }
            break;
        }
        case MD_IGNORE:
            text_from_markdown(sb, r, node->child);
            break;
        default:
            break;
        }
    }
    return TRUE;
}
