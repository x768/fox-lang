#define DEFINE_GLOBALS
#include "fox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>



enum {
	QUERY_EXEC,
	QUERY_ARRAY,
	QUERY_MAP,
	QUERY_SINGLE,
};

enum {
	RESULT_BUFFER_INIT_SIZE = 256,
};

typedef struct {
	RefHeader rh;

	int valid;
	char *host;
	char *user;
	int port;
	RefTimeZone *tz;
	MYSQL conn;
} RefMysql;

typedef struct {
	RefHeader rh;

	int valid;
	int is_map;
	Value connect;
	Mem mem;
	RefTimeZone *tz;
	MYSQL_STMT *stmt;

	MYSQL_BIND *bind;
	MYSQL_BIND *result;
	int n_field;
	MYSQL_FIELD *fields;
} RefMysqlCursor;

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_mysql;
static RefNode *cls_mysql;
static RefNode *cls_cursor;

static Value mysql_to_fox_value(MYSQL_BIND *b, RefMysqlCursor *cur, int n);

static void mysql_fox_error(MYSQL *conn)
{
	fs->throw_errorf(mod_mysql, "MySQLError", "%s", mysql_error(conn));
}
static void mysql_stmt_fox_error(MYSQL_STMT *stmt)
{
	fs->throw_errorf(mod_mysql, "MySQLError", "%s", mysql_stmt_error(stmt));
}
static int conn_check_open(RefMysql *my, RefNode *node)
{
	if (!my->valid) {
		fs->throw_errorf(mod_mysql, "MySQLError", "Connection already closed");
		return FALSE;
	}
	return TRUE;
}

static int conn_new(Value *vret, Value *v, RefNode *node)
{
	RefMysql *my = fs->new_buf(cls_mysql, sizeof(RefMysql));
	char *host = Value_cstr(v[1]);
	char *user = Value_cstr(v[2]);
	char *pass = Value_cstr(v[3]);
	char *db   = Value_cstr(v[4]);

	if (fg->stk_top > v + 5) {
		int err;
		int64_t port = fs->Value_int(v[5], &err);
		if (err || port < 1 || port > 65535) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal port number (0 - 65535)");
			return FALSE;
		}
		my->port = port;
	}
	my->host = fs->str_dup_p(host, -1, NULL);
	my->user = fs->str_dup_p(user, -1, NULL);

	mysql_init(&my->conn);
	mysql_real_connect(&my->conn, host, user, pass, db, my->port, NULL, 0);
	if (mysql_errno(&my->conn) != 0) {
		mysql_fox_error(&my->conn);
		return FALSE;
	}
//	mysql_options(&my->conn, MYSQL_SET_CHARSET_NAME, "utf8");
	if (mysql_set_character_set(&my->conn, "utf8mb4") != 0) {
		mysql_set_character_set(&my->conn, "utf8");
	}
	my->valid = TRUE;

	return TRUE;
}
static int conn_close(Value *vret, Value *v, RefNode *node)
{
	RefMysql *my = Value_vp(*v);

	free(my->host);
	my->host = NULL;
	free(my->user);
	my->user = NULL;

	if (my->valid) {
		mysql_close(&my->conn);
		my->valid = FALSE;
	}
	return TRUE;
}
static int conn_tostr(Value *vret, Value *v, RefNode *node)
{
	RefMysql *my = Value_vp(*v);

	if (my->valid) {
		*vret = fs->printf_Value("MySQL(host=%s, user=%s, pass=***, port=%d)", my->host, my->user, my->port);
	} else {
		*vret = fs->cstr_Value(fs->cls_str, "MySQL(closed)", -1);
	}

	return TRUE;
}

static int conn_select_db(Value *vret, Value *v, RefNode *node)
{
	RefMysql *my = Value_vp(*v);
	char *db = Value_cstr(v[1]);

	if (!conn_check_open(my, node)) {
		return FALSE;
	}
	if (mysql_select_db(&my->conn, db) != 0) {
		mysql_fox_error(&my->conn);
		return FALSE;
	}
	return TRUE;
}
static int conn_set_timezone(Value *vret, Value *v, RefNode *node)
{
	RefMysql *my = Value_vp(*v);
	my->tz = Value_vp(v[1]);
	return TRUE;
}

static int set_mysql_bind(MYSQL_BIND *mybind, Value v, Mem *mem)
{
	RefNode *type = fs->Value_type(v);

	if (type == fs->cls_int) {
		int err;
		int64_t i64 = fs->Value_int(v, &err);
		if (err) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Integer out of range (-2^63 - 2^63-1)");
			return FALSE;
		}
		if (i64 >= INT32_MIN && i64 <= INT32_MAX) {
			int32_t *p32 = fs->Mem_get(mem, sizeof(int32_t));
			*p32 = (int32_t)i64;

			mybind->buffer_type = MYSQL_TYPE_LONG;
			mybind->buffer = p32;
			mybind->buffer_length = sizeof(int32_t);
		} else {
			int64_t *p64 = fs->Mem_get(mem, sizeof(int32_t));
			*p64 = i64;

			mybind->buffer_type = MYSQL_TYPE_LONGLONG;
			mybind->buffer = p64;
			mybind->buffer_length = sizeof(int64_t);
		}
	} else if (type == fs->cls_float) {
		double *pd = fs->Mem_get(mem, sizeof(double));
		*pd = fs->float_Value(*pd);

		mybind->buffer_type = MYSQL_TYPE_DOUBLE;
		mybind->buffer = pd;
		mybind->buffer_length = sizeof(double);
	} else if (type == fs->cls_frac) {
		char *cbuf;
		int clen;
		char *p = fs->Value_frac_s(v, -1);
		if (p == NULL) {
			fs->throw_errorf(fs->mod_lang, "ValueError", "Frac value cannot representable (c.e. 0.3333333..)");
			return FALSE;
		}
		clen = strlen(p);
		cbuf = fs->Mem_get(mem, clen + 1);
		strcpy(cbuf, p);
		free(p);

		mybind->buffer_type = MYSQL_TYPE_STRING;
		mybind->buffer = cbuf;
		mybind->buffer_length = clen;
	} else if (type == fs->cls_str || type == fs->cls_bytes) {
		RefStr *rs = Value_vp(v);
		char *cbuf = fs->Mem_get(mem, rs->size);
		memcpy(cbuf, rs->c, rs->size);

		mybind->buffer_type = (type == fs->cls_str ? MYSQL_TYPE_STRING : MYSQL_TYPE_BLOB);
		mybind->buffer = cbuf;
		mybind->buffer_length = rs->size;
	} else if (type == fs->cls_time) {
		MYSQL_TIME *tm = fs->Mem_get(mem, sizeof(MYSQL_TIME));
		RefTime *d = Value_vp(v);
		tm->year = d->cal.year;
		tm->month = d->cal.month;
		tm->day = d->cal.day_of_month;
		tm->hour = d->cal.hour;
		tm->minute = d->cal.minute;
		tm->second = d->cal.second;

		mybind->buffer_type = MYSQL_TYPE_DATETIME;
		mybind->buffer = tm;
		mybind->buffer_length = sizeof(MYSQL_TIME);
	} else if (type == fs->cls_bool) {
		int8_t *p8 = fs->Mem_get(mem, 1);
		*p8 = (Value_bool(v) ? 1 : 0);

		mybind->buffer_type = MYSQL_TYPE_TINY;
		mybind->buffer = p8;
		mybind->buffer_length = sizeof(int8_t);
	} else if (type == fs->cls_null) {
		mybind->buffer_type = MYSQL_TYPE_NULL;
	} else {
		fs->throw_errorf(fs->mod_lang, "TypeError", "Bool, Int, Float, Str, Bytes or Null required but %n.", type);
		return FALSE;
	}
	return TRUE;
}
static MYSQL_BIND *create_bind_param(Value *argv, int argc, int n_param, Mem *mem)
{
	int i;
	MYSQL_BIND *mybind = fs->Mem_get(mem, sizeof(MYSQL_BIND) * n_param);
	memset(mybind, 0, sizeof(MYSQL_BIND) * n_param);

	for (i = 0; i < argc; i++) {
		if (!set_mysql_bind(&mybind[i], argv[i], mem)) {
			return NULL;
		}
	}
	return mybind;
}

static int init_result_bind(RefMysqlCursor *cur, MYSQL_STMT *stmt, Mem *mem)
{
	MYSQL_RES *res = mysql_stmt_result_metadata(stmt);
	MYSQL_BIND *mybind;
	int i;

	if (res == NULL) {
		return TRUE;
	}
	cur->n_field = mysql_num_fields(res);
	cur->fields = mysql_fetch_fields(res);
	mybind = fs->Mem_get(mem, sizeof(MYSQL_BIND) * cur->n_field);
	memset(mybind, 0, sizeof(MYSQL_BIND) * cur->n_field);

	for (i = 0; i < cur->n_field; i++) {
		MYSQL_BIND *b = &mybind[i];
		const MYSQL_FIELD *f = &cur->fields[i];
		int length = 0;

		b->buffer_type = f->type;
		switch (b->buffer_type) {
		case MYSQL_TYPE_TINY:
			length = sizeof(signed char);
			break;
		case MYSQL_TYPE_SHORT:
			length = sizeof(short);
			break;
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
			length = sizeof(int);
			break;
		case MYSQL_TYPE_LONGLONG:
			length = sizeof(int64_t);
			break;
		case MYSQL_TYPE_FLOAT:
			length = sizeof(float);
			break;
		case MYSQL_TYPE_DOUBLE:
			length = sizeof(double);
			break;
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
			length = RESULT_BUFFER_INIT_SIZE;
			b->length = fs->Mem_get(mem, sizeof(unsigned int));
			break;
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
			length = sizeof(MYSQL_TIME);
			break;
		default:
			break;
		}
		b->buffer = fs->Mem_get(mem, length);
		b->buffer_length = length;
		b->is_null = fs->Mem_get(mem, sizeof(my_bool));
		b->error = fs->Mem_get(mem, sizeof(my_bool));
	}
	mysql_stmt_bind_result(stmt, mybind);
	cur->result = mybind;

	return TRUE;
}
static int conn_query(Value *vret, Value *v, RefNode *node)
{
	int type = FUNC_INT(node);
	RefMysql *my = Value_vp(*v);
	Mem mem;
	int mem_init = FALSE;
	MYSQL_STMT *stmt = NULL;
	int n_param;
	RefStr *sql;
	Value *argv;
	int argc;

	if (!conn_check_open(my, node)) {
		return FALSE;
	}
	stmt = mysql_stmt_init(&my->conn);
	if (stmt == NULL) {
		mysql_fox_error(&my->conn);
		goto ERROR_END;
	}
	sql = Value_vp(v[1]);
	if (mysql_stmt_prepare(stmt, sql->c, sql->size) != 0) {
		mysql_stmt_fox_error(stmt);
		mysql_stmt_close(stmt);
		goto ERROR_END;
	}

	argv = v + 2;
	argc = (fg->stk_top - v) - 2;
	if (argc == 1 && fs->Value_type(argv[0]) == fs->cls_list) {
		RefArray *ra = Value_vp(argv[0]);
		argv = ra->p;
		argc = ra->size;
	}

	n_param = mysql_stmt_param_count(stmt);
	if (n_param != argc) {
		fs->throw_errorf(fs->mod_lang, "ArgumentError", "%d additional parameters excepted (%d given)", n_param, argc);
		return FALSE;
	}

	if (n_param > 0) {
		MYSQL_BIND *mybind;

		fs->Mem_init(&mem, 512);
		mem_init = TRUE;
		mybind = create_bind_param(argv, argc, n_param, &mem);
		if (mybind == NULL) {
			mysql_stmt_close(stmt);
			goto ERROR_END;
		}
		mysql_stmt_bind_param(stmt, mybind);
	}

	if (mysql_stmt_execute(stmt) != 0) {
		mysql_stmt_fox_error(stmt);
		goto ERROR_END;
	}

	if (type == QUERY_EXEC) {
		*vret = int32_Value(mysql_stmt_affected_rows(stmt));
		mysql_stmt_close(stmt);
		if (mem_init) {
			fs->Mem_close(&mem);
		}
	} else if (type == QUERY_SINGLE) {
		RefMysqlCursor cur;

		memset(&cur, 0, sizeof(cur));
		if (!mem_init) {
			fs->Mem_init(&mem, 512);
			mem_init = TRUE;
		}
		cur.tz = my->tz;
		if (!init_result_bind(&cur, stmt, &mem)) {
			mysql_stmt_close(stmt);
			goto ERROR_END;
		}
		if (cur.result != NULL) {
			switch (mysql_stmt_fetch(stmt)) {
			case 0:
			case MYSQL_DATA_TRUNCATED:
				*vret = mysql_to_fox_value(&cur.result[0], &cur, 0);
				break;
			default:
				break;
			}
		}
		mysql_stmt_close(stmt);
		fs->Mem_close(&mem);
	} else {
		RefMysqlCursor *cur = fs->new_buf(cls_cursor, sizeof(RefMysqlCursor));
		*vret = vp_Value(cur);

		if (!mem_init) {
			fs->Mem_init(&mem, 512);
			mem_init = TRUE;
		}
		cur->tz = my->tz;
		if (!init_result_bind(cur, stmt, &mem)) {
			mysql_stmt_close(stmt);
			goto ERROR_END;
		}
		cur->stmt = stmt;
		cur->mem = mem;

		cur->connect = *v;
		fs->Value_inc(*v);
		cur->valid = TRUE;
		cur->is_map = (type == QUERY_MAP);
	}

	return TRUE;

ERROR_END:
	if (mem_init) {
		fs->Mem_close(&mem);
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int cursor_close(Value *vret, Value *v, RefNode *node)
{
	RefMysqlCursor *cur = Value_vp(*v);

	if (cur->valid) {
		mysql_stmt_close(cur->stmt);
		fs->Mem_close(&cur->mem);
		cur->valid = FALSE;
	}
	fs->Value_dec(cur->connect);
	cur->connect = VALUE_NULL;

	return TRUE;
}

static Value mysql_to_fox_value(MYSQL_BIND *b, RefMysqlCursor *cur, int n)
{
	if (*b->is_null) {
		return VALUE_NULL;
	}

	switch (b->buffer_type) {
	case MYSQL_TYPE_TINY: {
		signed char *p = b->buffer;
		return int32_Value(*p);
	}
	case MYSQL_TYPE_SHORT: {
		short *p = b->buffer;
		return int32_Value(*p);
	}
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_LONG: {
		int *p = b->buffer;
		return int32_Value(*p);
	}
	case MYSQL_TYPE_LONGLONG: {
		int64_t *p = b->buffer;
		return fs->int64_Value(*p);
	}
	case MYSQL_TYPE_FLOAT: {
		float *p = b->buffer;
		return fs->float_Value(*p);
	}
	case MYSQL_TYPE_DOUBLE: {
		double *p = b->buffer;
		return fs->float_Value(*p);
	}
	case MYSQL_TYPE_NEWDECIMAL:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB: {
		int length = *b->length;

		if (length > b->buffer_length) {
			while (length >= b->buffer_length) {
				b->buffer_length *= 2;
			}
			b->buffer = fs->Mem_get(&cur->mem, b->buffer_length);
			mysql_stmt_fetch_column(cur->stmt, b, n, 0);
			length = *b->length;
		}
		if (b->buffer_type == MYSQL_TYPE_NEWDECIMAL) {
			char *cbuf = b->buffer;
			cbuf[length] = '\0';
			return fs->frac_s_Value(cbuf);
		} else if (cur->fields[n].charsetnr == 63) {
			return fs->cstr_Value(fs->cls_bytes, b->buffer, length);
		} else {
//			fprintf(stderr, "%s\n", get_charset_name(cur->fields[n].charsetnr));
			return fs->cstr_Value_conv(b->buffer, length, NULL);
		}
		break;
	}
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_TIMESTAMP: {
		MYSQL_TIME *tm = b->buffer;
		RefTime *rtm = fs->new_buf(fs->cls_time, sizeof(RefTime));

		rtm->cal.year = tm->year;
		rtm->cal.month = tm->month;
		rtm->cal.day_of_month = tm->day;
		rtm->cal.hour = tm->hour;
		rtm->cal.minute = tm->minute;
		rtm->cal.second = tm->second;
		rtm->cal.millisec = 0;
		rtm->tz = cur->tz;
		fs->adjust_timezone(rtm);

		return vp_Value(rtm);
	}
	case MYSQL_TYPE_TIME: {
		// 日付情報の無い時刻
		MYSQL_TIME *tm = b->buffer;
		return fs->printf_Value("%s%02d:%02d%02d", (tm->neg ? "-" : ""), tm->hour, tm->minute, tm->second);
	}
	default:
		break;
	}
	return VALUE_NULL;
}
static int cursor_columns(Value *vret, Value *v, RefNode *node)
{
	RefMysqlCursor *cur = Value_vp(*v);
	int i;

	int num = cur->n_field;
	RefArray *r = fs->refarray_new(num);
	*vret = vp_Value(r);

	if (cur->result != NULL) {
		for (i = 0; i < num; i++) {
			r->p[i] = fs->cstr_Value_conv(cur->fields[i].name, -1, NULL);
		}
	}

	return TRUE;
}
static int cursor_next(Value *vret, Value *v, RefNode *node)
{
	RefMysqlCursor *cur = Value_vp(*v);
	int num = cur->n_field;

	if (cur->result == NULL) {
		fs->throw_stopiter();
		return FALSE;
	}

	switch (mysql_stmt_fetch(cur->stmt)) {
	case 0:
	case MYSQL_DATA_TRUNCATED:
		break;
	case 1:
		mysql_stmt_fox_error(cur->stmt);
		return FALSE;
	case MYSQL_NO_DATA:
	default:
		fs->throw_stopiter();
		return FALSE;
	}

	if (cur->is_map) {
		int i;
		RefMap *rm = fs->refmap_new(num);
		*vret = vp_Value(rm);

		for (i = 0; i < num; i++) {
			Value key = fs->cstr_Value_conv(cur->fields[i].name, -1, NULL);
			HashValueEntry *ve = fs->refmap_add(rm, key, TRUE, FALSE);
			ve->val = mysql_to_fox_value(&cur->result[i], cur, i);
		}
	} else {
		int i;
		RefArray *r = fs->refarray_new(num);
		*vret = vp_Value(r);

		for (i = 0; i < num; i++) {
			r->p[i] = mysql_to_fox_value(&cur->result[i], cur, i);
		}
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

	cls_mysql = fs->define_identifier(m, m, "MySQL", NODE_CLASS, 0);
	cls_cursor = fs->define_identifier(m, m, "MySQLCursor", NODE_CLASS, 0);


	cls = cls_mysql;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, conn_new, 4, 5, NULL, fs->cls_str, fs->cls_str, fs->cls_str, fs->cls_str, fs->cls_int);

	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_close, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "select_db", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_select_db, 1, 1, NULL, fs->cls_str);
	n = fs->define_identifier(m, cls, "timezone=", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_set_timezone, 1, 1, NULL, fs->cls_timezone);
	n = fs->define_identifier(m, cls, "exec", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_EXEC, fs->cls_str);
	n = fs->define_identifier(m, cls, "query", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_ARRAY, fs->cls_str);
	n = fs->define_identifier(m, cls, "query_map", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_MAP, fs->cls_str);
	n = fs->define_identifier(m, cls, "single", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_SINGLE, fs->cls_str);

	fs->extends_method(cls, fs->cls_obj);


	cls = cls_cursor;

	n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, cursor_close, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
	fs->define_native_func_a(n, cursor_next, 0, 0, NULL);
	n = fs->define_identifier(m, cls, "columns", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, cursor_columns, 0, 0, NULL);
	fs->extends_method(cls, fs->cls_iterator);


	cls = fs->define_identifier(m, m, "MySQLError", NODE_CLASS, 0);
	cls->u.c.n_memb = 2;
	fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
	fs = a_fs;
	fg = a_fg;
	mod_mysql = m;

	define_class(m);
}
const char *module_version(const FoxStatic *a_fs)
{
	static char *buf = NULL;
	if (buf == NULL) {
		buf = malloc(256);
		sprintf(buf, "Build at\t" __DATE__ "\nMySQL\t%s\n", mysql_get_client_info());
#ifdef MARIADB_PACKAGE_VERSION
		strcat(buf, "MariaDB\t" MARIADB_PACKAGE_VERSION "\n");
#endif
	}
	return buf;
}
