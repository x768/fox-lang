#include "fox_vm.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


typedef struct SymbolEntry
{
    struct SymbolEntry *next;
    uint32_t hash;
    RefStr *s;
} SymbolEntry;

typedef struct RefStrEntry
{
    struct RefStrEntry *next;
    uint32_t hash;
    RefStr s;
} RefStrEntry;

static RefStrEntry **symbol_table;
static int symbol_table_size;
static int symbol_table_count;


#define HASH_INDEX(k, h) (((uint32_t)(uintptr_t)(k) >> 3) & ((h)->entry_num - 1))

void Hash_init(Hash *hash, Mem *mem, int init_size)
{
    hash->count = 0;

    init_size = align_pow2(init_size, 16);

    hash->entry_num = init_size;
    hash->entry = Mem_get(mem, sizeof(HashEntry*) * init_size);

    memset(hash->entry, 0, sizeof(HashEntry*) * init_size);
}

static void Hash_grow(Hash *hash, Mem *mem)
{
    int n = hash->entry_num;
    int i;

    HashEntry **e = Mem_get(mem, sizeof(HashEntry*) * n * 2);
    memcpy(e, hash->entry, sizeof(HashEntry*) * n);
    memset(e + n, 0, sizeof(HashEntry*) * n);

    hash->entry_num *= 2;

    for (i = 0; i < n; i++){
        HashEntry **pp = &e[i];
        HashEntry *pos = *pp;

        while (pos != NULL){
            int h = HASH_INDEX(pos->key, hash);
            if (h >= n) {
                HashEntry **next = &pos->next;
                HashEntry **pp2 = &e[h];

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
    if (mem == NULL) {
        free(hash->entry);
    }
    hash->entry = e;
}
void Hash_add_p(Hash *hash, Mem *mem, RefStr *key, void *ptr)
{
    HashEntry **pp, *pos;

    pp = &hash->entry[HASH_INDEX(key, hash)];
    pos = Mem_get(mem, sizeof(*pos));
    pos->next = *pp;
    pos->key = key;
    pos->p = ptr;
    *pp = pos;

    hash->count++;
    if (hash->count > hash->entry_num) {
        Hash_grow(hash, mem);
    }
}
void *Hash_get_p(const Hash *hash, RefStr *key)
{
    const HashEntry *pos = hash->entry[HASH_INDEX(key, hash)];

    for (; pos != NULL; pos = pos->next) {
        if (key == pos->key) {
            return pos->p;
        }
    }
    return NULL;
}
/**
 * あればそのHashEntryを返す
 * 無ければ追加して返す
 */
HashEntry *Hash_get_add_entry(Hash *hash, Mem *mem, RefStr *key)
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

/**
 * 見つからない場合はSymbolTableに追加しない
 */
void *Hash_get(const Hash *hash, const char *key_p, int key_size)
{
    RefStrEntry *pos;
    if (key_size < 0) {
        key_size = strlen(key_p);
    }
    pos = symbol_table[str_hash(key_p, key_size) & (symbol_table_size - 1)];

    for (; pos != NULL; pos = pos->next) {
        if (key_size == pos->s.size && memcmp(key_p, pos->s.c, key_size) == 0) {
            return Hash_get_p(hash, &pos->s);
        }
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void SymbolTable_grow(void)
{
    int n = symbol_table_size;
    int i;

    RefStrEntry **e = Mem_get(&fg->st_mem, sizeof(RefStrEntry*) * n * 2);
    memcpy(e, symbol_table, sizeof(RefStrEntry*) * n);
    memset(e + n, 0, sizeof(SymbolEntry*) * n);

    symbol_table_size *= 2;

    for (i = 0; i < n; i++){
        RefStrEntry **pp = &e[i];
        RefStrEntry *pos = *pp;

        while (pos != NULL){
            int h = (pos->hash & (symbol_table_size - 1));
            if (h >= n) {
                RefStrEntry **pp2 = &e[h];

                // delete
                *pp = pos->next;
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
    symbol_table = e;
}

/**
 * Hashのキーとして使うためにポインタに変換
 * 同じ文字列は同じポインタとなる
 * 一度登録すると消せない
 */
RefStr *intern(const char *p, int size)
{
    RefStrEntry **pp, *pos;
    uint32_t h;

    if (size < 0) {
        size = strlen(p);
    }
    h = str_hash(p, size);

    pp = &symbol_table[h & (symbol_table_size - 1)];
    pos = *pp;

    for (; pos != NULL; pos = pos->next) {
        if (h == pos->hash && size == pos->s.size && memcmp(p, pos->s.c, size) == 0) {
            return &pos->s;
        }
    }

    pos = Mem_get(&fg->st_mem, sizeof(RefStrEntry) + size + 1);
    pos->next = *pp;
    pos->hash = h;
    pos->s.rh.type = fs->cls_str,
    pos->s.rh.nref = -1;
    pos->s.rh.n_memb = 0;
    pos->s.rh.weak_ref = NULL;
    pos->s.size = size;
    memcpy(pos->s.c, p, size);
    pos->s.c[size] = '\0';
    *pp = pos;

    symbol_table_count++;
    if (symbol_table_size < symbol_table_count) {
        SymbolTable_grow();
    }

    return &pos->s;
}
/**
 * internと同じだが、登録されていない場合はnullを返す
 */
RefStr *get_intern(const char *p, int size)
{
    RefStrEntry **pp, *pos;
    uint32_t h;

    if (size < 0) {
        size = strlen(p);
    }
    h = str_hash(p, size);

    pp = &symbol_table[h & (symbol_table_size - 1)];
    pos = *pp;

    for (; pos != NULL; pos = pos->next) {
        if (h == pos->hash && size == pos->s.size && memcmp(p, pos->s.c, size) == 0) {
            return &pos->s;
        }
    }
    return NULL;
}
void g_intern_init(void)
{
    union {
        int64_t tm;
        struct {
            uint32_t lo;
            uint32_t hi;
        } s;
    } u;

    symbol_table_size = 256;
    symbol_table_count = 0;
    symbol_table = malloc(sizeof(SymbolEntry*) * symbol_table_size);
    memset(symbol_table, 0, sizeof(SymbolEntry*) * symbol_table_size);

    u.tm = get_now_time();
    fv->hash_seed = u.s.lo ^ u.s.hi;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * lstの前に追加して、追加した要素を返す
 */
PtrList *PtrList_add(PtrList **pp, int size, Mem *mem)
{
    PtrList *ls = Mem_get(mem, offsetof(PtrList, u) + size);
    ls->next = *pp;
    *pp = ls;
    return ls;
}
/**
 * lstの前に追加
 */
void PtrList_add_p(PtrList **pp, void *p, Mem *mem)
{
    PtrList *ls = Mem_get(mem, offsetof(PtrList, u) + sizeof(void*));
    ls->u.p = p;
    ls->next = *pp;
    *pp = ls;
}
/**
 * lstの後に追加して、追加した要素を返す
 */
PtrList *PtrList_push(PtrList **pp, int size, Mem *mem)
{
    PtrList *pos = *pp;
    int i = 0;

    while (pos != NULL) {
        pp = &pos->next;
        pos = *pp;
        i++;
    }
    pos = Mem_get(mem, offsetof(PtrList, u) + size);
    pos->next = NULL;
    *pp = pos;
    return pos;
}
void PtrList_close(PtrList *lst)
{
    while (lst != NULL) {
        PtrList *next = lst->next;
        free(lst);
        lst = next;
    }
}

