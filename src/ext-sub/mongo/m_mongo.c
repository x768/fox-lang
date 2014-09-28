#define DEFINE_GLOBALS
#include "fox.h"
#include <mongoc.h>
#include <math.h>
#include <pcre.h>


enum {
	INDEX_MONGODB_DB,
	INDEX_MONGODB_NAMESPACE,
	INDEX_MONGODB_NUM,
};

typedef struct {
	RefHeader rh;

	int valid;
	Value conn_str;
	int port;
	mongoc_client_t *conn;
} RefMongoDB;

typedef struct {
	RefHeader rh;

	int valid;
	int started;
	Value connect;
	mongoc_cursor_t *cur;
	bson_t query;
	bson_t field;
	int field_init;
} RefMongoCursor;

typedef struct {
	RefHeader rh;
	const bson_oid_t *oid;
} RefBsonOID;

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_mongo;
static RefNode *cls_mongo;
static RefNode *cls_mongodb;
static RefNode *cls_mongocollection;
static RefNode *cls_mongocursor;

static RefNode *cls_bsonoid;
static RefNode *cls_bsonregex;


// ASCII の範囲内でほとんどの文字を使うことができます。 しかし、" " や "." を使うことはできず、空文字列にすることもできません。 "system" という名前も予約済みで、使うことができません。
// あまり一般的ではありませんが、 "null"、"[x,y]"、"3"、"\""、"/" などは正しい形式のデータベース名です。
// コレクション名とは異なり、データベース名には "$" を含めてもかまいません。 
static int is_valid_mongo_name(RefStr *s, int collection)
{
	int i;
	int dot_ok = FALSE;

	for (i = 0; i < s->size; i++) {
		int c = s->c[i] & 0xFF;
		if (!isgraph(c)) {
			return FALSE;
		}
		if (collection) {
			if (c == '$') {
				return FALSE;
			}
			// .が連続していたらエラー
			if (c == '.') {
				if (!dot_ok) {
					return FALSE;
				}
				dot_ok = FALSE;
			} else {
				dot_ok = TRUE;
			}
		}
	}
	if (collection) {
		if (!dot_ok) {
			return FALSE;
		}
	}
	// 予約語
	if (s->size == 6 && memcmp(s->c, "system", 6) == 0) {
		return FALSE;
	}
	return TRUE;
}
static int int_to_hex(int i)
{
	if (i < 10) {
		return '0' + i;
	} else {
		return 'a' + i - 10;
	}
}
static void throw_already_closed(void)
{
	fs->throw_errorf(mod_mongo, "MongoError", "Connection already closed");
}
static void throw_iteration_already_started(void)
{
	fs->throw_errorf(mod_mongo, "MongoError", "Iteration already started");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int mongo_new(Value *vret, Value *v, RefNode *node)
{
	RefMongoDB *mon = fs->new_buf(cls_mongo, sizeof(RefMongoDB));
	const char *conn_str;

	*vret = vp_Value(mon);
	mon->valid = FALSE;

	if (fg->stk_top > v + 1) {
		RefStr *rs = Value_vp(v[1]);
		conn_str = rs->c;
		mon->conn_str = fs->Value_cp(v[1]);
	} else {
		conn_str = "mongodb://localhost/";
		mon->conn_str = fs->cstr_Value(fs->cls_str, conn_str, -1);
	}
	mon->conn = mongoc_client_new(conn_str);
	if (mon->conn == NULL) {
		fs->throw_errorf(mod_mongo, "MongoError", "Could not connect to mongod");
		return FALSE;
	}
	mon->valid = TRUE;

	return TRUE;
}

static int mongo_close(Value *vret, Value *v, RefNode *node)
{
	RefMongoDB *mon = Value_vp(*v);

	if (mon->valid) {
		mongoc_client_destroy(mon->conn);
		mon->valid = FALSE;
	}
	fs->Value_dec(mon->conn_str);
	mon->conn_str = VALUE_NULL;

	return TRUE;
}
static int mongo_tostr(Value *vret, Value *v, RefNode *node)
{
	RefMongoDB *mon = Value_vp(*v);

	if (mon->valid) {
		*vret = fs->printf_Value("Mongo(%v)", mon->conn_str);
	} else {
		*vret = fs->cstr_Value(fs->cls_str, "Mongo(closed)", -1);
	}

	return TRUE;
}
static int mongo_select_db(Value *vret, Value *v, RefNode *node)
{
	Ref *r = fs->new_ref(cls_mongodb);

	*vret = vp_Value(r);
	if (!is_valid_mongo_name(Value_vp(v[1]), FALSE)) {
		fs->throw_errorf(mod_mongo, "MongoError", "Illigal MongoDB name");
		return FALSE;
	}

	r->v[INDEX_MONGODB_DB] = fs->Value_cp(*v);
	r->v[INDEX_MONGODB_NAMESPACE] = fs->Value_cp(v[1]);
	
	return TRUE;
}
static int mongodb_select_col(Value *vret, Value *v, RefNode *node)
{
	Ref *r_src = Value_ref(*v);
	Ref *r_dst;
	RefStr *name_base = Value_vp(r_src->v[INDEX_MONGODB_NAMESPACE]);
	RefStr *name = Value_vp(v[1]);

	if (!is_valid_mongo_name(name, TRUE)) {
		fs->throw_errorf(mod_mongo, "MongoError", "Illigal MongoDB name");
		return FALSE;
	}

	r_dst = fs->new_ref(cls_mongocollection);
	r_dst->v[INDEX_MONGODB_DB] = fs->Value_cp(r_src->v[INDEX_MONGODB_DB]);
	*vret = vp_Value(r_dst);

	r_dst->v[INDEX_MONGODB_NAMESPACE] = fs->printf_Value("%r.%r", name_base, name);
	fs->Value_dec(vp_Value(name_base));
	
	return TRUE;
}
static int mongodb_tostr(Value *vret, Value *v, RefNode *node)
{
	const char *name = FUNC_VP(node);
	Ref *r = Value_ref(*v);
	RefMongoDB *mon = Value_vp(r->v[INDEX_MONGODB_DB]);
	RefStr *path = Value_vp(r->v[INDEX_MONGODB_NAMESPACE]);

	if (mon->valid) {
		*vret = fs->printf_Value("%s(conn_str=%r, path=%r)", name, Value_vp(mon->conn_str), path);
	} else {
		*vret = fs->cstr_Value(fs->cls_str, "%s(closed)", -1);
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static mongoc_collection_t *mongo_get_collection(Ref *r, RefMongoDB *mon)
{
	RefStr *path = Value_vp(r->v[INDEX_MONGODB_NAMESPACE]);
	char *path_p = fs->str_dup_p(path->c, path->size, NULL);
	char *col_p = path_p + path->size - 1;

	while (col_p > path_p) {
		if (*col_p == '.') {
			*col_p++ = '\0';
			break;
		}
		col_p--;
	}

	if (col_p > path_p) {
		mongoc_collection_t *coll = mongoc_client_get_collection(mon->conn, path_p, col_p);
		free(path_p);
		return coll;
	} else {
		return NULL;
	}
}
static void throw_mongo_error(bson_error_t *err)
{
	fs->throw_errorf(mod_mongo, "MongoError", "%s", err->message);
}

/////////////////////////////////////////////////////////////////////////////

static int get_mongo_conn(mongoc_client_t **pmongo, char **path_pp, Value *v, RefNode *node)
{
	Ref *r = Value_ref(*v);
	RefMongoDB *mon = Value_vp(r->v[INDEX_MONGODB_DB]);
	RefStr *path = Value_vp(r->v[INDEX_MONGODB_NAMESPACE]);

	if (mon->valid) {
		*pmongo = mon->conn;
		*path_pp = path->c;
		return TRUE;
	} else {
		throw_already_closed();
		return FALSE;
	}
	return TRUE;
}

static int mongo_get_count(int64_t *result, Value v, const bson_t *query)
{
	RefMongoDB *mon;
	Ref *r;
	bson_error_t err;
	mongoc_collection_t *coll;

	if (v == VALUE_NULL) {
		throw_already_closed();
		return FALSE;
	}

	r = Value_vp(v);
	mon = Value_vp(r->v[INDEX_MONGODB_DB]);

	if (!mon->valid) {
		throw_already_closed();
		return FALSE;
	}

	coll = mongo_get_collection(r, mon);
	if (coll == NULL) {
		throw_already_closed();
		return FALSE;
	}
	*result = mongoc_collection_count(coll, MONGOC_QUERY_NONE, query, 0, 0, NULL, &err);
	mongoc_collection_destroy(coll);

	if (*result < 0) {
		throw_mongo_error(&err);
		return FALSE;
	}

	return TRUE;
}

static int bson_iter_to_value(Value *v, bson_iter_t *iter, bson_type_t type)
{
	*v = VALUE_NULL;

	switch (type) {
	case BSON_TYPE_NULL:
		*v = VALUE_NULL;
		break;
	case BSON_TYPE_BOOL:
		*v = bool_Value(bson_iter_bool(iter));
		break;
	case BSON_TYPE_INT32: {
		int32_t i = bson_iter_int32(iter);
		*v = int32_Value(i);
		break;
	}
	case BSON_TYPE_INT64: {
		int64_t i = bson_iter_int64(iter);
		*v = fs->int64_Value(i);
		break;
	}
	case BSON_TYPE_DOUBLE: {
		double dbl = bson_iter_double(iter);
		if (isnan(dbl)) {
			// TODO
		} else if (isinf(dbl)) {
			// TODO
		} else {
			*v = fs->float_Value(dbl);
		}
		break;
	}
	case BSON_TYPE_UTF8:
	case BSON_TYPE_SYMBOL: {
		uint32_t len;
		const char *p = bson_iter_utf8(iter, &len);
		*v = fs->cstr_Value_conv(p, len, NULL);
		break;
	}
	case BSON_TYPE_BINARY: {
		Str s;
		s.p = bson_iterator_bin_data(iter);
		s.size = bson_iterator_bin_len(iter);
		fs->Value_new_str(v, fs->cls_bytes, s);
		break;
	}
	case BSON_TYPE_OID: {
		RefBsonOID *rid = fs->new_buf(cls_bsonoid, sizeof(RefBsonOID));
		rid->oid = bson_iter_oid(iter);
		break;
	}
	case BSON_TYPE_DATE_TIME: {
		RefInt64 *rt = fs->new_buf(fs->cls_timestamp, sizeof(RefInt64));
		*v = vp_Value(rt);
		rt->u.i = bson_iter_date_time(iter);
		break;
	}
	case BSON_TYPE_TIMESTAMP: {
		int64_t tm = bson_iterator_time_t(iter);
		tm *= 1000LL;
		fs->Value_setint(v, fs->cls_timestamp, tm);
		break;
	}
	case BSON_OBJECT: {
		bson_iter_t iter2;
		RefMap *rm = fs->refmap_new(0);

		*v = vp_Value(rm);
		bson_iterator_from_buffer(&iter2, bson_iterator_value(iter));

		for (;;) {
			Value key, val;

			bson_type type2 = bson_iterator_next(&iter2);
			if (type2 == BSON_EOO) {
				break;
			}
			fs->Value_new_str(&key, fs->cls_str, fs->Str_new_p(bson_iterator_key(&iter2)));
			if (!bson_iter_to_value(&val, &iter2, type2, e)) {
				return FALSE;
			}
			if (fs->map_add_elem(e, v, &key, &val, TRUE, FALSE) == NULL) {
				return FALSE;
			}
		}

		break;
	}
	case BSON_TYPE_ARRAY: {
		RefArray *ra = fs->refarray_new(0);
		*v = vp_Value(ra);
		bson_iter_t iter2;
		bson_iterator_from_buffer(&iter2, bson_iterator_value(iter));

		for (;;) {
			Value *pv;

			bson_type type2 = bson_iterator_next(&iter2);
			if (type2 == BSON_EOO) {
				break;
			}
			pv = fs->array_push_value(ra);
			if (!bson_iter_to_value(pv, &iter2, type2, e)) {
				return FALSE;
			}
		}
		break;
	}
	case BSON_TYPE_REGEX: {
		const char *pattern = bson_iterator_regex(iter);
		const char *opt = bson_iterator_regex_opts(iter);
		int opts = 0;
		Ref *r = fs->Value_new_ref(v, cls_bsonregex);

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
		fs->Value_new_str(&r->v[INDEX_REGEX_SRC], fs->cls_str, fs->Str_new_p(pattern));
		fs->Value_setint(&r->v[INDEX_REGEX_OPT], fs->cls_int, opts);
		break;
	}
	default:
		break;
	}
	return TRUE;
}
static int bson_to_map_value(Value *v, bson *bs)
{
	bson_iterator iter;

	bson_iterator_init(&iter, bs);
	fs->map_init_sized(v, 0);

	for (;;) {
		bson_type type = bson_iterator_next(&iter);
		Value key;
		Value val;

		if (type == BSON_EOO) {
			break;
		}
		fs->Value_new_str(&key, fs->cls_str, fs->Str_new_p(bson_iterator_key(&iter)));
		if (!bson_iter_to_value(&val, &iter, type, e)) {
			return FALSE;
		}
		if (fs->map_add_elem(e, v, &key, &val, TRUE, FALSE) == NULL) {
			return FALSE;
		}
	}

	return TRUE;
}

static int map_to_bson(bson *bs, Value *v);
static int array_to_bson(bson *bs, Value *v);

// TODO 循環参照チェック
static int elem_value_to_bson(bson *bs, Value *v, const char *name_p)
{
	const Node *val_type = fs->Value_type(v);

	if (val_type == fs->cls_int) {
		int err = FALSE;
		int64_t i64 = fs->Value_int(v, &err);
		if (err) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "NumberLong out of range (-2^63 - 2^63-1)");
			return FALSE;
		}
		bson_append_long(bs, name_p, i64);
	} else if (val_type == fs->cls_float) {
		double d = v->b.d;
		bson_append_double(bs, name_p, d);
	} else if (val_type == fs->cls_str) {
		Str val_s = fs->Value_str(v);
		bson_append_string_n(bs, name_p, val_s.p, val_s.size);
	} else if (val_type == fs->cls_timestamp) {
		int64_t i64 = VALUE_INT(*v);
		bson_append_date(bs, name_p, i64);
	} else if (val_type == fs->cls_array) {
		bson_append_start_array(bs, name_p);
		if (!array_to_bson(bs, v, e)) {
			return FALSE;
		}
		bson_append_finish_object(bs);
	} else if (val_type == fs->cls_map) {
		bson_append_start_object(bs, name_p);
		if (!map_to_bson(bs, v, e)) {
			return FALSE;
		}
		bson_append_finish_object(bs);
	} else if (val_type == fs->cls_regex || val_type == cls_bsonregex) {
		Ref *r = v->b.ref;
		char c_opts[6];
		char *p_opts = c_opts;
		char *pattern = fs->Str_dup_p(fs->Value_str(&r->v[INDEX_REGEX_SRC]), NULL);
		int opts = r->v[INDEX_REGEX_OPT].b.i;

		if ((opts & PCRE_CASELESS) != 0) {
			*p_opts++ = 'i';
		}
		if ((opts & PCRE_MULTILINE) != 0) {
			*p_opts++ = 'm';
		}
		*p_opts = '\0';
		if (bson_append_regex(bs, name_p, pattern, c_opts) != BSON_OK) {
			fs->throw_errorf(mod_mongo, "MongoError", "Syntax error on MongoRegex");
			free(pattern);
			return FALSE;
		}
		free(pattern);
	} else if (val_type == cls_bsonoid) {
		bson_oid_t *oid = VALUE_BUF(*v);
		bson_append_oid(bs, name_p, oid);
	} else {
		const Str *s = val_type->name;
		fs->throw_error(e, fs->mod_lang, "TypeError", "Int, Float, Str, Bytes, Regex, MongoRegex or Time required but %.*s", s->size, s->p);
		return FALSE;
	}
	return TRUE;
}
static int array_to_bson(bson *bs, Value *v)
{
	RefArray *arr = VALUE_BUF(*v);
	int i;

	for (i = 0; i < arr->size; i++) {
		char num[32];
		sprintf(num, "%d", i);

		if (!elem_value_to_bson(bs, &arr->p[i], num, e)) {
			return FALSE;
		}
	}
	return TRUE;
}
static int map_to_bson(bson *bs, Value *v)
{
	RefMap *map = VALUE_BUF(*v);
	int i;

	for (i = 0; i < map->entry_num; i++) {
		HashValueEntry *ep;
		for (ep = map->entry[i]; ep != NULL; ep = ep->next) {
			char *name_p;

			if (fs->Value_type(&ep->key) != fs->cls_str) {
				fs->throw_errorf(fs->mod_lang, "TypeError", "Argument must be map of {str:*, str:* ...}");
				return FALSE;
			}
			name_p = fs->Str_dup_p(fs->Value_str(&ep->key), NULL);
			if (!elem_value_to_bson(bs, &ep->val, name_p, e)) {
				free(name_p);
				return FALSE;
			}
			free(name_p);
		}
	}
	return TRUE;
}
static int mongocol_find(Value *vret, Value *v, RefNode *node)
{
	mongo *mon;
	char *path;
	MongoCursor *cur = fs->Value_new_ptr(vret, cls_mongocursor, sizeof(MongoCursor), NULL);

	if (!get_mongo_conn(&mon, &path, v, node)) {
		return FALSE;
	}
	mongo_cursor_init(&cur->cur, mon, path);
	free(path);

	bson_init(&cur->query);
	bson_append_start_object(&cur->query, "$query");
	if (fg->stk_top > v + 1) {
		if (!map_to_bson(&cur->query, &v[1], e)) {
			bson_destroy(&cur->query);
			goto ERROR_END;
		}
  	}
	bson_append_finish_object(&cur->query);
	if (fg->stk_top > v + 2) {
		bson_init(&cur->field);
		if (!map_to_bson(&cur->field, &v[2], e)) {
			bson_destroy(&cur->field);
			goto ERROR_END;
		}
		bson_finish(&cur->field);
		mongo_cursor_set_fields(&cur->cur, &cur->field);
		cur->field_init = TRUE;
	}
	cur->connect = *v;
	VALUE_INC(*v);
	cur->valid = TRUE;

	return TRUE;

ERROR_END:
	mongo_cursor_destroy(&cur->cur);
	return FALSE;
}
static int mongocol_find_one(Value *vret, Value *v, RefNode *node)
{
	mongo *mon;
	char *path;
	bson query;
	bson field;
	bson out;

	if (!get_mongo_conn(&mon, &path, v, node)) {
		return FALSE;
	}
	bson_init(&query);
	if (!map_to_bson(&query, &v[1])) {
		bson_destroy(&query);
		goto ERROR_END;
	}
	bson_finish(&query);

	bson_init(&field);
	if (fg->stk_top > v + 2) {
		if (!map_to_bson(&field, &v[2])) {
			bson_destroy(&field);
			goto ERROR_END;
		}
	}
	bson_finish(&field);
	if (mongo_find_one(mon, path, &query, &field, &out) != MONGO_OK) {
		fs->throw_errorf(mod_mongo, "MongoError", "An error occured");
		goto ERROR_END;
	}
	if (!bson_to_map_value(vret, &out)) {
		goto ERROR_END;
	}
	bson_destroy(&out);

	return TRUE;

ERROR_END:
	return FALSE;
}
static int mongocol_count(Value *vret, Value *v, RefNode *node)
{
	int64_t count;

	if (!mongo_get_count(&count, v, NULL)) {
		return FALSE;
	}
	fs->Value_setint(vret, fs->cls_int, count);

	return TRUE;
}
static int mongocol_insert(Value *vret, Value *v, RefNode *node)
{
	mongo *mon;
	char *path;
	bson data;

	if (!get_mongo_conn(&mon, &path, v, node)) {
		return FALSE;
	}
	bson_init(&data);
	if (!map_to_bson(&data, &v[1])) {
		bson_destroy(&data);
		goto ERROR_END;
	}
	bson_finish(&data);

	if (mongo_insert(mon, path, &data, NULL) != MONGO_OK) {
		fs->throw_errorf(mod_mongo, "MongoError", "An error occured");
		goto ERROR_END;
	}
	return TRUE;

ERROR_END:
	return FALSE;
}
static int mongocol_update(Value *vret, Value *v, RefNode *node)
{
	mongo *mon;
	char *path;
	bson data;
	bson cond;

	if (!get_mongo_conn(&mon, &path, v, node)) {
		return FALSE;
	}
	bson_init(&data);
	if (!map_to_bson(&data, &v[1])) {
		bson_destroy(&data);
		goto ERROR_END;
	}
	bson_finish(&data);

	bson_init(&cond);
	if (!map_to_bson(&cond, &v[1])) {
		bson_destroy(&cond);
		bson_destroy(&data);
		goto ERROR_END;
	}
	bson_finish(&cond);

	if (mongo_update(mon, path, &cond, &data, MONGO_UPDATE_MULTI, NULL) != MONGO_OK) {
		fs->throw_errorf(mod_mongo, "MongoError", "An error occured");
		goto ERROR_END;
	}
	return TRUE;

ERROR_END:
	return FALSE;
}
static int mongocol_remove(Value *vret, Value *v, RefNode *node)
{
	mongo *mon;
	char *path;
	bson data;

	if (!get_mongo_conn(&mon, &path, v, node)) {
		return FALSE;
	}
	bson_init(&data);
	if (!map_to_bson(&data, &v[1])) {
		bson_destroy(&data);
		goto ERROR_END;
	}
	bson_finish(&data);

	if (mongo_remove(mon, path, &data, NULL) != MONGO_OK) {
		fs->throw_errorf(mod_mongo, "MongoError", "An error occured");
		goto ERROR_END;
	}
	return TRUE;

ERROR_END:
	return FALSE;
}
static int mongocol_drop(Value *vret, Value *v, RefNode *node)
{
	mongoc_client_t *mon;
	char *path;

	if (!get_mongo_conn(&mon, &path, v, node)) {
		return FALSE;
	}

	return TRUE;
}
static int mongocur_sort(Value *vret, Value *v, RefNode *node)
{
	RefMongoCursor *cur = Value_vp(*v);

	if (cur->started) {
		throw_iteration_already_started();
		return FALSE;
	}

	bson_append_start_object(&cur->query, "$orderby");
	if (!map_to_bson(&cur->query, &v[1])) {
		bson_destroy(&cur->query);
		goto ERROR_END;
	}
	bson_append_finish_object(&cur->query);

	*vret = *v;
	VALUE_INC(*v);

	return TRUE;

ERROR_END:
	return FALSE;
}
static int mongocur_limit(Value *vret, Value *v, RefNode *node)
{
	RefMongoCursor *cur = Value_vp(*v);
	int64_t lim = fs->Value_int(&v[1], NULL);

	if (cur->started) {
		throw_iteration_already_started();
		return FALSE;
	}
	mongo_cursor_set_limit(&cur->cur, lim);

	*vret = *v;
	VALUE_INC(*v);

	return TRUE;
}
static int mongocur_skip(Value *vret, Value *v, RefNode *node)
{
	RefMongoCursor *cur = Value_vp(*v);
	int64_t n = fs->Value_int(&v[1], NULL);

	if (cur->started) {
		throw_iteration_already_started();
		return FALSE;
	}
	mongo_cursor_set_skip(&cur->cur, n);

	*vret = *v;
	VALUE_INC(*v);

	return TRUE;
}
static int mongocur_count(Value *vret, Value *v, RefNode *node)
{
	RefMongoCursor *cur = Value_vp(*v);
	int64_t count = 0LL;

	if (cur->valid) {
		bson_iterator iter;

		if (cur->started) {
			throw_iteration_already_started();
			return FALSE;
		}
		bson_finish(&cur->query);

		bson_iterator_init(&iter, &cur->query);
		if (bson_iterator_next(&iter) == BSON_OBJECT) {
			// 最初の要素は$queryであるのでチェックしない
			bson_t sub;
			bson_iterator_subobject(&iter, &sub);
			if (!mongo_get_count(&count, cur->connect, &sub)) {
				return FALSE;
			}
		}
	}
	fs->Value_setint(vret, fs->cls_int, count);

	return TRUE;
}
static int mongocur_next(Value *vret, Value *v, RefNode *node)
{
	RefMongoCursor *cur = Value_vp(*v);

	if (!cur->valid) {
		fs->throw_stopiter();
		return FALSE;
	}

	if (!cur->started) {
		bson_finish(&cur->query);
		mongo_cursor_set_query(&cur->cur, &cur->query);
		cur->started = TRUE;
	}
	if (mongo_cursor_next(&cur->cur) == MONGO_OK) {
		if (!bson_to_map_value(vret, &cur->cur.current)) {
			return FALSE;
		}
	} else {
		fs->throw_stopiter();
		return FALSE;
	}
	return TRUE;
}
static int mongocur_close(Value *vret, Value *v, RefNode *node)
{
	RefMongoCursor *cur = Value_vp(*v);

	if (cur->valid) {
		mongo_cursor_destroy(&cur->cur);
		bson_destroy(&cur->query);
		if (cur->field_init) {
			bson_destroy(&cur->field);
			cur->field_init = FALSE;
		}
		fs->Value_dec(cur->connect);
		cur->connect = VALUE_NULL;
		cur->valid = FALSE;
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////

static int bsonoid_tostr(Value *vret, Value *v, RefNode *node)
{
	bson_oid_t *oid = Value_vp(*v);
	char *cbuf = fs->Value_new_str_n(vret, fs->cls_str, 24 + 10);
	int i;

	strcpy(cbuf, "ObjectId(");
	for (i = 0; i < 12; i++) {
		int c = oid->bytes[i] & 0xFF;
		cbuf[i * 2 +  9] = int_to_hex(c >> 4);
		cbuf[i * 2 + 10] = int_to_hex(c & 0xF);
	}
	cbuf[24 + 9] = ')';

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
	Ref *r = fs->new_ref(cls_bsonregex);
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

static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;


	cls_mongo = fs->define_identifier(m, m, "Mongo", NODE_CLASS, 0);
	cls_mongodb = fs->define_identifier(m, m, "MongoDB", NODE_CLASS, 0);
	cls_mongocollection = fs->define_identifier(m, m, "MongoCollection", NODE_CLASS, 0);
	cls_mongocursor = fs->define_identifier(m, m, "MongoCursor", NODE_CLASS, 0);
	cls_bsonoid = fs->define_identifier(m, m, "BsonOID", NODE_CLASS, 0);
	cls_bsonregex = fs->define_identifier(m, m, "BsonRegex", NODE_CLASS, 0);


	cls = cls_mongo;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, mongo_new, 0, 3, cls, fs->cls_str, fs->cls_int, fs->cls_int);

	n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongo_close, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongo_close, 0, 0, NULL);
	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongo_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);

	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongo_select_db, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongo_select_db, 1, 1, NULL, NULL);
	fs->extends_method(cls, fs->cls_obj);


	cls = cls_mongodb;
	cls->u.c.n_memb = INDEX_MONGODB_NUM;

	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongodb_tostr, 0, 2, (void*)"MongoDB", fs->cls_str, fs->cls_locale);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongodb_select_col, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongodb_select_col, 1, 1, NULL, NULL);
	fs->extends_method(cls, fs->cls_obj);


	cls = cls_mongocollection;
	cls->u.c.n_memb = INDEX_MONGODB_NUM;

	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongodb_tostr, 0, 2, (void*)"MongoCollection", fs->cls_str, fs->cls_locale);
	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongodb_select_col, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier_p(m, cls, fs->str_missing, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongodb_select_col, 1, 1, NULL, NULL);
	n = fs->define_identifier(m, cls, "find", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocol_find, 0, 2, NULL, fs->cls_map, fs->cls_map);
	n = fs->define_identifier(m, cls, "find_one", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocol_find_one, 1, 2, NULL, fs->cls_map, fs->cls_map);
	n = fs->define_identifier(m, cls, "count", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocol_count, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "insert", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocol_insert, 1, 1, NULL, fs->cls_map);
	n = fs->define_identifier(m, cls, "update", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocol_update, 2, 2, NULL, fs->cls_map);
	n = fs->define_identifier(m, cls, "remove", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocol_remove, 1, 1, NULL, fs->cls_map);
	n = fs->define_identifier(m, cls, "drop", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocol_drop, 0, 0, NULL);
	fs->extends_method(cls, fs->cls_obj);
	

	cls = cls_mongocursor;
	n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocur_close, 0, 0, NULL);

	n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocur_next, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "sort", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocur_sort, 1, 1, NULL, fs->cls_map);
	n = fs->define_identifier(m, cls, "limit", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocur_limit, 1, 1, NULL, fs->cls_int);
	n = fs->define_identifier(m, cls, "skip", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocur_skip, 1, 1, NULL, fs->cls_int);
	n = fs->define_identifier(m, cls, "count", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, mongocur_count, 0, 0, NULL);
	fs->extends_method(cls, fs->cls_iterator);


	cls = cls_bsonoid;
	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, bsonoid_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, bsonoid_hash, 0, 0, NULL);

	n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
	fs->define_native_func_a(n, bsonoid_eq, 1, 1, NULL, cls_mongooid);
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
	mongo_init_sockets();
}

const char *module_version(const FoxStatic *a_fs)
{
	return "Build at\t" __DATE__ "\nMongoDB\t" MONGOC_VERSION_S "\n";
}
