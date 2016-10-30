#define DEFINE_GLOBALS
#include "fox.h"
#include <mongoc.h>
#include <math.h>
#include <pcre.h>


typedef struct {
    int nref;
    bson_t *bson;
} FoxBson;

typedef struct {
    RefHeader rh;

    bson_oid_t oid;
} RefBsonOID;


typedef struct {
    RefHeader rh;

    mongoc_client_t *client;
} RefMongoClient;

typedef struct {
    RefHeader rh;

    Value client;
    mongoc_database_t *db;
} RefMongoDatabase;

typedef struct {
    RefHeader rh;

    Value client;
    Value db;
    char *path;
    FoxBson *query;
    FoxBson *orderby;
    FoxBson *fields;
    int limit;
    int skip;
} RefMongoCollection;

typedef struct {
    RefHeader rh;

    Value client;
    mongoc_cursor_t *cur;
} RefMongoCursor;

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_mongo;

static RefNode *cls_client;
static RefNode *cls_database;
static RefNode *cls_collection;
static RefNode *cls_cursor;

static RefNode *cls_bsonoid;
static RefNode *cls_bsonregex;


static int int_to_hex(int i)
{
    if (i < 10) {
        return '0' + i;
    } else {
        return 'a' + i - 10;
    }
}
static int is_client_open(Value v, int auto_throw)
{
    if (Value_isref(v)) {
        RefMongoClient *cli = Value_vp(v);
        if (cli->client != NULL) {
            return TRUE;
        }
    }
    if (auto_throw) {
        fs->throw_errorf(mod_mongo, "MongoError", "MongoClient already closed");
    }
    return FALSE;
}
static void throw_bson_error(bson_error_t *e)
{
    fs->throw_errorf(mod_mongo, "MongoError", "%d.%d: %s", e->domain, e->code, e->message);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static FoxBson *FoxBson_new(bson_t *bson)
{
    FoxBson *b = malloc(sizeof(FoxBson));
    b->nref = 1;
    b->bson = bson;
    return b;
}
static FoxBson *FoxBson_cp(FoxBson *b)
{
    if (b != NULL) {
        b->nref++;
    }
    return b;
}
static void FoxBson_dec(FoxBson *b)
{
    if (b != NULL) {
        b->nref--;
        if (b->nref <= 0) {
            bson_destroy(b->bson);
            free(b);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int append_fox_value(void *data, const char *key, Value val)
{
    RefHeader *rh = (RefHeader*)data;

    if (rh->type == fs->cls_map) {
        Value vkey = fs->cstr_Value_conv(key, -1, NULL);
        HashValueEntry *ve = fs->refmap_add((RefMap*)rh, vkey, TRUE, FALSE);
        if (ve == NULL) {
            return FALSE;
        }
        ve->val = val;
        fs->Value_dec(vkey);
    } else if (rh->type == fs->cls_list) {
        RefArray *ra = (RefArray*)rh;
        long idx = strtol(key, NULL, 10);
        if (idx >= ra->size) {
            if (!fs->refarray_set_size(ra, idx + 1)) {
                return FALSE;
            }
        }
        ra->p[idx] = val;
    }
    return TRUE;
}
static bool bson_value_visit_utf8(
    const bson_iter_t *iter, const char *key,
    size_t v_utf8_len, const char *v_utf8,
    void *data)
{
    append_fox_value(data, key, fs->cstr_Value_conv(v_utf8, v_utf8_len, NULL));
    return false;
}
static bool bson_value_visit_int32(
    const bson_iter_t *iter, const char *key,
    int32_t v_int32,
    void *data)
{
    append_fox_value(data, key, int32_Value(v_int32));
    return false;
}
static bool bson_value_visit_int64(
    const bson_iter_t *iter, const char *key,
    int64_t v_int64,
    void *data)
{
    append_fox_value(data, key, fs->int64_Value(v_int64));
    return false;
}
static bool bson_value_visit_double(
    const bson_iter_t *iter, const char *key,
    double v_double,
    void *data)
{
    append_fox_value(data, key, fs->float_Value(v_double));
    return false;
}
static bool bson_value_visit_undefined(
    const bson_iter_t *iter, const char *key,
    void *data)
{
    RefMap *rm = fs->refmap_new(0);
    Value val = vp_Value(rm);

    Value vkey = fs->cstr_Value(fs->cls_str, "$undefined", -1);
    HashValueEntry *ve = fs->refmap_add(rm, vkey, TRUE, FALSE);
    fs->Value_dec(vkey);
    ve->val = VALUE_TRUE;

    append_fox_value(data, key, val);
    return false;
}
static bool bson_value_visit_null(
    const bson_iter_t *iter, const char *key,
    void *data)
{
    append_fox_value(data, key, VALUE_NULL);
    return false;
}
static bool bson_value_visit_oid(
    const bson_iter_t *iter, const char *key,
    const bson_oid_t *oid,
    void *data)
{
    RefBsonOID *rid = fs->buf_new(cls_bsonoid, sizeof(RefBsonOID));
    Value val = vp_Value(rid);
    rid->oid = *oid;
    append_fox_value(data, key, val);
    return false;
}
static bool bson_value_visit_binary(
    const bson_iter_t *iter, const char *key,
    bson_subtype_t v_subtype, size_t v_binary_len, const uint8_t *v_binary,
    void *data)
{
    Value val = fs->cstr_Value(fs->cls_bytes, (const char*)v_binary, v_binary_len);
    append_fox_value(data, key, val);
    return false;
}
static bool bson_value_visit_bool(
    const bson_iter_t *iter, const char *key,
    bool v_bool,
    void *data)
{
    append_fox_value(data, key, bool_Value(v_bool));
    return false;
}
static bool bson_value_visit_date_time(
    const bson_iter_t *iter, const char *key,
    int64_t msec_since_epoch,
    void *data)
{
    RefInt64 *rt = fs->buf_new(fs->cls_timestamp, sizeof(RefInt64));
    rt->u.i = msec_since_epoch;
    append_fox_value(data, key, vp_Value(rt));
    return false;
}
static bool bson_value_visit_regex(
    const bson_iter_t *iter, const char *key,
    const char *v_regex,
    const char *v_options,
    void *data)
{
    Ref *r = fs->ref_new(cls_bsonregex);
    const char *opt = v_options;
    int opts = 0;

    while (*opt != '\0') {
        switch (*opt) {
        case 'i':
            opts |= PCRE_CASELESS;
            break;
        case 'm':
            opts |= PCRE_MULTILINE;
            break;
        default:
            break;
        }
        opt++;
    }
    r->v[INDEX_REGEX_SRC] = fs->cstr_Value_conv(v_regex, -1, NULL);
    r->v[INDEX_REGEX_OPT] = int32_Value(opts);

    append_fox_value(data, key, vp_Value(r));
    return false;
}
static bool bson_value_visit_timestamp(
    const bson_iter_t *iter, const char *key,
    uint32_t v_timestamp,
    uint32_t v_increment,
    void *data)
{
    return false;
}
static bool bson_value_visit_symbol(
    const bson_iter_t *iter, const char *key,
    size_t v_symbol_len, const char *v_symbol,
    void *data)
{
    append_fox_value(data, key, fs->cstr_Value_conv(v_symbol, v_symbol_len, NULL));
    return false;
}

static bool bson_value_visit_document(const bson_iter_t *iter, const char *key, const bson_t *v_document, void *data);
static bool bson_value_visit_array(const bson_iter_t *iter, const char *key, const bson_t *v_document, void *data);

static const bson_visitor_t bson_value_visitors = {
    NULL, // bson_value_visit_before,
    NULL, // bson_value_visit_after,
    NULL, // bson_value_visit_corrupt,
    bson_value_visit_double,
    bson_value_visit_utf8,
    bson_value_visit_document,
    bson_value_visit_array,
    bson_value_visit_binary,
    bson_value_visit_undefined,
    bson_value_visit_oid,
    bson_value_visit_bool,
    bson_value_visit_date_time,
    bson_value_visit_null,
    bson_value_visit_regex,
    NULL, //bson_value_visit_dbpointer,
    NULL, //bson_value_visit_code,
    bson_value_visit_symbol,
    NULL, //bson_value_visit_codewscope,
    bson_value_visit_int32,
    bson_value_visit_timestamp,
    bson_value_visit_int64,
    NULL, //bson_value_visit_maxkey,
    NULL, //bson_value_visit_minkey,
};

static bool bson_value_visit_document(
    const bson_iter_t *iter, const char *key,
    const bson_t *v_document,
    void *data)
{
    bson_iter_t child;

    if (bson_iter_init(&child, v_document)) {
        Value val = vp_Value(fs->refmap_new(0));
        bson_iter_visit_all(&child, &bson_value_visitors, &val);
        append_fox_value(data, key, val);
        fs->Value_dec(val);
    }
    return false;
}
static bool bson_value_visit_array(
    const bson_iter_t *iter, const char *key,
    const bson_t *v_document,
    void *data)
{
    bson_iter_t child;

    if (bson_iter_init(&child, v_document)) {
        Value val = vp_Value(fs->refarray_new(0));
        bson_iter_visit_all(&child, &bson_value_visitors, &val);
        append_fox_value(data, key, val);
        fs->Value_dec(val);
    }
    return false;
}

// BSONをfoxの値に変換
static int bson_Value(Value *v, const bson_t *bson)
{
    bson_iter_t iter;
    *v = vp_Value(fs->refmap_new(0));

    if (bson_empty0(bson)) {
        return TRUE;
    }
    if (!bson_iter_init(&iter, bson)) {
        return TRUE;
    }
    bson_iter_visit_all(&iter, &bson_value_visitors, v);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int Value_bson_sub(bson_t *bson, const char *key, int key_size, Value v)
{
    const RefNode *v_type = fs->Value_type(v);

    if (v_type == fs->cls_int) {
        int err;
        int64_t val = fs->Value_int64(v, &err);
        if (err) {
            return FALSE;
        }
        if (!bson_append_int64(bson, key, key_size, val)) {
            return FALSE;
        }
    } else if (v_type == fs->cls_float) {
        double d = Value_float2(v);
        if (!bson_append_double(bson, key, key_size, d)) {
            return FALSE;
        }
    } else if (v_type == fs->cls_str) {
        RefStr *rs = Value_vp(v);
        if (!bson_append_utf8(bson, key, key_size, rs->c, rs->size)) {
            return FALSE;
        }
    } else if (v_type == fs->cls_bytes) {
        RefStr *rs = Value_vp(v);
        if (!bson_append_binary(bson, key, key_size, BSON_SUBTYPE_BINARY, (const uint8_t*)rs->c, rs->size)) {
            return FALSE;
        }
    } else if (v_type == fs->cls_bool) {
        if (!bson_append_bool(bson, key, key_size, Value_bool(v))) {
            return FALSE;
        }
    } else if (v_type == fs->cls_timestamp) {
        // TODO
    } else if (v_type == fs->cls_time) {
        // TODO
    } else if (v_type == fs->cls_regex) {
        // TODO
    } else if (v_type == cls_bsonregex) {
        // TODO
    } else if (v_type == cls_bsonoid) {
        RefBsonOID *oid = Value_vp(v);
        if (!bson_append_oid(bson, key, key_size, &oid->oid)) {
            return FALSE;
        }
    } else if (v_type == fs->cls_null) {
        if (!bson_append_null(bson, key, key_size)) {
            return FALSE;
        }
    } else if (v_type == fs->cls_map) {
        int i;
        RefMap *rm = Value_vp(v);
        bson_t child;

        if (!bson_append_document_begin(bson, key, key_size, &child)) {
            return FALSE;
        }
        for (i = 0; i < rm->entry_num; i++) {
            HashValueEntry *h = rm->entry[i];
            for (; h != NULL; h = h->next) {
                RefNode *key_type = fs->Value_type(h->key);
                RefStr *rs_key;
                if (key_type != fs->cls_str) {
                    fs->throw_errorf(fs->mod_lang, "TypeError", "Argument must be map of {str:*, str:* ...}");
                    return FALSE;
                }
                rs_key = Value_vp(h->key);
                if (!Value_bson_sub(&child, rs_key->c, rs_key->size, h->val)) {
                    return FALSE;
                }
            }
        }
        if (!bson_append_document_end(bson, &child)) {
            return FALSE;
        }
    } else if (v_type == fs->cls_list) {
        int i;
        RefArray *ra = Value_vp(v);
        char ckey[16];
        bson_t child;

        if (!bson_append_array_begin(bson, key, key_size, &child)) {
            return FALSE;
        }
        for (i = 0; i < ra->size; i++) {
            sprintf(ckey, "%d", i);
            if (!Value_bson_sub(&child, ckey, strlen(ckey), ra->p[i])) {
                return FALSE;
            }
        }
        if (!bson_append_array_end(bson, &child)) {
            return FALSE;
        }
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "%n cannot convert to BSON", v_type);
        return FALSE;
    }

    return TRUE;
}
static int RefMap_bson(bson_t **pbson, RefMap *rm)
{
    int i;
    bson_t *bson = bson_new();

    for (i = 0; i < rm->entry_num; i++) {
        HashValueEntry *h = rm->entry[i];
        for (; h != NULL; h = h->next) {
            RefNode *key_type = fs->Value_type(h->key);
            RefStr *rs_key;
            if (key_type != fs->cls_str) {
                fs->throw_errorf(fs->mod_lang, "TypeError", "Argument must be map of {str:*, str:* ...}");
                return FALSE;
            }
            rs_key = Value_vp(h->key);
            if (!Value_bson_sub(bson, rs_key->c, rs_key->size, h->val)) {
                if (fg->error == VALUE_NULL) {
                    fs->throw_errorf(mod_mongo, "MongoError", "Failed to convert BSON");
                }
                return FALSE;
            }
        }
    }

    *pbson = bson;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int client_new(Value *vret, Value *v, RefNode *node)
{
    RefMongoClient *cli = fs->buf_new(cls_client, sizeof(RefMongoClient));
    const char *conn_str;

    *vret = vp_Value(cli);

    if (fg->stk_top > v + 1) {
        RefNode *v1_type = fs->Value_type(v[1]);
        if (v1_type != fs->cls_str && v1_type != fs->cls_uri) {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_uri, v1_type, 1);
            return FALSE;
        }
        conn_str = Value_cstr(v[1]);
    } else {
        conn_str = "mongodb://localhost/";
    }
    cli->client = mongoc_client_new(conn_str);
    if (cli->client == NULL) {
        fs->throw_errorf(mod_mongo, "MongoError", "Could not connect to mongod");
        return FALSE;
    }

    return TRUE;
}

static int client_close(Value *vret, Value *v, RefNode *node)
{
    RefMongoClient *cli = Value_vp(*v);

    if (cli->client != NULL) {
        mongoc_client_destroy(cli->client);
        cli->client = NULL;
    }

    return TRUE;
}
static int client_tostr(Value *vret, Value *v, RefNode *node)
{
    RefMongoClient *cli = Value_vp(*v);

    if (cli->client != NULL) {
        const mongoc_uri_t *uri = mongoc_client_get_uri(cli->client);
        *vret = fs->printf_Value("MongoClient(%s)", mongoc_uri_get_string(uri));
    } else {
        *vret = fs->cstr_Value(fs->cls_str, "MongoClient(closed)", -1);
    }
    return TRUE;
}
static int client_get_db(Value *vret, Value *v, RefNode *node)
{
    RefMongoClient *cli = Value_vp(*v);
    RefMongoDatabase *db = fs->buf_new(cls_database, sizeof(RefMongoDatabase));
    *vret = vp_Value(db);

    if (!is_client_open(*v, TRUE)) {
        return FALSE;
    }
    db->db = mongoc_client_get_database(cli->client, Value_cstr(v[1]));
    if (db->db == NULL) {
        fs->throw_errorf(mod_mongo, "MongoError", "mongoc_client_get_database failed");
        return FALSE;
    }
    db->client = fs->Value_cp(*v);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static int database_close(Value *vret, Value *v, RefNode *node)
{
    RefMongoDatabase *db = Value_vp(*v);

    if (db->db != NULL) {
        mongoc_database_destroy(db->db);
        db->db = NULL;
    }
    fs->Value_dec(db->client);
    db->client = VALUE_NULL;

    return TRUE;
}
static int database_tostr(Value *vret, Value *v, RefNode *node)
{
    RefMongoDatabase *db = Value_vp(*v);

    if (db->db != NULL) {
        *vret = fs->printf_Value("MongoDatabase(%s)", mongoc_database_get_name(db->db));
    } else {
        *vret = fs->cstr_Value(fs->cls_str, "MongoDatabase(closed)", -1);
    }

    return TRUE;
}
static int database_get_col(Value *vret, Value *v, RefNode *node)
{
    RefMongoDatabase *db = Value_vp(*v);
    RefMongoCollection *col = fs->buf_new(cls_client, sizeof(RefMongoCollection));
    *vret = vp_Value(col);

    if (!is_client_open(db->client, TRUE)) {
        return FALSE;
    }
    col->client = fs->Value_cp(db->client);
    col->db = fs->Value_cp(*v);

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static RefMongoCollection *RefMongoCollection_copy(RefMongoCollection *col, const char *path)
{
    RefMongoCollection *col2 = fs->buf_new(cls_collection, sizeof(RefMongoCollection));

    col2->db = col->db;
    col2->client = fs->Value_cp(col->client);
    col2->query = FoxBson_cp(col->query);
    col2->orderby = FoxBson_cp(col->orderby);
    col2->fields = FoxBson_cp(col->fields);

    if (path != NULL) {
        if (col->path != NULL) {
            char *new_path = fs->str_printf("%s.%s", col->path, path);
            free(col->path);
            col2->path = new_path;
        } else {
            col2->path = fs->str_dup_p(path, -1, NULL);
        }
    } else {
        if (col->path != NULL) {
            col2->path = fs->str_dup_p(col->path, -1, NULL);
        }
    }
    return col2;
}

static int collection_close(Value *vret, Value *v, RefNode *node)
{
    RefMongoCollection *col = Value_vp(*v);

    fs->Value_dec(col->db);
    col->db = VALUE_NULL;
    fs->Value_dec(col->client);
    col->client = VALUE_NULL;

    return TRUE;
}
static int collection_tostr(Value *vret, Value *v, RefNode *node)
{
    RefMongoCollection *col = Value_vp(*v);
    RefMongoDatabase *db = Value_vp(col->db);

    *vret = fs->printf_Value("MongoCollection(%s.%s)", mongoc_database_get_name(db->db), col->path != NULL ? col->path : "");

    return TRUE;
}
static int collection_get_col(Value *vret, Value *v, RefNode *node)
{
    RefMongoCollection *col = Value_vp(*v);
    RefMongoCollection *col2;

    // TODO collection名の文字種チェック

    if (!is_client_open(col->client, TRUE)) {
        return FALSE;
    }
    col2 = RefMongoCollection_copy(col, Value_cstr(v[1]));
    *vret = vp_Value(col2);

    return TRUE;
}

static int collection_find(Value *vret, Value *v, RefNode *node)
{
    RefMongoCollection *col = Value_vp(*v);
    RefMongoCollection *col2 = RefMongoCollection_copy(col, NULL);
    *vret = vp_Value(col2);

    {
        bson_t *bson;
        if (!RefMap_bson(&bson, Value_vp(v[1]))) {
            goto ERROR_END;
        }
        FoxBson_dec(col2->query);
        col2->query = FoxBson_new(bson);
    }
    if (fg->stk_top > v + 2) {
        bson_t *bson;
        if (!RefMap_bson(&bson, Value_vp(v[2]))) {
            goto ERROR_END;
        }
        FoxBson_dec(col2->fields);
        col2->fields = FoxBson_new(bson);
    }

    return TRUE;

ERROR_END:
    return FALSE;
}
static int collection_iterator(Value *vret, Value *v, RefNode *node)
{
    RefMongoCollection *col = Value_vp(*v);
    RefMongoCursor *cur = fs->buf_new(cls_cursor, sizeof(RefMongoCursor));

    if (!is_client_open(col->client, TRUE)) {
        return FALSE;
    }
    cur->client = fs->Value_cp(col->client);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int cursor_close(Value *vret, Value *v, RefNode *node)
{
    RefMongoCursor *cur = Value_vp(*v);

    if (cur->cur != NULL) {
        mongoc_cursor_destroy(cur->cur);
        cur->cur = NULL;
    }
    fs->Value_dec(cur->client);
    cur->client = VALUE_NULL;

    return TRUE;
}
static int cursor_next(Value *vret, Value *v, RefNode *node)
{
    RefMongoCursor *cur = Value_vp(*v);
    const bson_t *doc;
    bson_error_t error;
    int ret;

    if (!is_client_open(cur->client, TRUE)) {
        return FALSE;
    }

    ret = mongoc_cursor_next(cur->cur, &doc);
    if (mongoc_cursor_error(cur->cur, &error)) {
        throw_bson_error(&error);
        return FALSE;
    }
    if (ret) {
        if (!bson_Value(vret, doc)) {
            return FALSE;
        }
    } else {
        fs->throw_stopiter();
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int bsonoid_tostr(Value *vret, Value *v, RefNode *node)
{
    bson_oid_t *oid = Value_vp(*v);
    RefStr *rs = fs->refstr_new_n(fs->cls_str, 23 + 10);
    int i;

    strcpy(rs->c, "BsonOID(");
    for (i = 0; i < 12; i++) {
        int c = oid->bytes[i] & 0xFF;
        rs->c[i * 2 +  9] = int_to_hex(c >> 4);
        rs->c[i * 2 + 10] = int_to_hex(c & 0xF);
    }
    rs->c[24 + 9] = ')';

    return TRUE;
}
static int bsonoid_eq(Value *vret, Value *v, RefNode *node)
{
    bson_oid_t *o1 = Value_vp(*v);
    bson_oid_t *o2 = Value_vp(v[1]);
    int i;
    int result = TRUE;

    for (i = 0; i < 12; i++) {
        if (o1->bytes[i] != o2->bytes[i]) {
            result = FALSE;
            break;
        }
    }
    *vret = Value_bool(result);

    return TRUE;
}
static int bsonoid_hash(Value *vret, Value *v, RefNode *node)
{
    bson_oid_t *oid = Value_vp(*v);
    uint32_t h = 0;
    int i;

    for (i = 0; i < 12; i++) {
        h = h * 31 + oid->bytes[i];
    }
    *vret = int32_Value(h & INT32_MAX);

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int bsonregex_new(Value *vret, Value *v, RefNode *node)
{
    Ref *r = fs->ref_new(cls_bsonregex);
    const RefNode *v1_type = fs->Value_type(v[1]);
    Value *src;
    int opts = 0;

    *vret = vp_Value(r);

    if (v1_type == fs->cls_regex) {
        Ref *r2 = Value_ref(v[1]);
        src = &r2->v[INDEX_REGEX_SRC];
        opts = Value_integral(r2->v[INDEX_REGEX_OPT]);
    } else if (v1_type == fs->cls_str) {
        src = &v[1];
        if (fg->stk_top > v + 2) {
            int i;
            RefStr *opt = Value_vp(v[2]);

            for (i = 0; i < opt->size; i++) {
                switch (opt->c[i]) {
                case 'i':
                    opts |= PCRE_CASELESS;
                    break;
                case 'm':
                    opts |= PCRE_MULTILINE;
                    break;
                default:
                    fs->throw_errorf(fs->mod_lang, "ValueError", "Unknowon MongoRegex option %c", opt->c[i]);
                    return FALSE;
                }
            }
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_regex, fs->cls_str, v1_type, 1);
        return FALSE;
    }

    r->v[INDEX_REGEX_SRC] = fs->Value_cp(*src);
    r->v[INDEX_REGEX_OPT] = int32_Value(opts);

    return TRUE;
}
static int bsonregex_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefStr *source = Value_vp(r->v[INDEX_REGEX_SRC]);
    int flags = Value_integral(r->v[INDEX_REGEX_OPT]);
    const char *opt_s;

    switch (flags) {
    case PCRE_CASELESS:
        opt_s = "i";
        break;
    case PCRE_MULTILINE:
        opt_s = "m";
        break;
    case PCRE_CASELESS | PCRE_MULTILINE:
        opt_s = "im";
        break;
    default:
        opt_s = "";
        break;
    }
    *vret = fs->printf_Value("MongoRegex(/%r/%s)", source, opt_s);

    return TRUE;
}
static int bsonregex_source(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    *vret = fs->Value_cp(r->v[INDEX_REGEX_SRC]);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void mongoc_log_func(
    mongoc_log_level_t log_level,
    const char         *log_domain,
    const char         *message,
    void               *user_data)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;


    cls_client = fs->define_identifier(m, m, "MongoClient", NODE_CLASS, 0);
    cls_database = fs->define_identifier(m, m, "MongoDatabase", NODE_CLASS, 0);
    cls_collection = fs->define_identifier(m, m, "MongoCollection", NODE_CLASS, 0);
    cls_cursor = fs->define_identifier(m, m, "MongoCursor", NODE_CLASS, 0);

    cls_bsonoid = fs->define_identifier(m, m, "BsonOID", NODE_CLASS, 0);
    cls_bsonregex = fs->define_identifier(m, m, "BsonRegex", NODE_CLASS, 0);


    cls = cls_client;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, client_new, 0, 1, cls, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, client_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, client_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, client_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, client_get_db, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, client_get_db, 1, 1, NULL, NULL);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_database;
    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, database_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, database_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, database_get_col, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, database_get_col, 1, 1, NULL, NULL);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_collection;
    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, collection_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, collection_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, collection_get_col, 1, 1, NULL, fs->cls_str);
    n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, collection_get_col, 1, 1, NULL, NULL);
    n = fs->define_identifier(m, cls, "find", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, collection_find, 0, 2, NULL, fs->cls_map, fs->cls_map);
    n = fs->define_identifier_p(m, cls, fs->str_iterator, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, collection_iterator, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_cursor;
    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cursor_close, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_next, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cursor_next, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_iterator);


    cls = cls_bsonoid;
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, bsonoid_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, bsonoid_hash, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, bsonoid_eq, 1, 1, NULL, cls_bsonoid);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_bsonregex;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, bsonregex_new, 1, 2, cls, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, bsonregex_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier(m, cls, "source", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, bsonregex_source, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_REGEX_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "MongoError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_mongo = m;

    define_class(m);
    mongoc_init();
    mongoc_log_set_handler(mongoc_log_func, NULL);
}

const char *module_version(const FoxStatic *a_fs)
{
    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    return "Build at\t" __DATE__ "\nMongoDB\t" MONGOC_VERSION_S "\n";
}
