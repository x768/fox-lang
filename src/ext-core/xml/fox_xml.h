#ifndef FOX_XML_H_
#define FOX_XML_H_

#include "fox_io.h"
#include "m_xml.h"
#include "m_unicode.h"

enum {
    TTYPE_NO_ENDTAG = 1,
    TTYPE_HEAD = 2,
    TTYPE_PCDATA = 4,
    TTYPE_RAW = 8,
    TTYPE_BLOCK = 16,       // ブロック要素
    TTYPE_C_INLINE = 32,    // 非ブロック要素だけを含む
    TTYPE_NO_SELF = 64,     // 自身と同じ要素を直下に含むことができない
    TTYPE_KEEP_SPACE = 128, // 空白・改行をそのまま出力
};
enum {
    TK_EOS,
    TK_ERROR,

    TK_TAG_START,     // <
    TK_TAG_END,       // >
    TK_TAG_END_CLOSE, // />
    TK_TAG_CLOSE,     // </abc>
    TK_CMD_START,     // <?xml
    TK_CMD_END,       // ?>
    TK_DOCTYPE,       // <!hoge

    TK_ATTR_NAME,     // key
    TK_STRING,        // "value"
    TK_EQUAL,         // =

    TK_TEXT,
};
enum {
    INDEX_DOCUMENT_HAS_DTD,
    INDEX_DOCUMENT_IS_PUBLIC,
    INDEX_DOCUMENT_FPI,
    INDEX_DOCUMENT_DTD_URI,
    INDEX_DOCUMENT_ROOT,
    INDEX_DOCUMENT_NUM,
};

typedef struct {
    int type;
    Str val;
    char *p;
    const char *end;
    int line;
    int xml;
    int loose;
} XMLTok;


#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;

extern RefNode *mod_xml;

extern RefNode *cls_document;
extern RefNode *cls_nodelist;
extern RefNode *cls_node;
extern RefNode *cls_elem;
extern RefNode *cls_text;

#ifdef DEFINE_GLOBALS
#undef extern
#endif

// m_xml.c
int ch_get_category(int ch);

// xml_parse.c
int get_tag_type_icase(Str s);
int is_valid_elem_name(const char *s_p, int s_size);
int resolve_entity(const char *p, const char *end);
void XMLTok_init(XMLTok *tk, char *src, const char *end, int xml, int loose);
void XMLTok_skip_next(XMLTok *tk);
int parse_html_body(Value *html, XMLTok *tk);
int parse_xml_begin(Ref *r, XMLTok *tk);
int parse_xml_body(Value *v, XMLTok *tk, int first);
int parse_doctype_declaration(Ref *r, XMLTok *tk);

// xml_select.c
int select_css(Value *vret, Value *v, int num, Str sel);
int delete_css(Value *vret, Value *v, int num, Str sel);
int delete_nodelist(Value *vret, Value *v, int num, RefArray *ra);


#endif /* FOX_XML_H_ */
