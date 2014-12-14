#include "fox_vm.h"
#include <string.h>
#include <stdio.h>


RefNode *Value_type(Value v)
{
    switch (v & 0x3ULL) {
    case 0ULL:
        if (v != VALUE_NULL) {
            return Value_ref_header(v)->type;
        }
        break;
    case 1ULL: {
        int idx = (v & 0xFFFFFFFF) >> 2;
        return fv->integral[idx];
    }
    default:
        fatal_errorf(NULL, "Value_type:not a value type");
        break;
    }
    return fs->cls_null;
}

/**
 * Valueから整数を取得(Int, Float, Rational
 * 取得できなければ0を返す
 */
int64_t Value_int64(Value v, int *err)
{
    if (err != NULL) {
        *err = FALSE;
    }
    if (Value_isint(v)) {
        return ((int64_t)v) >> 32;
    } else if (Value_isref(v)) {
        RefNode *type = Value_ref_header(v)->type;
        if (type == fs->cls_int) {
            RefInt *mp = Value_vp(v);
            int64_t ret;
            if (mp2_toint64(&mp->mp, &ret) != MP_OKAY) {
                if (err != NULL) {
                    *err = TRUE;
                }
                return INT64_MAX;
            }
            return ret;
        } else if (type == fs->cls_float) {
            RefFloat *pd = Value_vp(v);
            double d = pd->d;

            if (d > INT64_MAX) {
                if (err != NULL) {
                    *err = TRUE;
                }
                return INT64_MAX;
            } else if (d < -INT64_MAX) {
                if (err != NULL) {
                    *err = TRUE;
                }
                return -INT64_MAX;
            } else {
                return (int64_t)d;
            }
        } else if (type == fs->cls_frac) {
            int64_t ret;
            RefFrac *md = Value_vp(v);
            mp_int mp, rem;

            mp_init(&mp);
            mp_init(&rem);

            mp_div(&md->md[0], &md->md[1], &mp, &rem);
            mp_clear(&rem);

            if (mp2_toint64(&mp, &ret) != MP_OKAY) {
                if (mp.sign == MP_NEG) {
                    ret = -INT64_MAX;
                } else {
                    ret = INT64_MAX;
                }
                if (err != NULL) {
                    *err = TRUE;
                }
            }
            mp_clear(&mp);
            return ret;
        }
    }
    return 0LL;
}

/**
 * Valueから実数を取得
 * 取得できなければ0.0を返す
 */
double Value_float(Value v)
{
    if (Value_isint(v)) {
        return (double)(int32_t)Value_integral(v);
    } else if (Value_isref(v)) {
        RefNode *type = Value_ref_header(v)->type;
        if (type == fs->cls_float) {
            RefFloat *rd = Value_vp(v);
            return rd->d;
        } else if (type == fs->cls_int) {
            RefInt *mp = Value_vp(v);
            return mp2_todouble(&mp->mp);
        } else if (type == fs->cls_frac) {
            RefFrac *md = Value_vp(v);
            double d1 = mp2_todouble(&md->md[0]);
            double d2 = mp2_todouble(&md->md[1]);
            return d1 / d2;
        }
    }
    return 0.0;
}

/**
 * typeof(v) == Fracに限る
 * 10進数文字列表現
 * max_frac : 小数部の最大桁数 / -1:循環小数はエラー
 */
char *Value_frac_s(Value v, int max_frac)
{
    char *c_buf = NULL;
    RefFrac *md = Value_vp(v);
    mp_int mi, rem;

    mp_init(&mi);
    mp_init(&rem);
    mp_div(&md->md[0], &md->md[1], &mi, &rem);
    rem.sign = MP_ZPOS;

    // 小数部がない場合、整数部のみ出力
    if (mp_cmp_z(&rem) == 0) {
        c_buf = malloc(mp_radix_size(&mi, 10) + 1);
        mp_toradix(&mi, (unsigned char*)c_buf, 10);
    } else {
        int recr[2];
        int n_ex;
        mp_int ex;
        mp_int rem2;

        if (get_recurrence(recr, &md->md[1])) {
            if (max_frac < 0) {
                return NULL;
            }
            n_ex = max_frac;
        } else {
            n_ex = recr[0];
            if (n_ex > max_frac) {
                n_ex = max_frac;
            }
        }
        mp_init(&ex);
        mp_init(&rem2);
        mp_set(&ex, 10);

        if (n_ex > 1) {
            mp_expt_d(&ex, n_ex, &ex);     // ex = ex ** width_f
        }
        mp_mul(&rem, &ex, &rem);
        mp_div(&rem, &md->md[1], &rem, &rem2);

        c_buf = frac_tostr_sub(md->md[0].sign, &mi, &rem, n_ex);
        mp_clear(&ex);
        mp_clear(&rem2);
    }
    return c_buf;
}

/**
 * typeof(v) == Fracに限る
 * vをfactor倍してvalに入れる
 * 失敗したらreturn false
 */
int Value_frac10(int64_t *val, Value v, int factor)
{
    RefFrac *md = Value_vp(v);
    mp_int mp;
    mp_int rem;
    int ret;

    mp_init(&mp);
    mp_init(&rem);
    mp_mul_d(&md->md[0], factor, &mp);
    mp_div(&mp, &md->md[1], &mp, &rem);

    ret = mp2_toint64(&mp, val);
    mp_clear(&mp);
    mp_clear(&rem);
    return ret == MP_OKAY;
}

/**
 * Valueから文字列を取得
 * 取得できなければ""を返す
 */
Str Value_str(Value v)
{
    if (Value_isref(v) && (Value_ref_header(v)->type->opt & NODEOPT_STRCLASS) != 0) {
        RefStr *s = Value_vp(v);
        Str ret;
        ret.p = s->c;
        ret.size = s->size;
        return ret;
    } else {
        Str ret;
        ret.p = NULL;
        ret.size = 0;
        return ret;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void Value_inc(Value v)
{
    if (Value_isref(v)) {
        RefHeader *rh = Value_ref_header(v);
        if (rh->nref > 0) {
            rh->nref++;
        }
    }
}
void Value_dec(Value v)
{
    if (Value_isref(v)) {
        RefHeader *rh = Value_ref_header(v);
        if (rh->nref > 0 && --rh->nref == 0) {
            Value_release_ref(v);
        }
    }
}
void Value_push(const char *fmt, ...)
{
    const char *p;
    va_list va;
    va_start(va, fmt);

    for (p = fmt; *p != '\0'; p++) {
        switch (*p) {
        case 'N':   // NULL
            *fg->stk_top = VALUE_NULL;
            break;
        case 'b': {  // BOOL
            int i = va_arg(va, int);
            *fg->stk_top = bool_Value(i);
            break;
        }
        case 'd': {  // int32
            int32_t i = va_arg(va, int32_t);
            *fg->stk_top = int32_Value(i);
            break;
        }
        case 'D': {  // int64
            int64_t i = va_arg(va, int64_t);
            *fg->stk_top = int64_Value(i);
            break;
        }
        case 's': {  // const char*
            const char *pc = va_arg(va, const char*);
            *fg->stk_top = cstr_Value(fs->cls_str, pc, -1);
            break;
        }
        case 'S':  { // Str
            Str s = va_arg(va, Str);
            *fg->stk_top = cstr_Value(fs->cls_str, s.p, s.size);
            break;
        }
        case 'r': { // Ref互換
            RefHeader* rh = va_arg(va, RefHeader*);
            *fg->stk_top = Value_cp(vp_Value(rh));
            break;
        }
        case 'v': { // Value
            Value v = va_arg(va, Value);
            *fg->stk_top = v;
            Value_inc(v);
            break;
        }
        case 'i': {  // const Node *type, int
            RefNode *n = va_arg(va, RefNode*);
            int i = va_arg(va, int);
            *fg->stk_top = integral_Value(n, i);
            break;
        }
        default:
            fatal_errorf(NULL, "Value_push:unknown format %c", *p);
            break;
        }
        fg->stk_top++;
    }
    va_end(va);
}
void Value_pop()
{
    Value v = fg->stk_top[-1];
    if (Value_isref(v)) {
        RefHeader *r = Value_ref_header(v);
        if (r->nref > 0 && --r->nref == 0) {
            Value_release_ref(v);
        }
    }
    fg->stk_top--;
}

////////////////////////////////////////////////////////////////////////////////////////

Value int64_Value(int64_t i)
{
    if (i < -INT32_MAX || i > INT32_MAX) {
        RefInt *mp = buf_new(fs->cls_int, sizeof(RefInt));
        Value v = vp_Value(mp);
        mp_init(&mp->mp);
        mp2_set_int64(&mp->mp, i);
        return v;
    } else {
        return (i << 32) | 5ULL;
    }
}
Value float_Value(double dval)
{
    RefFloat *rf = buf_new(fs->cls_float, sizeof(RefFloat));
    Value v = vp_Value(rf);
    rf->d = dval;
    return v;
}

/**
 * str:    10進数小数
 * return: Value of Frac (エラー時はVALUE_NULL)
 */
Value frac_s_Value(const char *str)
{
    const char *end;
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));

    mp_init(&md->md[0]);
    mp_init(&md->md[1]);
    if (!mp2_read_rational(md->md, str, &end)) {
        return VALUE_NULL;
    }
    return vp_Value(md);
}
/**
 * return val / factor
 * ex) val = 123, factor = 100 -> 1.23d
 */
Value frac10_Value(int64_t val, int factor)
{
    RefFrac *md = buf_new(fs->cls_frac, sizeof(RefFrac));

    mp_init(&md->md[0]);
    mp_init(&md->md[1]);
    mp2_set_int64(&md->md[0], val);
    mp_set_int(&md->md[1], factor);
    mp2_fix_rational(&md->md[0], &md->md[1]);

    return vp_Value(md);
}
Value cstr_Value(RefNode *klass, const char *p, int size)
{
    RefStr *r;

    if (size < 0) {
        size = strlen(p);
    }
    r = malloc(sizeof(RefStr) + (size + 1));
    r->rh.nref = 1;
    r->rh.type = klass;
    r->rh.weak_ref = NULL;
    r->rh.n_memb = 0;

    r->size = size;
    memcpy(r->c, p, size);
    r->c[size] = '\0';

    fv->heap_count++;
    return vp_Value(r);
}
Value Value_cp(Value v)
{
    if (Value_isref(v)) {
        RefHeader *rh = Value_ref_header(v);
        if (rh->nref > 0) {
            rh->nref++;
        }
    }
    return v;
}
void Value_set(Value *vp, Value v)
{
    Value v2 = *vp;

    if (Value_isref(v)) {
        RefHeader *rh = Value_ref_header(v);
        if (rh->nref > 0) {
            rh->nref++;
        }
    }
    *vp = v;

    if (Value_isref(v2)) {
        RefHeader *rh = Value_ref_header(v2);
        if (rh->nref > 0 && --rh->nref == 0) {
            Value_release_ref(v2);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////

Ref *ref_new(RefNode *klass)
{
    int size = sizeof(RefHeader) + sizeof(Value) * klass->u.c.n_memb;
    Ref *r = malloc(size);
    memset(r, 0, size);
    r->rh.nref = 1;
    r->rh.type = klass;
    r->rh.n_memb = klass->u.c.n_memb;

    fv->heap_count++;
    return r;
}
Ref *ref_new_n(RefNode *klass, int n)
{
    int size = sizeof(RefHeader) + sizeof(Value) * (klass->u.c.n_memb + n);
    Ref *r = malloc(size);
    memset(r, 0, size);
    r->rh.nref = 1;
    r->rh.type = klass;
    r->rh.weak_ref = NULL;
    r->rh.n_memb = klass->u.c.n_memb + n;

    fv->heap_count++;
    return r;
}
/**
 * RefHeaderを持つ構造体
 * 可視のメンバでなければ、klass == NULLが許可される
 */
void *buf_new(RefNode *klass, int size)
{
    RefHeader *r = malloc(size);
    memset(r, 0, size);
    r->nref = 1;
    r->type = klass;
    r->weak_ref = NULL;

    fv->heap_count++;
    return r;
}

RefStr *refstr_new_n(RefNode *klass, int size)
{
    RefStr *r = malloc(sizeof(RefStr) + (size + 1));
    r->rh.nref = 1;
    r->rh.type = klass;
    r->rh.weak_ref = NULL;
    r->rh.n_memb = 0;
    r->size = size;

    fv->heap_count++;

    return r;
}

/**
 * 文字コードをUTF-8に変換してセットする
 * cs == NULLなら、入力はUTF-8
 * 変換できない文字は置き換え
 */
Value cstr_Value_conv(const char *p, int size, RefCharset *cs)
{
    if (cs == NULL || cs == fs->cs_utf8) {
        if (invalid_utf8_pos(p, size) >= 0) {
            StrBuf buf;
            StrBuf_init_refstr(&buf, size);
            alt_utf8_string(&buf, p, size);
            return StrBuf_str_Value(&buf, fs->cls_str);
        } else {
            return cstr_Value(fs->cls_str, p, size);
        }
    } else {
        IconvIO ic;
        if (IconvIO_open(&ic, cs, fs->cs_utf8, UTF8_ALTER_CHAR)) {
            StrBuf buf;
            StrBuf_init_refstr(&buf, 0);
            IconvIO_conv(&ic, &buf, p, size, FALSE, TRUE);
            IconvIO_close(&ic);
            return StrBuf_str_Value(&buf, fs->cls_str);
        }
    }
    return VALUE_NULL;
}
Value printf_Value(const char *fmt, ...)
{
    StrBuf buf;
    va_list va;

    StrBuf_init_refstr(&buf, 0);
    va_start(va, fmt);
    StrBuf_vprintf(&buf, fmt, va);
    va_end(va);

    return StrBuf_str_Value(&buf, fs->cls_str);
}

/**
 * mod_rootに登録する
 * Unresolvedは無効
 */
static RefNode *Module_new_static(RefStr *name)
{
    RefNode *m = Mem_get(&fg->st_mem, sizeof(RefNode));
    memset(m, 0, sizeof(*m));

    m->rh.type = fs->cls_module;
    m->rh.nref = -1;
    m->type = NODE_MODULE;
    m->name = name;
    m->u.m.src_path = NULL;

    Hash_init(&m->u.m.h, &fg->st_mem, 32);
//  Hash_init(&m->u.m.import, &fg->st_mem, 16);
//  Hash_init(&m->u.m.unresolved, &fg->cmp_mem, 32);

    // moduleプールに登録
    Hash_add_p(&fg->mod_root, &fg->st_mem, name, m);

    return m;
}
RefNode *Module_new_sys(const char *name_p)
{
    RefStr *rs = intern(name_p, -1);
    RefNode *m = Module_new_static(rs);
    char *ptr = Mem_get(&fg->st_mem, rs->size + 9);
    sprintf(ptr, "[system]%.*s", rs->size, rs->c);
    m->u.m.src_path = ptr;

    return m;
}

////////////////////////////////////////////////////////////////////////////////////////

int Value_hash(Value v, int32_t *phash)
{
    int32_t hash;

    if (Value_isintegral(v)) {
        hash = int32_hash(Value_integral(v));
    } else {
        RefNode *key_type = Value_type(v);

        if ((key_type->opt & NODEOPT_STRCLASS) != 0) {
            RefStr *rs = Value_vp(v);
            hash = str_hash(rs->c, rs->size);
        } else {
            RefNode *fn_hash = Hash_get_p(&key_type->u.c.h, fs->str_hash);
            RefNode *dst_type;

            Value_push("v", v);
            if (!call_function(fn_hash, 0)) {
                return FALSE;
            }
            dst_type = Value_type(fg->stk_top[-1]);
            if (dst_type != fs->cls_int) {
                throw_errorf(fs->mod_lang, "TypeError", "hash() return type must be 'Int' but %n", dst_type);
                return FALSE;
            }
            hash = Value_int64(fg->stk_top[-1], NULL);
            Value_pop();
        }
    }
    *phash = hash;
    return TRUE;
}
/**
 * 型同一チェックは行わない(型が違う場合未定義)
 */
int Value_eq(Value v1, Value v2, int *ret)
{
    if (Value_isintegral(v1)) {
        *ret = (v1 == v2);
    } else if (v1 == VALUE_NULL) {
        *ret = TRUE;
    } else {
        RefNode *type = Value_type(v1);

        if ((type->opt & NODEOPT_STRCLASS) != 0) {
            RefStr *r1 = Value_vp(v1);
            RefStr *r2 = Value_vp(v2);
            *ret = (r1->size == r2->size && memcmp(r1->c, r2->c, r1->size) == 0);
        } else {
            *fg->stk_top++ = Value_cp(v1);
            *fg->stk_top++ = Value_cp(v2);
            if (!call_member_func(fs->symbol_stock[T_EQ], 1, TRUE)) {
                return FALSE;
            }
            *ret = Value_bool(fg->stk_top[-1]);
            Value_pop();
        }
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////

/**
 * klassがtypeのサブクラスかどうか
 */
int is_subclass(RefNode *klass, RefNode *type)
{
    for (;;) {
        if (klass == type) {
            return TRUE;
        }
        if (klass == fs->cls_obj) {
            break;
        }
        klass = klass->u.c.super;
    }
    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////


/**
 * エイリアス定義ファイルを読み込んで、Hashに入れる
 * intern済みのRefStr*のハッシュを返す
 */
int load_aliases_file(Hash *ret, const char *filename)
{
    char *path = path_normalize(NULL, fs->fox_home, filename, -1, NULL);
    IniTok tk;

    if (!IniTok_load(&tk, path)) {
        fatal_errorf(NULL, "Cannot load file %q", path);
    }
    free(path);

    while (IniTok_next(&tk)) {
        if (tk.type == INITYPE_STRING) {
            const char *p = tk.val.p;
            const char *end = p + tk.val.size;
            RefStr *name = intern(tk.key.p, tk.key.size);

            while (p < end) {
                // tk.valを\tで分割する
                const char *top = p;
                while (p < end && *p != '\t') {
                    p++;
                }
                Hash_add_p(ret, &fg->st_mem, intern(top, p - top), name);
                if (p < end) {
                    p++;
                }
            }
        }
    }
    IniTok_close(&tk);

    return TRUE;
}
int get_loader_function(RefNode **fn, const char *type_p, int type_size, const char *prefix, Hash *hash)
{
    RefStr *module;
    RefNode *mod;
    RefNode *n;
    char *buf;
    int prefix_len;
    int i;

    if (type_size < 0) {
        type_size = strlen(type_p);
    }
    module = Hash_get(hash, type_p, type_size);
    if (module == NULL) {
        throw_errorf(fs->mod_io, "FileTypeError", "%S is not supported", Str_new(type_p, type_size));
        return FALSE;
    }

    mod = get_module_by_name(module->c, module->size, FALSE, TRUE);
    if (mod == NULL) {
        return FALSE;
    }

    prefix_len = strlen(prefix);
    buf = malloc(prefix_len + type_size + 1);
    strcpy(buf, prefix);

    for (i = 0; i < type_size; i++) {
        int c = type_p[i];
        if (isalnumu_fox(c)) {
            buf[prefix_len + i] = c;
        } else {
            buf[prefix_len + i] = '_';
        }
    }
    buf[prefix_len + type_size] = '\0';

    n = Hash_get(&mod->u.m.h, buf, prefix_len + type_size);
    if (n == NULL) {
        throw_errorf(fs->mod_lang, "NameError", "%r has no function named %s", module, buf);
        free(buf);
        return FALSE;
    }
    *fn = n;
    free(buf);

    return TRUE;
}
char *resource_to_path(Str name_s, const char *ext_p)
{
    char *name_p = str_dup_p(name_s.p, name_s.size, NULL);
    PtrList *lst;
    int i;

    if (ext_p == NULL) {
        ext_p = "";
    }

    // 識別子文字以外はエラー
    for (i = 0; i < name_s.size; i++) {
        if (name_p[i] == '.') {
            name_p[i] = SEP_C;
        } else if (!isalnumu_fox(name_p[i])) {
            goto ERROR_END;
        }
    }

    for (lst = fv->resource_path; lst != NULL; lst = lst->next) {
        char *path = str_printf("%s" SEP_S "%s%s", lst->u.c, name_p, ext_p);
        if (exists_file(path) == EXISTS_FILE) {
            free(name_p);
            return path;
        }
        free(path);
        path = NULL;
    }

ERROR_END:
    throw_errorf(fs->mod_file, "FileOpenError", "Cannot find file for %S", name_s);
    free(name_p);
    return NULL;
}
