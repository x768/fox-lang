#include "fox_parse.h"
#include "bigint.h"
#include <string.h>
#include <errno.h>
#include <pcre.h>


static void Tok_parse_digit(TokValue *v, Tok *tk)
{
    char *dst = tk->p;
    v->p = tk->p;
    v->type = TL_INT;
    v->base = 10;

    // \d+(\.\d+)?([Ee][\+\-]\d+)?
    for (;;) {
        int ch = *tk->p & 0xFF;
        if (isdigit_fox(ch)) {
            *dst++ = ch;
            tk->p++;
        } else if (ch == '_') {
            tk->p++;
        } else {
            break;
        }
    }
    if (*tk->p == '.' && isdigit_fox(tk->p[1])) {
        v->type = TL_FLOAT;
        *dst++ = *tk->p++;
        *dst++ = *tk->p++;

        for (;;) {
            int ch = *tk->p & 0xFF;
            if (isdigit(ch)) {
                *dst++ = ch;
                tk->p++;
            } else if (ch == '_') {
                tk->p++;
            } else {
                break;
            }
        }
    }
    if (*tk->p == 'e' || *tk->p == 'E') {
        v->type = TL_FLOAT;
        *dst++ = *tk->p++;
        if (*tk->p == '+' || *tk->p == '-') {
            *dst++ = *tk->p++;
        }
        if (!isdigit_fox(*tk->p)) {
            goto ERROR_END;
        }
        for (;;) {
            int ch = *tk->p;
            if (isdigit_fox(ch)) {
                *dst++ = ch;
                tk->p++;
            } else {
                break;
            }
        }
    }
    v->u.end = dst;

    if (*tk->p == 'd' || *tk->p == 'D') {
        tk->p++;
        if (tk->simple_mode) {
            v->type = T_ERR;
        } else {
            v->type = TL_FRAC;
        }
    }
    return;

ERROR_END:
    throw_errorf(fs->mod_lang, "TokenError", "Invalid number format");
    v->type = T_ERR;
}
static void Tok_parse_bin_digit(TokValue *v, Tok *tk)
{
    char *dst = tk->p;
    v->p = tk->p;

    if (*tk->p != '0' && *tk->p != '1') {
        throw_errorf(fs->mod_lang, "TokenError", "Unknown character %c", *tk->p);
        v->type = T_ERR;
        return;
    }
    tk->p++;
    dst++;
    for (;;) {
        int ch = *tk->p & 0xFF;
        if (ch == '0' || ch == '1') {
            *dst++ = ch;
            tk->p++;
        } else if (ch == '_') {
            tk->p++;
        } else {
            break;
        }
    }
    v->u.end = dst;
    v->type = TL_INT;
    v->base = 2;
}
static void Tok_parse_oct_digit(TokValue *v, Tok *tk)
{
    char *dst = tk->p;
    v->p = tk->p;

    if (*tk->p < '0' || *tk->p > '7') {
        throw_errorf(fs->mod_lang, "TokenError", "Unknown character %c", *tk->p);
        v->type = T_ERR;
        return;
    }
    tk->p++;
    dst++;
    for (;;) {
        int ch = *tk->p & 0xFF;
        if (ch >= '0' && ch <= '7') {
            *dst++ = ch;
            tk->p++;
        } else if (ch == '_') {
            tk->p++;
        } else {
            break;
        }
    }
    v->u.end = dst;
    v->type = TL_INT;
    v->base = 8;
}
static void Tok_parse_hex_digit(TokValue *v, Tok *tk)
{
    char *dst = tk->p;
    v->p = tk->p;

    if (!isxdigit_fox(*tk->p)) {
        throw_errorf(fs->mod_lang, "TokenError", "Unknown character %c", *tk->p);
        v->type = T_ERR;
        return;
    }
    tk->p++;
    dst++;

    // \h+(\.\h+)?([Pp][\+\-]\d+)?
    for (;;) {
        int ch = *tk->p;
        if (isxdigit(ch)) {
            *dst++ = ch;
            tk->p++;
        } else if (ch == '_') {
            tk->p++;
        } else {
            break;
        }
    }
    if (*tk->p == '.' && isdigit_fox(tk->p[1])) {
        // TODO 浮動小数点数の16進表記
    }

    v->u.end = dst;
    v->type = TL_INT;
    v->base = 16;
}
/**
 * 最初の1文字は飛ばす
 */
static int Keyword_eq(Str s, const char *key)
{
    const char *p = s.p;
    const char *end = p + s.size;

    p++;
    key++;

    while (p < end) {
        if (*p != *key) {
            return FALSE;
        }
        p++;
        key++;
    }
    return *key == '\0';
}
static void Tok_parse_ident(TokValue *v, Tok *tk)
{
    Str name;
    v->p = tk->p;

    if (*tk->p == '_') {
        tk->p++;
    }
    if (isupper_fox(*tk->p)) {
        v->type = TL_CONST;
    } else {
        v->type = TL_VAR;
    }

    while (isalnumu_fox(*tk->p)) {
        if (v->type == TL_CONST && islower_fox(*tk->p)) {
            v->type = TL_CLASS;
        }
        tk->p++;
    }

    v->u.end = tk->p;
    name = Str_new(v->p, v->u.end - v->p);
    tk->prev_id = TRUE;

    if (name.size == 0) {
        name.p = NULL;
    }
    if (tk->simple_mode) {
        return;
    }

    switch (*v->p) {
    case 'a':
        if (Keyword_eq(name, "abstract")) {
            v->type = TT_ABSTRACT;
        }
        break;
    case 'b':
        if (Keyword_eq(name, "break")) {
            v->type = TT_BREAK;
            tk->prev_id = FALSE;
        }
        break;
    case 'c':
        if (Keyword_eq(name, "class")) {
            v->type = TT_CLASS;
        } else if (Keyword_eq(name, "continue")) {
            v->type = TT_CONTINUE;
            tk->prev_id = FALSE;
        } else if (Keyword_eq(name, "case")) {
            v->type = TT_CASE;
            tk->prev_id = FALSE;
        } else if (Keyword_eq(name, "catch")) {
            v->type = TT_CATCH;
            tk->prev_id = FALSE;
        }
        break;
    case 'd':
        if (Keyword_eq(name, "def")) {
            v->type = TT_DEF;
        } else if (Keyword_eq(name, "default")) {
            v->type = TT_DEFAULT;
            tk->prev_id = FALSE;
        }
        break;
    case 'e':
        if (Keyword_eq(name, "else")) {
            v->type = TT_ELSE;
            tk->prev_id = FALSE;
        } else if (Keyword_eq(name, "elif")) {
            v->type = TT_ELIF;
            tk->prev_id = FALSE;
        }
        break;
    case 'f':
        if (Keyword_eq(name, "false")) {
            v->type = TT_FALSE;
        } else if (Keyword_eq(name, "for")) {
            v->type = TT_FOR;
            tk->prev_id = FALSE;
        }
        break;
    case 'i':
        if (Keyword_eq(name, "if")) {
            v->type = TT_IF;
            tk->prev_id = FALSE;
        } else if (Keyword_eq(name, "in")) {
            v->type = TT_IN;
            tk->prev_id = FALSE;
        } else if (Keyword_eq(name, "import")) {
            v->type = TT_IMPORT;
            tk->prev_id = FALSE;
        }
        break;
    case 'l':
        if (Keyword_eq(name, "let")) {
            v->type = TT_LET;
        }
        break;
    case 'n':
        if (Keyword_eq(name, "null")) {
            v->type = TT_NULL;
        }
        break;
    case 'r':
        if (Keyword_eq(name, "return")) {
            v->type = TT_RETURN;
            tk->prev_id = FALSE;
        }
        break;
    case 's':
        if (Keyword_eq(name, "switch")) {
            v->type = TT_SWITCH;
            tk->prev_id = FALSE;
        } else if (Keyword_eq(name, "super")) {
            v->type = TT_SUPER;
        }
        break;
    case 't':
        if (Keyword_eq(name, "this")) {
            v->type = TT_THIS;
        } else if (Keyword_eq(name, "true")) {
            v->type = TT_TRUE;
        } else if (Keyword_eq(name, "try")) {
            v->type = TT_TRY;
        } else if (Keyword_eq(name, "throw")) {
            v->type = TT_THROW;
            tk->prev_id = FALSE;
        }
        break;
    case 'v':
        if (Keyword_eq(name, "var")) {
            v->type = TT_VAR;
        }
        break;
    case 'w':
        if (Keyword_eq(name, "while")) {
            v->type = TT_WHILE;
            tk->prev_id = FALSE;
        }
        break;
    case 'y':
        if (Keyword_eq(name, "yield")) {
            v->type = TT_YIELD;
            tk->prev_id = FALSE;
        }
        break;
    }
}
static void Tok_parse_ident_var(TokValue *v, Tok *tk)
{
    v->p = tk->p;
    tk->p++;

    while (isalnumu_fox(*tk->p)) {
        tk->p++;
    }

    v->u.end = tk->p;
    v->type = TL_VAR;
    tk->prev_id = TRUE;
}
static void Tok_parse_single_str(TokValue *v, Tok *tk, int term, int type)
{
    char *dst = tk->p;
    v->p = tk->p;

    for (;;) {
        char ch = *tk->p;

        switch (ch) {
        case '\0':
#ifdef DEBUGGER
            if (tk->is_test) {
                v->type = T_WANTS_NEXT;
            } else {
#endif
                throw_errorf(fs->mod_lang, "TokenError", "Unterminated string");
                v->type = T_ERR;
#ifdef DEBUGGER
            }
#endif
            return;
        case '\r':
            throw_errorf(fs->mod_lang, "TokenError", "Illigal character '\\r'");
            v->type = T_ERR;
            return;
        default:
            if (ch == '\n') {
                tk->head_line++;
            }
            if (ch == term) {
                tk->p++;
                if (*tk->p == term && term != ')' && term != ']' && term != '}') {
                    // 'abc''def'
                    ch = *tk->p++;
                } else {
                    goto BREAK;
                }
            } else {
                tk->p++;
            }
            *dst++ = ch;
            break;
        }
    }

BREAK:
    v->type = type;
    v->u.end = dst;
    if (type == TL_STR) {
        if (invalid_utf8_pos(v->p, v->u.end - v->p) >= 0) {
            throw_errorf(fs->mod_lang, "TokenError", "Invalid UTF-8 sequence");
            v->type = T_ERR;
        }
    }
}
static void Tok_parse_double_str(TokValue *v, Tok *tk, int cat, int term)
{
    char *dst = tk->p;
    v->p = tk->p;

    for (;;) {
        char ch = *tk->p;

        switch (ch) {
        case '\0':
#ifdef DEBUGGER
            if (tk->is_test) {
                v->type = T_WANTS_NEXT;
            } else {
#endif
                throw_errorf(fs->mod_lang, "TokenError", "Unterminated string");
                v->type = T_ERR;
#ifdef DEBUGGER
            }
#endif
            return;
        case '$':
            tk->p++;
            if (tk->simple_mode) {
                *dst++ = ch;
            } else {
                ch = *tk->p;
                if (ch == '{') {
                    // "${val}"
                    tk->p_nest_n++;
                    if (tk->p_nest_n > MAX_EXPR_STACK) {
                        throw_errorf(fs->mod_lang, "ParseError", "Expression too complex");
                        v->type = T_ERR;
                        return;
                    }
                    tk->p_nest[tk->p_nest_n] = CURLY_STRING | term;
                    tk->p++;
                    v->type = (cat ? TL_STRCAT_MID : TL_STRCAT_BEGIN);
                    goto FINALLY;
                } else if (ch == '$') {
                    // "$$" -> '$'
                    tk->p++;
                    *dst++ = '$';
                } else if (isalpha(ch) || ch == '_') {
                    // "$val"
                    tk->p_nest_n++;
                    if (tk->p_nest_n > MAX_EXPR_STACK) {
                        throw_errorf(fs->mod_lang, "ParseError", "Expression too complex");
                        v->type = T_ERR;
                        return;
                    }
                    tk->p_nest[tk->p_nest_n] = CURLY_IDENT | term;
                    v->type = (cat ? TL_STRCAT_MID : TL_STRCAT_BEGIN);
                    goto FINALLY;
                } else {
                    throw_errorf(fs->mod_lang, "TokenError", "Unknown character after $");
                    v->type = T_ERR;
                    return;
                }
            }
            break;
        case '\\':
            tk->p++;
            ch = *tk->p;
            tk->p++;

            /*
             * \0        NUL
             * \b        bell
             * \f        formfeed (hex 0C)
             * \n        linefeed (hex 0A)
             * \r        carriage return (hex 0D)
             * \t        tab (hex 09)
             * \xhh      character with hex code hh (< 0x80)
             * \x{hhh..} character with hex code (UCS4)
             * \uhhhh    character with hex code (UCS2)
             */
            switch (ch) {
            case '\0':
                throw_errorf(fs->mod_lang, "TokenError", "Unterminated string");
                v->type = T_ERR;
                return;
            case '0':
                *dst++ = '\0';
                break;
            case 'b':
                *dst++ = '\b';
                break;
            case 'f':
                *dst++ = '\f';
                break;
            case 'n':
                *dst++ = '\n';
                break;
            case 'r':
                *dst++ = '\r';
                break;
            case 't':
                *dst++ = '\t';
                break;
            case 'u': {
                int ch2 = parse_hex((const char**)&tk->p, NULL, 4);
                if (ch2 < 0) {
                    throw_errorf(fs->mod_lang, "TokenError", "Illigal string hexformat");
                    v->type = T_ERR;
                    return;
                }
                if (ch2 < 0x80) {
                    *dst++ = ch2;
                } else if (ch2 < 0x800) {
                    *dst++ = 0xC0 | (ch2 >> 6);
                    *dst++ = 0x80 | (ch2 & 0x3F);
                } else if (ch2 < SURROGATE_U_BEGIN) {
                    *dst++ = 0xE0 | (ch2 >> 12);
                    *dst++ = 0x80 | ((ch2 >> 6) & 0x3F);
                    *dst++ = 0x80 | (ch2 & 0x3F);
                } else if (ch2 < SURROGATE_L_BEGIN) {
                    // 上位サロゲート
                    int ch3;
                    if (*tk->p != '\\' || tk->p[1] != 'u') {
                        throw_errorf(fs->mod_lang, "TokenError", "Missing Low Surrogate");
                        v->type = T_ERR;
                        return;
                    }
                    tk->p += 2;
                    ch3 = parse_hex((const char**)&tk->p, NULL, 4);
                    if (ch3 < SURROGATE_L_BEGIN || ch3 >= SURROGATE_END) {
                        throw_errorf(fs->mod_lang, "TokenError", "Invalid Low Surrogate");
                        v->type = T_ERR;
                        return;
                    }
                    ch2 = (ch2 - SURROGATE_U_BEGIN) * 0x400 + 0x10000;
                    ch3 = ch2 | (ch3 - SURROGATE_L_BEGIN);

                    *dst++ = 0xF0 | (ch3 >> 18);
                    *dst++ = 0x80 | ((ch3 >> 12) & 0x3F);
                    *dst++ = 0x80 | ((ch3 >> 6) & 0x3F);
                    *dst++ = 0x80 | (ch3 & 0x3F);
                } else if (ch2 < SURROGATE_END) {
                    // 下位サロゲート
                    throw_errorf(fs->mod_lang, "TokenError", "Illigal codepoint (Surrogate) %U", ch2);
                    v->type = T_ERR;
                    return;
                } else {
                    *dst++ = 0xE0 | (ch2 >> 12);
                    *dst++ = 0x80 | ((ch2 >> 6) & 0x3F);
                    *dst++ = 0x80 | (ch2 & 0x3F);
                }
                break;
            }
            case 'x':
                ch = *tk->p;
                if (ch == '{') {
                    int ch2;

                    tk->p++;
                    ch2 = parse_hex((const char**)&tk->p, NULL, -1);
                    if (ch2 < 0) {
                        throw_errorf(fs->mod_lang, "TokenError", "Illigal string hexformat");
                        v->type = T_ERR;
                        return;
                    }
                    if (*tk->p != '}') {
                        throw_errorf(fs->mod_lang, "TokenError", "Illigal string hexformat");
                        v->type = T_ERR;
                        return;
                    }

                    if (!str_add_codepoint(&dst, ch2, "TokenError")) {
                        v->type = T_ERR;
                        return;
                    }
                    tk->p++;
                } else {
                    int ch2 = parse_hex((const char**)&tk->p, NULL, 2);

                    if (ch2 < 0) {
                        throw_errorf(fs->mod_lang, "TokenError", "Illigal string hexformat");
                        v->type = T_ERR;
                        return;
                    }
                    if (ch2 >= 0x80) {
                        throw_errorf(fs->mod_lang, "TokenError", "Illigal hexadecimal value");
                        v->type = T_ERR;
                        return;
                    }
                    *dst++ = ch2;
                }
                break;
            case '\r':
                throw_errorf(fs->mod_lang, "TokenError", "Illigal character '\\r'");
                v->type = T_ERR;
                return;
            case '\n': // ignore
                tk->head_line++;
                break;
            default:
                if (isalnum(ch)) {
                    throw_errorf(fs->mod_lang, "TokenError", "Unknown backslash sequence");
                    v->type = T_ERR;
                    return;
                }
                *dst++ = ch;
                break;
            }
            break;
        case '\r':
            throw_errorf(fs->mod_lang, "TokenError", "Illigal character '\\r'");
            v->type = T_ERR;
            return;
        case '\n':
            tk->head_line++;
            tk->p++;
            *dst++ = '\n';
            break;
        default:
            if (ch == term) {
                tk->p++;
                v->type = (cat ? TL_STRCAT_END : TL_STR);
                tk->prev_id = TRUE;
                goto FINALLY;
            }
            tk->p++;
            *dst++ = ch;
            break;
        }
    }
FINALLY:
    v->u.end = dst;
    if (invalid_utf8_pos(v->p, v->u.end - v->p) >= 0) {
        throw_errorf(fs->mod_lang, "TokenError", "Invalid UTF-8 sequence");
        v->type = T_ERR;
    }
    return;
}
static void Tok_parse_double_bin(TokValue *v, Tok *tk, int term)
{
    char *dst = tk->p;
    v->p = tk->p;

    for (;;) {
        char ch = *tk->p;

        switch (ch) {
        case '\0':
#ifdef DEBUGGER
            if (tk->is_test) {
                v->type = T_WANTS_NEXT;
            } else {
#endif
                throw_errorf(fs->mod_lang, "TokenError", "Unterminated string");
                v->type = T_ERR;
#ifdef DEBUGGER
            }
#endif
            return;
        case '\\':
            tk->p++;
            ch = *tk->p;
            tk->p++;

            /*
             * \0        NUL
             * \b        bell (hex 1B)
             * \f        formfeed (hex 0C)
             * \n        linefeed (hex 0A)
             * \r        carriage return (hex 0D)
             * \t        tab (hex 09)
             * \xhh      character with hex code hh
             */
            switch (ch) {
            case '0':
                *dst++ = '\0';
                break;
            case 'b':
                *dst++ = '\b';
                break;
            case 'f':
                *dst++ = '\f';
                break;
            case 'n':
                *dst++ = '\n';
                break;
            case 'r':
                *dst++ = '\r';
                break;
            case 't':
                *dst++ = '\t';
                break;
            case '\r':
                throw_errorf(fs->mod_lang, "TokenError", "Illigal character '\\r'");
                v->type = T_ERR;
                return;
            case '\n':
                break;
            case '\0':
                throw_errorf(fs->mod_lang, "TokenError", "Unterminated string");
                v->type = T_ERR;
                return;
            case 'x': {
                int ch2 = parse_hex((const char**)&tk->p, NULL, 2);
                if (ch2 < 0) {
                    throw_errorf(fs->mod_lang, "TokenError", "Illigal string hexformat");
                    v->type = T_ERR;
                    return;
                }
                *dst++ = ch2;
                break;
            }
            default:
                if (isalnum(ch)) {
                    throw_errorf(fs->mod_lang, "TokenError", "Unknown backslash sequence");
                    v->type = T_ERR;
                    return;
                }
                *dst++ = ch;
                break;
            }
            break;
        case '\r':
            throw_errorf(fs->mod_lang, "TokenError", "Illigal character '\\r'");
            v->type = T_ERR;
            return;
        case '\n':
            tk->head_line++;
            tk->p++;
            *dst++ = '\n';
            break;
        default:
            if (ch == term) {
                tk->p++;
                v->u.end = dst;
                v->type = TL_BYTES;
                return;
            }
            tk->p++;
            *dst++ = ch;
            break;
        }
    }
}
static void Tok_parse_binhex(TokValue *v, Tok *tk)
{
    char *dst = tk->p;
    v->p = tk->p;

    for (;;) {
        char ch = *tk->p;

        if (islexspace_fox(ch)) {
            tk->p++;
        } else if (isxdigit(ch)) {
            char ch2;
            ch = *tk->p;

            if (!isxdigit(ch)) {
                throw_errorf(fs->mod_lang, "TokenError", "Illigal string hexformat");
                v->type = T_ERR;
                return;
            }
            ch2 = char2hex(ch);

            tk->p++;
            ch = *tk->p;
            if (isxdigit(ch)) {
                ch2 <<= 4;
                ch2 |= char2hex(ch);
                tk->p++;
            }
            *dst++ = ch2;
        } else {
            switch (ch) {
            case '\0':
#ifdef DEBUGGER
                if (tk->is_test) {
                    v->type = T_WANTS_NEXT;
                } else {
#endif
                    throw_errorf(fs->mod_lang, "TokenError", "Unterminated string");
                    v->type = T_ERR;
#ifdef DEBUGGER
                }
#endif
                return;
            case '"':
                tk->p++;
                v->u.end = dst;
                v->type = TL_BYTES;
                return;
            case '\r':
                throw_errorf(fs->mod_lang, "TokenError", "Illigal character '\\r'");
                v->type = T_ERR;
                return;
            case '\n':
                tk->head_line++;
                tk->p++;
                break;
            default:
                throw_errorf(fs->mod_lang, "TokenError", "Unknown binhex charactor (0-9,A-F,a-f or space only)");
                v->type = T_ERR;
                return;
            }
        }
    }
}
static void Tok_parse_regex(TokValue *v, Tok *tk, char term)
{
    char *pattern = tk->p;
    const char *end;
    int options = 0;
    int trim_space = FALSE;

    v->p = pattern;
    while (*tk->p != term) {
        switch (*tk->p) {
        case '\\':
            tk->p++;
            break;
        case '\n':
            tk->head_line++;
            break;
        case '\0':
#ifdef DEBUGGER
            if (tk->is_test) {
                v->type = T_WANTS_NEXT;
            } else {
#endif
                throw_errorf(fs->mod_lang, "TokenError", "Unterminated Regexp");
                v->type = T_ERR;
#ifdef DEBUGGER
            }
#endif
            return;
        default:
            break;
        }

        tk->p++;
    }
    end = tk->p;
    *tk->p++ = '\0';

    for (;;) {
        switch (*tk->p) {
        case 'i':
            options |= PCRE_CASELESS;
            break;
        case 's':
            options |= PCRE_DOTALL;
            break;
        case 'm':
            options |= PCRE_MULTILINE;
            break;
        case 'u':
            options |= PCRE_UTF8;
            break;
        case 'x':
            trim_space = TRUE;
            break;
        default:
            if (isalpha(*tk->p)) {
                throw_errorf(fs->mod_lang, "RegexError", "Unknown Regex option");
                v->type = T_ERR;
                return;
            }
            v->u.flag = options;
            goto BREAK;
        }
        tk->p++;
    }

BREAK:
    if (trim_space) {
        const char *src = pattern;
        char *dst = pattern;

        // 空白を除去
        // #から行末を除去
        while (src < end) {
            if (isspace_fox(*src)) {
                src++;
            } else if (*src == '\\') {
                // コメント記号(#)自体をエスケープできる
                *dst++ = *src++;
                if (*src == '\n') {
                    tk->head_line++;
                }
                *dst++ = *src++;
            } else if (*src == '#') {
                // 行末の次まで飛ばす
                while (src < end) {
                    if (*src == '\n') {
                        src++;
                        tk->head_line++;
                        break;
                    }
                    src++;
                }
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
        end = dst;
    }
    if (invalid_utf8_pos(pattern, end - pattern) >= 0) {
        throw_errorf(fs->mod_lang, "TokenError", "Invalid UTF-8 sequence");
        v->type = T_ERR;
    } else {
        v->type = TL_REGEX;
    }
}
static void Tok_parse_heredoc(TokValue *v, Tok *tk)
{
    Str term;

    while (islexspace_fox(*tk->p)) {
        tk->p++;
    }

    if (!isalpha(*tk->p) && *tk->p != '_') {
        throw_errorf(fs->mod_lang, "TokenError", "Unknown character after <<<");
        v->type = T_ERR;
        return;
    }

    term.p = tk->p;
    while (isalnum(*tk->p) || *tk->p == '_') {
        tk->p++;
    }
    term.size = tk->p - term.p;

    if (*tk->p == '\t' || *tk->p == ' ') {
        tk->p++;
    } else if (*tk->p == '\r') {
        throw_errorf(fs->mod_lang, "TokenError", "Unknown character '\\r'");
        v->type = T_ERR;
        return;
    }
    if (*tk->p != '\n') {
#ifdef DEBUGGER
        if (tk->is_test) {
            v->type = T_WANTS_NEXT;
        } else {
#endif
            throw_errorf(fs->mod_lang, "TokenError", "Unknown character after <<<");
            v->type = T_ERR;
#ifdef DEBUGGER
        }
#endif
        return;
    }
    tk->p++;
    tk->head_line++;

    v->p = tk->p;
    for (;;) {
        // 行の先頭がtermで始まる場合は、文字列の終わりとする
        if (memcmp(term.p, tk->p, term.size) == 0) {
            char c = tk->p[term.size];
            if (!isalnum(c) && c != '_') {
                char *ptr = tk->p - 1;
                v->u.end = ptr;
                tk->p += term.size;
                break;
            }
        }
        while (*tk->p != '\0' && *tk->p != '\n') {
            tk->p++;
        }
        if (*tk->p == '\n') {
            tk->head_line++;
            tk->p++;
        } else {
#ifdef DEBUGGER
            if (tk->is_test) {
                v->type = T_WANTS_NEXT;
            } else {
#endif
                throw_errorf(fs->mod_lang, "TokenError", "Unterminated heredoc");
                v->type = T_ERR;
#ifdef DEBUGGER
            }
#endif
            return;
        }
    }

    if (v->u.end < v->p) {
        v->u.end = v->p;
    }
    if (invalid_utf8_pos(v->p, v->u.end - v->p) >= 0) {
        throw_errorf(fs->mod_lang, "TokenError", "Invalid UTF-8 sequence");
        v->type = T_ERR;
    }
    v->type = TL_STR;
}

static int get_pair_char(int ch)
{
    switch (ch) {
    case '(':
        return ')';
    case '[':
        return ']';
    case '{':
        return '}';
    case '<':
        return '>';
    default:
        if (ispunct(ch)) {
            return ch;
        } else {
            return -1;
        }
    }
}

/**
 * トークンを1つ先読みし、TokValueに追加する
 */
static void Tok_fetch(TokValue *v, Tok *tk)
{
    int prev_id;
    char ch;
    int prev_space;

    // "val = $val"
    if (tk->p_nest_n >= 0){
        int nest = tk->p_nest[tk->p_nest_n];
        if ((nest & CURLY_MASK) == CURLY_IDENT) {
            ch = *tk->p;
            if (isalphau_fox(ch)) {
                Tok_parse_ident(v, tk);
            } else {
                tk->p_nest_n--;
                Tok_parse_double_str(v, tk, TRUE, nest & ~CURLY_MASK);
            }
            tk->prev_id = FALSE;
            return;
        }
    }

    prev_space = FALSE;
START:
    for (;;) {
        ch = *tk->p;
        if (ch != ' ' && ch != '\t') {
            break;
        }
        tk->p++;
        prev_space = TRUE;
    }

    prev_id = tk->prev_id;
    tk->prev_id = FALSE;
    v->line = tk->head_line;

    switch (*tk->p) {
    case '\0':
        v->type = T_EOF;
        break;
    case '\n':
        tk->p++;
        tk->head_line++;
        if (tk->ignore_nl) {
            goto START;
        }
        v->type = T_NL;
        break;
    case '#': {
        // \rはエラー
        const char *p = ++tk->p;
        while (*tk->p != '\0' && *tk->p != '\r' && *tk->p != '\n') {
            tk->p++;
        }
        tk->str_val.p = p;
        tk->str_val.size = tk->p - p;
        v->type = TL_PRAGMA;

        if (*tk->p == '\n') {
            tk->p++;
            tk->head_line++;
        }
        break;
    }
    case '(':
        v->type = (prev_space ? T_LP_C : T_LP);
        tk->p++;
        break;
    case ')':
        v->type = T_RP;
        tk->p++;
        tk->prev_id = TRUE;
        break;
    case '[':
        v->type = (prev_space ? T_LB_C : T_LB);
        tk->p++;
        break;
    case ']':
        v->type = T_RB;
        tk->p++;
        tk->prev_id = TRUE;
        break;
    case '{':
        tk->p_nest_n++;
        if (tk->p_nest_n > MAX_EXPR_STACK) {
            throw_errorf(fs->mod_lang, "ParseError", "Expression too complex");
            return;
        }
        tk->p_nest[tk->p_nest_n] = CURLY_NORMAL;
        v->type = T_LC;
        tk->p++;
        break;
    case '}':
        if (tk->p_nest_n < 0){
            v->type = T_RC;
            tk->p++;
        } else {
            int nest = tk->p_nest[tk->p_nest_n];
            switch (nest & CURLY_MASK) {
            case CURLY_NORMAL:
                v->type = T_RC;
                tk->p++;
                tk->p_nest_n--;
                break;
            case CURLY_STRING:
                tk->p++;
                tk->p_nest_n--;
                Tok_parse_double_str(v, tk, TRUE, nest & ~CURLY_MASK);
                tk->prev_id = TRUE;
                break;
            }
        }
        break;
    case ',':
        v->type = T_COMMA;
        tk->p++;
        break;
    case ':':
        if (tk->wait_colon == 0 && tk->p_nest_n >= 0 && (tk->p_nest[tk->p_nest_n] & CURLY_MASK) == CURLY_STRING) {
            // :待ちではなく、文字列解析中の場合
            tk->p++;
            v->p = tk->p;
            while (*tk->p != '\0' && *tk->p != '}') {
                tk->p++;
            }
            v->u.end = tk->p;
            v->type = TL_FORMAT;
        } else {
            v->type = T_COLON;
            tk->p++;
        }
        break;
    case '?':
        v->type = T_QUEST;
        tk->p++;
        break;
    case ';':
        v->type = T_SEMICL;
        tk->p++;
        break;
    case '.':
        tk->p++;
        if (*tk->p == '.') {
            tk->p++;
            if (*tk->p == '.') {
                tk->p++;
                v->type = T_DOT3;
            } else {
                v->type = T_DOT2;
            }
        } else if (*tk->p == '?') {
            tk->p++;
            v->type = T_QMEMB;
        } else {
            v->type = T_MEMB;
        }
        break;
    case '~':
        v->type = T_INV;
        tk->p++;
        break;
    case '*':
        tk->p++;
        if (*tk->p == '=') {
            v->type = T_L_MUL;
            tk->p++;
        } else {
            v->type = T_MUL;
        }
        break;
    case '+':
        tk->p++;
        if (*tk->p == '+') {
            v->type = T_INC;
            tk->prev_id = TRUE;
            tk->p++;
        } else if (*tk->p == '=') {
            v->type = T_L_ADD;
            tk->p++;
        } else {
            v->type = T_ADD;
        }
        break;
    case '-':
        tk->p++;
        if (*tk->p == '-') {
            v->type = T_DEC;
            tk->prev_id = TRUE;
            tk->p++;
        } else if (*tk->p == '=') {
            v->type = T_L_SUB;
            tk->p++;
        } else {
            v->type = T_SUB;
        }
        break;
    case '=':
        tk->p++;
        if (*tk->p == '=') {
            tk->p++;
            v->type = T_EQ;
        } else if (*tk->p == '>') {
            tk->p++;
            v->type = T_ARROW;
        } else {
            v->type = T_LET;
        }
        break;
    case '!':
        tk->p++;
        if (*tk->p == '=') {
            tk->p++;
            v->type = T_NEQ;
        } else {
            v->type = T_NOT;
        }
        break;
    case '|':
        tk->p++;
        if (*tk->p == '|') {
            tk->p++;
            if (*tk->p == '=') {
                tk->p++;
                v->type = T_L_LOR;
            } else {
                v->type = T_LOR;
            }
        } else if (*tk->p == '=') {
            tk->p++;
            v->type = T_L_OR;
        } else {
            v->type = T_OR;
        }
        break;
    case '&':
        tk->p++;
        if (*tk->p == '&') {
            tk->p++;
            if (*tk->p == '=') {
                tk->p++;
                v->type = T_L_LAND;
            } else {
                v->type = T_LAND;
            }
        } else if (*tk->p == '=') {
            tk->p++;
            v->type = T_L_AND;
        } else {
            v->type = T_AND;
        }
        break;
    case '^':
        tk->p++;
        if (*tk->p == '=') {
            tk->p++;
            v->type = T_L_XOR;
        } else {
            v->type = T_XOR;
        }
        break;

    case '<':
        tk->p++;
        if (*tk->p == '<') {
            tk->p++;
            if (*tk->p == '=') {
                tk->p++;
                v->type = T_L_LSH;
            } else if (*tk->p == '<') {
                tk->p++;
                Tok_parse_heredoc(v, tk);
                tk->prev_id = TRUE;
            } else {
                v->type = T_LSH;
            }
        } else if (*tk->p == '=') {
            tk->p++;
            if (*tk->p == '>') {
                tk->p++;
                v->type = T_CMP;
            } else {
                v->type = T_LE;
            }
        } else {
            v->type = T_LT;
        }
        break;
    case '>':
        tk->p++;
        if (*tk->p == '>') {
            tk->p++;
            if (*tk->p == '=') {
                tk->p++;
                v->type = T_L_RSH;
            } else {
                v->type = T_RSH;
            }
        } else if (*tk->p == '=') {
            tk->p++;
            v->type = T_GE;
        } else {
            v->type = T_GT;
        }
        break;

    case '%':
        tk->p++;
        if (!prev_id || (prev_space && !islexspace_fox(*tk->p))) {
            int term;
            int type = *tk->p;

            if (isalpha(type)) {
                tk->p++;
            } else {
                type = 'Q';
            }
            term = get_pair_char(*tk->p);
            if (term == -1) {
                throw_errorf(fs->mod_lang, "TokenError", "Illigal terminate char at %% notation");
                v->type = T_ERR;
                return;
            }
            tk->p++;

            switch (type) {
            case 'Q':  // "str"
                Tok_parse_double_str(v, tk, FALSE, term);
                tk->prev_id = TRUE;
                break;
            case 'q':  // 'str'
                Tok_parse_single_str(v, tk, term, TL_STR);
                tk->prev_id = TRUE;
                break;
            case 'r':  // /str/
                Tok_parse_regex(v, tk, term);
                tk->prev_id = TRUE;
                break;
            case 'b':  // b'abc'
                Tok_parse_single_str(v, tk, term, TL_BYTES);
                break;
            case 'B':  // b"abc"
                Tok_parse_double_bin(v, tk, term);
                break;
            default:
                throw_errorf(fs->mod_lang, "TokenError", "Illigal terminate char at %% notation");
                v->type = T_ERR;
                return;
            }
            tk->prev_id = TRUE;
        } else if (*tk->p == '=') {
            v->type = T_L_MOD;
            tk->p++;
        } else {
            v->type = T_MOD;
        }
        break;
    case '/':
        tk->p++;
        if (*tk->p == '/') {
            while (*tk->p != '\0' && *tk->p != '\n') {
                tk->p++;
            }
            goto START;
        } else if (*tk->p == '*') {
            // コメントが閉じていなくてもエラーにしない
            for (; *tk->p != '\0'; tk->p++) {
                if (tk->p[0] == '*' && tk->p[1] == '/') {
                    tk->p += 2;
                    break;
                }
                if (*tk->p == '\n') {
                    tk->head_line++;
                }
            }
            goto START;
        } else if (!prev_id || (prev_space && !islexspace_fox(*tk->p) && *tk->p != '=')) {
            Tok_parse_regex(v, tk, '/');
            tk->prev_id = TRUE;
        } else if (*tk->p != '\0' && *tk->p == '=') {
            v->type = T_L_DIV;
            tk->p++;
        } else {
            v->type = T_DIV;
        }
        break;

    case '\'':
        tk->p++;
        Tok_parse_single_str(v, tk, '\'', TL_STR);
        tk->prev_id = TRUE;
        break;
    case '"':
        tk->p++;
        Tok_parse_double_str(v, tk, FALSE, '"');
        break;

    case '0':
        switch (tk->p[1]) {
        case 'x':
            tk->p += 2;
            Tok_parse_hex_digit(v, tk);
            tk->prev_id = TRUE;
            break;
        case 'b':
            tk->p += 2;
            Tok_parse_bin_digit(v, tk);
            tk->prev_id = TRUE;
            break;
        case 'o':
            tk->p += 2;
            Tok_parse_oct_digit(v, tk);
            tk->prev_id = TRUE;
            break;
        case '.':
            if (isdigit_fox(tk->p[2])) {
                Tok_parse_digit(v, tk);
                tk->prev_id = TRUE;
            } else {
                v->p = tk->p;
                tk->p++;
                v->u.end = tk->p;

                v->type = TL_INT;
                v->base = 10;
                tk->prev_id = TRUE;
            }
            break;
        case 'd':
            v->p = tk->p;
            v->u.end = tk->p + 1;
            tk->p += 2;
            v->type = TL_FRAC;
            break;
        default:
            Tok_parse_digit(v, tk);
            tk->prev_id = TRUE;
            break;
        }
        break;
    case '$':
    case '@':
        Tok_parse_ident_var(v, tk);
        break;
    default: {
        char ch2;
        ch = *tk->p;
        ch2 = tk->p[1];

        if (ch == 'b' && (ch2 == '"' || ch2 == '\'')) {
            tk->p += 2;
            if (ch2 == '"') {
                // b"hello \0 \xFF"
                Tok_parse_double_bin(v, tk, '"');
            } else {
                Tok_parse_single_str(v, tk, '\'', TL_BYTES);
            }
            tk->prev_id = TRUE;
        } else if (ch == 'x' && ch2 == '"') {
            // x"00 1F 09 11"
            tk->p += 2;
            Tok_parse_binhex(v, tk);
            tk->prev_id = TRUE;
        } else if (isdigit_fox(ch)) {
            Tok_parse_digit(v, tk);
            tk->prev_id = TRUE;
        } else if (isalphau_fox(ch)) {
            Tok_parse_ident(v, tk);
        } else {
            throw_errorf(fs->mod_lang, "TokenError", "Unknown character %c", ch);
            v->type = T_ERR;
        }
        break;
    }
    }
}

/**
 * トークンを1個読み進める
 * 一部のトークンは先読みする
 */
void Tok_next(Tok *tk)
{
    if (tk->prev_n != tk->prev_top) {
        // 先読みトークンが残っている場合は、そちらを使う
        tk->v = tk->fetch[tk->prev_n];
        tk->prev_n = (tk->prev_n + 1) & (PREFETCH_MAX - 1);
    } else {
        Tok_fetch(&tk->v, tk);
        if (tk->v.type == T_ERR) {
            add_stack_trace(tk->module, NULL, tk->v.line);
        }
    }

    // 値の変換が必要な項目のみ
    switch (tk->v.type) {
    case TL_BYTES:
    case TL_STR:
    case TL_STRCAT_BEGIN:
    case TL_STRCAT_MID:
    case TL_STRCAT_END:
    case TL_FORMAT:
    case TL_CONST:
    case TL_CLASS:
    case TL_VAR:
        tk->str_val.p = tk->v.p;
        tk->str_val.size = tk->v.u.end - tk->v.p;
        break;
    case TL_REGEX:
        tk->str_val.p = tk->v.p;
        tk->str_val.size = tk->v.u.end - tk->v.p;
        tk->int_val = tk->v.u.flag;
        break;
    case TL_INT: {
        char c = *tk->v.u.end;
        long val;
        *tk->v.u.end = '\0';
        errno = 0;
        val = strtol(tk->v.p, NULL, tk->v.base);

        if (errno != 0 || val <= INT32_MIN || val > INT32_MAX) {
            BigInt_init(&tk->bi_val[0]);
            cstr_BigInt(&tk->bi_val[0], tk->v.base, tk->v.p, -1);
            tk->v.type = TL_BIGINT;
        } else {
            tk->v.type = TL_INT;
            tk->int_val = (int32_t)val;
        }
        *tk->v.u.end = c;
        break;
    }
    case TL_FRAC: {
        char c = *tk->v.u.end;
        *tk->v.u.end = '\0';

        BigInt_init(&tk->bi_val[0]);
        BigInt_init(&tk->bi_val[1]);
        if (!cstr_BigRat(tk->bi_val, tk->v.p, NULL)) {
            throw_errorf(fs->mod_lang, "TokenError", "Decimal riteral out of range");
            add_stack_trace(tk->module, NULL, tk->v.line);
            tk->v.type = T_ERR;
        }
        *tk->v.u.end = c;
        break;
    }
    case TL_FLOAT: {
        char c = *tk->v.u.end;
        *tk->v.u.end = '\0';

        errno = 0;
        tk->real_val = strtod(tk->v.p, NULL);
        if (errno != 0) {
            throw_errorf(fs->mod_lang, "TokenError", "Float riteral out of range");
            add_stack_trace(tk->module, NULL, tk->v.line);
            tk->v.type = T_ERR;
        }
        *tk->v.u.end = c;
        break;
    }
    }
}
/**
 * トークンを先読みして、トークンの種類を返す
 * 先読み1の場合は、n = 0
 */
int Tok_peek(Tok *tk, int n)
{
    if (n < ((tk->prev_top - tk->prev_n) & (PREFETCH_MAX - 1))) {
        return tk->fetch[(tk->prev_n + n) & (PREFETCH_MAX - 1)].type;
    } else {
        TokValue *v = &tk->fetch[(tk->prev_n + n) & (PREFETCH_MAX - 1)];
        tk->prev_top = (tk->prev_n + n + 1) & (PREFETCH_MAX - 1);
        Tok_fetch(v, tk);
        if (v->type == T_ERR) {
            add_stack_trace(tk->module, NULL, v->line);
        }
        return v->type;
    }
}
/**
 * 改行を読み飛ばす
 */
void Tok_skip(Tok *tk)
{
    while (tk->v.type == T_NL) {
        Tok_next(tk);
    }
}
/**
 * 1つ進めてから改行を読み飛ばす
 */
void Tok_next_skip(Tok *tk)
{
    do {
        Tok_next(tk);
    } while (tk->v.type == T_NL);
}
RefStr *Tok_intern(Tok *tk)
{
    return intern(tk->str_val.p, tk->str_val.size);
}

void Tok_init(Tok *tk, RefNode *module, char *buf)
{
    tk->top = buf;
    tk->p = buf;
    tk->head_line = 1;
    tk->prev_id = FALSE;
    tk->parse_cls = NULL;
    tk->parse_genr = FALSE;
    tk->v.type = T_NL;
    tk->simple_mode = FALSE;
    tk->module = module;
    tk->ignore_nl = FALSE;

    tk->wait_colon = 0;
    tk->p_nest_n = -1;
    tk->p_nest = malloc(sizeof(int) * MAX_EXPR_STACK);

    tk->prev_n = 0;
    tk->prev_top = 0;

    Tok_next(tk);

    if (fv->cmp_dynamic) {
        tk->cmp_mem = NULL;
        tk->st_mem = NULL;
    } else {
        tk->cmp_mem = &fv->cmp_mem;
        tk->st_mem = &fg->st_mem;
    }
}
void Tok_close(Tok *tk)
{
    free(tk->p_nest);
    tk->p_nest = NULL;
}

#ifdef DEBUGGER

void Tok_init_continue(Tok *tk, char *buf)
{
    tk->top = buf;
    tk->p = buf;
}
int Tok_next_test(Tok *tk)
{
    Tok_fetch(&tk->v, tk);
    return FALSE;
}

#endif

void Tok_simple_init(Tok *tk, char *buf)
{
    memset(tk, 0, sizeof(Tok));
    tk->p = buf;
    tk->top = buf;
    tk->head_line = 1;
    tk->p_nest_n = -1;
    tk->simple_mode = TRUE;
}
void Tok_simple_next(Tok *tk)
{
    Tok_fetch(&tk->v, tk);

    // 値の変換が必要な項目のみ
    switch (tk->v.type) {
    case TL_BYTES:
    case TL_STR:
    case TL_CONST:
    case TL_CLASS:
    case TL_VAR:
        tk->str_val.p = tk->v.p;
        tk->str_val.size = tk->v.u.end - tk->v.p;
        break;
    case TL_REGEX:
        tk->str_val.p = tk->v.p;
        tk->str_val.size = tk->v.u.end - tk->v.p;
        tk->int_val = tk->v.u.flag;
        break;
    case TL_INT: {
        char c = *tk->v.u.end;
        long val;
        *tk->v.u.end = '\0';
        errno = 0;
        val = strtol(tk->v.p, NULL, tk->v.base);

        if (errno != 0 || val <= INT32_MIN || val > INT32_MAX) {
            throw_errorf(fs->mod_lang, "TokenError", "Int riteral out of range");
            add_stack_trace(tk->module, NULL, tk->v.line);
            tk->v.type = T_ERR;
        } else {
            tk->v.type = TL_INT;
            tk->int_val = (int32_t)val;
        }
        *tk->v.u.end = c;
        break;
    }
    case TL_FLOAT: {
        char c = *tk->v.u.end;
        *tk->v.u.end = '\0';

        errno = 0;
        tk->real_val = strtod(tk->v.p, NULL);
        if (errno != 0) {
            throw_errorf(fs->mod_lang, "TokenError", "Float riteral out of range");
            add_stack_trace(tk->module, NULL, tk->v.line);
            tk->v.type = T_ERR;
        }
        *tk->v.u.end = c;
        break;
    }
    }
}
