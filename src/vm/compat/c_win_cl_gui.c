#include "compat.h"
#include "fox.h"
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
static int load_windows_tz_file(IniTok *tk)
{
    int ret;
    StrBuf buf;

    StrBuf_init(&buf, fs->fox_home->size + 20);
    StrBuf_printf(&buf, "%r" SEP_S "data" SEP_S "windows-tz.txt", fs->fox_home);
    StrBuf_add_c(&buf, '\0');

    ret = IniTok_load(tk, buf.p);
    StrBuf_close(&buf);

    return ret;
}
static void windows_tz_to_tzid(char *buf, int max, Str name)
{
    IniTok tk;

    if (!load_windows_tz_file(&tk)) {
        goto ERROR_END;
    }

    while (IniTok_next(&tk)) {
        if (tk.type == INITYPE_STRING && str_eq(tk.key.p, tk.key.size, name.p, name.size)) {
            int len = tk.val.size;
            if (len >= max) {
                len = max - 1;
            }
            memcpy(buf, tk.val.p, len);
            buf[len] = '\0';
            return;
        }
    }

ERROR_END:
    strcpy(buf, "Etc/UTC");
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

RefCharset *get_console_charset(void)
{
    RefCharset *cs = get_charset_from_cp(GetConsoleCP());
    if (cs != NULL) {
        return cs;
    }
    return fs->cs_utf8;
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
