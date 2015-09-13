#include "fox_xml.h"
#include <string.h>
#include <stdlib.h>


typedef struct {
    const char *name;
    int code;
} Entity;

typedef struct {
    Value *html;
    RefArray *html_ra;
    Value *html_vm;

    Value *head;
    RefArray *head_ra;
    Value *head_vm;

    Value *title;
    RefArray *title_ra;
    Value *title_vm;

    Value *body;
    RefArray *body_ra;
    Value *body_vm;
    
    RefArray *ra_list;
    int html_node_done;
} HTMLDoc;

////////////////////////////////////////////////////////////////////////////////////

static int Str_eqi_head(Str s, const char *p)
{
    int i;

    for (i = 0; i < s.size; i++) {
        if (tolower_fox(s.p[i]) != tolower_fox(p[i])) {
            return FALSE;
        }
    }
    return TRUE;
}
int resolve_entity(const char *p, const char *end)
{
    static const Entity entities[] = {
            {"AElig", 198},
            {"Aacute", 193},
            {"Acirc", 194},
            {"Agrave", 192},
            {"Alpha", 913},
            {"Aring", 197},
            {"Atilde", 195},
            {"Auml", 196},
            {"Beta", 914},
            {"Ccedil", 199},
            {"Chi", 935},
            {"Dagger", 8225},
            {"Delta", 916},
            {"ETH", 208},
            {"Eacute", 201},
            {"Ecirc", 202},
            {"Egrave", 200},
            {"Epsilon", 917},
            {"Eta", 919},
            {"Euml", 203},
            {"Gamma", 915},
            {"Iacute", 205},
            {"Icirc", 206},
            {"Igrave", 204},
            {"Iota", 921},
            {"Iuml", 207},
            {"Kappa", 922},
            {"Lambda", 923},
            {"Mu", 924},
            {"Ntilde", 209},
            {"Nu", 925},
            {"OElig", 338},
            {"Oacute", 211},
            {"Ocirc", 212},
            {"Ograve", 210},
            {"Omega", 937},
            {"Omicron", 927},
            {"Oslash", 216},
            {"Otilde", 213},
            {"Ouml", 214},
            {"Phi", 934},
            {"Pi", 928},
            {"Prime", 8243},
            {"Psi", 936},
            {"Rho", 929},
            {"Scaron", 352},
            {"Sigma", 931},
            {"THORN", 222},
            {"Tau", 932},
            {"Theta", 920},
            {"Uacute", 218},
            {"Ucirc", 219},
            {"Ugrave", 217},
            {"Upsilon", 933},
            {"Uuml", 220},
            {"Xi", 926},
            {"Yacute", 221},
            {"Yuml", 376},
            {"Zeta", 918},
            {"aacute", 225},
            {"acirc", 226},
            {"acute", 180},
            {"aelig", 230},
            {"agrave", 224},
            {"alefsym", 8501},
            {"alpha", 945},
            {"amp", 38},
            {"and", 8743},
            {"ang", 8736},
            {"apos", 39},
            {"aring", 229},
            {"asymp", 8776},
            {"atilde", 227},
            {"auml", 228},
            {"bdquo", 8222},
            {"beta", 946},
            {"brvbar", 166},
            {"bull", 8226},
            {"cap", 8745},
            {"ccedil", 231},
            {"cedil", 184},
            {"cent", 162},
            {"chi", 967},
            {"circ", 710},
            {"clubs", 9827},
            {"cong", 8773},
            {"copy", 169},
            {"crarr", 8629},
            {"cup", 8746},
            {"curren", 164},
            {"dArr", 8659},
            {"dagger", 8224},
            {"darr", 8595},
            {"deg", 176},
            {"delta", 948},
            {"diams", 9830},
            {"divide", 247},
            {"eacute", 233},
            {"ecirc", 234},
            {"egrave", 232},
            {"empty", 8709},
            {"emsp", 8195},
            {"ensp", 8194},
            {"epsilon", 949},
            {"equiv", 8801},
            {"eta", 951},
            {"eth", 240},
            {"euml", 235},
            {"euro", 8364},
            {"ewierp", 8472},
            {"exist", 8707},
            {"fnof", 402},
            {"forall", 8704},
            {"frac12", 189},
            {"frac14", 188},
            {"frac34", 190},
            {"frasl", 8260},
            {"gamma", 947},
            {"ge", 8805},
            {"gt", 62},
            {"hArr", 8660},
            {"harr", 8596},
            {"hearts", 9829},
            {"hellip", 8230},
            {"iacute", 237},
            {"icirc", 238},
            {"iexcl", 161},
            {"igrave", 236},
            {"image", 8465},
            {"infin", 8734},
            {"int", 8747},
            {"iota", 953},
            {"iquest", 191},
            {"isin", 8712},
            {"iuml", 239},
            {"kappa", 954},
            {"lArr", 8656},
            {"lambda", 955},
            {"lang", 9001},
            {"laquo", 171},
            {"larr", 8592},
            {"lceil", 8968},
            {"ldquo", 8220},
            {"le", 8804},
            {"lfloor", 8970},
            {"lowast", 8727},
            {"loz", 9674},
            {"lrm", 8206},
            {"lsaquo", 8249},
            {"lsquo", 8216},
            {"lt", 60},
            {"macr", 175},
            {"mdash", 8212},
            {"micro", 181},
            {"middot", 183},
            {"minus", 8722},
            {"mu", 956},
            {"nabla", 8711},
            {"nbsp", 160},
            {"ndash", 8211},
            {"ne", 8800},
            {"ni", 8715},
            {"not", 172},
            {"notin", 8713},
            {"nsub", 8836},
            {"ntilde", 241},
            {"nu", 957},
            {"oacute", 243},
            {"ocirc", 244},
            {"oelig", 339},
            {"ograve", 242},
            {"oline", 8254},
            {"omega", 969},
            {"omicron", 959},
            {"oplus", 8853},
            {"or", 8744},
            {"ordf", 170},
            {"ordm", 186},
            {"oslash", 248},
            {"otilde", 245},
            {"otimes", 8855},
            {"ouml", 246},
            {"para", 182},
            {"part", 8706},
            {"permil", 8240},
            {"perp", 8869},
            {"phi", 966},
            {"pi", 960},
            {"piv", 982},
            {"plusmn", 177},
            {"pound", 163},
            {"prime", 8242},
            {"prod", 8719},
            {"prop", 8733},
            {"psi", 968},
            {"quot", 34},
            {"rArr", 8658},
            {"radic", 8730},
            {"rang", 9002},
            {"raquo", 187},
            {"rarr", 8594},
            {"rceil", 8969},
            {"rdquo", 8221},
            {"real", 8476},
            {"reg", 174},
            {"rfloor", 8971},
            {"rho", 961},
            {"rlm", 8207},
            {"rsaquo", 8250},
            {"rsquo", 8217},
            {"sbquo", 8218},
            {"scaron", 353},
            {"sdot", 8901},
            {"sect", 167},
            {"shy", 173},
            {"sigma", 963},
            {"sigmaf", 962},
            {"sim", 8764},
            {"spades", 9824},
            {"sub", 8834},
            {"sube", 8838},
            {"sum", 8721},
            {"sup", 8835},
            {"sup1", 185},
            {"sup2", 178},
            {"sup3", 179},
            {"supe", 8839},
            {"szlig", 223},
            {"tau", 964},
            {"there4", 8756},
            {"theta", 952},
            {"thetasym", 977},
            {"thinsp", 8201},
            {"thorn", 254},
            {"tilde", 732},
            {"times", 215},
            {"trade", 8482},
            {"uArr", 8657},
            {"uacute", 250},
            {"uarr", 8593},
            {"ucirc", 251},
            {"ugrave", 249},
            {"uml", 168},
            {"upsih", 978},
            {"upsilon", 965},
            {"uuml", 252},
            {"xi", 958},
            {"yacute", 253},
            {"yen", 165},
            {"yuml", 255},
            {"zeta", 950},
            {"zwj", 8205},
            {"zwnj", 8204},
    };

    int size = end - p;
    int lo = 0;
    int hi = sizeof(entities) / sizeof(entities[0]) - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const Entity *e = &entities[mid];
        int cmp = memcmp(p, e->name, size);
        if (cmp < 0) {
            hi = mid - 1;
        } else if (cmp > 0) {
            lo = mid + 1;
        } else if (e->name[size] != 0) {
            lo = mid + 1;
        } else {
            return e->code;
        }
    }

    return -1;
}

static Str get_elem_name(XMLTok *tk)
{
    int first = TRUE;
    Str ret;
    ret.p = tk->p;
    
    // 最初の1文字:Letter
    // 2文字目以降:Letter Digit - _ : .
    if (tk->p < tk->end) {
        for (;;) {
            int ch = fs->utf8_codepoint_at(tk->p);
            int type = ch_get_category(ch);

            if (first) {
                if (type < CATE_Ll || type > CATE_Mn) {
                    break;
                }
            } else {
                if (type < CATE_Ll || type > CATE_No) {
                    if (ch != '-' && ch != '_' && ch != ':' && ch != '.') {
                        break;
                    }
                }
            }

            fs->utf8_next((const char**)&tk->p, tk->end);
            first = FALSE;
        }
    }
    ret.size = tk->p - ret.p;

    return ret;
}
/**
 * 既定値 : インライン要素で、何でも含むことができる
 */
static int get_tag_type(Str s)
{
    if (s.size == 0) {
        return 0;
    }

    switch (s.p[0]) {
    case 'a':
        if (str_eq(s.p, s.size,  "a", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "address", -1)) {
            // 内容はinlineとpのみ
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "area", -1)) {
            return TTYPE_NO_ENDTAG;
        }
        break;
    case 'b':
        if (str_eq(s.p, s.size,  "br", -1)) {
            return TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "base", -1)) {
            return TTYPE_HEAD | TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "basefont", -1)) {
            return TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "bdo", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "b", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "big", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "body", -1)) {
            return TTYPE_BLOCK;
        }
        break;
    case 'c':
        if (str_eq(s.p, s.size,  "center", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "caption", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "colgroup", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "col", -1)) {
            return TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "code", -1)) {
            return TTYPE_BLOCK | TTYPE_C_INLINE;
        }
        break;
    case 'd':
        if (str_eq(s.p, s.size,  "div", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "dfn", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "dl", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "dt", -1)) {
            return TTYPE_BLOCK | TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "dd", -1)) {
            return TTYPE_BLOCK | TTYPE_NO_SELF;
        }
        break;
    case 'e':
        if (str_eq(s.p, s.size,  "em", -1)) {
            return TTYPE_C_INLINE;
        }
        break;
    case 'f':
        if (str_eq(s.p, s.size,  "form", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "fieldset", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "font", -1)) {
            return TTYPE_C_INLINE;
        }
        break;
    case 'h':
        if (s.size == 2 && s.p[1] >= '1' && s.p[1] <= '6') {
            return TTYPE_BLOCK | TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "hr", -1)) {
            return TTYPE_BLOCK | TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "head", -1)) {
            return TTYPE_BLOCK;
        }
        break;
    case 'i':
        if (str_eq(s.p, s.size,  "img", -1)) {
            return TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "input", -1)) {
            return TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "i", -1)) {
            return TTYPE_C_INLINE;
        }
        break;
    case 'l':
        if (str_eq(s.p, s.size,  "link", -1)) {
            return TTYPE_HEAD | TTYPE_NO_ENDTAG;
        } else if (str_eq(s.p, s.size,  "li", -1)) {
            return TTYPE_BLOCK | TTYPE_NO_SELF;
        } else if (str_eq(s.p, s.size,  "legent", -1)) {
            return TTYPE_BLOCK;
        }
        break;
    case 'm':
        if (str_eq(s.p, s.size,  "meta", -1)) {
            return TTYPE_HEAD | TTYPE_NO_ENDTAG;
        }
        break;
    case 'n':
        if (str_eq(s.p, s.size,  "noscript", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "noframes", -1)) {
            return TTYPE_BLOCK;
        }
        break;
    case 'o':
        if (str_eq(s.p, s.size,  "ol", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "optgroup", -1)) {
            return TTYPE_BLOCK;
        } else if (str_eq(s.p, s.size,  "option", -1)) {
            return TTYPE_BLOCK;
        }
        break;
    case 'p':
        if (str_eq(s.p, s.size,  "p", -1)) {
            return TTYPE_BLOCK | TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "pre", -1)) {
            return TTYPE_BLOCK | TTYPE_C_INLINE | TTYPE_KEEP_SPACE;
        } else if (str_eq(s.p, s.size,  "param", -1)) {
            return TTYPE_NO_ENDTAG;
        }
        break;
    case 'q':
        if (str_eq(s.p, s.size,  "q", -1)) {
            return TTYPE_C_INLINE;
        }
        break;
    case 's':
        if (str_eq(s.p, s.size,  "script", -1)) {
            return TTYPE_BLOCK | TTYPE_RAW;
        } else if (str_eq(s.p, s.size,  "style", -1)) {
            return TTYPE_HEAD | TTYPE_RAW;
        } else if (str_eq(s.p, s.size,  "span", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "strong", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "sub", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "sup", -1)) {
            return TTYPE_C_INLINE;
        } else if (str_eq(s.p, s.size,  "small", -1)) {
            return TTYPE_C_INLINE;
        }
        break;
    case 't':
        if (s.size == 2) {
            if (s.p[1] == 'r' || s.p[1] == 'd' || s.p[1] == 'h') {
                return TTYPE_BLOCK | TTYPE_NO_SELF;
            } else if (s.p[1] == 't') {
                return TTYPE_C_INLINE;
            }
        } else if (s.size == 5) {
            if (str_eq(s.p, s.size,  "title", -1)) {
                return TTYPE_HEAD | TTYPE_PCDATA | TTYPE_BLOCK;
            } else if (str_eq(s.p, s.size,  "table", -1)) {
                return TTYPE_BLOCK;
            } else if (str_eq(s.p, s.size,  "thead", -1)) {
                return TTYPE_BLOCK;
            } else if (str_eq(s.p, s.size,  "tfoot", -1)) {
                return TTYPE_BLOCK;
            } else if (str_eq(s.p, s.size,  "tbody", -1)) {
                return TTYPE_BLOCK;
            }
        } else if (str_eq(s.p, s.size,  "textarea", -1)) {
            return TTYPE_RAW;
        }
        break;
    case 'u':
        if (str_eq(s.p, s.size,  "ul", -1)) {
            return TTYPE_BLOCK;
        }
        break;
    }
    return 0;
}
int get_tag_type_icase(const char *s_p, int s_size)
{
    char cbuf[16];
    const char *p = s_p + s_size;
    int i;
    int size;

    while (p > s_p) {
        if (p[-1] == ':') {
            break;
        }
        p--;
    }
    size = s_size - (p - s_p);

    for (i = 0; i < size && i < 15; i++) {
        cbuf[i] = tolower_fox(p[i]);
    }
    cbuf[i] = '\0';
    return get_tag_type(Str_new(cbuf, -1));
}

int is_valid_elem_name(const char *s_p, int s_size)
{
    const char *p = s_p;
    const char *end = p + s_size;
    int first = TRUE;

    if (s_size == 0) {
        return FALSE;
    }

    while (p < end) {
        int ch = fs->utf8_codepoint_at(p);
        int type = ch_get_category(ch);

        if (first) {
            if (type < CATE_Ll || type > CATE_Mn) {
                return FALSE;
            }
        } else {
            if (type < CATE_Ll || type > CATE_No) {
                if (ch != '-' && ch != '_' && ch != ':' && ch != '.') {
                    return FALSE;
                }
            }
        }

        fs->utf8_next(&p, end);
        first = FALSE;
    }
    return TRUE;
}
static void throw_unexpected_token(XMLTok *tk)
{
    fs->throw_errorf(mod_xml, "XMLParseError", "Unexpected token at line %d", tk->line);
}

////////////////////////////////////////////////////////////////////////////////////

void XMLTok_init(XMLTok *tk, char *src, const char *end, int xml, int loose)
{
    tk->p = src;
    tk->end = end;
    tk->type = TK_EOS;
    tk->xml = xml;
    tk->loose = loose;
    tk->line = 1;
}

static int resolve_xml_entity(const char *p, const char *end)
{
    int len = end - p;

    switch (len) {
    case 2:
        if (memcmp(p, "lt", 2) == 0) {
            return '<';
        } else if (memcmp(p, "gt", 2) == 0) {
            return '>';
        }
        break;
    case 3:
        if (memcmp(p, "amp", 3) == 0) {
            return '&';
        }
        break;
    case 4:
        if (memcmp(p, "quot", 4) == 0) {
            return '&';
        } else if (memcmp(p, "apos", 4) == 0) {
            return '\'';
        }
        break;
    }
    return -1;
}
static int XMLTok_parse_entity(char **ppdst, XMLTok *tk)
{
    char *dst = *ppdst;
    char *tmp = tk->p;
    int code = -1;

    tk->p++;
    if (*tk->p == '#') {
        tk->p++;
        if (*tk->p == 'x' || *tk->p == 'X') {
            tk->p++;
            code = fs->parse_hex((const char**)&tk->p, NULL, 8);
            if (code >= 0) {
                if (*tk->p == ';') {
                    tk->p++;
                } else {
                    code = -1;
                }
            }
        } else if (isdigit_fox(*tk->p)){
            code = 0;
            while (isdigit_fox(*tk->p)) {
                if (code >= 0) {
                    code = code * 10 + (*tk->p - '0');
                    if (code >= 0x110000) {
                        code = -1;
                    }
                }
                tk->p++;
            }
            if (code >= 0) {
                if (*tk->p == ';') {
                    tk->p++;
                } else {
                    code = -1;
                }
            }
        }
    } else {
        const char *top = tk->p;
        while (tk->p < tk->end) {
            int ch = *tk->p;
            if (!isdigit_fox(ch) && !isupper_fox(ch) && !islower_fox(ch)) {
                break;
            }
            tk->p++;
        }
        if (*tk->p == ';') {
            if (tk->loose) {
                code = resolve_entity(top, tk->p);
            } else {
                code = resolve_xml_entity(top, tk->p);
            }
            tk->p++;
        } else {
            code = -1;
        }
    }
    if (code >= 0) {
        // #???;をUTF-8にする
        if (code < 0x80) {
            *dst++ = code;
        } else if (code < 0x800) {
            *dst++ = 0xC0 | (code >> 6);
            *dst++ = 0x80 | (code & 0x3F);
        } else if (code < 0xD800) {
            *dst++ = 0xE0 | (code >> 12);
            *dst++ = 0x80 | ((code >> 6) & 0x3F);
            *dst++ = 0x80 | (code & 0x3F);
        } else if (code < 0xDC00) {
            // サロゲート(不正なコード)
            if (!tk->loose) {
                return FALSE;
            }
            memcpy(dst, UTF8_ALTER_CHAR, UTF8_ALTER_CHAR_LEN);
            dst += UTF8_ALTER_CHAR_LEN;
        } else if (code < 0x10000) {
            *dst++ = 0xE0 | (code >> 12);
            *dst++ = 0x80 | ((code >> 6) & 0x3F);
            *dst++ = 0x80 | (code & 0x3F);
        } else if (code < 0x110000) {
            *dst++ = 0xF0 | (code >> 18);
            *dst++ = 0x80 | ((code >> 12) & 0x3F);
            *dst++ = 0x80 | ((code >> 6) & 0x3F);
            *dst++ = 0x80 | (code & 0x3F);
        } else {
            if (!tk->loose) {
                return FALSE;
            }
            // 不正なコード
            memcpy(dst, UTF8_ALTER_CHAR, UTF8_ALTER_CHAR_LEN);
            dst += UTF8_ALTER_CHAR_LEN;
        }
    } else {
        if (!tk->loose) {
            return FALSE;
        }
        tk->p = tmp + 1;
        *dst++ = '&';
    }

    *ppdst = dst;
    return TRUE;
}
static void XMLTok_parse_text(XMLTok *tk)
{
    char *dst = tk->p;
    tk->val.p = dst;

    while (*tk->p != '\0') {
        int ch = *tk->p;

        if (ch == '<') {
            if (tk->p[1] == '!' && tk->p[2] == '-' && tk->p[3] == '-') {
                // コメントが出現したら中断
                break;
            } else if (tk->xml) {
                // <![CDATA[が出現したら ]]>まで飛ばす
                if (memcmp(tk->p + 1, "![CDATA[", 8) == 0) {
                    tk->p += 9;
                    while (*tk->p != '\0') {
                        if (tk->p[0] == ']' && tk->p[1] == ']' && tk->p[2] == '>') {
                            tk->p += 3;
                            break;
                        }
                        if (*tk->p == '\n') {
                            tk->line++;
                        }
                        *dst++ = *tk->p;
                        tk->p++;
                    }
                    if (*tk->p == '\0' && !tk->loose) {
                        // 閉じていない
                        tk->type = TK_ERROR;
                        return;
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        } else if (ch == '&') {
            if (!XMLTok_parse_entity(&dst, tk)) {
                tk->type = TK_ERROR;
                return;
            }
        } else {
            if (ch == '\n') {
                tk->line++;
            }
            *dst++ = ch;
            tk->p++;
        }
    }

    tk->val.size = dst - tk->val.p;
    tk->type = TK_TEXT;
}
static void XMLTok_parse_string(XMLTok *tk, int term)
{
    char *dst = tk->p;
    tk->val.p = dst;

    while (*tk->p != '\0') {
        int ch = *tk->p;

        if (ch == term) {
            tk->p++;
            tk->val.size = dst - tk->val.p;
            tk->type = TK_STRING;
            return;
        } else if (ch == '&') {
            XMLTok_parse_entity(&dst, tk);
        } else {
            if (ch == '\n') {
                tk->line++;
            }
            *dst++ = ch;
            tk->p++;
        }
    }
}
static void XMLTok_tolower(XMLTok *tk)
{
    char *p = (char*)tk->val.p;
    const char *end = p + tk->val.size;

    while (p < end) {
        *p = tolower_fox(*p);
        p++;
    }
}
/**
 * タグの外
 */
static void XMLTok_next(XMLTok *tk)
{
    switch (*tk->p) {
    case '\0':
        tk->type = TK_EOS;
        return;
    case '<':
        switch (tk->p[1]) {
        case '!':
            if (tk->p[2] == '-' && tk->p[3] == '-') {
                // コメント
                tk->p += 4;
                tk->val.p = tk->p;
                while (tk->p[0] != '\0') {
                    if (tk->p[0] == '-' && tk->p[1] == '-' && tk->p[2] == '>') {
                        tk->val.size = tk->p - tk->val.p;
                        tk->p += 3;
                        tk->type = TK_COMMENT;
                        return;
                    }
                    tk->p++;
                }
                if (*tk->p == '\0' && !tk->loose) {
                    // コメントが閉じていない
                    tk->type = TK_ERROR;
                    return;
                }
                tk->type = TK_COMMENT;
                return;
            } else if (memcmp(tk->p + 2, "[CDATA[", 7) == 0) {
                XMLTok_parse_text(tk);
            } else {
                tk->p += 2;
                tk->val = get_elem_name(tk);
                tk->type = TK_DOCTYPE;
            }
            break;
        case '?': // <?tag
            tk->p += 2;
            tk->val = get_elem_name(tk);
            if (tk->val.size == 0) {
                tk->type = TK_ERROR;
                return;
            }
            tk->type = TK_DECL_START;
            return;
        case '/':  // </tag>
            tk->p += 2;
            tk->val = get_elem_name(tk);
            if (tk->val.size == 0) {
                tk->type = TK_ERROR;
                return;
            }
            if (!tk->xml) {
                // HTMLはタグ名を小文字に変換
                XMLTok_tolower(tk);
            }
            while (isspace_fox(*tk->p)) {
                tk->p++;
            }
            if (*tk->p != '>') {
                while (*tk->p != '\0' && *tk->p != '>') {
                    tk->p++;
                }
                tk->type = TK_ERROR;
                return;
            }
            if (*tk->p != '\0') {
                tk->p++;
            }
            tk->type = TK_TAG_CLOSE;
            break;
        default:
            tk->p++;
            tk->val = get_elem_name(tk);
            if (tk->val.size == 0) {
                tk->type = TK_ERROR;
                return;
            }
            if (!tk->xml) {
                // HTMLはタグ名を小文字に変換
                XMLTok_tolower(tk);
            }
            tk->type = TK_TAG_START;
            return;
        }
        break;
    default:
        XMLTok_parse_text(tk);
        break;
    }
}

/**
 * タグ終了まで
 */
static void XMLTok_next_tag(XMLTok *tk)
{
    while (tk->p < tk->end && isspace_fox(*tk->p)) {
        if (*tk->p == '\n') {
            tk->line++;
        }
        tk->p++;
    }
    switch (*tk->p) {
    case '\0':
        tk->type = TK_EOS;
        return;
    case '>':
        tk->p++;
        tk->type = TK_TAG_END;
        break;
    case '/':
        if (tk->p[1] == '>') {
            tk->p += 2;
            tk->type = TK_TAG_END_CLOSE;
        }
        break;
    case '"':
        tk->p++;
        XMLTok_parse_string(tk, '"');
        break;
    case '\'':
        tk->p++;
        XMLTok_parse_string(tk, '\'');
        break;
    case '?':
        if (tk->p[1] == '>') {
            tk->p += 2;
            tk->type = TK_DECL_END;
        } else {
            tk->p++;
            tk->type = TK_ERROR;
        }
        break;
    case '=':
        tk->p++;
        tk->type = TK_EQUAL;
        break;
    default:
        tk->val = get_elem_name(tk);
        if (tk->val.size > 0) {
            tk->type = TK_ATTR_NAME;
        } else {
            tk->type = TK_ERROR;
        }
        break;
    }
}

/**
 * <?xml ... ?>
 */
static void XMLTok_next_decl(XMLTok *tk)
{
    const char *top;

    while (tk->p < tk->end && isspace_fox(*tk->p)) {
        if (*tk->p == '\n') {
            tk->line++;
        }
        tk->p++;
    }

    top = tk->p;
    while (tk->p < tk->end - 1) {
        if (tk->p[0] == '?' && tk->p[1] == '>') {
            tk->val.p = top;
            tk->val.size = tk->p - top;
            tk->p += 2;
            tk->type = TK_DECL_BODY;
            return;
        }
        tk->p++;
    }
    tk->type = TK_ERROR;
}

/**
 * <script></script>
 */
static void XMLTok_next_raw(XMLTok *tk, Str tag_name)
{
    tk->val.p = tk->p;

    for (;;) {
        if (tk->p[0] == '<' && tk->p[1] == '/') {
            if (Str_eqi_head(tag_name, tk->p + 2)) {
                tk->val.size = tk->p - tk->val.p;
                tk->p += 2 + tag_name.size;
                break;
            } else {
                tk->p += 2;
            }
        } else if (*tk->p == '\0') {
            tk->val.size = tk->p - tk->val.p;
            break;
        } else {
            tk->p++;
        }
    }
    while (isspace_fox(*tk->p)) {
        tk->p++;
    }
    if (*tk->p == '>') {
        tk->p++;
    }
    tk->type = TK_TEXT;
}

void XMLTok_skip_next(XMLTok *tk)
{
    while (isspace_fox(*tk->p)) {
        tk->p++;
    }
    XMLTok_next(tk);
}
static int XMLTok_isspace(XMLTok *tk)
{
    const char *p = tk->val.p;
    const char *end = p + tk->val.size;

    while (p < end) {
        if (!isspace_fox(*p)) {
            return FALSE;
        }
        p++;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////

static void xml_elem_init(Value *v, RefArray **p_ra, Value **p_vm, const char *name_p, int name_size)
{
    Ref *r = fs->ref_new(cls_elem);
    RefArray *ra = fs->refarray_new(0);

    r->v[INDEX_ELEM_CHILDREN] = vp_Value(ra);
    *v = vp_Value(r);

    ra->rh.type = cls_nodelist;
    if (p_ra != NULL) {
        *p_ra = Value_vp(r->v[INDEX_ELEM_CHILDREN]);
    }
    if (p_vm != NULL) {
        *p_vm = &r->v[INDEX_ELEM_ATTR];
    }
    r->v[INDEX_ELEM_NAME] = fs->cstr_Value(fs->cls_str, name_p, name_size);
}
static int xml_elem_add_attr(Value *v, Str skey, Str sval, int loose)
{
    RefMap *rm;

    if (Value_isref(*v)) {
        rm = Value_vp(*v);
    } else {
        rm = fs->refmap_new(0);
        *v = vp_Value(rm);
    }
    {
        Value key = fs->cstr_Value(fs->cls_str, skey.p, skey.size);
        HashValueEntry *ve = fs->refmap_add(rm, key, loose, TRUE);
        if (ve == NULL) {
            fs->Value_dec(key);
            return FALSE;
        }
        ve->val = fs->cstr_Value(fs->cls_str, sval.p, sval.size);
        fs->Value_dec(key);
    }
    return TRUE;
}

static void parse_pcdata(RefArray *ra, Str tag_name, XMLTok *tk)
{
    if (ra->size > 0) {
        return;
    }

    for (;;) {
        switch (tk->type) {
        case TK_EOS:
            return;
        case TK_TAG_START:
            // ">" まで無視する
            XMLTok_next_tag(tk);
            for (;;) {
                switch (tk->type) {
                case TK_EOS:
                    return;
                case TK_TAG_END:
                    XMLTok_next(tk);
                    goto BREAK_TAG;
                default:
                    XMLTok_next_tag(tk);
                    break;
                }
            }
            break;
        case TK_TAG_CLOSE:
            XMLTok_tolower(tk);
            if (str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
                XMLTok_next(tk);
                return;
            }
            XMLTok_next(tk);
            break;
        case TK_TEXT:
            if (tk->val.size > 0) {
                Value *vp = fs->refarray_push(ra);
                *vp = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
            }
            XMLTok_next(tk);
            break;
        case TK_COMMENT:
            // 無視
            XMLTok_next(tk);
            break;
        default:
            XMLTok_next(tk);
            return;
        }
    BREAK_TAG:
        ;
    }
}
static void parse_raw_text(RefArray *ra, XMLTok *tk)
{
    if (tk->type == TK_TEXT) {
        Value *vp = fs->refarray_push(ra);
        *vp = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
        XMLTok_next(tk);
    }
}
int parse_doctype_declaration(Ref *r, XMLTok *tk)
{
    int phase = 0;
    r->v[INDEX_DOCUMENT_HAS_DTD] = VALUE_TRUE;
    r->v[INDEX_DOCUMENT_IS_PUBLIC] = VALUE_TRUE;

    for (;;) {
        XMLTok_next_tag(tk);

        switch (tk->type) {
        case TK_ATTR_NAME:
        case TK_STRING:
            switch (phase) {
            case 0:
                if (!tk->loose) {
                    if (!is_valid_elem_name(tk->val.p, tk->val.size)) {
                        fs->throw_errorf(mod_xml, "XMLParseError", "Invalid root-name");
                        return FALSE;
                    }
                }
                r->v[INDEX_DOCUMENT_HAS_DTD] = VALUE_TRUE;
                break;
            case 1:
                if (str_eqi(tk->val.p, tk->val.size, "PUBLIC", -1)) {
                    r->v[INDEX_DOCUMENT_IS_PUBLIC] = VALUE_TRUE;
                } else if (str_eqi(tk->val.p, tk->val.size, "SYSTEM", -1)) {
                    r->v[INDEX_DOCUMENT_IS_PUBLIC] = VALUE_FALSE;
                } else if (!tk->loose) {
                    fs->throw_errorf(mod_xml, "XMLParseError", "PUBLIC or SYSTEM required");
                    return FALSE;
                }
                break;
            case 2:
            case 3:
                if (tk->val.size >= 3 && memcmp(tk->val.p, "-//", 3) == 0) {
                    if (r->v[INDEX_DOCUMENT_FPI] == VALUE_NULL) {
                        r->v[INDEX_DOCUMENT_FPI] = fs->cstr_Value(fs->cls_str, tk->val.p, tk->val.size);
                    } else if (!tk->loose) {
                        fs->throw_errorf(mod_xml, "XMLParseError", "Illigal DOCTYPE");
                        return FALSE;
                    }
                } else {
                    if (r->v[INDEX_DOCUMENT_DTD_URI] == VALUE_NULL) {
                        r->v[INDEX_DOCUMENT_DTD_URI] = fs->cstr_Value(fs->cls_str, tk->val.p, tk->val.size);
                    } else if (!tk->loose) {
                        fs->throw_errorf(mod_xml, "XMLParseError", "Illigal DOCTYPE");
                        return FALSE;
                    }
                }
                break;
            }
            phase++;
            break;
        case TK_TAG_END:
        case TK_EOS:
            return TRUE;
        default:
            if (!tk->loose) {
                fs->throw_errorf(mod_xml, "XMLParseError", "Illigal DOCTYPE");
                return FALSE;
            }
            break;
        }
    }
}
/**
 * v, v_ra : 親要素
 * <p>aaa<p>bbbのように、閉じタグが省略されている場合、FALSEを返す
 */
static int parse_html_sub(Value *v, RefArray *v_ra, HTMLDoc *html, XMLTok *tk, Str parent_tag)
{
    switch (tk->type) {
    case TK_EOS:
        return TRUE;
    case TK_ERROR:
        // 無視
        XMLTok_next(tk);
        break;
    case TK_DECL_START: { // <?
        // ?>までコメント扱い
        StrBuf sb;
        fs->StrBuf_init_refstr(&sb, 0);
        fs->StrBuf_add_c(&sb, '?');
        fs->StrBuf_add(&sb, tk->val.p, tk->val.size);

        XMLTok_next_decl(tk);
        if (tk->type == TK_DECL_BODY) {
            Value *vp = fs->refarray_push(v_ra);
            fs->StrBuf_add_c(&sb, ' ');
            fs->StrBuf_add(&sb, tk->val.p, tk->val.size);
            fs->StrBuf_add_c(&sb, '?');
            *vp = fs->StrBuf_str_Value(&sb, cls_comment);
            XMLTok_next(tk);
        } else {
            StrBuf_close(&sb);
            return TRUE;
        }
        break;
    }
    case TK_DOCTYPE:  // <!
        // >まで無視する
        XMLTok_next_tag(tk);

        for (;;) {
            switch (tk->type) {
            case TK_EOS:
                return TRUE;
            case TK_TAG_END:
                XMLTok_next(tk);
                goto BREAK_ALL;
            default:
                XMLTok_next_tag(tk);
                break;
            }
        }
        break;
    case TK_TAG_START: {
        RefArray *ra;
        Value *vm;
        Str tag_name = tk->val;
        int tag_type = get_tag_type(tag_name);

        if ((tag_type & TTYPE_BLOCK) != 0) {
            // ブロック要素を含むことができない要素の中にブロック要素が出現した
            int parent_type = get_tag_type(parent_tag);
            if ((parent_type & TTYPE_C_INLINE) != 0) {
                return FALSE;
            }
        }
        if ((tag_type & TTYPE_NO_SELF) != 0) {
            // 自身と同じ要素を含むことができない要素の中に自身と同じ要素が出現した
            if (str_eq(tag_name.p, tag_name.size, parent_tag.p, parent_tag.size)) {
                return FALSE;
            }
        }
        if (!html->html_node_done) {
            *fs->refarray_push(html->ra_list) = fs->Value_cp(*html->html);
            html->html_node_done = TRUE;
        }

        if (str_eq(tag_name.p, tag_name.size, "html", -1)) {
            vm = html->html_vm;
            ra = html->html_ra;
        } else if (str_eq(tag_name.p, tag_name.size, "head", -1)) {
            vm = html->head_vm;
            ra = html->head_ra;
        } else if (str_eq(tag_name.p, tag_name.size, "body", -1)) {
            vm = html->body_vm;
            ra = html->body_ra;
        } else if (str_eq(tag_name.p, tag_name.size, "title", -1)) {
            vm = html->title_vm;
            ra = html->title_ra;
        } else if ((tag_type & TTYPE_HEAD) != 0) {
            // titleを一番下に持ってくる
            Value *vp;
            ra = html->head_ra;
            fs->refarray_push(ra);
            vp = &ra->p[ra->size - 2];
            ra->p[ra->size - 1] = *vp;
            xml_elem_init(vp, &ra, &vm, tag_name.p, tag_name.size);
        } else {
            Value *vp = fs->refarray_push(v_ra);
            xml_elem_init(vp, &ra, &vm, tag_name.p, tag_name.size);
        }
        XMLTok_next_tag(tk);

        for (;;) {
            switch (tk->type) {
            case TK_EOS:
                return TRUE;
            case TK_TAG_END:
                if ((tag_type & TTYPE_RAW) != 0) {
                    XMLTok_next_raw(tk, tag_name);
                } else {
                    XMLTok_next(tk);
                }
                goto BREAK_TAG;
            case TK_TAG_END_CLOSE:
                XMLTok_next(tk);
                // 閉じタグを省略
                tag_type |= TTYPE_NO_ENDTAG;
                goto BREAK_TAG;
            case TK_ATTR_NAME: {
                Str skey;
                XMLTok_tolower(tk);
                skey = tk->val;
                XMLTok_next_tag(tk);
                if (tk->type == TK_EQUAL) {
                    XMLTok_next_tag(tk);
                    if (tk->type == TK_STRING || tk->type == TK_ATTR_NAME) {
                        HashValueEntry *ve;
                        Value key = fs->cstr_Value(fs->cls_str, skey.p, skey.size);
                        if (*vm == VALUE_NULL) {
                            *vm = vp_Value(fs->refmap_new(0));
                        }
                        ve = fs->refmap_add(Value_vp(*vm), key, FALSE, FALSE);
                        if (ve != NULL) {
                            ve->val = fs->cstr_Value(fs->cls_str, tk->val.p, tk->val.size);
                        }
                        fs->Value_dec(key);
                        XMLTok_next_tag(tk);
                    }
                } else {
                    Value key = fs->cstr_Value(fs->cls_str, skey.p, skey.size);
                    HashValueEntry *ve = fs->refmap_add(Value_vp(*vm), key, FALSE, FALSE);
                    if (ve != NULL) {
                        ve->val = key;
                    } else {
                        fs->Value_dec(key);
                    }
                }
                break;
            }
            default:
                XMLTok_next_tag(tk);
                break;
            }
        }
    BREAK_TAG:
        if ((tag_type & TTYPE_NO_ENDTAG) == 0) {
            if ((tag_type & TTYPE_PCDATA) != 0) {
                parse_pcdata(ra, tag_name, tk);
            } else if ((tag_type & TTYPE_RAW) != 0) {
                parse_raw_text(ra, tk);
            } else {
                while (tk->type != TK_TAG_CLOSE && tk->type != TK_EOS) {
                    if (!parse_html_sub(vm, ra, html, tk, tag_name)) {
                        // 閉じタグが省略されている
                        goto BREAK_TAG2;
                    }
                }
                if (tk->type != TK_EOS) {
                    if (str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
                        XMLTok_next(tk);
                    }
                }
            BREAK_TAG2:
                ;
            }
        }
        break;
    }
    case TK_TEXT: {
        if (!XMLTok_isspace(tk)) {
            Value *vp = fs->refarray_push(v_ra);
            *vp = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
        }
        XMLTok_next(tk);
        break;
    }
    case TK_COMMENT: {
        Value *vp = fs->refarray_push(v_ra);
        *vp = fs->cstr_Value(cls_comment, tk->val.p, tk->val.size);
        XMLTok_next(tk);
        break;
    }
    default:
        // 不明なトークンは無視
        XMLTok_next(tk);
        break;
    }

BREAK_ALL:
    return TRUE;
}
int parse_html_body(Value *html, RefArray *ra, XMLTok *tk)
{
    HTMLDoc ht;
    Str body = Str_new("body", 4);

    ht.html = html;
    xml_elem_init(ht.html, &ht.html_ra, &ht.html_vm, "html", 4);
    ht.head = fs->refarray_push(ht.html_ra);
    xml_elem_init(ht.head, &ht.head_ra, &ht.head_vm, "head", 4);
    ht.title = fs->refarray_push(ht.head_ra);
    xml_elem_init(ht.title, &ht.title_ra, &ht.title_vm, "title", 5);
    ht.body = fs->refarray_push(ht.html_ra);
    xml_elem_init(ht.body, &ht.body_ra, &ht.body_vm, body.p, body.size);
    ht.ra_list = ra;
    ht.html_node_done = FALSE;

    while (tk->type != TK_EOS) {
        if (!parse_html_sub(ht.body, ht.body_ra, &ht, tk, body)) {
            return FALSE;
        }
    }
    if (!ht.html_node_done) {
        *fs->refarray_push(ra) = fs->Value_cp(*html);
    }
    return TRUE;
}
int parse_xml_begin(Ref *r, XMLTok *tk)
{
    r->v[INDEX_DOCUMENT_HAS_DTD] = VALUE_FALSE;
    r->v[INDEX_DOCUMENT_IS_PUBLIC] = VALUE_TRUE;

    if (tk->type == TK_DECL_START) {
        // ?>まで無視する
        XMLTok_next_tag(tk);

        for (;;) {
            switch (tk->type) {
            case TK_EOS:
                fs->throw_errorf(mod_xml, "XMLParseError", "Missing '?>'");
                return FALSE;
            case TK_ERROR:
                fs->throw_errorf(mod_xml, "XMLParseError", "Unknown token at line %d", tk->line);
                return FALSE;
            case TK_DECL_END:
                XMLTok_next(tk);
                goto BREAK_CMD;
            case TK_ATTR_NAME: {
                XMLTok_next_tag(tk);
                if (tk->type == TK_EQUAL) {
                    XMLTok_next_tag(tk);
                    if (tk->type == TK_STRING) {
                        XMLTok_next_tag(tk);
                    } else {
                        fs->throw_errorf(mod_xml, "XMLParseError", "Missing string after '=' at line %d", tk->line);
                        return FALSE;
                    }
                }
                break;
            }
            default:
                throw_unexpected_token(tk);
                return FALSE;
            }
        }
    BREAK_CMD:
        ;
    }
    if (tk->type == TK_TEXT && XMLTok_isspace(tk)) {
        XMLTok_next(tk);
    }
    if (tk->type == TK_DOCTYPE) {
        if (!parse_doctype_declaration(r, tk)) {
            return FALSE;
        }
        if (tk->type != TK_TAG_END) {
            throw_unexpected_token(tk);
            return FALSE;
        }
        XMLTok_next(tk);
    }
    if (tk->type == TK_TEXT && XMLTok_isspace(tk)) {
        XMLTok_next(tk);
    }

    return TRUE;
}
/**
 * 一つの要素または1つのテキスト要素を解析する
 */
int parse_xml_body(Value *v, XMLTok *tk)
{
    switch (tk->type) {
    case TK_TAG_START: {
        RefArray *ra;
        Value *vm;
        Str tag_name = tk->val;
        xml_elem_init(v, &ra, &vm, tag_name.p, tag_name.size);
        XMLTok_next_tag(tk);

        for (;;) {
            switch (tk->type) {
            case TK_EOS:
                fs->throw_errorf(mod_xml, "XMLParseError", "Missing '>' at line %d", tk->line);
                return FALSE;
            case TK_ERROR:
                throw_unexpected_token(tk);
                return FALSE;
            case TK_TAG_END_CLOSE:
                XMLTok_next(tk);
                goto BREAK_ALL;
            case TK_TAG_END:
                XMLTok_next(tk);
                for (;;) {
                    switch (tk->type) {
                    case TK_TAG_START:
                    case TK_DECL_START:
                    case TK_TEXT:
                    case TK_COMMENT: {
                        Value *ve = fs->refarray_push(ra);
                        if (!parse_xml_body(ve, tk)) {
                            return FALSE;
                        }
                        break;
                    }
                    case TK_TAG_CLOSE:
                        if (!str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
                            if (!tk->loose) {
                                fs->throw_errorf(mod_xml, "XMLParseError", "Tag name mismatch (<%S>...</%S>) at line %d",
                                        tag_name, tk->val, tk->line);
                                return FALSE;
                            }
                            // 閉じタグが対応していない場合は、閉じタグが省略されているとみなす
                            XMLTok_tolower(tk);
                            if (str_eq(tk->val.p, tk->val.size, tag_name.p, tag_name.size)) {
                                // 大文字小文字を同一視(エラー処理)
                                XMLTok_next(tk);
                            }
                        } else {
                            XMLTok_next(tk);
                        }
                        goto BREAK_ALL;
                    default:
                        throw_unexpected_token(tk);
                        return FALSE;
                    }
                }
            case TK_ATTR_NAME: {
                Str skey = tk->val;

                XMLTok_next_tag(tk);
                if (tk->type == TK_EQUAL) {
                    XMLTok_next_tag(tk);
                    if (tk->type == TK_STRING || (tk->loose && tk->type == TK_ATTR_NAME)) {
                        if (!xml_elem_add_attr(vm, skey, tk->val, tk->loose)) {
                            return FALSE;
                        }
                        XMLTok_next_tag(tk);
                    } else {
                        fs->throw_errorf(mod_xml, "XMLParseError", "Missing string after '=' at line %d", tk->line);
                        return FALSE;
                    }
                } else {
                    if (tk->loose) {
                        if (!xml_elem_add_attr(vm, skey, tk->val, tk->loose)) {
                            return FALSE;
                        }
                    } else {
                        fs->throw_errorf(mod_xml, "XMLParseError", "Missing '=' at line %d", tk->line);
                        return FALSE;
                    }
                }
                break;
            }
            default:
                throw_unexpected_token(tk);
                return FALSE;
            }
        }
        break;
    }
    case TK_TEXT:
        *v = fs->cstr_Value(cls_text, tk->val.p, tk->val.size);
        XMLTok_next(tk);
        break;
    case TK_COMMENT:
        *v = fs->cstr_Value(cls_comment, tk->val.p, tk->val.size);
        XMLTok_next(tk);
        break;
    case TK_DECL_START: {
        Ref *r = fs->ref_new(cls_decl);
        *v = vp_Value(r);

        r->v[INDEX_DECL_NAME] = fs->cstr_Value(fs->cls_str, tk->val.p, tk->val.size);
        XMLTok_next_decl(tk);
        if (tk->type == TK_DECL_BODY) {
            r->v[INDEX_DECL_CONTENT] = fs->cstr_Value(fs->cls_str, tk->val.p, tk->val.size);
            XMLTok_next(tk);
        } else {
            if (!tk->loose) {
                throw_unexpected_token(tk);
                return FALSE;
            }
        }
        break;
    }
    case TK_EOS:
        break;
    case TK_ERROR:
        fs->throw_errorf(mod_xml, "XMLParseError", "Unknown token at line %d", tk->line);
        return FALSE;
    default:
        throw_unexpected_token(tk);
        return FALSE;
    }

BREAK_ALL:
    return TRUE;
}
