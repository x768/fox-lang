#include "fox_windows.h"


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

int ole_new(Value *vret, Value *v, RefNode *node)
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
int ole_close(Value *vret, Value *v, RefNode *node)
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

int ole_invoke_method(Value *vret, Value *v, RefNode *node)
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

int ole_get_iterator(Value *vret, Value *v, RefNode *node)
{
    RefOleObject *ole = Value_vp(*v);
    RefOleEnumerator *ret;
    VARIANT result;
    HRESULT hr;
    IDispatch *pd = ole->pdisp;
    void *vp = NULL;
    DISPPARAMS param;
    RefNode *cls_oleenum = FUNC_VP(node);

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

int oleenum_next(Value *vret, Value *v, RefNode *node)
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

int oleenum_close(Value *vret, Value *v, RefNode *node)
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
