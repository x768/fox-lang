#define DEFINE_GLOBALS

#define UNICODE
#define WINVER 0x500

#include "fox.h"
#include "m_number.h"
#include <string.h>
#include <limits.h>

#ifndef WIN32
#error This module is only for win32
#endif

#include <windows.h>


enum {
    INDEX_REGITER_PARENT,
    INDEX_REGITER_CUR,
    INDEX_REGITER_NUM,
};

typedef struct {
    RefHeader rh;

    int valid;
    IDispatch *pdisp;
} RefOleObject;

typedef struct {
    RefHeader rh;

    int valid;
    IEnumVARIANT *penum;
    VARIANT current;
} RefOleEnumerator;

typedef struct {
    RefHeader rh;

    HKEY hroot;
    HKEY hkey;
    Value path;
    int write_mode;
} RefRegKey;

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_windows;
static RefNode *cls_oleobject;
static RefNode *cls_oleenum;
static RefNode *cls_regkey;
static RefTimeZone *default_tz;


static BSTR cstr_to_BSTR(const char *src_p, int src_size)
{
    int wlen = utf8_to_utf16(NULL, src_p, src_size);
    BSTR bstr = SysAllocStringLen(NULL, wlen + 1);
    utf8_to_utf16(bstr, src_p, src_size);
    return bstr;
}
static Value BSTR_Value(BSTR bstr)
{
    if (bstr != NULL) {
        int blen = wcslen((wchar_t*)bstr);
        int len = utf16_to_utf8(NULL, (wchar_t*)bstr, blen);
        RefStr *rs = fs->refstr_new_n(fs->cls_str, len);
        utf16_to_utf8(rs->c, (wchar_t*)bstr, blen);
        return vp_Value(rs);
    } else {
        return vp_Value(fs->str_0);
    }
}
static Value wstr_Value(const wchar_t *wstr)
{
    int wlen = wcslen(wstr);
    int len = utf16_to_utf8(NULL, wstr, wlen);
    RefStr *rs = fs->refstr_new_n(fs->cls_str, len);
    utf16_to_utf8(rs->c, wstr, wlen);
    return vp_Value(rs);
}
static Value oledate_Value(double dt)
{
    SYSTEMTIME st;
    FILETIME ft, ftm;
    int64_t i_tm;

    VariantTimeToSystemTime(dt, &st);
    SystemTimeToFileTime(&st, &ft);
    LocalFileTimeToFileTime(&ft, &ftm);  // おそらくローカル時間なので、UTCに戻す

    i_tm = (uint64_t)ftm.dwLowDateTime | ((uint64_t)ftm.dwHighDateTime << 32);
    // 1601年1月1日から 100ナノ秒間隔でカウント
    // 1601-01-01から1969-01-01までの秒数を引く
    i_tm = i_tm / 10000 - 11644473600000LL;

    return fs->time_Value(i_tm, default_tz);
}
static double Value_oledate(Value v)
{
    FILETIME ft;
    SYSTEMTIME st;
    double dt;
    int64_t i_tm = fs->Value_timestamp(v, default_tz);

    // 1601年1月1日から 100ナノ秒間隔でカウント
    // 1601-01-01から1969-01-01までの秒数を引く
    i_tm = (i_tm + 11644473600000LL) * 10000;
    ft.dwLowDateTime = i_tm;
    ft.dwHighDateTime = i_tm >> 32;

    FileTimeToSystemTime(&ft, &st);
    SystemTimeToVariantTime(&st, &dt);
    return dt;
}
/**
 * typeof(v) == Fracに限る
 * vをfactor倍してvalに入れる
 * 失敗したらreturn false
 */
static int Value_frac10(int64_t *val, Value v, int factor)
{
    RefFrac *md = Value_vp(v);
    BigInt mp, mp_fac;
    int ret;

    fs->BigInt_init(&mp);
    fs->BigInt_init(&mp_fac);

    fs->BigInt_copy(&mp, &md->bi[0]);
    fs->int64_BigInt(&mp_fac, factor);
    fs->BigInt_mul(&mp, &mp, &mp_fac);
    fs->BigInt_divmod(&mp, NULL, &mp, &md->bi[1]);

    ret = fs->BigInt_int64(&mp, val);

    fs->BigInt_close(&mp);
    fs->BigInt_close(&mp_fac);
    return ret;
}
/**
 * return val / factor
 * ex) val = 123, factor = 100 -> 1.23d
 */
static Value frac10_Value(int64_t val, int factor)
{
    RefFrac *md = fs->buf_new(fs->cls_frac, sizeof(RefFrac));

    fs->BigInt_init(&md->bi[0]);
    fs->BigInt_init(&md->bi[1]);
    fs->int64_BigInt(&md->bi[0], val);
    fs->int64_BigInt(&md->bi[1], factor);
    fs->BigRat_fix(md->bi);

    return vp_Value(md);
}

static void throw_windows_error(const char *err_class, DWORD errcode)
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
static void throw_ole_error(const char *err_class, EXCEPINFO *excep)
{
    DWORD code;
    char *src = NULL;
    char *desc = NULL;

    if (excep->pfnDeferredFillIn != NULL) {
        excep->pfnDeferredFillIn(excep);
    }
    if (excep->wCode != 0) {
        code = excep->wCode;
    } else {
        code = excep->scode;
    }

    if (excep->bstrSource != NULL) {
        src = utf16_to_cstr(excep->bstrSource, -1);
    }
    if (excep->bstrDescription != NULL) {
        desc = utf16_to_cstr(excep->bstrDescription, -1);
    }

    fs->throw_errorf(mod_windows, err_class, "(%x) %s %s", code, (src != NULL ? src : "<Unknown>"), (desc != NULL ? desc : ""));
    free(src);
    free(desc);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int ole_new(Value *vret, Value *v, RefNode *node)
{
    CLSID clsid;
    IDispatch *pdisp;
    RefStr *name = Value_vp(v[1]);
    BSTR bname = cstr_to_BSTR(name->c, name->size);
    HRESULT hr = CLSIDFromProgID(bname, &clsid);

    RefOleObject *ole = fs->buf_new(cls_oleobject, sizeof(RefOleObject));
    *vret = vp_Value(ole);

    if (FAILED(hr)) {
        hr = CLSIDFromString(bname, &clsid);
    }
    if (FAILED(hr)) {
        goto ERROR_END;
    }
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, &IID_IDispatch, (void **)&pdisp);
    if (FAILED(hr)) {
        goto ERROR_END;
    }

    SysFreeString(bname);
    ole->valid = TRUE;
    ole->pdisp = pdisp;
    return TRUE;

ERROR_END:
    SysFreeString(bname);

    fs->throw_errorf(mod_windows, "OLEError", "Cannot create object %S", name);
    return FALSE;
}
static int ole_close(Value *vret, Value *v, RefNode *node)
{
    RefOleObject *ole = Value_vp(*v);

    if (ole->valid) {
        IDispatch *p = ole->pdisp;
        p->lpVtbl->Release(p);
        ole->valid = FALSE;
    }
    return TRUE;
}

static int fox_value_to_variant(VARIANT *va, Value v, Mem *mem, Hash *hash);

static int fox_value_to_safearray(VARIANT *va, Value v, Mem *mem, Hash *hash)
{
    SAFEARRAY *sa = NULL;
    SAFEARRAYBOUND bound;
    VARIANT *p_va;
    RefArray *ra = Value_vp(v);
    int i;
    int result = TRUE;
    HashEntry *he;

    bound.lLbound = 0;
    bound.cElements = ra->size;
    sa = SafeArrayCreate(VT_VARIANT, 1, &bound);

    SafeArrayLock(sa);
    p_va = (VARIANT*)sa->pvData;

    he = fs->Hash_get_add_entry(hash, mem, (RefStr*)ra);
    if (he->p != NULL) {
        fs->throw_error_select(THROW_LOOP_REFERENCE);
        return FALSE;
    } else {
        he->p = (void*)1;
    }

    for (i = 0; i < ra->size; i++) {
        if (!fox_value_to_variant(&p_va[i], ra->p[i], mem, hash)) {
            result = TRUE;
            break;
        }
    }

    he->p = NULL;

    SafeArrayUnlock(sa);
    va->vt = VT_ARRAY|VT_VARIANT;
    va->parray = sa;

    return result;
}
static int fox_value_to_variant(VARIANT *va, Value v, Mem *mem, Hash *hash)
{
    RefNode *type = fs->Value_type(v);

    if (type == fs->cls_str) {
        RefStr *rs = Value_vp(v);
        va->vt = VT_BSTR;
        va->bstrVal = cstr_to_BSTR(rs->c, rs->size);
    } else if (type == fs->cls_int) {
        int32_t i64 = fs->Value_int64(v, NULL);
        if (i64 < INT32_MIN || i64 > INT32_MAX) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Integer out of range");
            return FALSE;
        } else {
            va->vt = VT_I4;
            va->lVal = i64;
        }
    } else if (type == fs->cls_float) {
        va->vt = VT_R8;
        va->dblVal = Value_float2(v);
    } else if (type == fs->cls_frac) {
        // 10000倍してint64に変換
        va->vt = VT_CY;
        if (!Value_frac10(&va->cyVal.int64, v, 10000)) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Currency out of range");
            return FALSE;
        }
    } else if (type == fs->cls_bool) {
        va->vt = VT_BOOL;
        va->boolVal = (Value_bool(v) ? TRUE : FALSE);
    } else if (type == fs->cls_datetime || type == fs->cls_timestamp) {
        va->vt = VT_DATE;
        va->date = Value_oledate(v);
    } else if (type == fs->cls_null) {
        va->vt = VT_NULL;
    } else if (type == fs->cls_list) {
        Mem m;
        Hash h;
        if (mem == NULL) {
            mem = &m;
            hash = &h;
            fs->Mem_init(mem, 1024);
            fs->Hash_init(hash, mem, 128);
        }
        if (!fox_value_to_safearray(va, v, mem, hash)) {
            return FALSE;
        }
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Cannot convert %n to Valiant", type);
        return FALSE;
    }
    return TRUE;
}

static int safearray_to_fox_value(Value *v, SAFEARRAY *sa)
{
    return TRUE;
}
static int variant_to_fox_value(Value *v, VARIANT *va)
{
    switch (va->vt & ~VT_BYREF) {
    case VT_I2:
        if (V_ISBYREF(va)) {
            *v = int32_Value(*va->piVal);
        } else {
            *v = int32_Value(va->iVal);
        }
        break;
    case VT_I4:
        if (V_ISBYREF(va)) {
            *v = int32_Value(*va->plVal);
        } else {
            *v = int32_Value(va->lVal);
        }
        break;
    case VT_R4: {
        double d;
        if (V_ISBYREF(va)) {
            d = *va->pfltVal;
        } else {
            d = va->fltVal;
        }
        *v = fs->float_Value(fs->cls_float, d);
        break;
    }
    case VT_R8: {
        double d;
        if (V_ISBYREF(va)) {
            d = *va->pdblVal;
        } else {
            d = va->dblVal;
        }
        *v = fs->float_Value(fs->cls_float, d);
        break;
    }
    case VT_CY: {
        int64_t i64;
        if (V_ISBYREF(va)) {
            i64 = va->pcyVal->int64;
        } else {
            i64 = va->cyVal.int64;
        }
        // 1/10000した値をfracに変換
        *v = frac10_Value(i64, 10000);
        if (*v == VALUE_NULL) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Currency out of range");
            return FALSE;
        }
        break;
    }
    case VT_BOOL:
        if (V_ISBYREF(va)) {
            *v = bool_Value(*va->pboolVal);
        } else {
            *v = bool_Value(va->boolVal);
        }
        break;
    case VT_BSTR:
        if (V_ISBYREF(va)) {
            *v = BSTR_Value(*va->pbstrVal);
        } else {
            *v = BSTR_Value(va->bstrVal);
        }
        break;
    case VT_DATE:
        if (V_ISBYREF(va)) {
            *v = oledate_Value(*va->pdate);
        } else {
            *v = oledate_Value(va->date);
        }
        break;
    case VT_EMPTY:
    case VT_NULL:
        *v = VALUE_NULL;
        break;
    case VT_DISPATCH: {
        IDispatch *idisp;
        if (V_ISBYREF(va)) {
            idisp = *va->ppdispVal;
        } else {
            idisp = va->pdispVal;
        }
        if (idisp != NULL) {
            RefOleObject *ole = fs->buf_new(cls_oleobject, sizeof(RefOleObject));
            *v = vp_Value(ole);
            ole->valid = TRUE;
            ole->pdisp = idisp;
            ole->pdisp->lpVtbl->AddRef(ole->pdisp);
        } else {
            *v = VALUE_NULL;
        }
        break;
    }
    case VT_ARRAY:
        if (V_ISBYREF(va)) {
            if (!safearray_to_fox_value(v, *va->pparray)) {
                return FALSE;
            }
        } else {
            if (!safearray_to_fox_value(v, va->parray)) {
                return FALSE;
            }
        }
        break;
    default:
        *v = VALUE_NULL;
        break;
    }
    return TRUE;
}

/**
 * 第1引数はメソッド名
 */
static int invoke_method_sub(IDispatch *pdisp, VARIANT *result, RefStr *name, int offset, int flags)
{
    Value *v = fg->stk_base;
    int argc = fg->stk_top - v - offset;
    int i;
    DISPID dispid, prop_dispid;
    HRESULT hr;
    EXCEPINFO excep;
    UINT arg_err = 0;
    int ret = TRUE;

    BSTR bname = cstr_to_BSTR(name->c, name->size);

    memset(&excep, 0, sizeof(excep));
    hr = pdisp->lpVtbl->GetIDsOfNames(pdisp, &IID_NULL, &bname, 1, GetUserDefaultLCID(), &dispid);
    if (SUCCEEDED(hr)) {
        // 引数の配列を作成
        VARIANTARG *va = malloc(sizeof(VARIANTARG) * (argc + 1));
        DISPPARAMS param;

        for (i = 0; i < argc; i++) {
            va[i].vt = VT_EMPTY;
        }
        for (i = 0; i < argc; i++) {
            // 逆順に積む
            if (!fox_value_to_variant(&va[argc - i - 1], v[i + offset], NULL, NULL)) {
                ret = FALSE;
                goto FINALLY;
            }
        }

        param.rgvarg = va;
        param.cArgs = argc;

        if ((flags & DISPATCH_PROPERTYPUT) != 0) {
            prop_dispid = DISPID_PROPERTYPUT;
            param.rgdispidNamedArgs = &prop_dispid;
            param.cNamedArgs = 1;
        } else {
            param.rgdispidNamedArgs = NULL;
            param.cNamedArgs = 0;
        }
        hr = pdisp->lpVtbl->Invoke(pdisp, dispid, &IID_NULL, GetUserDefaultLCID(), flags, &param, result, &excep, &arg_err);

        if (FAILED(hr)) {
            if (excep.wCode == 0 && excep.scode == 0) {
                throw_windows_error("OLEError", hr);
            } else {
                throw_ole_error("OLEError", &excep);
            }
            ret = FALSE;
            goto FINALLY;
        }
    FINALLY:
        for (i = 0; i < argc; i++) {
            VariantClear(&va[i]);
        }
        free(va);
        return ret;
    } else {
        fs->throw_errorf(mod_windows, "OLEError", "Method %S not found", name);
        return FALSE;
    }
}

static int ole_invoke_method(Value *vret, Value *v, RefNode *node)
{
    int flags = FUNC_INT(node);
    RefOleObject *ole = Value_vp(*v);
    VARIANT result;

    if (!ole->valid) {
        fs->throw_errorf(mod_windows, "OLEError", "Object already closed");
        return FALSE;
    }
    memset(&result, 0, sizeof(result));
    VariantInit(&result);
    if (!invoke_method_sub(ole->pdisp, &result, Value_vp(v[1]), 2, flags)) {
        return FALSE;
    }
    variant_to_fox_value(vret, &result);
    VariantClear(&result);

    return TRUE;
}

static int ole_get_iterator(Value *vret, Value *v, RefNode *node)
{
    RefOleObject *ole = Value_vp(*v);
    RefOleEnumerator *ret;
    VARIANT result;
    HRESULT hr;
    IDispatch *pd = ole->pdisp;
    void *vp = NULL;
    DISPPARAMS param;

    if (!ole->valid) {
        fs->throw_errorf(mod_windows, "OLEError", "Object already closed");
        return FALSE;
    }

    VariantInit(&result);
    param.rgvarg = NULL;
    param.rgdispidNamedArgs = NULL;
    param.cNamedArgs = 0;
    param.cArgs = 0;

    hr = pd->lpVtbl->Invoke(
        pd, DISPID_NEWENUM, &IID_NULL, GetUserDefaultLCID(),
        DISPATCH_METHOD | DISPATCH_PROPERTYGET,
        &param, &result, NULL, NULL);
    if (FAILED(hr)) {
        VariantClear(&result);
        goto ERROR_END;
    }
    if (V_VT(&result) == VT_UNKNOWN) {
        hr = V_UNKNOWN(&result)->lpVtbl->QueryInterface(V_UNKNOWN(&result), &IID_IEnumVARIANT, &vp);
    } else if (V_VT(&result) == VT_DISPATCH) {
        hr = V_DISPATCH(&result)->lpVtbl->QueryInterface(V_DISPATCH(&result), &IID_IEnumVARIANT, &vp);
    }
    if (FAILED(hr) || vp == NULL) {
        VariantClear(&result);
        goto ERROR_END;
    }
    VariantClear(&result);

    ret = fs->buf_new(cls_oleenum, sizeof(RefOleEnumerator));
    *vret = vp_Value(ret);
    ret->valid = TRUE;
    ret->penum = vp;
    VariantInit(&ret->current);
    return TRUE;

ERROR_END:
    throw_windows_error("OLEError", hr);
    return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int oleenum_next(Value *vret, Value *v, RefNode *node)
{
    RefOleEnumerator *ole = Value_vp(*v);
    int result = FALSE;

    if (ole->valid) {
        IEnumVARIANT *p = ole->penum;
        VariantClear(&ole->current);
        if (p->lpVtbl->Next(p, 1, &ole->current, NULL) == S_OK) {
            variant_to_fox_value(vret, &ole->current);
            result = TRUE;
        }
    }
    if (!result) {
        fs->throw_stopiter();
        return FALSE;
    }

    return TRUE;
}

static int oleenum_close(Value *vret, Value *v, RefNode *node)
{
    RefOleEnumerator *ole = Value_vp(*v);

    if (ole->valid) {
        IEnumVARIANT *p = ole->penum;
        p->lpVtbl->Release(p);
        VariantClear(&ole->current);
        ole->valid = FALSE;
    }

    return TRUE;
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
        *vret = wstr_Value((wchar_t*)buf);
        break;
    }
    case REG_MULTI_SZ: {
        RefArray *ra = fs->refarray_new(0);
        const wchar_t *wp = (const wchar_t*)buf;
        const wchar_t *end = wp + (data_size / sizeof(wchar_t));

        *vret = vp_Value(ra);
        while (*wp != L'\0' && wp < end) {
            Value *vp = fs->refarray_push(ra);
            *vp = wstr_Value(wp);
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

static int get_timezone(Value *vret, Value *v, RefNode *node)
{
    *vret = fs->Value_cp(vp_Value(default_tz));
    return TRUE;
}
static int set_timezone(Value *vret, Value *v, RefNode *node)
{
    RefTimeZone *tz = Value_vp(v[1]);
    fs->Value_dec(vp_Value(default_tz));
    fs->Value_inc(vp_Value(tz));
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

#undef DEFINE_INT
}

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier_p(m, m, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, windows_dispose, 0, 0, NULL);

    n = fs->define_identifier(m, m, "set_timezone", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, set_timezone, 1, 1, NULL, fs->cls_timezone);

    n = fs->define_identifier(m, m, "get_timezone", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, get_timezone, 0, 0, NULL);
}

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;
    RefNode *cls_regiter;

    cls_oleobject = fs->define_identifier(m, m, "OLEObject", NODE_CLASS, 0);
    cls_oleenum = fs->define_identifier(m, m, "OLEEnumerator", NODE_CLASS, 0);
    cls_regkey = fs->define_identifier(m, m, "RegKey", NODE_CLASS, 0);
    cls_regiter = fs->define_identifier(m, m, "RegKeyIterator", NODE_CLASS, 0);

    cls = cls_oleobject;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, ole_new, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
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
    fs->define_native_func_a(n, ole_get_iterator, 0, 0, NULL);

    fs->extends_method(cls, fs->cls_obj);


    cls = cls_oleenum;
    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, oleenum_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, oleenum_next, 0, 0, NULL);
    cls->u.c.super = fs->cls_iterator;
    fs->extends_method(cls, fs->cls_iterator);


    cls = cls_regkey;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, regkey_new, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
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
    cls->u.c.super = fs->cls_iterator;
    fs->extends_method(cls, fs->cls_iterator);


    cls = fs->define_identifier(m, m, "OLEError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);

    cls = fs->define_identifier(m, m, "RegistoryError", NODE_CLASS, 0);
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
    return "Build at\t" __DATE__ "\n";
}
