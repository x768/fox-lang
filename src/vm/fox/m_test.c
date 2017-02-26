#include "fox_vm.h"


#ifndef NO_DEBUG

#if 0

typedef struct HashEntry2
{
    RefStr *key;
    void *p;
} HashEntry2;

typedef struct Hash2
{
    HashEntry2 *entry;
    int32_t entry_num;
    int32_t count;
} Hash2;

#define HASH_INDEX(k, h) (((uint32_t)(uintptr_t)(k) >> 3) & ((h)->entry_num - 1))

void Hash2_init(Hash2 *hash, Mem *mem, int init_size)
{
    hash->count = 0;

    init_size = align_pow2(init_size, 16);

    hash->entry_num = init_size;
    hash->entry = Mem_get(mem, sizeof(HashEntry2) * init_size);

    memset(hash->entry, 0, sizeof(HashEntry2) * init_size);
}

static void Hash2_grow(Hash2 *hash, Mem *mem)
{
    int n = hash->entry_num;
    int i;

    HashEntry2 *e = Mem_get(mem, sizeof(HashEntry2) * n * 2);
    memset(e, 0, sizeof(HashEntry2) * n * 2);
    hash->entry_num *= 2;

    for (i = 0; i < n; i++){
        HashEntry2 *he2 = &hash->entry[i];
        uint32_t idx = HASH_INDEX(he2->key, hash);
        e[idx] = *he2;
    }
    if (mem == NULL) {
        free(hash->entry);
    }
    hash->entry = e;
}
void Hash2_add_p(Hash2 *hash, Mem *mem, RefStr *key, void *ptr)
{
    HashEntry2 *pos, *p;

    if (hash->count * 2 > hash->entry_num * 3) {
        Hash2_grow(hash, mem);
    }

    pos = &hash->entry[HASH_INDEX(key, hash)];
    p = pos;

    for (;;) {
        if (p->key == NULL) {
            break;
        }
        p++;
        if (p >= hash->entry + hash->entry_num) {
            p = hash->entry;
        }
    }
    pos->key = key;
    pos->p = ptr;
    hash->count++;
}
void *Hash2_get_p(const Hash2 *hash, RefStr *key)
{
    const HashEntry2 *p = &hash->entry[HASH_INDEX(key, hash)];

    while (p->key != NULL) {
        if (p->key == key) {
            return p->p;
        }
        p++;
        if (p >= hash->entry + hash->entry_num) {
            p = hash->entry;
        }
    }
    return NULL;
}
/**
 * あればそのHashEntryを返す
 * 無ければ追加して返す
 */
HashEntry2 *Hash2_get_add_entry(Hash2 *hash, Mem *mem, RefStr *key)
{
    HashEntry **pp = &hash->entry[HASH_INDEX(key, hash)];
    HashEntry *pos = *pp;

    while (pos != NULL) {
        if (key == pos->key) {
            return pos;
        }
        pp = &pos->next;
        pos = *pp;
    }

    pos = Mem_get(mem, sizeof(*pos));
    pos->next = NULL;
    pos->key = key;
    pos->p = NULL;
    *pp = pos;

    hash->count++;
    if (hash->count > hash->entry_num) {
        Hash_grow(hash, mem);
    }

    return pos;
}


#endif



static int lang_testfunc(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}

void define_test_driver(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "testfunc", NODE_FUNC_N, 0);
    define_native_func_a(n, lang_testfunc, 1, 1, NULL, NULL);
}

#endif
