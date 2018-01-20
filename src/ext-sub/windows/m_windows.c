#define DEFINE_GLOBALS

#include "fox_windows.h"
#include "fox_io.h"

enum {
    INDEX_REGITER_PARENT,
    INDEX_REGITER_CUR,
    INDEX_REGITER_NUM,
};
enum {
    INDEX_NETRESOURCE_LOCALNAME,
    INDEX_NETRESOURCE_REMOTENAME,
    INDEX_NETRESOURCE_COMMENT,
    INDEX_NETRESOURCE_PROVIDER,
    INDEX_NETRESOURCE_NUM,
};

typedef struct {
    RefHeader rh;

    HKEY hroot;
    HKEY hkey;
    Value path;
    int write_mode;
} RefRegKey;

static RefNode *cls_regkey;
static RefNode *cls_winfile;
static RefNode *cls_netresource;


void throw_windows_error(const char *err_class, DWORD errcode)
{
    enum {
        MESSAGE_MAX = 512,
    };
    wchar_t buf[MESSAGE_MAX];
    char *cbuf;
    int len, cbuf_len;

    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errcode, LOCALE_NEUTRAL, buf, MESSAGE_MAX, NULL);
    cbuf_len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
    cbuf = malloc(cbuf_len + 4);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, cbuf, cbuf_len + 1, NULL, NULL);

    // 末尾の改行を削除
    len = strlen(cbuf);
    if (len >= 2 && cbuf[len - 2] == '\r' && cbuf[len - 1] == '\n') {
        cbuf[len - 2] = '\0';
    }

    fs->throw_errorf(mod_windows, err_class, "(%x) %s", errcode, cbuf);
    free(cbuf);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int head_equals(RefStr *s, const char *head, Str *path)
{
    int head_size = strlen(head);

    if (s->size == head_size) {
        if (memcmp(s->c, head, head_size) == 0) {
            path->p = NULL;
            path->size = 0;
            return TRUE;
        }
    } else if (s->size > head_size) {
        if (memcmp(s->c, head, head_size) == 0) {
            if (s->c[head_size] == SEP_C) {
                path->p = s->c + head_size + 1;
                path->size = s->size - head_size - 1;
                return TRUE;
            }
        }
    }
    return FALSE;
}

/**
 * in  : HKEY_CURRENT_USER\Software\Microsoft
 * out : HKEY <= HKEY_CURRENT_USER
 * out : path <= 'Software\Microsoft'
 */
static int regpath_split(HKEY *hkey, Str *path, RefStr *src)
{
    if (head_equals(src, "HKEY_CLASSES_ROOT", path)) {
        *hkey = HKEY_CLASSES_ROOT;
        return TRUE;
    } else if (head_equals(src, "HKEY_CURRENT_CONFIG", path)) {
        *hkey = HKEY_CURRENT_CONFIG;
        return TRUE;
    } else if (head_equals(src, "HKEY_CURRENT_USER", path)) {
        *hkey = HKEY_CURRENT_USER;
        return TRUE;
    } else if (head_equals(src, "HKEY_LOCAL_MACHINE", path)) {
        *hkey = HKEY_LOCAL_MACHINE;
        return TRUE;
    } else if (head_equals(src, "HKEY_USERS", path)) {
        *hkey = HKEY_USERS;
        return TRUE;
    }
    return FALSE;
}
static const char *regkey_to_cstr(HKEY hkey)
{
    if (hkey == HKEY_CLASSES_ROOT) {
        return "HKEY_CLASSES_ROOT";
    } else if (hkey == HKEY_CURRENT_CONFIG) {
        return "HKEY_CURRENT_CONFIG";
    } else if (hkey == HKEY_CURRENT_USER) {
        return "HKEY_CURRENT_USER";
    } else if (hkey == HKEY_LOCAL_MACHINE) {
        return "HKEY_LOCAL_MACHINE";
    } else if (hkey == HKEY_USERS) {
        return "HKEY_USERS";
    } else {
        return "";
    }
}

static int regkey_new(Value *vret, Value *v, RefNode *node)
{
    HKEY hroot, hkey;
    Str path;
    wchar_t *path_p;
    int write_mode = FALSE;
    int ret = 0;
    RefRegKey *r = fs->buf_new(cls_regkey, sizeof(RefRegKey));
    *vret = vp_Value(r);

    if (!regpath_split(&hroot, &path, Value_vp(v[1]))) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown registory key");
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        RefStr *mode = Value_vp(v[2]);
        if (str_eq(mode->c, mode->size, "w", -1)) {
            write_mode = TRUE;
        } else if (str_eq(mode->c, mode->size, "r", -1)) {
        } else {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown mode");
            return FALSE;
        }
    }

    path_p = cstr_to_utf16(path.p, path.size);
    if (write_mode) {
        ret = RegCreateKeyExW(hroot, path_p, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL);
    } else {
        ret = RegOpenKeyExW(hroot, path_p, 0, KEY_READ, &hkey);
    }
    if (ret != ERROR_SUCCESS) {
        r->hkey = INVALID_HANDLE_VALUE;
        free(path_p);
        throw_windows_error("RegistoryError", ret);
        return FALSE;
    }
    free(path_p);

    r->hroot = hroot;
    r->hkey = hkey;
    r->path = fs->cstr_Value(fs->cls_str, path.p, path.size);
    r->write_mode = write_mode;

    return TRUE;
}
static int regkey_tostr(Value *vret, Value *v, RefNode *node)
{
    RefRegKey *r = Value_vp(*v);
    *vret = fs->printf_Value("RegKey(%s\\%r)", regkey_to_cstr(r->hroot), Value_vp(r->path));
    return TRUE;
}
static int regkey_iterator(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_regiter = FUNC_VP(node);
    Ref *r = fs->ref_new(cls_regiter);
    *vret = vp_Value(r);

    r->v[INDEX_REGITER_PARENT] = fs->Value_cp(*v);
    r->v[INDEX_REGITER_CUR] = int32_Value(0);

    return TRUE;
}
static int regkey_close(Value *vret, Value *v, RefNode *node)
{
    RefRegKey *r = Value_vp(*v);

    if (r->hkey != INVALID_HANDLE_VALUE) {
        RegCloseKey(r->hkey);
        r->hkey = INVALID_HANDLE_VALUE;
    }

    return TRUE;
}
static int regkey_getvalue(Value *vret, Value *v, RefNode *node)
{
    RefRegKey *r = Value_vp(*v);
    RefStr *name = Value_vp(v[1]);
    wchar_t *value_name = cstr_to_utf16(name->c, name->size);
    DWORD type;
    DWORD data_size;
    uint8_t *buf;
    int ret = 0;

    ret = RegQueryValueExW(r->hkey, value_name, NULL, &type, NULL, &data_size);
    if (ret != 0) {
        goto ERROR_END;
    }

    data_size += 2;
    buf = malloc(data_size);

    ret = RegQueryValueExW(r->hkey, value_name, NULL, &type, buf, &data_size);
    if (ret != 0) {
        free(buf);
        goto ERROR_END;
    }
    switch (type) {
    case REG_BINARY:
        *vret = fs->cstr_Value(fs->cls_bytes, (const char*)buf, data_size);
        break;
    case REG_DWORD: {
        int32_t val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        *vret = int32_Value(val);
        break;
    }
    case REG_DWORD_BIG_ENDIAN: {
        int32_t val = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
        *vret = int32_Value(val);
        break;
    }
    case REG_QWORD: {
        int64_t val = (int64_t)buf[0] | ((int64_t)buf[1] << 8) | ((int64_t)buf[2] << 16) | ((int64_t)buf[3] << 24);
        val |= ((int64_t)buf[4] << 32) | ((int64_t)buf[5] << 40) | ((int64_t)buf[6] << 48) | ((int64_t)buf[7] << 56);
        *vret = fs->int64_Value(val);
        break;
    }
    case REG_SZ: {
        *vret = fs->wstr_Value(fs->cls_str, (wchar_t*)buf, -1);
        break;
    }
    case REG_MULTI_SZ: {
        RefArray *ra = fs->refarray_new(0);
        const wchar_t *wp = (const wchar_t*)buf;
        const wchar_t *end = wp + (data_size / sizeof(wchar_t));

        *vret = vp_Value(ra);
        while (*wp != L'\0' && wp < end) {
            Value *vp = fs->refarray_push(ra);
            *vp = fs->wstr_Value(fs->cls_str, wp, -1);
            wp += wcslen(wp) + 1;
        }
        break;
    }
    default:
        break;
    }
    free(buf);
    free(value_name);
    return TRUE;

ERROR_END:
    free(value_name);
    throw_windows_error("RegistoryError", ret);
    return FALSE;
}
static int regkey_setvalue_sub(HKEY hkey, RefStr *r_value, DWORD type, const void *buf, int size)
{
    wchar_t *value_name = cstr_to_utf16(r_value->c, r_value->size);
    int ret = RegSetValueEx(hkey, value_name, 0, type, (const BYTE*)buf, size);

    free(value_name);
    if (ret != 0) {
        throw_windows_error("RegistoryError", ret);
        return FALSE;
    }
    return TRUE;
}
static int regkey_setint(Value *vret, Value *v, RefNode *node)
{
    RefRegKey *r = Value_vp(*v);
    DWORD type = FUNC_INT(node);
    int size;
    int err;

    int64_t i64 = fs->Value_int64(v[2], &err);
    if (err) {
        goto ERROR_END;
    }
    if (type == REG_DWORD) {
        if (i64 < LONG_MIN || i64 > LONG_MAX) {
            goto ERROR_END;
        }
        size = 4;
    } else {
        size = 8;
    }
    if (!regkey_setvalue_sub(r->hkey, Value_vp(v[1]), type, &i64, size)) {
        return FALSE;
    }
    return TRUE;

ERROR_END:
    fs->throw_errorf(mod_windows, "RegistoryError", "%s out of range (-2^63 - 2^63-1)", type == REG_QWORD ? "QWORD" : "DWORD");
    return FALSE;
}
static int regkey_setstr(Value *vret, Value *v, RefNode *node)
{
    RefRegKey *r = Value_vp(*v);
    DWORD type = FUNC_INT(node);
    RefStr *r_value = Value_vp(v[2]);
    wchar_t *buf = cstr_to_utf16(r_value->c, r_value->size);

    if (!regkey_setvalue_sub(r->hkey, Value_vp(v[1]), type, buf, (wcslen(buf) + 1) * sizeof(wchar_t*))) {
        free(buf);
        return FALSE;
    }
    free(buf);

    return TRUE;
}
static int regkey_setmultistr(Value *vret, Value *v, RefNode *node)
{
    //TODO
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int regiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int32_t cur = Value_integral(r->v[INDEX_REGITER_CUR]);
    RefRegKey *r_p = Value_vp(r->v[INDEX_REGITER_PARENT]);
    HKEY hKey = r_p->hkey;

    for (;;) {
        wchar_t wbuf[MAX_PATH];
        DWORD wbuf_size = MAX_PATH;

        cur++;
        switch (RegEnumKeyExW(hKey, cur, wbuf, &wbuf_size, 0, NULL, NULL, NULL)) {
        case ERROR_SUCCESS: {
            //MessageBoxW(NULL, L"1", L"a", 0);
            int write_mode = r_p->write_mode;
            HKEY hKey2;
            int ret;
            RefStr *path_r = Value_vp(r_p->path);

            if (write_mode) {
                ret = RegCreateKeyExW(hKey, wbuf, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey2, NULL);
            } else {
                ret = RegOpenKeyExW(hKey, wbuf, 0, KEY_READ, &hKey2);
            }
            if (ret != ERROR_SUCCESS) {
                throw_windows_error("RegistoryError", ret);
                return FALSE;
            }

            if (wbuf[0] != '\0') {
                char *cbuf;
                RefRegKey *r2 = fs->buf_new(cls_regkey, sizeof(RefRegKey));
                *vret = vp_Value(r2);

                r2->hroot = r_p->hroot;
                r2->hkey = r_p->hkey;
                r2->write_mode = write_mode;

                cbuf = utf16_to_cstr(wbuf, -1);
                r2->path = fs->printf_Value("%r\\%s", path_r, cbuf);
                free(cbuf);
                r->v[INDEX_REGITER_CUR] = int32_Value(cur);
                return TRUE;
            }
            break;
        }
        case ERROR_NO_MORE_ITEMS:
            //MessageBoxW(NULL, L"2", L"a", 0);
            r->v[INDEX_REGITER_CUR] = int32_Value(cur);
            fs->throw_stopiter();
            return FALSE;
        default:
            //MessageBoxW(NULL, L"3", L"a", 0);
            break;
        }
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

enum {
    WINFILE_CREATE_TIME,
    WINFILE_MODIFIED_TIME,
    WINFILE_ACCESS_TIME,
};

static int winfile_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs;

    fs->Value_push("rv", fs->cls_file, v[1]);
    if (!fs->call_member_func(fs->str_new, 1, TRUE)) {
        return FALSE;
    }
    *vret = fg->stk_top[-1];
    fg->stk_top--;

    rs = Value_vp(*vret);
    rs->rh.type = cls_winfile;

    return TRUE;
}
static int winfile_get_time(Value *vret, Value *v, RefNode *node)
{
    int64_t timeval;
    RefStr *rs = Value_vp(*v);
    wchar_t *wtmp = filename_to_utf16(rs->c, NULL);
    HANDLE hFile = CreateFileW(wtmp, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wtmp);

    if (hFile == INVALID_HANDLE_VALUE) {
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(rs->c, rs->size));
        return FALSE;
    } else {
        FILETIME ftm;
        int64_t i64;
        int result = 0;
        switch (FUNC_INT(node)) {
        case WINFILE_CREATE_TIME:
            result = GetFileTime(hFile, &ftm, NULL, NULL);
            break;
        //case WINFILE_MODIFIED_TIME:
        //    result = GetFileTime(hFile, NULL, NULL, &ftm);
        //    break;
        case WINFILE_ACCESS_TIME:
            result = GetFileTime(hFile, NULL, &ftm, NULL);
            break;
        }
        CloseHandle(hFile);
        if (result == 0) {
            fs->throw_errorf(fs->mod_io, "ReadError", "Cannot read file time %Q", Str_new(rs->c, rs->size));
            return FALSE;
        }

        i64 = (uint64_t)ftm.dwLowDateTime | ((uint64_t)ftm.dwHighDateTime << 32);
        // 1601年1月1日から 100ナノ秒間隔でカウント
        // 1601-01-01から1969-01-01までの秒数を引く
        timeval = i64 / 10000 - 11644473600000LL;
    }
    {
        RefInt64 *tm = fs->buf_new(fs->cls_timestamp, sizeof(RefInt64));
        tm->u.i = timeval;
        *vret = vp_Value(tm);
    }

    return TRUE;
}
static int winfile_set_time(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(*v);
    wchar_t *wtmp = filename_to_utf16(rs->c, NULL);
    HANDLE hFile = CreateFileW(wtmp, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wtmp);

    if (hFile == INVALID_HANDLE_VALUE) {
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(rs->c, rs->size));
        return FALSE;
    } else {
        RefInt64 *i64 = Value_vp(v[1]);
        // 1601年1月1日から 100ナノ秒間隔でカウント
        int64_t timeval = (i64->u.i + 11644473600000LL) * 10000;
        FILETIME ftm;
        int result = 0;

        ftm.dwLowDateTime = (DWORD)(timeval & 0xffffFFFF);
        ftm.dwHighDateTime = (DWORD)(timeval >> 32);

        switch (FUNC_INT(node)) {
        case WINFILE_CREATE_TIME:
            result = SetFileTime(hFile, &ftm, NULL, NULL);
            break;
        case WINFILE_MODIFIED_TIME:
            result = SetFileTime(hFile, NULL, NULL, &ftm);
            break;
        case WINFILE_ACCESS_TIME:
            result = SetFileTime(hFile, NULL, &ftm, NULL);
            break;
        }
        CloseHandle(hFile);
        if (result == 0) {
            fs->throw_errorf(fs->mod_io, "WriteError", "Cannot write file time %Q", Str_new(rs->c, rs->size));
            return FALSE;
        }
    }
    return TRUE;
}
static int winfile_get_attr(Value *vret, Value *v, RefNode *node)
{
    DWORD mask = FUNC_INT(node);
    RefStr *rs = Value_vp(*v);
    wchar_t *wtmp = filename_to_utf16(rs->c, NULL);
    DWORD attr = GetFileAttributes(wtmp);
    free(wtmp);

    if (attr == (DWORD)-1) {
        fs->throw_errorf(fs->mod_io, "ReadError", "Cannot read file attribute %Q", Str_new(rs->c, rs->size));
        return FALSE;
    } else {
        *vret = bool_Value((attr & mask) != 0);
    }

    return TRUE;
}
static int winfile_set_attr(Value *vret, Value *v, RefNode *node)
{
    DWORD mask = FUNC_INT(node);
    RefStr *rs = Value_vp(*v);
    wchar_t *wtmp = filename_to_utf16(rs->c, NULL);
    DWORD attr = GetFileAttributes(wtmp);

    if (attr == (DWORD)-1) {
        goto ERROR_END;
    } else {
        if (attr == FILE_ATTRIBUTE_NORMAL) {
            attr = 0;
        }
        if (Value_bool(v[1])) {
            attr |= mask;
        } else {
            attr &= ~mask;
        }
        if (attr == 0) {
            attr = FILE_ATTRIBUTE_NORMAL;
        }
        if (SetFileAttributes(wtmp, attr) == 0) {
            goto ERROR_END;
        }
    }
    free(wtmp);

    return TRUE;

ERROR_END:
    free(wtmp);
    fs->throw_errorf(fs->mod_io, "WriteError", "Cannot write file time %Q", Str_new(rs->c, rs->size));
    return FALSE;
}
static int winfile_to_short_path(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(*v);
    wchar_t *wsrc = filename_to_utf16(rs->c, NULL);
    int dstlen = wcslen(wsrc) + 4;
    wchar_t *wdst = malloc(dstlen * sizeof(wchar_t));

    int result = GetShortPathNameW(wsrc, wdst, dstlen);

    if (result != 0) {
        int offset = (wdst[0] == '\\' && wdst[1] == '\\' && wdst[2] == '?' && wdst[3] == '\\') ? 4 : 0;
        *vret = fs->wstr_Value(cls_winfile, wdst + offset, -1);
    } else {
        fs->throw_errorf(fs->mod_io, "ReadError", "Cannot read short path %Q", Str_new(rs->c, rs->size));
    }

    free(wsrc);
    free(wdst);

    return result != 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int get_netresources(Value *vret, Value *v, RefNode *node)
{
    enum {
        NETRESOURCE_NUM = 8192,
    };
    HANDLE hNet;
    NETRESOURCEW *res;
    RefArray *ra;

    if (WNetOpenEnumW(RESOURCE_CONTEXT, RESOURCETYPE_ANY, 0, NULL, &hNet) != 0) {
        throw_windows_error("WNetError", GetLastError());
        return FALSE;
    }

    ra = fs->refarray_new(0);
    *vret = vp_Value(ra);
    res = malloc(sizeof(NETRESOURCEW) * NETRESOURCE_NUM);

    for (;;) {
        DWORD size = sizeof(NETRESOURCEW) * NETRESOURCE_NUM;
        DWORD count = 0xffffFFFF;

        if (WNetEnumResourceW(hNet, &count, res, &size) != NO_ERROR) {
            break;
        }
        for (int i = 0; i < (int)count; i++) {
            NETRESOURCEW *p = &res[i];
            if (p->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER){
                Value *vp = fs->refarray_push(ra);
                Ref *r = fs->ref_new(cls_netresource);
                *vp = vp_Value(r);

                if (p->lpLocalName != NULL) {
                    r->v[INDEX_NETRESOURCE_LOCALNAME] = fs->wstr_Value(fs->cls_str, p->lpLocalName, -1);
                }
                if (p->lpRemoteName != NULL) {
                    r->v[INDEX_NETRESOURCE_REMOTENAME] = fs->wstr_Value(fs->cls_str, p->lpRemoteName, -1);
                }
                if (p->lpComment != NULL) {
                    r->v[INDEX_NETRESOURCE_COMMENT] = fs->wstr_Value(fs->cls_str, p->lpComment, -1);
                }
                if (p->lpProvider != NULL) {
                    r->v[INDEX_NETRESOURCE_PROVIDER] = fs->wstr_Value(fs->cls_str, p->lpProvider, -1);
                }
            }
        }
    }
    WNetCloseEnum(hNet);
    free(res);

    return TRUE;
}

static int netresource_get(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    *vret = fs->Value_cp(r->v[FUNC_INT(node)]);
    return TRUE;
}
static int netresource_get_shared_disks(Value *vret, Value *v, RefNode *node)
{
    enum {
        NETRESOURCE_NUM = 8192,
    };
    HANDLE hNet;
    NETRESOURCEW *res;
    RefArray *ra;
    Ref *r = Value_vp(*v);
    const char *c_remote_name = Value_cstr(r->v[INDEX_NETRESOURCE_REMOTENAME]);
    NETRESOURCEW nr;
    wchar_t remote_name[256];

    if (utf8_to_utf16(NULL, c_remote_name, -1) >= 256) {
        fs->throw_errorf(mod_windows, "WNetError", "Cannot find NetResource(%s)", c_remote_name);
        return FALSE;
    }

    memset(&nr, 0, sizeof(NETRESOURCEW));
    nr.dwScope = RESOURCE_GLOBALNET;
    nr.dwType = RESOURCETYPE_DISK;
    nr.dwUsage = RESOURCEUSAGE_CONTAINER;
    utf8_to_utf16(remote_name, c_remote_name, -1);
    nr.lpRemoteName = remote_name;

    if (WNetOpenEnumW(RESOURCE_GLOBALNET, RESOURCETYPE_ANY, 0, &nr, &hNet) != 0) {
        throw_windows_error("WNetError", GetLastError());
        return FALSE;
    }

    ra = fs->refarray_new(0);
    *vret = vp_Value(ra);
    res = malloc(sizeof(NETRESOURCEW) * NETRESOURCE_NUM);

    for (;;) {
        DWORD size = sizeof(NETRESOURCEW) * NETRESOURCE_NUM;
        DWORD count = 0xffffFFFF;

        if (WNetEnumResourceW(hNet, &count, res, &size) != NO_ERROR) {
            break;
        }
        for (int i = 0; i < (int)count; i++) {
            NETRESOURCEW *p = &res[i];
            if (p->dwType == RESOURCETYPE_DISK && p->dwDisplayType == RESOURCEDISPLAYTYPE_SHARE) {
                if (p->lpRemoteName != NULL) {
                    Value *vp = fs->refarray_push(ra);
                    *vp = fs->wstr_Value(cls_winfile, p->lpRemoteName, -1);
                }
            }
        }
    }
    WNetCloseEnum(hNet);
    free(res);

    return TRUE;
}
static int netresource_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_vp(*v);
    *vret = fs->printf_Value("NetResource(%v)", r->v[INDEX_NETRESOURCE_REMOTENAME]);
    return TRUE;
}
static int output_debug_string(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    wchar_t *wstr = cstr_to_utf16(rs->c, rs->size);
    OutputDebugStringW(wstr);
    free(wstr);
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int get_timezone(Value *vret, Value *v, RefNode *node)
{
    *vret = fs->Value_cp(vp_Value(default_tz));
    return TRUE;
}
static int set_timezone(Value *vret, Value *v, RefNode *node)
{
    RefTimeZone *tz = Value_vp(v[1]);
    fs->unref(vp_Value(default_tz));
    fs->addref(vp_Value(tz));
    default_tz = tz;
    return TRUE;
}

static int windows_dispose(Value *vret, Value *v, RefNode *node)
{
    OleUninitialize();
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_int(RefNode *m, const char *name, int i)
{
    RefNode *n = fs->define_identifier(m, m, name, NODE_CONST, 0);
    n->u.k.val = int32_Value(i);
}
static void define_const(RefNode *m)
{
#define DEFINE_INT(s) define_int(m, #s, s)

    DEFINE_INT(IDOK);
    DEFINE_INT(IDCANCEL);
    DEFINE_INT(IDABORT);
    DEFINE_INT(IDRETRY);
    DEFINE_INT(IDIGNORE);
    DEFINE_INT(IDYES);
    DEFINE_INT(IDNO);
    DEFINE_INT(IDCLOSE);
    DEFINE_INT(IDHELP);
    DEFINE_INT(IDTRYAGAIN);
    DEFINE_INT(IDCONTINUE);

    DEFINE_INT(MB_OK);
    DEFINE_INT(MB_OKCANCEL);
    DEFINE_INT(MB_ABORTRETRYIGNORE);
    DEFINE_INT(MB_YESNOCANCEL);
    DEFINE_INT(MB_YESNO);
    DEFINE_INT(MB_RETRYCANCEL);
    DEFINE_INT(MB_OK);

    DEFINE_INT(MB_ICONERROR);
    DEFINE_INT(MB_ICONHAND);
    DEFINE_INT(MB_ICONSTOP);
    DEFINE_INT(MB_ICONQUESTION);
    DEFINE_INT(MB_ICONINFORMATION);

    DEFINE_INT(CP_ACP);
    DEFINE_INT(CP_UTF8);
#undef DEFINE_INT
}

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier_p(m, m, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, windows_dispose, 0, 0, NULL);

    n = fs->define_identifier(m, m, "set_timezone", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, set_timezone, 1, 1, NULL, fs->cls_timezone);

    n = fs->define_identifier(m, m, "get_timezone", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_timezone, 0, 0, NULL);

    n = fs->define_identifier(m, m, "get_netresources", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_netresources, 0, 0, NULL);

    n = fs->define_identifier(m, m, "debug_log", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, output_debug_string, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier(m, m, "cpio_tobytes", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cpio_tobytes, 2, 2, NULL, fs->cls_str, fs->cls_int);
    n = fs->define_identifier(m, m, "cpio_tostr", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cpio_tostr, 2, 2, NULL, fs->cls_bytes, fs->cls_int);
}

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;
    RefNode *cls_regiter;
    RefNode *cls_oleenum;
    RefNode *cls_codepageio;

    cls_oleobject = fs->define_identifier(m, m, "OLEObject", NODE_CLASS, 0);
    cls_oleenum = fs->define_identifier(m, m, "OLEEnumerator", NODE_CLASS, 0);
    cls_regkey = fs->define_identifier(m, m, "RegKey", NODE_CLASS, 0);
    cls_regiter = fs->define_identifier(m, m, "RegKeyIterator", NODE_CLASS, 0);
    cls_winfile = fs->define_identifier(m, m, "WinFile", NODE_CLASS, 0);
    cls_netresource = fs->define_identifier(m, m, "NetResource", NODE_CLASS, 0);
    cls_codepageio = fs->define_identifier(m, m, "CodePageIO", NODE_CLASS, 0);

    cls = cls_oleobject;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, ole_new, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ole_close, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_method_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ole_invoke_method, 0, -1, (void*)(DISPATCH_METHOD|DISPATCH_PROPERTYGET));
    n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ole_invoke_method, 1, 1, (void*)(DISPATCH_METHOD|DISPATCH_PROPERTYGET), NULL);
    n = fs->define_identifier_p(m, cls, fs->str_missing_set, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ole_invoke_method, 2, 2, (void*)(DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF), NULL, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ole_invoke_method, 1, 1, (void*)(DISPATCH_METHOD|DISPATCH_PROPERTYGET), NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ole_invoke_method, 2, 2, (void*)(DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF), NULL, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ole_get_iterator, 0, 0, cls_oleenum);

    fs->extends_method(cls, fs->cls_obj);


    cls = cls_oleenum;
    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, oleenum_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, oleenum_next, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_iterator);


    cls = cls_regkey;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, regkey_new, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_dtor, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_close, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_iterator, 0, 0, cls_regiter);

    n = fs->define_identifier(m, cls, "get", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_getvalue, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "set_sz", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_setstr, 2, 2, (void*)REG_SZ, fs->cls_str, fs->cls_str);
    n = fs->define_identifier(m, cls, "set_expand_sz", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_setstr, 2, 2, (void*)REG_EXPAND_SZ, fs->cls_str, fs->cls_str);
    n = fs->define_identifier(m, cls, "set_multi_sz", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_setmultistr, 2, 2, NULL, fs->cls_str, fs->cls_list);
    n = fs->define_identifier(m, cls, "set_dword", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_setint, 2, 2, (void*)REG_DWORD, fs->cls_str, fs->cls_int);
    n = fs->define_identifier(m, cls, "set_qword", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regkey_setint, 2, 2, (void*)REG_QWORD, fs->cls_str, fs->cls_int);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_regiter;
    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, regiter_next, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_REGITER_NUM;
    fs->extends_method(cls, fs->cls_iterator);


    cls = cls_winfile;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, winfile_new, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier(m, cls, "ctime", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, winfile_get_time, 0, 0, (void*)WINFILE_CREATE_TIME);
    n = fs->define_identifier(m, cls, "atime", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, winfile_get_time, 0, 0, (void*)WINFILE_ACCESS_TIME);
    n = fs->define_identifier(m, cls, "ctime=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_time, 1, 1, (void*)WINFILE_CREATE_TIME, fs->cls_timestamp);
    n = fs->define_identifier(m, cls, "mtime=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_time, 1, 1, (void*)WINFILE_MODIFIED_TIME, fs->cls_timestamp);
    n = fs->define_identifier(m, cls, "atime=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_time, 1, 1, (void*)WINFILE_ACCESS_TIME, fs->cls_timestamp);

    n = fs->define_identifier(m, cls, "archive", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, winfile_get_attr, 0, 0, (void*)FILE_ATTRIBUTE_ARCHIVE);
    n = fs->define_identifier(m, cls, "compressed", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, winfile_get_attr, 0, 0, (void*)FILE_ATTRIBUTE_COMPRESSED);
    n = fs->define_identifier(m, cls, "encrypted", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, winfile_get_attr, 0, 0, (void*)FILE_ATTRIBUTE_ENCRYPTED);
    n = fs->define_identifier(m, cls, "hidden", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, winfile_get_attr, 0, 0, (void*)FILE_ATTRIBUTE_HIDDEN);
    n = fs->define_identifier(m, cls, "readonly", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, winfile_get_attr, 0, 0, (void*)FILE_ATTRIBUTE_READONLY);

    n = fs->define_identifier(m, cls, "archive=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_attr, 1, 1, (void*)FILE_ATTRIBUTE_ARCHIVE, fs->cls_bool);
    n = fs->define_identifier(m, cls, "compressed=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_attr, 1, 1, (void*)FILE_ATTRIBUTE_COMPRESSED, fs->cls_bool);
    n = fs->define_identifier(m, cls, "encrypted=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_attr, 1, 1, (void*)FILE_ATTRIBUTE_ENCRYPTED, fs->cls_bool);
    n = fs->define_identifier(m, cls, "hidden=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_attr, 1, 1, (void*)FILE_ATTRIBUTE_HIDDEN, fs->cls_bool);
    n = fs->define_identifier(m, cls, "readonly=", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_set_attr, 1, 1, (void*)FILE_ATTRIBUTE_READONLY, fs->cls_bool);

    n = fs->define_identifier(m, cls, "to_short_path", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, winfile_to_short_path, 0, 0, NULL);

    fs->extends_method(cls, fs->cls_file);


    cls = cls_netresource;
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, netresource_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier(m, cls, "localname", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, netresource_get, 0, 0, (void*)INDEX_NETRESOURCE_LOCALNAME);
    n = fs->define_identifier(m, cls, "remotename", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, netresource_get, 0, 0, (void*)INDEX_NETRESOURCE_REMOTENAME);
    n = fs->define_identifier(m, cls, "comment", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, netresource_get, 0, 0, (void*)INDEX_NETRESOURCE_COMMENT);
    n = fs->define_identifier(m, cls, "provider", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, netresource_get, 0, 0, (void*)INDEX_NETRESOURCE_PROVIDER);
    n = fs->define_identifier(m, cls, "shared_disks", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, netresource_get_shared_disks, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_NETRESOURCE_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_codepageio;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, codepageio_new, 2, 2, cls_codepageio, fs->cls_streamio, fs->cls_int);

    n = fs->define_identifier(m, cls, "gets", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, codepageio_gets, 0, 0, (void*)TEXTIO_M_GETS);
    n = fs->define_identifier(m, cls, "getln", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, codepageio_gets, 0, 0, (void*)TEXTIO_M_GETLN);
    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, codepageio_gets, 0, 0, (void*)TEXTIO_M_NEXT);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, codepageio_write, 0, -1, NULL);
    n = fs->define_identifier(m, cls, "codepage", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, codepageio_codepage, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_CODEPAGEIO_NUM;
    fs->extends_method(cls, fs->cls_textio);


    cls = fs->define_identifier(m, m, "OLEError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);

    cls = fs->define_identifier(m, m, "RegistoryError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);

    cls = fs->define_identifier(m, m, "WNetError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_windows = m;

    OleInitialize(0);
    define_class(m);
    define_const(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\n";
}
