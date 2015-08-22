#include "fox_vm.h"
#include <string.h>
#include <stdio.h>


enum {
    MSORT_MIN = 7,  // この要素数を越える場合マージソートを行う
};
enum {
    INDEX_ITERFILTER_ITER,
    INDEX_ITERFILTER_FUNC,
    INDEX_ITERFILTER_NUM,
};
enum {
    INDEX_LISTITER_VAL,
    INDEX_LISTITER_TYPE,
    INDEX_LISTITER_IDX,
    INDEX_LISTITER_NUM,
};
enum {
    INDEX_RANGE_BEGIN,
    INDEX_RANGE_END,
    INDEX_RANGE_STEP,
    INDEX_RANGE_IS_DEC,
    INDEX_RANGE_OPEN,
    INDEX_RANGE_NUM,

    INDEX_RANGE_NEXT = INDEX_RANGE_NUM,
    INDEX_RANGEITER_NUM,
};


static RefNode *cls_listiter;


static int inspect_sub(StrBuf *buf, Value v, Hash *hash, Mem *mem)
{
    RefNode *type = Value_type(v);

    if (type == fs->cls_list) {
        RefArray *r = Value_vp(v);
        HashEntry *he = Hash_get_add_entry(hash, mem, (RefStr*)r);
        int i;

        if (he->p != NULL) {
            // 循環参照
            if (!StrBuf_add(buf, "[...]", 5)) {
                return FALSE;
            }
            he->p = NULL;
            return TRUE;
        } else {
            he->p = (void*)1;
        }

        if (!StrBuf_add_c(buf, '[')) {
            return FALSE;
        }
        for (i = 0; i < r->size; i++) {
            if (i > 0) {
                if (!StrBuf_add(buf, ", ", 2)) {
                    return FALSE;
                }
            }
            if (!inspect_sub(buf, r->p[i], hash, mem)) {
                return TRUE;
            }
        }
        if (!StrBuf_add_c(buf, ']')) {
            return FALSE;
        }
        he->p = NULL;
    } else if (type == fs->cls_map || type == fv->cls_mimeheader) {
        RefMap *r = Value_vp(v);
        HashEntry *he = Hash_get_add_entry(hash, mem, (RefStr*)r);
        int i, first = TRUE;

        if (he->p != NULL) {
            // 循環参照
            if (!StrBuf_add(buf, "{...}", 5)) {
                return FALSE;
            }
            he->p = NULL;
            return TRUE;
        } else {
            he->p = (void*)1;
        }

        if (!StrBuf_add_c(buf, '{')) {
            return FALSE;
        }

        for (i = 0; i < r->entry_num; i++) {
            HashValueEntry *ve = r->entry[i];
            for (; ve != NULL; ve = ve->next) {

                if (first) {
                    first = FALSE;
                } else {
                    if (!StrBuf_add(buf, ", ", 2)) {
                        return FALSE;
                    }
                }
                if (!inspect_sub(buf, ve->key, hash, mem)) {
                    return TRUE;
                }
                if (!StrBuf_add_c(buf, ':')) {
                    return FALSE;
                }
                if (!inspect_sub(buf, ve->val, hash, mem)) {
                    return TRUE;
                }
            }
        }
        if (!StrBuf_add_c(buf, '}')) {
            return FALSE;
        }

        return TRUE;
    } else if (type == fs->cls_set) {
        RefMap *r = Value_vp(v);
        HashEntry *he = Hash_get_add_entry(hash, mem, (RefStr*)r);
        int i, first = TRUE;

        if (he->p != NULL) {
            // 循環参照
            if (!StrBuf_add(buf, "Set(...)", 8)) {
                return FALSE;
            }
            he->p = NULL;
            return TRUE;
        } else {
            he->p = (void*)1;
        }

        if (!StrBuf_add(buf, "Set(", 4)) {
            return FALSE;
        }

        for (i = 0; i < r->entry_num; i++) {
            HashValueEntry *ve = r->entry[i];
            for (; ve != NULL; ve = ve->next) {
                if (first) {
                    first = FALSE;
                } else {
                    if (!StrBuf_add(buf, ", ", 2)) {
                        return FALSE;
                    }
                }
                if (!inspect_sub(buf, ve->key, hash, mem)) {
                    return TRUE;
                }
            }
        }
        if (!StrBuf_add_c(buf, ')')) {
            return FALSE;
        }

        return TRUE;
    } else if (type == fs->cls_str) {
        RefStr *rs = Value_vp(v);
        if (!StrBuf_add_c(buf, '"')) {
            return FALSE;
        }
        add_backslashes_sub(buf, rs->c, rs->size, ADD_BACKSLASH_UCS4);
        if (!StrBuf_add_c(buf, '"')) {
            return FALSE;
        }
    } else if (type == fs->cls_null) {
        if (!StrBuf_add(buf, "null", 4)) {
            return FALSE;
        }
    } else {
        if (!StrBuf_add_v(buf, v)) {
            return FALSE;
        }
    }

    return TRUE;
}
int col_tostr(Value *vret, Value *v, RefNode *node)
{
    Mem mem;
    Hash hash;
    StrBuf buf;

    Mem_init(&mem, 1024);
    Hash_init(&hash, &mem, 128);

    StrBuf_init_refstr(&buf, 0);
    if (!inspect_sub(&buf, *v, &hash, &mem)) {
        Mem_close(&mem);
        StrBuf_close(&buf);
        return FALSE;
    }
    *vret = StrBuf_str_Value(&buf, fs->cls_str);
    Mem_close(&mem);

    return TRUE;
}

static int col_hash_sub(uint32_t *pret, Value v, Hash *hash, Mem *mem)
{
    RefNode *type = Value_type(v);

    if (type == fs->cls_list) {
        RefArray *r = Value_vp(v);
        HashEntry *he = Hash_get_add_entry(hash, mem, (RefStr*)r);
        int i;

        if (he->p != NULL) {
            throw_error_select(THROW_LOOP_REFERENCE);
            return FALSE;
        } else {
            he->p = (void*)1;
        }

        r->lock_count++;
        for (i = 0; i < r->size; i++) {
            if (!col_hash_sub(pret, r->p[i], hash, mem)) {
                r->lock_count--;
                return TRUE;
            }
        }
        r->lock_count--;
        he->p = NULL;
    } else if (type == fs->cls_map || type == fs->cls_set) {
        RefMap *r = Value_vp(v);
        HashEntry *he = Hash_get_add_entry(hash, mem, (RefStr*)r);
        int i;

        if (he->p != NULL) {
            throw_error_select(THROW_LOOP_REFERENCE);
            return FALSE;
        } else {
            he->p = (void*)1;
        }

        r->lock_count++;
        for (i = 0; i < r->entry_num; i++) {
            HashValueEntry *ve = r->entry[i];
            for (; ve != NULL; ve = ve->next) {
                if (!col_hash_sub(pret, ve->key, hash, mem)) {
                    r->lock_count--;
                    return FALSE;
                }
                if (!col_hash_sub(pret, ve->val, hash, mem)) {
                    r->lock_count--;
                    return FALSE;
                }
            }
        }
        r->lock_count--;
        he->p = NULL;
    } else {
        int32_t ret;
        if (!Value_hash(v, &ret)) {
            return FALSE;
        }
        *pret = *pret * 31 + ret;
    }

    return TRUE;
}
int col_hash(Value *vret, Value *v, RefNode *node)
{
    Mem mem;
    Hash hash;
    uint32_t ret = 0;

    Mem_init(&mem, 1024);
    Hash_init(&hash, &mem, 128);

    if (!col_hash_sub(&ret, *v, &hash, &mem)) {
        Mem_close(&mem);
        return FALSE;
    }

    *vret = int32_Value(ret & INTPTR_MAX);
    Mem_close(&mem);

    return TRUE;
}

// 循環参照は全てエラーとする
static int col_eq_sub(int *ret, Value v1, Value v2, Hash *hash1, Hash *hash2, Mem *mem)
{
    RefNode *type = Value_type(v1);

    if (type != Value_type(v2)) {
        *ret = FALSE;
        return TRUE;
    }
    if (type == fs->cls_list) {
        RefArray *r1 = Value_vp(v1);
        RefArray *r2 = Value_vp(v2);
        HashEntry *he1 = Hash_get_add_entry(hash1, mem, (RefStr*)r1);
        HashEntry *he2 = Hash_get_add_entry(hash2, mem, (RefStr*)r2);
        int i;

        // 循環参照チェック
        if (he1->p != NULL || he2->p != NULL) {
            throw_error_select(THROW_LOOP_REFERENCE);
            return FALSE;
        }

        // 参照先が同じならtrue
        if (r1 == r2) {
            return TRUE;
        }
        // 長さが違うのでfalse
        if (r1->size != r2->size) {
            *ret = FALSE;
            return TRUE;
        }
        he1->p = (void*)1;
        he2->p = (void*)1;

        r1->lock_count++;
        r2->lock_count++;
        for (i = 0; i < r1->size; i++) {
            if (!col_eq_sub(ret, r1->p[i], r2->p[i], hash1, hash2, mem)) {
                r1->lock_count--;
                r2->lock_count--;
                return FALSE;
            }
            if (!*ret) {
                r1->lock_count--;
                r2->lock_count--;
                return TRUE;
            }
        }
        r1->lock_count--;
        r2->lock_count--;

        he1->p = NULL;
        he2->p = NULL;
    } else if (type == fs->cls_map || type == fs->cls_set) {
        RefMap *r1 = Value_vp(v1);
        RefMap *r2 = Value_vp(v2);
        HashEntry *he1 = Hash_get_add_entry(hash1, mem, (RefStr*)r1);
        HashEntry *he2 = Hash_get_add_entry(hash2, mem, (RefStr*)r2);
        int i;

        // 循環参照チェック
        if (he1->p != NULL || he2->p != NULL) {
            throw_error_select(THROW_LOOP_REFERENCE);
            return FALSE;
        }

        // 参照先が同じならtrue
        if (r1 == r2) {
            return TRUE;
        }
        // 要素数が違うのでfalse
        if (r1->count != r2->count) {
            *ret = FALSE;
            return TRUE;
        }
        he1->p = (void*)1;
        he2->p = (void*)1;

        r1->lock_count++;
        r2->lock_count++;
        for (i = 0; i < r1->entry_num; i++) {
            HashValueEntry *ve1 = r1->entry[i];
            for (; ve1 != NULL; ve1 = ve1->next) {
                HashValueEntry *ve2;
                if (!refmap_get(&ve2, r2, ve1->key)) {
                    r1->lock_count--;
                    r2->lock_count--;
                    return FALSE;
                }
                if (ve2 == NULL) {
                    *ret = FALSE;
                    r1->lock_count--;
                    r2->lock_count--;
                    return TRUE;
                }
                if (!col_eq_sub(ret, ve1->val, ve2->val, hash1, hash2, mem)) {
                    r1->lock_count--;
                    r2->lock_count--;
                    return FALSE;
                }
                if (!*ret) {
                    r1->lock_count--;
                    r2->lock_count--;
                    return TRUE;
                }
            }
        }
        r1->lock_count--;
        r2->lock_count--;

        he1->p = NULL;
        he2->p = NULL;
    } else {
        if (!Value_eq(v1, v2, ret)) {
            return FALSE;
        }
    }
    return TRUE;
}
int col_eq(Value *vret, Value *v, RefNode *node)
{
    Mem mem;
    Hash hash1, hash2;
    int result = TRUE;

    Mem_init(&mem, 1024);
    Hash_init(&hash1, &mem, 128);
    Hash_init(&hash2, &mem, 128);

    if (!col_eq_sub(&result, v[0], v[1], &hash1, &hash2, &mem)) {
        Mem_close(&mem);
        return FALSE;
    }
    Mem_close(&mem);
    *vret = bool_Value(result);

    return TRUE;
}

int list_cmp_sub(int *ret, RefArray *r1, RefArray *r2, Hash *hash1, Hash *hash2, Mem *mem)
{
    HashEntry *he1 = Hash_get_add_entry(hash1, mem, (RefStr*)r1);
    HashEntry *he2 = Hash_get_add_entry(hash2, mem, (RefStr*)r2);
    int i;
    int size;

    // 循環参照チェック
    if (he1->p != NULL || he2->p != NULL) {
        throw_error_select(THROW_LOOP_REFERENCE);
        return FALSE;
    }
    // 参照先が同じならtrue
    if (r1 == r2) {
        return TRUE;
    }
    he1->p = (void*)1;
    he2->p = (void*)1;

    size = (r1->size < r2->size) ? r1->size : r2->size;

    for (i = 0; i < size; i++) {
        Value v1 = r1->p[i];
        Value v2 = r2->p[i];
        RefNode *v1_type = Value_type(v1);
        RefNode *v2_type = Value_type(v2);

        if (v1_type != v2_type) {
            throw_errorf(fs->mod_lang, "TypeError",
                "Compare operator given difference type values (%n and %n)", v1_type, v2_type);
            return FALSE;
        }
        if (v1_type == fs->cls_list) {
            int ret2;
            if (!list_cmp_sub(&ret2, Value_vp(v1), Value_vp(v2), hash1, hash2, mem)) {
                return FALSE;
            }
            if (ret2 != 0) {
                *ret = ret2;
                return TRUE;
            }
        } else {
            int ret2;
            *fg->stk_top++ = Value_cp(v1);
            *fg->stk_top++ = Value_cp(v2);
            if (!call_member_func(fs->symbol_stock[T_CMP], 1, TRUE)) {
                return FALSE;
            }
            ret2 = Value_int64(fg->stk_top[-1], NULL);
            Value_pop();
            if (ret2 != 0) {
                *ret = ret2;
                return TRUE;
            }
        }
    }

    if (r1->size < r2->size) {
        *ret = -1;
    } else if (r1->size > r2->size) {
        *ret = 1;
    } else {
        *ret = 0;
    }
    return TRUE;
}
int list_cmp(Value *vret, Value *v, RefNode *node)
{
    Mem mem;
    Hash hash1, hash2;
    int result = TRUE;

    Mem_init(&mem, 1024);
    Hash_init(&hash1, &mem, 128);
    Hash_init(&hash2, &mem, 128);

    if (!list_cmp_sub(&result, Value_vp(v[0]), Value_vp(v[1]), &hash1, &hash2, &mem)) {
        Mem_close(&mem);
        return FALSE;
    }
    Mem_close(&mem);
    *vret = int32_Value(result);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////

RefArray *refarray_new(int size)
{
    RefArray *r = buf_new(fs->cls_list, sizeof(RefArray));

    if (size > 0) {
        int alloc_size = align_pow2(size, 32);
        Value *va = malloc(sizeof(Value) * alloc_size);
        memset(va, 0, sizeof(Value) * alloc_size);

        r->size = size;
        r->alloc_size = alloc_size;
        r->p = va;
    } else {
        r->size = 0;
        r->alloc_size = 0;
        r->p = NULL;
    }
    return r;
}
/**
 * 末尾に要素を追加してポインタを返す
 */
Value *refarray_push(RefArray *ra)
{
    int size = ra->size + 1;
    int alloc_size = ra->alloc_size;

    if (size > alloc_size) {
        alloc_size = align_pow2(size, 32);
        ra->p = realloc(ra->p, sizeof(Value) * alloc_size);
        ra->alloc_size = alloc_size;
    }
    ra->size = size;
    return &ra->p[size - 1];
}
int refarray_set_size(RefArray *ra, int size)
{
    if (size < 0) {
        throw_errorf(fs->mod_lang, "ValueError", "Negative value was given");
        return FALSE;
    } else if (sizeof(Value) * size > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    } else if (size < ra->size) {
        int i;
        for (i = size; i < ra->size; i++) {
            Value_dec(ra->p[i]);
        }
        ra->size = size;
    } else if (size > ra->size) {
        if (size > ra->alloc_size) {
            ra->alloc_size = align_pow2(size, 32);
            ra->p = realloc(ra->p, sizeof(Value) * ra->alloc_size);
        }
        memset(ra->p + ra->size, 0, sizeof(Value) * (ra->alloc_size - ra->size));
        ra->size = size;
    }
    return TRUE;
}

/**
 * 配列末尾に文字列を追加
 */
void refarray_push_str(RefArray *arr, const char *p, int size, RefNode *type)
{
    Value *v_tmp = refarray_push(arr);
    *v_tmp = cstr_Value(type, p, size);
}
/**
 * サイズを拡大する
 * grow : 増加サイズ
 */
static void array_grow_size(RefArray *r, int grow)
{
    int prev_size = r->size;
    int size = prev_size + grow;
    int alloc_size = r->alloc_size;

    if (size > alloc_size) {
        alloc_size = align_pow2(size, 32);
        r->p = realloc(r->p, sizeof(Value) * alloc_size);
        r->alloc_size = alloc_size;
    }
    memset(r->p + prev_size, 0, (size - prev_size) * sizeof(Value));
    r->size = size;
}
/**
 * 再帰的に配列を生成する
 */
static int array_new_sub(Value *vret, int n)
{
    int64_t size;
    Value *v = fg->stk_base + n;
    RefNode *type = Value_type(*v);
    RefArray *r;

    if (type != fs->cls_int) {
        throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_int, type, n);
        return FALSE;
    }

    size = Value_int64(*v, NULL);
    if (size < 0 || size > INT32_MAX) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number (argument #%d)", n);
        return FALSE;
    }
    if (size * sizeof(Value) > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    r = refarray_new(size);
    *vret = vp_Value(r);

    if (v + 1 < fg->stk_top) {
        Value *va = r->p;
        int i;
        for (i = 0; i < size; i++) {
            if (!array_new_sub(&va[i], n + 1)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int array_new_elems(Value *vret, Value *v, RefNode *node)
{
    int i;
    int size = fg->stk_top - fg->stk_base - 1;
    Value *src = v + 1;
    Value *dst;
    RefArray *r = refarray_new(size);
    *vret = vp_Value(r);

    dst = r->p;
    for (i = 0; i < size; i++) {
        dst[i] = Value_cp(src[i]);
    }

    return TRUE;
}
static int array_new_sized(Value *vret, Value *v, RefNode *node)
{
    if (!array_new_sub(vret, 1)) {
        return FALSE;
    }

    return TRUE;
}
static int array_dispose(Value *vret, Value *v, RefNode *node)
{
    RefArray *r = Value_vp(*v);
    int i;

    if (r->p != NULL) {
        for (i = 0; i < r->size; i++) {
            Value_dec(r->p[i]);
        }
        free(r->p);
        r->p = NULL;
    }
    return TRUE;
}

static int array_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value d = v[1];
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefArray *ra;
    uint32_t size;
    int i;

    if (!stream_read_uint32(r, &size)) {
        return FALSE;
    }
    if (size > 0xffffff) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        return FALSE;
    }
    if (size * sizeof(Value) > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    ra = refarray_new(size);
    *vret = vp_Value(ra);

    for (i = 0; i < size; i++) {
        Value_push("v", d);
        if (!call_member_func(fs->str_read, 0, TRUE)) {
            return FALSE;
        }
        fg->stk_top--;
        ra->p[i] = *fg->stk_top;
    }

    return TRUE;
}
/**
 * 0 : 要素数
 * 4 : 要素
 */
static int array_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value d = v[1];
    Value w = Value_ref(d)->v[INDEX_MARSHALDUMPER_SRC];
    RefArray *ra = Value_vp(*v);
    int i;

    ra->lock_count++;
    if (!stream_write_uint32(w, ra->size)) {
        goto ERROR_END;
    }
    for (i = 0; i < ra->size; i++) {
        Value_push("vv", d, ra->p[i]);
        if (!call_member_func(fs->str_write, 1, TRUE)) {
            goto ERROR_END;
        }
        Value_pop();
    }
    ra->lock_count--;
    return TRUE;

ERROR_END:
    ra->lock_count--;
    return FALSE;
}

/**
 * array[i]
 * 範囲外の添字の場合は例外
 */
static int array_index(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    int64_t idx = Value_int64(v[1], NULL);

    if (idx < 0) {
        idx += ra->size;
    }
    if (idx < 0 || idx >= ra->size) {
        throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], ra->size);
        return FALSE;
    }
    *vret = Value_cp(ra->p[idx]);

    return TRUE;
}
/**
 * array[i] = 100
 * 範囲外の添字の場合は例外
 */
static int array_index_set(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    int64_t idx = Value_int64(v[1], NULL);

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (idx < 0) {
        idx += ra->size;
    }
    if (idx < 0 || idx >= ra->size) {
        throw_error_select(THROW_INVALID_INDEX__VAL_INT, v[1], ra->size);
        return FALSE;
    }
    Value_dec(ra->p[idx]);
    ra->p[idx] = Value_cp(v[2]);

    return TRUE;
}
static int array_size(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    *vret = int32_Value(ra->size);
    return TRUE;
}
static int array_set_size(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    int err;
    int64_t size = Value_int64(v[1], &err);

    if (err || size < INT32_MIN || size > INT32_MAX) {
        throw_errorf(fs->mod_lang, "ValueError", "array size too large");
        return FALSE;
    }
    if (!refarray_set_size(ra, size)) {
        return FALSE;
    }
    return TRUE;
}
static int array_empty(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    *vret = bool_Value(ra->size == 0);
    return TRUE;
}

/**
 * List.cat([1, 2], [3], ...)
 */
static int array_new_cat(Value *vret, Value *v, RefNode *node)
{
    int len = 0;
    int i;
    Value *vp;
    RefArray *ra;

    for (vp = v + 1; vp < fg->stk_top; vp++) {
        RefNode *type = Value_type(*vp);
        if (type == fs->cls_list) {
            RefArray *a = Value_vp(*vp);
            len += a->size;
        } else {
            throw_errorf(fs->mod_lang, "TypeError", "List required but %n (argument #%d)", type, (int)(vp - v));
            return FALSE;
        }
    }
    ra = refarray_new(len);
    i = 0;
    for (vp = v + 1; vp < fg->stk_top; vp++) {
        RefArray *a = Value_vp(*vp);
        memcpy(ra->p + i, a->p, a->size * sizeof(Value));
        i += a->size;
    }

    for (i = 0; i < ra->size; i++) {
        Value_inc(ra->p[i]);
    }
    *vret = vp_Value(ra);

    return TRUE;
}
/**
 * [1,2] * 3 -> [1,2,1,2,1,2]
 */
static int array_mul(Value *vret, Value *v, RefNode *node)
{
    RefArray *r;
    int err = FALSE;
    RefArray *r1 = Value_vp(v[0]);
    int n = Value_int64(v[1], &err);
    int i;

    if (err || n < 0 || n > INT_MAX / sizeof(Value)) {
        throw_errorf(fs->mod_lang, "ValueError", "Illigal Array operand");
        return FALSE;
    }

    r = refarray_new(r1->size * n);
    *vret = vp_Value(r);
    for (i = 0; i < n; i++) {
        memcpy(r->p + r1->size * i, r1->p, r1->size * sizeof(Value));
    }
    for (i = 0; i < r->size; i++) {
        Value_inc(r->p[i]);
    }

    return TRUE;
}
/**
 * 部分配列を複製して返す
 */
static int array_sub(Value *vret, Value *v, RefNode *node)
{
    RefArray *r = Value_vp(*v);
    int64_t begin, end;

    if (fg->stk_top > v + 1) {
        begin = Value_int64(v[1], NULL);
        if (begin < 0) {
            begin += r->size;
        }
        if (begin < 0) {
            begin = 0;
        } else if (begin > r->size) {
            begin = r->size;
        }
    } else {
        begin = 0;
    }
    if (fg->stk_top > v + 2) {
        end = Value_int64(v[2], NULL);
        if (end < 0) {
            end += r->size;
        }
        if (end < begin) {
            end = begin;
        } else if (end > r->size) {
            end = r->size;
        }
    } else {
        end = r->size;
    }

    {
        int i;
        int size2 = end - begin;
        RefArray *r2 = refarray_new(size2);
        Value *r2p = r2->p;

        memcpy(r2p, r->p + begin, size2 * sizeof(Value));
        for (i = 0; i < size2; i++) {
            Value_inc(r2p[i]);
        }
        *vret = vp_Value(r2);
    }
    return TRUE;
}

static int array_has_key(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    int64_t idx = Value_int64(v[1], NULL);

    *vret = bool_Value(idx >= 0 && idx < ra->size);

    return TRUE;
}

/**
 * 配列の要素を返すイテレータ
 */
static int array_iterator(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    Ref *r = ref_new(cls_listiter);
    int type = FUNC_INT(node);

    ra->lock_count++;

    r->v[INDEX_LISTITER_VAL] = Value_cp(*v);
    // 最初にnextを呼び出すので、初期値を-1にする
    r->v[INDEX_LISTITER_IDX] = int32_Value(-1);
    r->v[INDEX_LISTITER_TYPE] = int32_Value(type);
    *vret = vp_Value(r);

    return TRUE;
}

/**
 * 配列の添字を返すイテレータ
 */
static int array_keys(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    RefNode *rangeiter = FUNC_VP(node);
    Ref *r = ref_new(rangeiter);

    r->v[INDEX_RANGE_BEGIN] = int32_Value(0);
    r->v[INDEX_RANGE_END] = int32_Value(ra->size);
    r->v[INDEX_RANGE_STEP] = int32_Value(1);
    *vret = vp_Value(r);

    return TRUE;
}

/**
 * 要素の後ろに追加する
 * 複数追加可能
 */
static int array_push(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    Value *v1 = v + 1;
    int argc = fg->stk_top - fg->stk_base - 1;
    int prev_size = ra->size;
    int i;

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }

    array_grow_size(ra, argc);
    memcpy(ra->p + prev_size, v1, sizeof(Value) * argc);
    for (i = 0; i < argc; i++) {
        Value_inc(v1[i]);
    }

    return TRUE;
}
/**
 * 配列の末尾の要素を除去
 * 取り除いた値を返す
 * 配列の長さが0の場合は例外
 */
static int array_pop(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (ra->size >= 1) {
        *vret = ra->p[ra->size - 1];
        ra->size--;
    } else {
        throw_errorf(fs->mod_lang, "IndexError", "Array has no elements.");
        return FALSE;
    }
    return TRUE;
}
/**
 * 要素の前に追加する
 * 複数追加可能
 */
static int array_unshift(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    Value *v1 = v + 1;
    int argc = fg->stk_top - fg->stk_base - 1;
    int prev_size = ra->size;
    int i;

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    array_grow_size(ra, argc);
    memmove(&ra->p[argc], &ra->p[0], sizeof(Value) * prev_size);
    memcpy(ra->p, v1, sizeof(Value) * argc);
    for (i = 0; i < argc; i++) {
        Value_inc(ra->p[i]);
    }

    return TRUE;
}
/**
 * 配列の先頭の要素を除去
 * 取り除いた値を返す
 * 配列の長さが0の場合は例外
 */
static int array_shift(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (ra->size >= 1) {
        *vret = ra->p[0];
        ra->size--;
        memmove(&ra->p[0], &ra->p[1], sizeof(Value) * ra->size);
    } else {
        throw_errorf(fs->mod_lang, "IndexError", "Array has no elements.");
        return FALSE;
    }
    return TRUE;
}
static int array_splice(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    RefArray *append = NULL;
    RefArray tmp;
    RefArray *ret;
    int start, len;

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (fg->stk_top > v + 3) {
        append = Value_vp(v[3]);
    } else {
        tmp.p = NULL;
        tmp.size = 0;
        append = &tmp;
    }
    calc_splice_position(&start, &len, ra->size, v);
    ret = refarray_new(len);
    *vret = vp_Value(ret);
    memcpy(ret->p, ra->p + start, len * sizeof(Value));

    if (append->size > len) {
        array_grow_size(ra, append->size - len);
        memmove(ra->p + start + append->size, ra->p + start + len, (ra->size - start - append->size) * sizeof(Value));
        memcpy(ra->p + start, append->p, append->size * sizeof(Value));
    } else {
        if (append->size < len) {
            memmove(ra->p + start + append->size, ra->p + start + len, (ra->size - start - len) * sizeof(Value));
        }
        if (append->size > 0) {
            memcpy(ra->p + start, append->p, append->size * sizeof(Value));
        }
        ra->size = ra->size + append->size - len;
    }

    return TRUE;
}
/**
 * 要素を削除してサイズを0にする
 * 確保されている配列の領域は使いまわす
 */
static int array_clear(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (ra->p != NULL) {
        int i;
        for (i = 0; i < ra->size; i++) {
            Value_dec(ra->p[i]);
        }
    }
    ra->size = 0;

    return TRUE;
}

static int array_join_sub(StrBuf *buf, Value v, int is_str)
{
    RefNode *v_type = Value_type(v);

    if (is_str) {
        if (v_type == fv->cls_strio) {
            RefBytesIO *bio = Value_vp(Value_ref(v)->v[INDEX_TEXTIO_STREAM]);
            if (!StrBuf_add(buf, bio->buf.p, bio->buf.size)) {
                return FALSE;
            }
        } else {
            if (!StrBuf_add_v(buf, v)) {
                return FALSE;
            }
        }
    } else {
        if (v_type == fs->cls_bytes) {
            RefStr *s = Value_vp(v);
            if (!StrBuf_add_r(buf, s)) {
                return FALSE;
            }
        } else if (v_type == fs->cls_bytesio) {
            RefBytesIO *bio = Value_vp(Value_ref(v)->v[INDEX_TEXTIO_STREAM]);
            if (!StrBuf_add(buf, bio->buf.p, bio->buf.size)) {
                return FALSE;
            }
        } else {
            throw_errorf(fs->mod_lang, "TypeError", "Bytes or BytesIO required but %n", v_type);
            return FALSE;
        }
    }
    return TRUE;
}
/**
 * 配列を連結して返す
 * 引数がStrの場合は全要素のto_strを結合
 * 引数がBytesの場合は、Str,Bytesはそのまま結合、BytesIOはto_bytes()を結合、それ以外はto_str()を結合
 */
static int array_join(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    RefStr *sep = Value_vp(v[1]);
    int is_str = (Value_type(v[1]) == fs->cls_str);
    StrBuf buf;
    int i;

    StrBuf_init(&buf, 0);
    for (i = 0; i < ra->size; i++) {
        if (i > 0) {
            if (!StrBuf_add_r(&buf, sep)) {
                goto ERROR_END;
            }
        }

        if (!array_join_sub(&buf, ra->p[i], is_str)) {
            goto ERROR_END;
        }
    }
    *vret = cstr_Value((is_str ? fs->cls_str : fs->cls_bytes), buf.p, buf.size);
    StrBuf_close(&buf);
    return TRUE;

ERROR_END:
    StrBuf_close(&buf);
    return FALSE;
}

int value_cmp_invoke(Value v1, Value v2, Value fn)
{
    Value v;

    if (fn != VALUE_NULL) {
        Value_push("vvv", fn, v1, v2);
        if (!call_function_obj(2)) {
            return VALUE_CMP_ERROR;
        }
    } else if (Value_isint(v1) && Value_isint(v2)) {
        int32_t i1 = Value_integral(v1);
        int32_t i2 = Value_integral(v2);
        return (i1 == i2 ? VALUE_CMP_EQ : (i1 > i2 ? VALUE_CMP_GT : VALUE_CMP_LT));
    } else {
        RefNode *v1_type = Value_type(v1);

        if (v1_type == fs->cls_str || v1_type == fs->cls_bytes) {
            RefNode *v2_type = Value_type(v2);
            int cmp;
            if (v2_type != v1_type) {
                throw_errorf(fs->mod_lang, "TypeError", "%n required but %n", v1_type, v2_type);
                return VALUE_CMP_ERROR;
            }
            cmp = refstr_cmp(Value_vp(v1), Value_vp(v2));
            if (cmp < 0) {
                return VALUE_CMP_LT;
            } else if (cmp > 0) {
                return VALUE_CMP_GT;
            } else {
                return VALUE_CMP_EQ;
            }
        }

        Value_push("vv", v1, v2);
        if (!call_member_func(fs->symbol_stock[T_CMP], 1, TRUE)) {
            return VALUE_CMP_ERROR;
        }
    }

    v = fg->stk_top[-1];
    if (Value_isint(v)) {
        int32_t iv = Value_integral(v);
        fg->stk_top--;
        return (iv == 0 ? VALUE_CMP_EQ : (iv > 0 ? VALUE_CMP_GT : VALUE_CMP_LT));
    } else if (Value_type(v) == fs->cls_int) {
        RefInt *mp = Value_vp(v);
        int ret = (mp->mp.sign == MP_NEG ? VALUE_CMP_LT : VALUE_CMP_GT);
        Value_pop();
        return ret;
    } else {
        throw_errorf(fs->mod_lang, "TypeError", "op_cmp return value must be Int but %n", Value_type(v));
        return VALUE_CMP_ERROR;
    }
}

/**
 * v[low] - v[high - 1]までの範囲をソート
 */
static int msort_sub(Value *v, int low, int high, Value fn, Value *tmp)
{
    // 要素数が少ない場合は単純なソート
    if (high - low <= MSORT_MIN) {
        int i, j;
        for (i = low + 1; i < high; i++) {
            for (j = i; j > low; j--) {
                int comp = value_cmp_invoke(v[j - 1], v[j], fn);
                if (comp == VALUE_CMP_ERROR) {
                    return FALSE;
                }
                if (comp == VALUE_CMP_GT) {
                    Value tv = v[j];
                    v[j] = v[j - 1];
                    v[j - 1] = tv;
                } else {
                    break;
                }
            }
        }
        return TRUE;
    }

    {
        int mid = (low + high) / 2;
        int i, j, k;
        if (!msort_sub(v, low, mid, fn, tmp)) {
            return FALSE;
        }
        if (!msort_sub(v, mid, high, fn, tmp)) {
            return FALSE;
        }
        // 前半の要素をそのままtmpにコピー
        memcpy(tmp + low, v + low, (mid - low) * sizeof(Value));
        // 後半の要素を逆順にtmpにコピー
        for (i = mid, j = high - 1; i < high; i++, j--) {
            tmp[i] = v[j];
        }
        // tmpの両端から取り出したデータをマージ
        i = low; j = high - 1;
        for (k = low; k < high; k++) {
            int comp = value_cmp_invoke(tmp[i], tmp[j], fn);
            if (comp == VALUE_CMP_ERROR) {
                // エラーで巻き戻った時に配列の状態が中途半端にならないようにする
                for (; k < high; k++) {
                    v[k] = tmp[i++];
                }
                return FALSE;
            }
            if (comp == VALUE_CMP_LT) {
                v[k] = tmp[i++];
            } else {
                v[k] = tmp[j--];
            }
        }
    }

    return TRUE;
}

/**
 * 自分自身をソートして自分自身を返す
 */
static int array_sort_self(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (ra->size >= 2) {
        Value *tmp;
        if (ra->size > MSORT_MIN) {
            tmp = malloc(ra->size * sizeof(Value));
        } else {
            tmp = NULL;
        }
        if (fg->stk_top > v + 1) {
            // 引数の関数をもとにソート
            if (!msort_sub(ra->p, 0, ra->size, v[1], tmp)) {
                return FALSE;
            }
        } else {
            // op_cmpをもとにソート
            if (!msort_sub(ra->p, 0, ra->size, VALUE_NULL, tmp)) {
                return FALSE;
            }
        }
        free(tmp);
    }
    *vret = Value_cp(*v);

    return TRUE;
}
/**
 * 自分自身をソートして自分自身を返す
 */
static int array_map_sub(Value *va, int size, Value fn);

static void array_dispose_all(Value *v, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        Value_dec(v[i]);
    }
}
/**
 * v[low] - v[high - 1]までの範囲をソート
 */
static int msort_by_sub(Value *v, Value *by, int low, int high, Value *tmp, Value *by_tmp)
{
    // 要素数が少ない場合は単純なソート
    if (high - low <= MSORT_MIN) {
        int i, j;
        for (i = low + 1; i < high; i++) {
            for (j = i; j > low; j--) {
                int comp = value_cmp_invoke(by[j - 1], by[j], VALUE_NULL);
                if (comp == VALUE_CMP_ERROR) {
                    return FALSE;
                }
                if (comp == VALUE_CMP_GT) {
                    Value tv = v[j];
                    v[j] = v[j - 1];
                    v[j - 1] = tv;

                    tv = by[j];
                    by[j] = by[j - 1];
                    by[j - 1] = tv;
                } else {
                    break;
                }
            }
        }
        return TRUE;
    }

    {
        int mid = (low + high) / 2;
        int i, j, k;
        if (!msort_by_sub(v, by, low, mid, tmp, by_tmp)) {
            return FALSE;
        }
        if (!msort_by_sub(v, by, mid, high, tmp, by_tmp)) {
            return FALSE;
        }
        // 前半の要素をそのままtmpにコピー
        memcpy(tmp + low, v + low, (mid - low) * sizeof(Value));
        memcpy(by_tmp + low, by + low, (mid - low) * sizeof(Value));
        // 後半の要素を逆順にtmpにコピー
        for (i = mid, j = high - 1; i < high; i++, j--) {
            tmp[i] = v[j];
            by_tmp[i] = by[j];
        }
        // tmpの両端から取り出したデータをマージ
        i = low; j = high - 1;
        for (k = low; k < high; k++) {
            int comp = value_cmp_invoke(by_tmp[i], by_tmp[j], VALUE_NULL);
            if (comp == VALUE_CMP_ERROR) {
                // エラーで巻き戻った時に配列の状態が中途半端にならないようにする
                for (; k < high; k++) {
                    v[k] = tmp[i];
                    by[k] = by_tmp[i];
                    i++;
                }
                return FALSE;
            }
            if (comp == VALUE_CMP_LT) {
                v[k] = tmp[i];
                by[k] = by_tmp[i];
                i++;
            } else {
                v[k] = tmp[j];
                by[k] = by_tmp[j];
                j--;
            }
        }
    }

    return TRUE;
}
static int array_sort_by_self(Value *vret, Value *v, RefNode *node)
{
    Value *by = NULL;
    Value *tmp = NULL;
    Value *by_tmp = NULL;
    RefArray *ra = Value_vp(*v);

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (ra->size >= 2) {
        int i;
        int size = ra->size;

        by = malloc(size * sizeof(Value));

        if (size > MSORT_MIN) {
            tmp = malloc(size * sizeof(Value));
            by_tmp = malloc(size * sizeof(Value));
        }
        // ソートキーを作成
        memcpy(by, ra->p, size * sizeof(Value));
        for (i = 0; i < size; i++) {
            Value_inc(by[i]);
        }
        if (!array_map_sub(by, size, v[1])) {
            goto ERROR_END;
        }
        if (!msort_by_sub(ra->p, by, 0, ra->size, tmp, by_tmp)) {
            goto ERROR_END;
        }

        free(tmp);
        free(by_tmp);
        array_dispose_all(by, size);
        free(by);
    }
    *vret = Value_cp(*v);

    return TRUE;

ERROR_END:
    if (by != NULL) {
        array_dispose_all(by, ra->size);
        free(by);
    }
    free(by_tmp);
    free(tmp);
    return FALSE;
}
/**
 * 自分自身を入れ替えて自分自身を返す
 */
static int array_reverse_self(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    int i;
    int len = ra->size;
    int n = len / 2;

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    // 前と後ろを入れ替え
    for (i = 0; i < n; i++) {
        Value tmp = ra->p[i];
        ra->p[i] = ra->p[len - i - 1];
        ra->p[len - i - 1] = tmp;
    }
    *vret = Value_cp(*v);

    return TRUE;
}
static int array_map_sub(Value *va, int size, Value fn)
{
    int i;

    for (i = 0; i < size; i++) {
        Value_push("vv", fn, va[i]);
        if (!call_function_obj(1)) {
            return FALSE;
        }
        Value_dec(va[i]);
        fg->stk_top--;
        va[i] = *fg->stk_top;
    }
    return TRUE;
}
/**
 * 自分自身を書き換えて自分自身を返す
 */
static int array_map_self(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (!array_map_sub(ra->p, ra->size, v[1])) {
        return FALSE;
    }
    *vret = Value_cp(*v);

    return TRUE;
}
/**
 * 自分自身を書き換えて自分自身を返す
 */
static int array_select_self(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    Value fn = v[1];
    int i, j = 0;

    if (ra->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }

    for (i = 0; i < ra->size; i++) {
        Value_push("vv", fn, ra->p[i]);
        if (!call_function_obj(1)) {
            return FALSE;
        }
        // 戻り値がtrueなら残す
        // falseなら削除
        // 途中で例外が発生しても参照カウンタが狂わないようにする
        fg->stk_top--;
        if (Value_bool(*fg->stk_top)) {
            Value_dec(*fg->stk_top);

            if (j < i) {
                ra->p[j] = ra->p[i];
                ra->p[i] = VALUE_NULL;
            }
            j++;
        } else {
            Value_dec(ra->p[i]);
            ra->p[i] = VALUE_NULL;
        }
    }
    ra->size = j;
    *vret = Value_cp(*v);

    return TRUE;
}

/**
 * 検索して見つかった場合/見つからなかった場合は
 * FALSE.true/falseを返す
 * TRUE.index/nullを返す
 */
static int array_index_of(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = Value_vp(*v);
    int ret_index = FUNC_INT(node);
    Value v1 = v[1];
    int size = ra->size;
    int i;

    if (Value_isint(v1)) {
        for (i = 0; i < size; i++) {
            Value va = ra->p[i];
            if (v1 == va) {
                break;
            }
        }
    } else {
        RefNode *type = Value_type(v1);
        if (type == fs->cls_str || fs->cls_bytes) {
            RefStr *r1 = Value_vp(v1);
            for (i = 0; i < size; i++) {
                Value va = ra->p[i];
                if (Value_type(va) == type && refstr_eq(Value_vp(va), r1)) {
                    break;
                }
            }
        } else {
            RefNode *fn_eq = Hash_get_p(&type->u.c.h, fs->symbol_stock[T_EQ]);
            if (fn_eq == NULL) {
                throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, type, fs->symbol_stock[T_EQ]);
                return FALSE;
            }

            for (i = 0; i < size; i++) {
                Value va = ra->p[i];
                if (Value_type(va) == type) {
                    if (type == fs->cls_str || type == fs->cls_bytes) {
                        if (refstr_eq(Value_vp(v1), Value_vp(va))) {
                            break;
                        }
                    } else {
                        Value_push("vv", v1, va);
                        if (!call_function(fn_eq, 1)) {
                            return FALSE;
                        }
                        fg->stk_top--;
                        if (Value_bool(*fg->stk_top)) {
                            Value_dec(*fg->stk_top);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (ret_index) {
        if (i < size) {
            *vret = int32_Value(i);
        }
    } else {
        *vret = bool_Value(i < size);
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int arrayiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefArray *ra = Value_vp(r->v[INDEX_LISTITER_VAL]);
    int type = Value_integral(r->v[INDEX_LISTITER_TYPE]);
    int idx = Value_integral(r->v[INDEX_LISTITER_IDX]);

    idx++;
    if (idx < ra->size) {
        if (type == ITERATOR_BOTH) {
            Ref *r2 = ref_new(fv->cls_entry);
            Value v2 = ra->p[idx];

            r2->v[INDEX_ENTRY_KEY] = int32_Value(idx);
            r2->v[INDEX_ENTRY_VAL] = Value_cp(v2);
            *vret = vp_Value(r2);
        } else {
            *vret = Value_cp(ra->p[idx]);
        }
        r->v[INDEX_LISTITER_IDX] = int32_Value(idx);
    } else {
        throw_stopiter();
        return FALSE;
    }

    return TRUE;
}
static int arrayiter_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefArray *ra = Value_vp(r->v[INDEX_LISTITER_VAL]);
    ra->lock_count--;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

int pairvalue_hash(Value *vret, Value *v, RefNode *node)
{
    int32_t h1, h2;
    Ref *r = Value_ref(*v);

    if (!Value_hash(r->v[0], &h1) || !Value_hash(r->v[1], &h2)) {
        return FALSE;
    }
    *vret = int32_Value(h1 ^ h2);

    return TRUE;
}
int pairvalue_eq(Value *vret, Value *v, RefNode *node)
{
    Ref *r1 = Value_ref(*v);
    Ref *r2 = Value_ref(v[1]);

    RefNode *t_k1 = Value_type(r1->v[0]);
    RefNode *t_v1 = Value_type(r1->v[1]);
    RefNode *t_k2 = Value_type(r2->v[0]);
    RefNode *t_v2 = Value_type(r2->v[1]);
    int result;

    if (t_k1 == t_k2 && t_v1 == t_v2) {
        int ret;
        if (!Value_eq(r1->v[0], r2->v[0], &ret)) {
            return FALSE;
        }
        if (ret) {
            if (!Value_eq(r1->v[1], r2->v[1], &result)) {
                return FALSE;
            }
        } else {
            result = FALSE;
        }
    } else {
        result = FALSE;
    }
    *vret = bool_Value(result);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

int range_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = ref_new(fs->cls_range);
    Value v1 = v[1];
    Value v2 = v[2];
    RefNode *v1_type = Value_type(v1);
    RefNode *v2_type = Value_type(v2);

    *vret = vp_Value(r);

    if (v1_type != v2_type) {
        throw_errorf(fs->mod_lang, "TypeError", "Both arguments are different type (%n and %n)", v1_type, v2_type);
        return FALSE;
    }
    if (Hash_get_p(&v1_type->u.c.h, fs->symbol_stock[T_CMP]) == NULL) {
        throw_errorf(fs->mod_lang, "TypeError", "%n has no op_cmp method", v1_type);
        return FALSE;
    }

    r->v[INDEX_RANGE_BEGIN] = Value_cp(v1);
    r->v[INDEX_RANGE_END] = Value_cp(v2);

    switch (value_cmp_invoke(r->v[INDEX_RANGE_BEGIN], r->v[INDEX_RANGE_END], VALUE_NULL)) {
    case VALUE_CMP_ERROR:
        return FALSE;
    case VALUE_CMP_GT:
        r->v[INDEX_RANGE_IS_DEC] = VALUE_TRUE;
        break;
    default:
        r->v[INDEX_RANGE_IS_DEC] = VALUE_FALSE;
        break;
    }
    if (fg->stk_top > v + 3) {
        if (Value_bool(v[3])) {
            r->v[INDEX_RANGE_OPEN] = VALUE_TRUE;
        }
        if (fg->stk_top > v + 4) {
            // step
            r->v[INDEX_RANGE_STEP] = Value_cp(v[4]);
        }
    }
    return TRUE;
}
int range_marshal_read(Value *vret, Value *v, RefNode *node)
{
    int i;
    char c;
    int rd_size;

    Ref *r = ref_new(fs->cls_range);
    Value d = v[1];
    Value rd = Value_ref(*v)->v[INDEX_MARSHALDUMPER_SRC];

    *vret = vp_Value(r);

    for (i = 0; i < INDEX_RANGE_IS_DEC; i++) {
        Value_push("v", d);
        if (!call_member_func(fs->str_read, 0, TRUE)) {
            return FALSE;
        }
        fg->stk_top--;
        r->v[i] = *fg->stk_top;
    }
    rd_size = 1;
    if (!stream_read_data(rd, NULL, &c, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    r->v[INDEX_RANGE_IS_DEC] = bool_Value(c);

    rd_size = 1;
    if (!stream_read_data(rd, NULL, &c, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    r->v[INDEX_RANGE_OPEN] = bool_Value(c);

    return TRUE;
}
int range_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value d = v[1];
    Value w = Value_ref(*v)->v[INDEX_MARSHALDUMPER_SRC];
    int i;
    char is_dec = Value_bool(r->v[INDEX_RANGE_IS_DEC]) ? 1 : 0;
    char is_open = Value_bool(r->v[INDEX_RANGE_OPEN]) ? 1 : 0;

    for (i = 0; i < INDEX_RANGE_IS_DEC; i++) {
        Value_push("vv", d, r->v[i]);
        if (!call_member_func(fs->str_write, 1, TRUE)) {
            return FALSE;
        }
        Value_pop();
    }
    if (!stream_write_data(w, &is_dec, 1)) {
        return FALSE;
    }
    if (!stream_write_data(w, &is_open, 1)) {
        return FALSE;
    }

    return TRUE;
}

static int range_tostr(Value *vret, Value *v, RefNode *node)
{
    Value *pair = ((Ref*)Value_vp(*v))->v;
    StrBuf buf;

    StrBuf_init(&buf, 0);
    if (!StrBuf_printf(&buf, "Range(begin=%v, end=%v", pair[INDEX_RANGE_BEGIN], pair[INDEX_RANGE_END])) {
        goto ERROR_END;
    }

    if (pair[INDEX_RANGE_STEP] != VALUE_NULL) {
        if (!StrBuf_printf(&buf, ", step=%v", pair[INDEX_RANGE_STEP])) {
            goto ERROR_END;
        }
    }
    if (Value_bool(pair[INDEX_RANGE_OPEN])) {
        if (!StrBuf_add(&buf, ", open", 6)) {
            goto ERROR_END;
        }
    }
    if (!StrBuf_add_c(&buf, ')')) {
        goto ERROR_END;
    }

    *vret = cstr_Value(fs->cls_str, buf.p, buf.size);
    StrBuf_close(&buf);

    return TRUE;

ERROR_END:
    StrBuf_close(&buf);
    return FALSE;
}
static int range_empty(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int dec = Value_bool(r->v[INDEX_RANGE_IS_DEC]);

    switch (value_cmp_invoke(r->v[INDEX_RANGE_BEGIN], r->v[INDEX_RANGE_END], VALUE_NULL)) {
    case VALUE_CMP_ERROR:
        return FALSE;
    case VALUE_CMP_LT:
        *vret = bool_Value(dec);
        break;
    case VALUE_CMP_GT:
        *vret = bool_Value(dec);
        break;
    default:
        *vret = VALUE_FALSE;
        break;
    }

    return TRUE;
}

static int range_in(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value v1 = v[1];
    int ret = TRUE;
    Value begin;
    Value end;

    if (Value_bool(r->v[INDEX_RANGE_IS_DEC])) {
        begin = r->v[INDEX_RANGE_END];
        end = r->v[INDEX_RANGE_BEGIN];
    } else {
        begin = r->v[INDEX_RANGE_BEGIN];
        end = r->v[INDEX_RANGE_END];
    }

    switch (value_cmp_invoke(v1, begin, VALUE_NULL)) {
    case VALUE_CMP_ERROR:
        return FALSE;
    case VALUE_CMP_LT:
        ret = FALSE;
        break;
    }

    switch (value_cmp_invoke(v1, end, VALUE_NULL)) {
    case VALUE_CMP_ERROR:
        return FALSE;
    case VALUE_CMP_EQ:
        ret = Value_bool(r->v[INDEX_RANGE_OPEN]);
        break;
    case VALUE_CMP_GT:
        ret = FALSE;
        break;
    }

    *vret = bool_Value(ret);
    return TRUE;
}

static int range_iter(Value *vret, Value *v, RefNode *node)
{
    int i;
    Ref *r = Value_ref(*v);
    RefNode *rangeiter = FUNC_VP(node);
    Ref *r2 = ref_new(rangeiter);
    Value *r2v = r2->v;

    *vret = vp_Value(r2);

    memcpy(r2v, r->v, sizeof(Value) * INDEX_RANGE_NUM);
    for (i = 0; i < INDEX_RANGE_NUM; i++) {
        Value_inc(r2v[i]);
    }

    return TRUE;
}

static int range_get(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int idx = FUNC_INT(node);
    *vret = Value_cp(r->v[idx]);
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int rangeiter_inf(Value *vret, Value *v, RefNode *node)
{
    RefNode *rangeiter = FUNC_VP(node);
    Ref *r = ref_new(rangeiter);
    *vret = vp_Value(r);

    r->v[INDEX_RANGE_BEGIN] = Value_cp(v[1]);
    if (fg->stk_top > v + 2) {
        r->v[INDEX_RANGE_STEP] = Value_cp(v[2]);
    }
    return TRUE;
}
static int rangeiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value *pbegin = &r->v[INDEX_RANGE_BEGIN];
    Value begin = *pbegin;
    Value end = r->v[INDEX_RANGE_END];
    int is_open = Value_bool(r->v[INDEX_RANGE_OPEN]);
    int dec = Value_bool(r->v[INDEX_RANGE_IS_DEC]);

    if (Value_integral(r->v[INDEX_RANGE_NEXT]) == 2) {
        Value step = r->v[INDEX_RANGE_STEP];

        if (Value_isint(begin) && Value_isint(end) && step == VALUE_NULL) {
            // Int型の場合だけ最適化
            int32_t ibegin = Value_integral(begin) + (dec ? -1 : 1);
            int32_t iend = Value_integral(end);

            if ((dec ? ibegin > iend : ibegin < iend) || (is_open && ibegin == iend)) {
                *vret = int32_Value(ibegin);
                *pbegin = int32_Value(ibegin);
            } else {
                goto STOP_ITER;
            }
            return TRUE;
        } else {
            if (step == VALUE_NULL) {
                // iter++
                RefStr *sym = fs->symbol_stock[dec ? T_DEC : T_INC];
                Value_push("v", begin);
                if (!call_member_func(sym, 0, TRUE)) {
                    return FALSE;
                }
                Value_dec(*pbegin);
                fg->stk_top--;
                *pbegin = *fg->stk_top;
            } else {
                // iter += step
                Value_push("vv", begin, step);
                if (!call_member_func(fs->symbol_stock[T_ADD], 1, TRUE)) {
                    return FALSE;
                }
                Value_dec(*pbegin);
                fg->stk_top--;
                *pbegin = *fg->stk_top;
            }
            begin = *pbegin;
        }
    } else {
        r->v[INDEX_RANGE_NEXT] = int32_Value(2);
    }

    // endに達していたらfalseを返す
    if (end != VALUE_NULL) {
        switch (value_cmp_invoke(begin, end, VALUE_NULL)) {
        case VALUE_CMP_ERROR:
            return FALSE;
        case VALUE_CMP_LT:
            if (dec) {
                goto STOP_ITER;
            } else {
                *vret = Value_cp(begin);
            }
            break;
        case VALUE_CMP_GT:
            if (dec) {
                *vret = Value_cp(begin);
            } else {
                goto STOP_ITER;
            }
            break;
        case VALUE_CMP_EQ:
            if (is_open) {
                *vret = Value_cp(begin);
            } else {
                goto STOP_ITER;
            }
            break;
        default:
            goto STOP_ITER;
        }
    } else {
        *vret = Value_cp(begin);
    }

    return TRUE;

STOP_ITER:
    throw_stopiter();
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int iterator_new(Value *vret, Value *v, RefNode *node)
{
    // 必ず継承先でnewするので、初期化されているはず
    *vret = Value_cp(*v);
    return TRUE;
}
static int iterator_to_list(Value *vret, Value *v, RefNode *node)
{
    int max_size = fs->max_alloc / sizeof(Value);
    RefArray *ra = refarray_new(0);
    *vret = vp_Value(ra);

    for (;;) {
        Value *va;
        
        if (ra->size >= max_size) {
            throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }

        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        va = refarray_push(ra);
        fg->stk_top--;
        *va = *fg->stk_top;
    }
    return TRUE;
}
static int iterator_to_set(Value *vret, Value *v, RefNode *node)
{
    int max_size = fs->max_alloc / sizeof(Value);
    int count = 0;

    RefMap *rm = refmap_new(0);
    *vret = vp_Value(rm);
    rm->rh.type = fs->cls_set;

    for (;;) {
        count++;
        if (count >= max_size) {
            throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
            return FALSE;
        }
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }

        if (refmap_add(rm, fg->stk_top[-1], TRUE, FALSE) == NULL) {
            return FALSE;
        }
        Value_pop();
    }

    return TRUE;
}
static int iterator_join(Value *vret, Value *v, RefNode *node)
{
    RefStr *sep = Value_vp(v[1]);
    int is_str = (Value_type(v[1]) == fs->cls_str);

    StrBuf buf;
    int first = TRUE;

    StrBuf_init(&buf, 0);
    for (;;) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        if (!first) {
            if (!StrBuf_add_r(&buf, sep)) {
                goto ERROR_END;
            }
        } else {
            first = FALSE;
        }
        if (!array_join_sub(&buf, fg->stk_top[-1], is_str)) {
            goto ERROR_END;
        }
        Value_pop();
    }
    *vret = cstr_Value((is_str ? fs->cls_str : fs->cls_bytes), buf.p, buf.size);
    StrBuf_close(&buf);
    return TRUE;

ERROR_END:
    StrBuf_close(&buf);
    return FALSE;
}
static int iterator_count(Value *vret, Value *v, RefNode *node)
{
    int count = 0;
    Value v1 = v[1];

    for (;;) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        Value_push("v", v1);
        if (!call_member_func(fs->symbol_stock[T_EQ], 1, TRUE)) {
            return FALSE;
        }
        if (Value_bool(fg->stk_top[-1])) {
            count++;
            Value_pop();
        } else {
            fg->stk_top--;
        }
    }
    *vret = int32_Value(count);
    return TRUE;
}
static int iterator_count_if(Value *vret, Value *v, RefNode *node)
{
    int count = 0;
    Value *v_fn = v + 1;

    for (;;) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        fg->stk_top[0] = fg->stk_top[-1];
        fg->stk_top[-1] = *v_fn;
        Value_inc(*v_fn);
        fg->stk_top += 1;
        if (!call_function_obj(1)) {
            return FALSE;
        }
        if (Value_bool(fg->stk_top[-1])) {
            count++;
            Value_pop();
        } else {
            fg->stk_top--;
        }
    }
    *vret = int32_Value(count);
    return TRUE;
}
static int iterator_find_if(Value *vret, Value *v, RefNode *node)
{
    Value *v_fn = v + 1;

    for (;;) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        fg->stk_top[1] = fg->stk_top[-1];
        fg->stk_top[0] = *v_fn;
        Value_inc(fg->stk_top[-1]);
        Value_inc(*v_fn);
        fg->stk_top += 2;
        if (!call_function_obj(1)) {
            return FALSE;
        }
        if (Value_bool(fg->stk_top[-1])) {
            Value_pop();
            fg->stk_top--;
            *vret = *fg->stk_top;
            return TRUE;
        } else {
            fg->stk_top--;
            Value_pop();
        }
    }
    return TRUE;
}
static int iterator_skip(Value *vret, Value *v, RefNode *node)
{
    int last = Value_int64(v[1], NULL);
    if (last < 0) {
        throw_errorf(fs->mod_lang, "ValueError", "Argument #1 must >= 0");
        return FALSE;
    }

    while (last > 0) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        Value_pop();
        last--;
    }
    *vret = Value_cp(*v);

    return TRUE;
}
static int iterator_map_select(Value *vret, Value *v, RefNode *node)
{
    RefNode *cls_itersub = FUNC_VP(node);
    Ref *r = ref_new(cls_itersub);

    r->v[INDEX_ITERFILTER_ITER] = Value_cp(v[0]);
    r->v[INDEX_ITERFILTER_FUNC] = Value_cp(v[1]);
    *vret = vp_Value(r);

    return TRUE;
}
static int iterator_reduce(Value *vret, Value *v, RefNode *node)
{
    Value *result;

    Value_push("vv", v[2], v[1]);
    result = &fg->stk_top[-1];

    for (;;) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        if (!call_function_obj(2)) {
            return FALSE;
        }
        fg->stk_top[0] = fg->stk_top[-1];
        fg->stk_top[-1] = Value_cp(v[2]);
        fg->stk_top++;
    }

    *vret = *result;
    *result = VALUE_NULL;
    return TRUE;
}
static int iterator_all_any(Value *vret, Value *v, RefNode *node)
{
    Value *v_fn = v + 1;
    int all = FUNC_INT(node);

    for (;;) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        fg->stk_top[0] = fg->stk_top[-1];
        fg->stk_top[-1] = *v_fn;
        Value_inc(*v_fn);
        fg->stk_top += 1;
        if (!call_function_obj(1)) {
            return FALSE;
        }
        if (Value_bool(fg->stk_top[-1])) {
            Value_pop();
            if (!all) {
                *vret = VALUE_TRUE;
                return TRUE;
            }
        } else {
            fg->stk_top--;
            if (all) {
                *vret = VALUE_FALSE;
                return TRUE;
            }
        }
    }
    *vret = bool_Value(all);
    return TRUE;
}
static int iterator_each(Value *vret, Value *v, RefNode *node)
{
    Value *v_fn = v + 1;

    for (;;) {
        Value_push("v", *v);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            if (Value_type(fg->error) == fs->cls_stopiter) {
                // throw StopIteration
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                break;
            } else {
                return FALSE;
            }
        }
        fg->stk_top[0] = fg->stk_top[-1];
        fg->stk_top[-1] = *v_fn;
        Value_inc(*v_fn);
        fg->stk_top += 1;
        if (!call_function_obj(1)) {
            return FALSE;
        }
        Value_pop();
    }
    return TRUE;
}
static int iterator_sort(Value *vret, Value *v, RefNode *node)
{
    RefStr *sym = FUNC_VP(node);
    int argc = (fg->stk_top > v + 1 ? 1 : 0);

    Value_push("v", *v);
    if (!call_member_func(intern("to_list", -1), 0, TRUE)) {
        return FALSE;
    }

    if (argc >= 1) {
        Value_push("v", v[1]);
    }
    if (!call_member_func(sym, argc, TRUE)) {
        return FALSE;
    }
    fg->stk_top--;
    *vret = *fg->stk_top;

    return TRUE;
}
static int itermap_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);

    Value_push("vv", r->v[INDEX_ITERFILTER_FUNC], r->v[INDEX_ITERFILTER_ITER]);
    if (!call_member_func(fs->str_next, 0, TRUE)) {
        return FALSE;
    }
    if (!call_function_obj(1)) {
        return FALSE;
    }
    fg->stk_top--;
    *vret = *fg->stk_top;

    return TRUE;
}
static int iterfilter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value it = r->v[INDEX_ITERFILTER_ITER];
    Value fn = r->v[INDEX_ITERFILTER_FUNC];

    for (;;) {
        Value *cur;

        Value_push("v", it);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            return FALSE;
        }
        cur = &fg->stk_top[-1];
        Value_push("vv", fn, *cur);
        if (!call_function_obj(1)) {
            return FALSE;
        }
        if (Value_bool(fg->stk_top[-1])) {
            *vret = Value_cp(*cur);
            return TRUE;
        }
        Value_pop();
        Value_pop();
    }
}
static int iterlimit_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    Value it = r->v[INDEX_ITERFILTER_ITER];
    int last = Value_integral(r->v[INDEX_ITERFILTER_FUNC]);

    if (last > 0) {
        last--;
        r->v[INDEX_ITERFILTER_FUNC] = int32_Value(last);

        Value_push("v", it);
        if (!call_member_func(fs->str_next, 0, TRUE)) {
            return FALSE;
        }
        fg->stk_top--;
        *vret = *fg->stk_top;
        return TRUE;
    } else {
        throw_stopiter();
        return FALSE;
    }
}
static int generator_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int pc = Value_integral(r->v[INDEX_GENERATOR_PC]);
    int nstack = Value_integral(r->v[INDEX_GENERATOR_NSTACK]);
    RefNode *func = Value_vp(r->v[INDEX_GENERATOR_FUNC]);
    Value *stk_base = fg->stk_base;

    if (pc == 0) {
        throw_stopiter();
        return FALSE;
    }

    fg->stk_base = fg->stk_top;
    memcpy(fg->stk_base, &r->v[INDEX_GENERATOR_LOCAL], nstack * sizeof(Value));
    memset(&r->v[INDEX_GENERATOR_LOCAL], 0, nstack * sizeof(Value));
    fg->stk_top = fg->stk_base + nstack;

    if (!invoke_code(func, pc)) {
        r->v[INDEX_GENERATOR_PC] = int32_Value(0);
        return FALSE;
    }
    *vret = *fg->stk_base;
    fg->stk_base = stk_base;
    fg->stk_top = stk_base + 1;

    return TRUE;
}
static int iterable_new(Value *vret, Value *v, RefNode *node)
{
    *vret = Value_cp(*v);
    return TRUE;
}
static int iterable_invoke(Value *vret, Value *v, RefNode *node)
{
    int argc = fg->stk_top - v - 1;
    int i;

    Value_push("v", *v);
    if (!call_member_func(fs->str_iterator, 0, TRUE)) {
        return FALSE;
    }

    for (i = 1; i <= argc; i++) {
        Value_push("v", v[i]);
    }
    if (!call_member_func(node->name, argc, TRUE)) {
        return FALSE;
    }
    fg->stk_top--;
    *vret = *fg->stk_top;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

void define_lang_col_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    RefStr *empty = intern("empty", -1);
    RefStr *size = intern("size", -1);

    RefNode *cls_iterselect = define_identifier(m, m, "IteratorSelect", NODE_CLASS, 0);
    RefNode *cls_itermap = define_identifier(m, m, "IteratorMap", NODE_CLASS, 0);
    RefNode *cls_iterlimit = define_identifier(m, m, "IteratorLimit", NODE_CLASS, 0);
    RefNode *cls_rangeiter = define_identifier(m, m, "RangeIter", NODE_CLASS, 0);
    cls_listiter = define_identifier(m, m, "ListIter", NODE_CLASS, 0);


    // Iterator
    cls = fs->cls_iterator;
    n = define_identifier(m, cls, "_new", NODE_NEW_N, 0);
    define_native_func_a(n, iterator_new, 0, 0, NULL);

    n = define_identifier(m, cls, "to_list", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_to_list, 0, 0, NULL);
    n = define_identifier(m, cls, "to_set", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_to_set, 0, 0, NULL);
    n = define_identifier(m, cls, "join", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_join, 1, 1, NULL, fs->cls_sequence);
    n = define_identifier(m, cls, "count", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_count, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "count_if", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_count_if, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "find_if", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_find_if, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "skip", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_skip, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "limit", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_map_select, 1, 1, cls_iterlimit, fs->cls_int);
    n = define_identifier(m, cls, "map", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_map_select, 1, 1, cls_itermap, fs->cls_fn);
    n = define_identifier(m, cls, "select", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_map_select, 1, 1, cls_iterselect, fs->cls_fn);
    n = define_identifier(m, cls, "reduce", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_reduce, 2, 2, NULL, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "all", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_all_any, 1, 1, (void*)TRUE, fs->cls_fn);
    n = define_identifier(m, cls, "any", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_all_any, 1, 1, (void*)FALSE, fs->cls_fn);
    n = define_identifier(m, cls, "each", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_each, 1, 1, cls_itermap, fs->cls_fn);
    n = define_identifier(m, cls, "sort", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_sort, 0, 1, intern("sort_self", -1), fs->cls_fn);
    n = define_identifier(m, cls, "sort_by", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_sort, 0, 1, intern("sort_by_self", -1), fs->cls_fn);
    n = define_identifier(m, cls, "reverse", NODE_FUNC_N, 0);
    define_native_func_a(n, iterator_sort, 0, 1, intern("reverse_self", -1), fs->cls_fn);
    extends_method(cls, fs->cls_obj);


    // IteratorMap
    cls = cls_itermap;
    cls->u.c.n_memb = INDEX_ITERFILTER_NUM;
    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, itermap_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);

    // IteratorSelect
    cls = cls_iterselect;
    cls->u.c.n_memb = INDEX_ITERFILTER_NUM;
    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, iterfilter_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);

    // IteratorLimit
    cls = cls_iterlimit;
    cls->u.c.n_memb = INDEX_ITERFILTER_NUM;
    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, iterlimit_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);

    // Generator
    cls = fv->cls_generator;
    n = define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    define_native_func_a(n, generator_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);


    // Iterable
    cls = fs->cls_iterable;
    n = define_identifier(m, cls, "_new", NODE_NEW_N, 0);
    define_native_func_a(n, iterable_new, 0, 0, NULL);

    n = define_identifier(m, cls, "to_list", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 0, 0, NULL);
    n = define_identifier(m, cls, "to_set", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 0, 0, NULL);
    n = define_identifier(m, cls, "join", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_sequence);
    n = define_identifier(m, cls, "count", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "count_if", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "find_if", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "map", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "select", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "each", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "sort", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 0, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "sort_by", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 0, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "reverse", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 0, 0, NULL);
    n = define_identifier(m, cls, "reduce", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 2, 2, NULL, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "all", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "any", NODE_FUNC_N, 0);
    define_native_func_a(n, iterable_invoke, 1, 1, NULL, fs->cls_fn);
    extends_method(cls, fs->cls_obj);


    // List
    cls = fs->cls_list;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, array_new_elems, 0, -1, NULL);
    fv->func_array_new = n;
    n = define_identifier(m, cls, "sized", NODE_NEW_N, 0);
    define_native_func_a(n, array_new_sized, 1, -1, NULL, NULL);
    n = define_identifier(m, cls, "cat", NODE_NEW_N, 0);
    define_native_func_a(n, array_new_cat, 0, -1, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, array_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, array_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, col_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, array_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, col_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, col_eq, 1, 1, NULL, fs->cls_list);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, list_cmp, 1, 1, NULL, fs->cls_list);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, array_index, 1, 1, NULL, fs->cls_int);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    define_native_func_a(n, array_index_set, 2, 2, NULL, fs->cls_int, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_MUL], NODE_FUNC_N, 0);
    define_native_func_a(n, array_mul, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "sub", NODE_FUNC_N, 0);
    define_native_func_a(n, array_sub, 1, 2, NULL, fs->cls_int, fs->cls_int);
    n = define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    define_native_func_a(n, array_sub, 0, 0, NULL);
    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, array_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, size, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, array_size, 0, 0, NULL);
    n = define_identifier(m, cls, "size=", NODE_FUNC_N, 0);
    define_native_func_a(n, array_set_size, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "index_of", NODE_FUNC_N, 0);
    define_native_func_a(n, array_index_of, 1, 1, (void*) TRUE, NULL);
    n = define_identifier(m, cls, "has_key", NODE_FUNC_N, 0);
    define_native_func_a(n, array_has_key, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "has_value", NODE_FUNC_N, 0);
    define_native_func_a(n, array_index_of, 1, 1, (void*) FALSE, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, array_index_of, 1, 1, (void*) FALSE, NULL);
    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, array_iterator, 0, 0, (void*) ITERATOR_VAL);
    n = define_identifier(m, cls, "entries", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, array_iterator, 0, 0, (void*) ITERATOR_BOTH);
    n = define_identifier(m, cls, "keys", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, array_keys, 0, 0, cls_rangeiter);
    n = define_identifier(m, cls, "values", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, array_iterator, 0, 0, cls_rangeiter);
    n = define_identifier(m, cls, "push", NODE_FUNC_N, 0);
    define_native_func_a(n, array_push, 1, -1, NULL, NULL);
    n = define_identifier(m, cls, "pop", NODE_FUNC_N, 0);
    define_native_func_a(n, array_pop, 0, 0, NULL, NULL);
    n = define_identifier(m, cls, "unshift", NODE_FUNC_N, 0);
    define_native_func_a(n, array_unshift, 1, -1, NULL, NULL);
    n = define_identifier(m, cls, "shift", NODE_FUNC_N, 0);
    define_native_func_a(n, array_shift, 0, 0, NULL, NULL);
    n = define_identifier(m, cls, "splice", NODE_FUNC_N, 0);
    define_native_func_a(n, array_splice, 0, 3, NULL, fs->cls_int, fs->cls_int, fs->cls_list);
    n = define_identifier(m, cls, "clear", NODE_FUNC_N, 0);
    define_native_func_a(n, array_clear, 0, 0, NULL, NULL);
    n = define_identifier(m, cls, "join", NODE_FUNC_N, 0);
    define_native_func_a(n, array_join, 1, 1, NULL, fs->cls_sequence);
    n = define_identifier(m, cls, "sort_self", NODE_FUNC_N, 0);
    define_native_func_a(n, array_sort_self, 0, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "sort_by_self", NODE_FUNC_N, 0);
    define_native_func_a(n, array_sort_by_self, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "reverse_self", NODE_FUNC_N, 0);
    define_native_func_a(n, array_reverse_self, 0, 0, NULL);
    n = define_identifier(m, cls, "map_self", NODE_FUNC_N, 0);
    define_native_func_a(n, array_map_self, 1, 1, NULL, fs->cls_fn);
    n = define_identifier(m, cls, "select_self", NODE_FUNC_N, 0);
    define_native_func_a(n, array_select_self, 1, 1, NULL, fs->cls_fn);
    extends_method(cls, fs->cls_iterable);

    // ListIter
    cls = cls_listiter;
    cls->u.c.n_memb = INDEX_LISTITER_NUM;
    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, arrayiter_dispose, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_next, NODE_FUNC_N, 0);
    define_native_func_a(n, arrayiter_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);

    // Range
    cls = fs->cls_range;
    cls->u.c.n_memb = INDEX_RANGE_NUM;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, range_new, 2, 4, cls, NULL, NULL, fs->cls_bool, NULL);
    fv->func_range_new = n;
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, range_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, range_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, range_marshal_write, 1, 1, (void*) FALSE, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, range_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, pairvalue_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, pairvalue_eq, 1, 1, NULL, fs->cls_range);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, range_in, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, range_iter, 0, 0, cls_rangeiter);
    n = define_identifier(m, cls, "begin", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, range_get, 0, 0, (void*) INDEX_RANGE_BEGIN);
    n = define_identifier(m, cls, "end", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, range_get, 0, 0, (void*) INDEX_RANGE_END);
    n = define_identifier(m, cls, "step", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, range_get, 0, 0, (void*) INDEX_RANGE_STEP);
    extends_method(cls, fs->cls_iterable);


    // RangeIter
    cls = cls_rangeiter;
    cls->u.c.n_memb = INDEX_RANGEITER_NUM;
    n = define_identifier(m, cls, "inf", NODE_NEW_N, 0);
    define_native_func_a(n, rangeiter_inf, 1, 2, cls, NULL, NULL);

    n = define_identifier_p(m, cls, fs->str_next, NODE_FUNC_N, 0);
    define_native_func_a(n, rangeiter_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);
}
