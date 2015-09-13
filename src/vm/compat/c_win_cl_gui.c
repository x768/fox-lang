#include "compat.h"
#include "fox_parse.h"
#include <windows.h>

/**
 * OSで設定されている地域と言語を取得
 */
const char *get_default_locale(void)
{
    static const char *def_locale = NULL;
    if (def_locale == NULL) {
        wchar_t buf[18];

        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, buf, 8);
        wcscat(buf, L"_");
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buf + wcslen(buf), 8);
        def_locale = utf16to8(buf);
    } else {
        def_locale = "(neutral)";
    }
    return def_locale;
}


/////////////////////////////////////////////////////////////////////////////////

enum {
    MAX_NAME_LEN = 64,
};

static void copy16to8(char *dst, const wchar_t *src)
{
    int i;
    for (i = 0; src[i] != L'\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}
static void windows_tz_to_tzid(char *buf, int max, Str name)
{
    Tok tk;
    char *path = path_normalize(NULL, fs->fox_home, "data" SEP_S "windows-tz.txt", -1, NULL);
    char *ini_buf = read_from_file(NULL, path, NULL);

    free(path);
    if (buf == NULL) {
        goto ERROR_END;
    }

    Tok_simple_init(&tk, ini_buf);
    Tok_simple_next(&tk);

    for (;;) {
        switch (tk.v.type) {
        case TL_STR:
            if (str_eq(tk.str_val.p, tk.str_val.size, name.p, name.size)) {
                Tok_simple_next(&tk);
                if (tk.v.type != T_LET) {
                    goto ERROR_END;
                }
                Tok_simple_next(&tk);
                if (tk.v.type != TL_STR) {
                    goto ERROR_END;
                }
                memcpy(buf, tk.str_val.p, tk.str_val.size);
                buf[tk.str_val.size] = '\0';
                free(ini_buf);
                return;
            } else {
                // 行末まで飛ばす
                while (tk.v.type == T_LET || tk.v.type == TL_STR) {
                    Tok_simple_next(&tk);
                }
            }
            break;
        case T_NL:
        case T_SEMICL:
            Tok_simple_next(&tk);
            break;
        case T_EOF:
            goto ERROR_END;
        default:
            fatal_errorf("Parse error at line %d (windows-tz.txt)", tk.v.line);
            break;
        }
    }

ERROR_END:
    strcpy(buf, "Etc/UTC");
    free(ini_buf);
}
static void convert_english_tzname(char *dst, const wchar_t *src)
{
    const wchar_t *path = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones";
    HKEY hKey;
    int i;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        strcpy(dst, "UTC");
        return;
    }
    for (i = 0; ; i++) {
        wchar_t wbuf[MAX_NAME_LEN];
        DWORD wbuf_size = MAX_NAME_LEN;

        switch (RegEnumKeyExW(hKey, i, wbuf, &wbuf_size, 0, NULL, NULL, NULL)) {
        case ERROR_SUCCESS: {
            HKEY hKey2;
            if (RegOpenKeyExW(hKey, wbuf, 0, KEY_READ, &hKey2) == ERROR_SUCCESS) {
                wchar_t wbuf2[MAX_NAME_LEN];
                DWORD wbuf2_size = MAX_NAME_LEN * sizeof(wchar_t);
                DWORD type;

                if (RegQueryValueExW(hKey2, L"Std", NULL, &type, (BYTE*)wbuf2, &wbuf2_size) == ERROR_SUCCESS) {
                    if (type == REG_SZ && wcscmp(wbuf2, src) == 0) {
                        copy16to8(dst, wbuf);
                        RegCloseKey(hKey2);
                        goto BREAK;
                    }
                }
                RegCloseKey(hKey2);
            }
            break;
        }
        case ERROR_NO_MORE_ITEMS:
            strcpy(dst, "UTC");
            goto BREAK;
        default:
            break;
        }
    }
BREAK:
    RegCloseKey(hKey);
}

void get_local_timezone_name(char *buf, int max)
{
    char cbuf[MAX_NAME_LEN];
    TIME_ZONE_INFORMATION tzi;
    memset(&tzi, 0, sizeof(tzi));

    GetTimeZoneInformation(&tzi);
    convert_english_tzname(cbuf, tzi.StandardName);
    windows_tz_to_tzid(buf, max, Str_new(cbuf, -1));
}

//////////////////////////////////////////////////////////////////////////////////////////////

static const char **get_cmd_args(int *pargc)
{
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), pargc);
    int argc = *pargc;
    const char **argv = Mem_get(&fg->st_mem, sizeof(char*) * (argc + 1));
    int i;

    for (i = 0; i < argc; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        char *p = Mem_get(&fg->st_mem, len + 1);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, p, len + 1, NULL, NULL);
        argv[i] = p;
    }
    argv[argc] = NULL;

    return argv;
}
