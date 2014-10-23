#include "fox_vm.h"
#include <string.h>
#include <stdio.h>

// class MapIter
enum {
    INDEX_MAPITER_VAL,
    INDEX_MAPITER_TYPE,
    INDEX_MAPITER_PTR,
    INDEX_MAPITER_IDX,
    INDEX_MAPITER_NUM,
};

static RefNode *cls_mapiter;

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * eppp  : HashValueEntryのポインタのポインタを返却する
 * phash : keyのハッシュ値を返却する
 */
static int map_search_sub(HashValueEntry ***eppp, uint32_t *phash, RefMap *h, Value key)
{
    RefNode *key_type = Value_type(key);
    HashValueEntry *ep, **epp;
    int32_t hash;

    if (!Value_hash(key, &hash)) {
        return FALSE;
    }

    // 一致しているか調べる
    epp = &h->entry[hash & (h->entry_num - 1)];
    ep = *epp;
    while (ep != NULL) {
        if (hash == ep->hash) {
            RefNode *key2_type = Value_type(ep->key);
            if (key_type == key2_type) {
                int ret;
                if (!Value_eq(key, ep->key, &ret)) {
                    return FALSE;
                }
                if (ret) {
                    // 見つかった
                    break;
                }
            }
        }
        epp = &ep->next;
        ep = *epp;
    }
    // 見つからなかった
    *eppp = epp;
    *phash = hash;
    return TRUE;
}

/**
 * HashValueEntryを2倍に広げる
 */
static void map_grow_entry(RefMap *map)
{
    int n = map->entry_num;
    int i;

    HashValueEntry **e = malloc(sizeof(HashValueEntry) * n * 2);
    memcpy(e, map->entry, sizeof(HashValueEntry*) * n);
    memset(e + n, 0, sizeof(HashValueEntry*) * n);

    map->entry_num *= 2;

    for (i = 0; i < n; i++){
        HashValueEntry **pp = &e[i];
        HashValueEntry *pos = *pp;

        while (pos != NULL){
            int h = pos->hash & (map->entry_num - 1);
            if (h >= n) {
                HashValueEntry **next = &pos->next;
                HashValueEntry **pp2 = &e[h];

                // delete
                *pp = *next;
                // add
                pos->next = *pp2;
                *pp2 = pos;

                pos = *pp;
            } else {
                pp = &pos->next;
                pos = *pp;
            }
        }
    }
    free(map->entry);
    map->entry = e;
}

/**
 * 同じキーがあれば上書き
 * なければ追加
 * 例外発生時はNULLを返す
 * keyのカウンタを増やす
 */
HashValueEntry *refmap_add(RefMap *rm, Value key, int overwrite, int raise_error)
{
    HashValueEntry *ep, **epp;
    uint32_t hash;

    if (!map_search_sub(&epp, &hash, rm, key)) {
        return NULL;
    }

    ep = *epp;
    if (ep != NULL) {
        if (overwrite) {
            // 一致（上書き）
            if (Value_isref(ep->val)) {
                RefHeader *rh = Value_ref_header(ep->val);
                if (rh->nref > 0 && --rh->nref == 0) {
                    Value_release_ref(ep->val);
                }
            }
            ep->val = VALUE_NULL;
            return ep;
        } else {
            if (raise_error) {
                throw_errorf(fs->mod_lang, "IndexError", "Duplicate key detected");
                return NULL;
            } else {
                return ep;
            }
        }
    } else {
        // 一致しない（追加）
        ep = malloc(sizeof(HashValueEntry));
        ep->next = NULL;
        ep->hash = hash;
        ep->key = Value_cp(key);
        ep->val = VALUE_NULL;
        *epp = ep;
        rm->count++;

        if (rm->count > rm->entry_num) {
            map_grow_entry(rm);
        }
        return ep;
    }
}
int refmap_get(HashValueEntry **ret, RefMap *rm, Value key)
{
    HashValueEntry *ep, **epp;
    uint32_t hash;

    if (!map_search_sub(&epp, &hash, rm, key)) {
        return FALSE;
    }
    ep = *epp;
    if (ep != NULL) {
        *ret = ep;
    } else {
        *ret = NULL;
    }
    return TRUE;
}
HashValueEntry *refmap_get_strkey(RefMap *rm, const char *key_p, int key_size)
{
    HashValueEntry *ep, **epp;
    uint32_t hash;

    if (key_size < 0) {
        key_size = strlen(key_p);
    }
    hash = str_hash(key_p, key_size);

    // 一致しているか調べる
    epp = &rm->entry[hash & (rm->entry_num - 1)];
    ep = *epp;
    while (ep != NULL) {
        if (hash == ep->hash) {
            RefNode *key_type = Value_type(ep->key);
            if (key_type == fs->cls_str) {
                RefStr *rs = Value_vp(ep->key);
                if (rs->size == key_size && memcmp(rs->c, key_p, key_size) == 0) {
                    // 見つかった
                    return ep;
                }
            }
        }
        epp = &ep->next;
        ep = *epp;
    }
    // 見つからなかった
    return NULL;
}
/**
 * vからkey要素を取り除いて、valに返す
 */
int refmap_del(Value *val, RefMap *rm, Value key)
{
    HashValueEntry *ep, **epp;
    uint32_t hash;

    if (!map_search_sub(&epp, &hash, rm, key)) {
        return FALSE;
    }
    ep = *epp;
    if (ep != NULL) {
        // 一致（削除）
        Value_dec(ep->key);
        *epp = ep->next;
        if (val != NULL) {
            *val = ep->val;
        } else {
            Value_dec(ep->val);
        }
        free(ep);
        rm->count--;
    } else {
        if (val != NULL) {
            *val = VALUE_NULL;
        }
    }
    return TRUE;
}

RefMap *refmap_new(int size)
{
    RefMap *rm = buf_new(fs->cls_map, sizeof(RefMap));
    int max = align_pow2(size == 0 ? 32 : size, 32);
    HashValueEntry **entry = malloc(sizeof(HashValueEntry*) * max);

    memset(entry, 0, sizeof(HashValueEntry*) * max);
    rm->entry = entry;
    rm->entry_num = max;
    rm->count = 0;

    return rm;
}

static int map_new_elems(Value *vret, Value *v, RefNode *node)
{
    Value *src = v + 1;
    int argc = fg->stk_top - src;
    int i;
    RefMap *rm;

    if (argc % 2 != 0) {
        throw_errorf(fs->mod_lang, "ArgumentError", "Argument count must be even");
        return FALSE;
    }

    rm = refmap_new(argc / 2);
    *vret = vp_Value(rm);
    for (i = 0; i < argc; i += 2) {
        HashValueEntry *ve = refmap_add(rm, src[i], FALSE, TRUE);
        if (ve == NULL) {
            return FALSE;
        }
        ve->val = Value_cp(src[i + 1]);
    }

    return TRUE;
}
int map_dispose(Value *vret, Value *v, RefNode *node)
{
    RefMap *r = Value_vp(*v);
    int i;

    for (i = 0; i < r->entry_num; i++) {
        HashValueEntry *p = r->entry[i];
        while (p != NULL) {
            HashValueEntry *prev = p;
            Value_dec(p->key);
            Value_dec(p->val);
            p = p->next;

            free(prev);
        }
    }
    free(r->entry);
    r->entry = NULL;
    r->entry_num = 0;
    return TRUE;
}

static int map_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value dumper = v[1];
    Value r = Value_ref(dumper)->v[INDEX_MARSHALDUMPER_SRC];
    int is_map = FUNC_INT(node);
    RefMap *rm;
    uint32_t size;
    int i;

    if (!read_int32(&size, r)) {
        return FALSE;
    }
    if (size > 0xffffff) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        return FALSE;
    }
    if (size * sizeof(HashValueEntry) > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    rm = refmap_new(size);
    *vret = vp_Value(rm);

    for (i = 0; i < size; i++) {
        Value key;
        Value_push("v", dumper);
        if (!call_member_func(fs->str_read, 0, TRUE)) {
            return FALSE;
        }
        fg->stk_top--;
        key = *fg->stk_top;
        if (is_map) {
            Value val;
            HashValueEntry *ve;

            Value_push("v", dumper);
            if (!call_member_func(fs->str_read, 0, TRUE)) {
                return FALSE;
            }
            fg->stk_top--;
            val = *fg->stk_top;

            ve = refmap_add(rm, key, TRUE, FALSE);
            if (ve == NULL) {
                Value_dec(key);
                Value_dec(val);
                return FALSE;
            }
            ve->val = val;
        } else {
            // Set
            if (refmap_add(rm, key, TRUE, FALSE) == NULL) {
                return FALSE;
            }
        }
        Value_dec(key);
    }

    return TRUE;
}
/**
 * 0 : 要素数
 * 4 : 要素(key, value, key, value, ...)
 */
static int map_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value dumper = v[1];
    Value w = Value_ref(dumper)->v[INDEX_MARSHALDUMPER_SRC];

    RefMap *rm = Value_vp(*v);
    int is_map = FUNC_INT(node);
    int i;

    rm->lock_count++;
    if (!write_int32(rm->count, w)) {
        goto ERROR_END;
    }
    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ep = rm->entry[i];
        for (; ep != NULL; ep = ep->next) {
            if (is_map) {
                Value_push("vv", dumper, ep->key);
                if (!call_member_func(fs->str_write, 1, TRUE)) {
                    goto ERROR_END;
                }
                Value_pop();
            }

            Value_push("vv", dumper, ep->val);
            if (!call_member_func(fs->str_write, 1, TRUE)) {
                goto ERROR_END;
            }
            Value_pop();
        }
    }
    rm->lock_count--;
    return TRUE;

ERROR_END:
    rm->lock_count--;
    return FALSE;
}


/**
 * Map/Setの要素を全て解放する
 * entryの配列は使いまわす
 */
static int map_clear(Value *vret, Value *v, RefNode *node)
{
    RefMap *r = Value_vp(*v);
    int i;

    if (r->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    for (i = 0; i < r->entry_num; i++) {
        HashValueEntry *p = r->entry[i];
        while (p != NULL) {
            HashValueEntry *prev = p;
            Value_dec(p->key);
            Value_dec(p->val);
            p = p->next;

            free(prev);
        }
        r->entry[i] = NULL;
    }
    r->count = 0;

    return TRUE;
}

// a["hoge"]
int map_index(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    HashValueEntry *ep = NULL;
    Value key = v[1];

    if (!refmap_get(&ep, rm, key)) {
        return FALSE;
    }
    if (ep != NULL) {
        // 一致
        *vret = Value_cp(ep->val);
    }

    return TRUE;
}
// a["hoge"] = 10
static int map_index_set(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    HashValueEntry *ve = NULL;

    if (rm->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    ve = refmap_add(rm, v[1], TRUE, FALSE);
    if (ve == NULL) {
        return FALSE;
    }
    ve->val = Value_cp(v[2]);

    return TRUE;
}

int map_has_key(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    HashValueEntry *ep;
    Value key = v[1];

    if (!refmap_get(&ep, rm, key)) {
        return FALSE;
    }
    *vret = bool_Value(ep != NULL);

    return TRUE;
}
/**
 * 検索して見つかった場合/見つからなかった場合は
 * 1.true/falseを返す
 * 2.index/nullを返す
 */
int map_index_of(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    int ret_index = FUNC_INT(node);
    Value v1 = v[1];
    RefNode *type = Value_type(v1);
    int i;

    RefNode *fn_eq = Hash_get_p(&type->u.c.h, fs->symbol_stock[T_EQ]);
    if (fn_eq == NULL) {
        throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, type, fs->symbol_stock[T_EQ]);
        return FALSE;
    }

    rm->lock_count++;
    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ep = rm->entry[i];
        for (; ep != NULL; ep = ep->next) {
            Value va = ep->val;

            if (Value_type(va) == type) {
                if (type == fs->cls_str) {
                    if (refstr_eq(Value_vp(v1), Value_vp(va))) {
                        break;
                    }
                } else {
                    Value_push("vv", v1, va);
                    if (!call_function(fn_eq, 1)) {
                        goto ERROR_END;
                    }
                    fg->stk_top--;
                    if (Value_bool(*fg->stk_top)) {
                        Value_dec(*fg->stk_top);
                        if (ret_index) {
                            *vret = Value_cp(ep->key);
                        } else {
                            *vret = VALUE_TRUE;
                        }
                        return TRUE;
                    }
                }
            }
        }
    }
    if (!ret_index) {
        *vret = VALUE_FALSE;
    }

    rm->lock_count--;
    return TRUE;
ERROR_END:
    rm->lock_count--;
    return FALSE;
}
static int map_add_entry(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    Ref *r = Value_ref(v[1]);
    HashValueEntry *ve;

    if (rm->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    ve = refmap_add(rm, r->v[0], TRUE, FALSE);
    if (ve == NULL) {
        return FALSE;
    }
    ve->val = Value_cp(r->v[1]);
    return TRUE;
}
// 削除した要素を返す
static int map_delete(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    Value key = v[1];

    if (rm->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    if (!refmap_del(vret, rm, key)) {
        return FALSE;
    }

    return TRUE;
}

int map_empty(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    *vret = bool_Value(rm->count == 0);

    return TRUE;
}
int map_size(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    *vret = int32_Value(rm->count);
    return TRUE;
}

int map_iterator(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    int type = FUNC_INT(node);
    Ref *r = ref_new(cls_mapiter);

    rm->lock_count++;
    r->v[INDEX_MAPITER_VAL] = Value_cp(*v);
    r->v[INDEX_MAPITER_TYPE] = int32_Value(type);
    r->v[INDEX_MAPITER_PTR] = ptr_Value(NULL);
    r->v[INDEX_MAPITER_IDX] = int32_Value(0);
    *vret = vp_Value(r);

    return TRUE;
}
/**
 * 複製
 */
int map_dup(Value *vret, Value *v, RefNode *node)
{
    RefMap *src = Value_vp(*v);
    RefNode *type = FUNC_VP(node);
    int i;
    int max = src->entry_num;
    RefMap *dst = buf_new(type, sizeof(RefMap));

    *vret = vp_Value(dst);
    dst->count = src->count;
    dst->entry_num = max;
    dst->entry = malloc(sizeof(HashValueEntry*) * max);

    for (i = 0; i < max; i++) {
        HashValueEntry *hsrc = src->entry[i];
        HashValueEntry **hdst = &dst->entry[i];
        for (; hsrc != NULL; hsrc = hsrc->next) {
            HashValueEntry *he = malloc(sizeof(HashValueEntry));

            he->key = Value_cp(hsrc->key);
            he->val = Value_cp(hsrc->val);
            he->hash = hsrc->hash;
            *hdst = he;
            hdst = &he->next;
        }
        *hdst = NULL;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int mapiter_next(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefMap *map = Value_vp(r->v[INDEX_MAPITER_VAL]);
    int idx = Value_integral(r->v[INDEX_MAPITER_IDX]);
    HashValueEntry *ep = Value_ptr(r->v[INDEX_MAPITER_PTR]);

    if (ep != NULL) {
        ep = ep->next;
    }
    while (ep == NULL && idx < map->entry_num) {
        ep = map->entry[idx];
        idx++;
    }

    r->v[INDEX_MAPITER_IDX] = int32_Value(idx);
    r->v[INDEX_MAPITER_PTR] = ptr_Value(ep);

    if (ep != NULL) {
        switch (Value_integral(r->v[INDEX_MAPITER_TYPE])) {
        case ITERATOR_KEY:
            *vret = Value_cp(ep->key);
            break;
        case ITERATOR_VAL:
            *vret = Value_cp(ep->val);
            break;
        default: {
            Ref *r2 = ref_new(fv->cls_entry);
            *vret = vp_Value(r2);
            r2->v[INDEX_ENTRY_KEY] = Value_cp(ep->key);
            r2->v[INDEX_ENTRY_VAL] = Value_cp(ep->val);
            break;
        }
        }
    } else {
        throw_stopiter();
        return FALSE;
    }
    return TRUE;
}
static int mapiter_dispose(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefMap *rm = Value_vp(r->v[INDEX_MAPITER_VAL]);
    rm->lock_count--;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int mapentry_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = ref_new(fv->cls_entry);
    *vret = vp_Value(r);
    r->v[INDEX_ENTRY_KEY] = Value_cp(v[1]);
    r->v[INDEX_ENTRY_VAL] = Value_cp(v[2]);
    return TRUE;
}

/**
 * key同士を比較
 */
static int mapentry_cmp(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[0];
    Value v2 = v[1];
    Value vr;

    Value_push("vv", Value_ref(v1)->v[INDEX_ENTRY_KEY], Value_ref(v2)->v[INDEX_ENTRY_KEY]);
    if (!call_member_func(fs->symbol_stock[T_CMP], 1, TRUE)) {
        return FALSE;
    }

    vr = fg->stk_top[-1];
    if (Value_isint(vr) && Value_integral(vr) == 0) {
        Value_pop();
        // keyが同じなら、valueを比較
        Value_push("vv", Value_ref(v1)->v[INDEX_ENTRY_VAL], Value_ref(v2)->v[INDEX_ENTRY_VAL]);
        if (!call_member_func(fs->symbol_stock[T_CMP], 1, TRUE)) {
            return FALSE;
        }
    }
    *vret = fg->stk_top[-1];
    fg->stk_top--;

    return TRUE;
}
static int mapentry_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    StrBuf buf;

    StrBuf_init(&buf, 0);
    if (!StrBuf_printf(&buf, "Entry(%v, %v)", r->v[INDEX_ENTRY_KEY], r->v[INDEX_ENTRY_VAL])) {
        goto ERROR_END;
    }
    *vret = cstr_Value(fs->cls_str, buf.p, buf.size);
    StrBuf_close(&buf);
    return TRUE;

ERROR_END:
    StrBuf_close(&buf);
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int set_new_elems(Value *vret, Value *v, RefNode *node)
{
    Value *src = v + 1;
    int argc = fg->stk_top - src;
    int i;

    RefMap *rm = refmap_new(argc);
    *vret = vp_Value(rm);
    rm->rh.type = fs->cls_set;

    for (i = 0; i < argc; i++) {
        // 同じ要素があればエラー
        if (refmap_add(rm, src[i], FALSE, TRUE) == NULL) {
            return FALSE;
        }
    }
    return TRUE;
}
int set_add_value(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    Value *v1 = v + 1;
    int argc = fg->stk_top - fg->stk_base - 1;
    int i;

    if (rm->lock_count > 0) {
        throw_error_select(THROW_CANNOT_MODIFY_ON_ITERATION);
        return FALSE;
    }
    for (i = 0; i < argc; i++) {
        if (refmap_add(rm, v1[i], TRUE, FALSE) == NULL) {
            return FALSE;
        }
    }

    return TRUE;
}
int set_and(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    RefMap *rm1 = Value_vp(v[1]);
    int i;

    RefMap *rm2 = refmap_new(rm->count);
    *vret = vp_Value(rm2);
    rm2->rh.type = fs->cls_set;

    rm->lock_count++;
    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ep = rm->entry[i];
        for (; ep != NULL; ep = ep->next) {
            Value key = ep->key;
            HashValueEntry *ep2 = NULL;

            // v1に同じ値があれば追加
            if (!refmap_get(&ep2, rm1, key)) {
                goto ERROR_END;
            }
            if (ep2 != NULL) {
                if (refmap_add(rm2, key, TRUE, FALSE) == NULL) {
                    goto ERROR_END;
                }
            }
        }
    }
    rm->lock_count--;
    return TRUE;

ERROR_END:
    rm->lock_count--;
    return FALSE;
}
int set_or(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    RefMap *rm1 = Value_vp(v[1]);
    int i;

    RefMap *rm2 = refmap_new(rm->count);
    *vret = vp_Value(rm2);
    rm2->rh.type = fs->cls_set;

    rm->lock_count++;
    rm1->lock_count++;

    // hをそのままvretにコピー
    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ep = rm->entry[i];
        for (; ep != NULL; ep = ep->next) {
            Value key = ep->key;
            if (refmap_add(rm2, key, TRUE, FALSE) == NULL) {
                goto ERROR_END;
            }
        }
    }
    // h1をvretに追加
    for (i = 0; i < rm1->entry_num; i++) {
        HashValueEntry *ep = rm1->entry[i];
        for (; ep != NULL; ep = ep->next) {
            Value key = ep->key;
            if (refmap_add(rm2, key, TRUE, FALSE) == NULL) {
                goto ERROR_END;
            }
        }
    }

    rm->lock_count--;
    rm1->lock_count--;
    return TRUE;

ERROR_END:
    rm->lock_count--;
    rm1->lock_count--;
    return FALSE;
}
int set_xor(Value *vret, Value *v, RefNode *node)
{
    RefMap *rm = Value_vp(*v);
    RefMap *rm1 = Value_vp(v[1]);
    int i;

    RefMap *rm2 = refmap_new(rm->count);
    *vret = vp_Value(rm2);
    rm2->rh.type = fs->cls_set;

    rm->lock_count++;
    rm1->lock_count++;

    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *ep = rm->entry[i];
        for (; ep != NULL; ep = ep->next) {
            Value key = ep->key;
            HashValueEntry *ep2 = NULL;

            // v1に同じ値がなければ追加
            if (!refmap_get(&ep2, rm1, key)) {
                goto ERROR_END;
            }
            if (ep2 == NULL) {
                if (refmap_add(rm2, key, TRUE, FALSE) == NULL) {
                    goto ERROR_END;
                }
            }
        }
    }

    for (i = 0; i < rm1->entry_num; i++) {
        HashValueEntry *ep = rm1->entry[i];
        for (; ep != NULL; ep = ep->next) {
            Value key = ep->key;
            HashValueEntry *ep2 = NULL;

            // vに同じ値がなければ追加
            if (!refmap_get(&ep2, rm, key)) {
                goto ERROR_END;
            }
            if (ep2 == NULL) {
                if (refmap_add(rm2, key, TRUE, FALSE) == NULL) {
                    goto ERROR_END;
                }
            }
        }
    }

    rm->lock_count--;
    rm1->lock_count--;
    return TRUE;

ERROR_END:
    rm->lock_count--;
    rm1->lock_count--;
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////

void define_lang_map_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    RefStr *empty = intern("empty", -1);
    RefStr *size = intern("size", -1);

    cls_mapiter = define_identifier(m, m, "MapIter", NODE_CLASS, 0);
    fv->cls_entry = define_identifier(m, m, "Entry", NODE_CLASS, 0);


    // Map
    cls = fs->cls_map;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, map_new_elems, 0, -1, NULL);
    fv->func_map_new = n;
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, map_marshal_read, 1, 1, (void*) TRUE, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, map_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, map_marshal_write, 1, 1, (void*) TRUE, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, col_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, col_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, col_eq, 1, 1, NULL, fs->cls_map);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    define_native_func_a(n, map_index, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    define_native_func_a(n, map_index_set, 2, 2, NULL, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    define_native_func_a(n, map_index, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->str_missing_set, NODE_FUNC_N, 0);
    define_native_func_a(n, map_index_set, 2, 2, NULL, NULL, NULL);
    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, size, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_size, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, map_iterator, 0, 0, (void*)ITERATOR_BOTH);
    n = define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    define_native_func_a(n, map_dup, 0, 0, fs->cls_map);
    n = define_identifier(m, cls, "entries", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_BOTH);
    n = define_identifier(m, cls, "keys", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_KEY);
    n = define_identifier(m, cls, "values", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_VAL);
    n = define_identifier(m, cls, "has_key", NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "index_of", NODE_FUNC_N, 0);
    define_native_func_a(n, map_index_of, 1, 1, (void*) TRUE, NULL);
    n = define_identifier(m, cls, "has_value", NODE_FUNC_N, 0);
    define_native_func_a(n, map_index_of, 1, 1, (void*) FALSE, NULL);
    n = define_identifier(m, cls, "add", NODE_FUNC_N, 0);
    define_native_func_a(n, map_add_entry, 1, 1, NULL, fv->cls_entry);
    n = define_identifier(m, cls, "delete", NODE_FUNC_N, 0);
    define_native_func_a(n, map_delete, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "clear", NODE_FUNC_N, 0);
    define_native_func_a(n, map_clear, 0, 0, NULL, NULL);
    extends_method(cls, fs->cls_iterable);

    // MapIter
    cls = cls_mapiter;
    cls->u.c.n_memb = INDEX_MAPITER_NUM;
    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, mapiter_dispose, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_next, NODE_FUNC_N, 0);
    define_native_func_a(n, mapiter_next, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);


    // Entry
    cls = fv->cls_entry;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, mapentry_new, 2, 2, NULL, NULL, NULL);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, mapentry_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, pairvalue_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, pairvalue_eq, 1, 1, NULL, fv->cls_entry);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, mapentry_cmp, 1, 1, NULL, fv->cls_entry);
    n = define_identifier(m, cls, "key", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, native_get_member, 0, 0, (void*) INDEX_ENTRY_KEY);
    n = define_identifier(m, cls, "value", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, native_get_member, 0, 0, (void*) INDEX_ENTRY_VAL);

    cls->u.c.n_memb = INDEX_ENTRY_NUM;
    extends_method(cls, fs->cls_obj);

    // Set
    // Mapのkeyのみ使う
    cls = fs->cls_set;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, set_new_elems, 0, -1, NULL);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, map_marshal_read, 1, 1, (void*) FALSE, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, map_dispose, 0, 0, NULL);

    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, col_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, map_marshal_write, 1, 1, (void*) FALSE, fs->cls_marshaldumper);
    n = define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, col_hash, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    define_native_func_a(n, col_eq, 1, 1, NULL, fs->cls_set);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_AND], NODE_FUNC_N, 0);
    define_native_func_a(n, set_and, 1, 1, NULL, fs->cls_set);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_OR], NODE_FUNC_N, 0);
    define_native_func_a(n, set_or, 1, 1, NULL, fs->cls_set);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_XOR], NODE_FUNC_N, 0);
    define_native_func_a(n, set_xor, 1, 1, NULL, fs->cls_set);
    n = define_identifier_p(m, cls, empty, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_empty, 0, 0, NULL);
    n = define_identifier_p(m, cls, size, NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, map_size, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    define_native_func_a(n, map_iterator, 0, 0, (void*) ITERATOR_KEY);
    n = define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    define_native_func_a(n, map_dup, 0, 0, cls);
    n = define_identifier(m, cls, "has", NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, NULL);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, map_has_key, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "add", NODE_FUNC_N, 0);
    define_native_func_a(n, set_add_value, 1, -1, NULL, NULL);
    n = define_identifier(m, cls, "delete", NODE_FUNC_N, 0);
    define_native_func_a(n, map_delete, 1, 1, NULL, NULL);
    n = define_identifier(m, cls, "clear", NODE_FUNC_N, 0);
    define_native_func_a(n, map_clear, 0, 0, NULL, NULL);
    extends_method(cls, fs->cls_iterable);
}

