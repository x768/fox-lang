#include "fox_vm.h"


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
        while (e[idx].key != NULL) {
            idx++;
            if (idx > hash->entry_num) {
                idx = 0;
            }
        }
        e[idx] = *he2;
    }
    if (mem == NULL) {
        free(hash->entry);
    }
    hash->entry = e;
}
int Hash2_add_p(Hash2 *hash, Mem *mem, RefStr *key, void *ptr)
{
    HashEntry2 *p;

    if (hash->count + hash->count / 2 > hash->entry_num) {
        Hash2_grow(hash, mem);
    }

    p = &hash->entry[HASH_INDEX(key, hash)];

    while (p->key != NULL) {
        if (p->key == key) {
            return FALSE;
        }
        p++;
        if (p >= hash->entry + hash->entry_num) {
            p = hash->entry;
        }
    }
    p->key = key;
    p->p = ptr;
    hash->count++;
    return TRUE;
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
    HashEntry2 *p;

    if (hash->count + hash->count / 2 > hash->entry_num) {
        Hash2_grow(hash, mem);
    }

    p = &hash->entry[HASH_INDEX(key, hash)];

    while (p->key != NULL) {
        if (p->key == key) {
            return p;
        }
        p++;
        if (p >= hash->entry + hash->entry_num) {
            p = hash->entry;
        }
    }
    hash->count++;
    return p;
}



static int lang_testfunc(Value *vret, Value *v, RefNode *node)
{
    Mem mem;
    Hash2 h;
    int i;
    char tmp[16];

    Mem_init(&mem, 1024);
    Hash2_init(&h, &mem, 16);

    for (i = 0; i < 100; i++) {
        sprintf(tmp, "%d", i);
        RefStr *p = intern(tmp, -1);
        Hash2_add_p(&h, &mem, p, p);
    }
    for (i = 0; i < 100; i++) {
        sprintf(tmp, "%d", i);
        RefStr *p = Hash2_get_p(&h, intern(tmp, -1));
        fprintf(stderr, "%s:%s\n", tmp, p ? p->c : NULL);
    }
    fprintf(stderr, "size=%d, alloc_size=%d\n", h.count, h.entry_num);

    Mem_close(&mem);
    return TRUE;
}

void define_test_driver(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "testfunc", NODE_FUNC_N, 0);
    define_native_func_a(n, lang_testfunc, 0, 0, NULL);
}

#endif
