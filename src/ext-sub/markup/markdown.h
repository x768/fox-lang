#ifndef MARKDOWN_H_INCLUDED
#define MARKDOWN_H_INCLUDED

#include "markup.h"
#include "m_xml.h"

enum {
    MD_NONE,
    MD_EOS,
    MD_FATAL_ERROR,
    MD_IGNORE,        // child要素を出力

    // 1行
    MD_NEWLINE,       // 空行
    MD_HEADING,       // opt=1...6 <h1>child</h1>, <h2>child</h2>
    MD_HEADING_D,     // === or ---
    MD_PARAGRAPH,     // <p>child</p>
    MD_HORIZONTAL,    // <hr>
    MD_BLOCK_PLUGIN,  // %plugin

    // インライン要素
    MD_TEXT,          // text
    MD_EM,            // <em>child</em>
    MD_STRONG,        // <strong>child</strong>
    MD_STRIKE,        // <strike>child</strike>
    MD_CODE_INLINE,   // <code>#hilight:link text</code>
    MD_LINK,          // <a href="link" title="title">child</a>
    MD_CODE,          // title=filename, <pre><code></code></pre>
    MD_TEXT_COLOR,    // <span class="title">text</span>
    MD_INLINE_PLUGIN, // [%plugin]

    MD_LINK_BRAKET,   // [hogehoge]
    MD_LINK_BRAKET_NEXT,// [hoge]    # [...]の後ろに限る
    MD_LINK_PAREN,    // (hogehoge)  # [...]の後ろに限る
    MD_LINK_FOOTNOTE, // 

    // 単一要素
    MD_IMAGE,         // <img src="link" alt="title">
    MD_TEX_FORMULA,   // optional: $$ {e} ^ {i\pi} + 1 = 0 $$
    MD_COLON,         // :
    MD_PLUGIN,

    // 複数行
    MD_UNORDERD_LIST, // <ul>child</ul>
    MD_ORDERD_LIST,   // <ol>child</ol>
    MD_DEFINE_LIST,   // <dl>child</dl>
    MD_FOOTNOTE_LIST, // <ol id="footnote">...
    MD_LIST_ITEM,     // <li>child</li>
    MD_DEFINE_DT,     // <dt>child</dt>
    MD_DEFINE_DD,     // <dd>child</dd>
    MD_BLOCKQUOTE,    // title=, link=, <blockquote></blockquote>

    // テーブル
    MD_TABLE,         // "|","||",... <table></table>
    MD_TABLE_ROW,     // <tr></tr>
    MD_TABLE_HEADER,  // <th></th>
    MD_TABLE_CELL,    // <td></td>
};
enum {
    TABLESEP_NONE,
    TABLESEP_LINE,    // -----
    TABLESEP_LEFT,    // :----
    TABLESEP_CENTER,  // :---:
    TABLESEP_RIGHT,   // ----:
    TABLESEP_TH,      // *----
};
enum {
    INDEX_MARKDOWN_MD,
    INDEX_MARKDOWN_NUM,
};
enum {
    INDEX_MD_ELEM_TEXT,
    INDEX_MD_ELEM_LINK,
    INDEX_MD_ELEM_ID,
    INDEX_MD_ELEM_NUM,
};

enum {
    OPT_LINK_RESOLVED,
    OPT_LINK_PLUGIN_DONE,
    OPT_LINK_NAME_REF,
    OPT_TEXT_BACKSLASHES,
    OPT_TEXT_NO_BACKSLASHES,
};

enum {
    SIMPLE_HASH_MAX = 32,
};

typedef struct MDNode {
    struct MDNode *child;
    struct MDNode *next;
    struct MDNode *parent;

    uint16_t type;
    uint16_t opt;
    uint16_t indent;
    uint16_t footnote_id;

    char *cstr;
    char *href;
} MDNode;

typedef struct MDNodeLink
{
    struct MDNodeLink *next;
    MDNode *node;
} MDNodeLink;

typedef struct SimpleHash
{
    struct SimpleHash *next;
    uint32_t hash;
    const char *name;
    MDNode *node;
} SimpleHash;

typedef struct {
    int tabstop;
    int heading_level;
    int footnote_id;
    int link_done;

    Mem mem;
    SimpleHash *link_map[SIMPLE_HASH_MAX];
    Hash hilight;
    MDNode *root;
    MDNodeLink *heading;
    MDNodeLink **heading_p;
    MDNodeLink *footnote;
    MDNodeLink **footnote_p;
} Markdown;

typedef struct {
    Markdown *md;
    const char *p;
    int head;
    int prev_link;
    int table_row;

    uint16_t type;
    uint16_t opt;
    uint16_t bq_level;
    uint16_t indent;

    StrBuf val;
} MDTok;


#ifndef DEFINE_GLOBALS
#define extern
#endif

extern RefNode *mod_xml;
extern const XMLStatic *xst;

extern RefNode *cls_xmlelem;
extern RefNode *cls_xmltext;
extern RefNode *cls_nodelist;

extern RefStr *str_link_callback;
extern RefStr *str_image_callback;
extern RefStr *str_inline_plugin;
extern RefStr *str_block_plugin;

#ifndef DEFINE_GLOBALS
#undef extern
#endif

// m_markup.c
MDNode *SimpleHash_get_node(SimpleHash **hash, const char *name);
void SimpleHash_add_node(SimpleHash **hash, Mem *mem, const char *name, MDNode *node);
MDNode *MDNode_new(int type, Markdown *md);

// md_parse.c
int parse_markdown(Markdown *md, const char *p);
int link_markdown(Ref *r);

// md_make.c
Ref *xmlelem_new(const char *name);
int xml_from_markdown(Ref *root, Markdown *md, MDNode *node);
int text_from_markdown(StrBuf *sb, Markdown *md, MDNode *node);


#endif /* MARKDOWN_H_INCLUDED */
