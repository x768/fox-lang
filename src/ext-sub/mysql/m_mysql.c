/*
 * m_mysql.c
 *
 *  Created on: 2012/07/28
 *      Author: frog
 */

#define DEFINE_GLOBALS
#include "fox_so.h"
#include "mpi.h"
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <winsock.h>
#endif
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
	int valid;
	char *host;
	char *user;
	int port;
	TimeZone *tz;
	MYSQL conn;
} Mysql;

typedef struct {
	int valid;
	int is_map;
	Value connect;
	Mem mem;
	TimeZone *tz;
	MYSQL_STMT *stmt;

	MYSQL_BIND *bind;
	MYSQL_BIND *result;
	int n_field;
	MYSQL_FIELD *fields;
} MysqlCursor;

static Module *mod_mysql;
static Node *cls_mysql;
static Node *cls_cursor;

static void mysql_to_fox_value(Value *v, MYSQL_BIND *b, MysqlCursor *cur, int n);

static void mysql_fox_error(MYSQL *conn, Engine *e)
{
	so->throw_error(e, mod_mysql, "MySQLError", "%s", mysql_error(conn));
}
static void mysql_stmt_fox_error(MYSQL_STMT *stmt, Engine *e)
{
	so->throw_error(e, mod_mysql, "MySQLError", "%s", mysql_stmt_error(stmt));
}
static int conn_check_open(Mysql *my, Engine *e, Node *node)
{
	if (!my->valid) {
		so->throw_error(e, mod_mysql, "MySQLError", "Connection already closed");
		so->add_trace(e, NULL, node, 0);
		return FALSE;
	}
	return TRUE;
}

static int conn_new(Value *vret, Value *v, Engine *e, Node *node)
{
	Mysql *my = so->Value_new_ptr(vret, cls_mysql, sizeof(Mysql), NULL);
	char *host = so->Str_dup_p(so->Value_str(&v[1]), NULL);
	char *user = so->Str_dup_p(so->Value_str(&v[2]), NULL);
	char *pass = so->Str_dup_p(so->Value_str(&v[3]), NULL);
	char *db   = so->Str_dup_p(so->Value_str(&v[4]), NULL);

	if (e->stk_top > v + 5) {
		int64_t port = so->Value_int(&v[5], NULL);
		if (port < 1 || port > 65535) {
			so->throw_error(e, so->mod_lang, "ValueError", "Illigal port number (0 - 65535)");
			so->add_trace(e, NULL, node, 0);
			return FALSE;
		}
		my->port = port;
	}
	my->host = host;
	my->user = user;

	mysql_init(&my->conn);
	mysql_real_connect(&my->conn, host, user, pass, db, my->port, NULL, 0);
	if (mysql_errno(&my->conn) != 0) {
		mysql_fox_error(&my->conn, e);
		so->add_trace(e, NULL, node, 0);
		free(pass);
		free(db);
		return FALSE;
	}
	free(pass);
	free(db);
//	mysql_options(&my->conn, MYSQL_SET_CHARSET_NAME, "utf8");
	if (mysql_set_character_set(&my->conn, "utf8mb4") != 0) {
		mysql_set_character_set(&my->conn, "utf8");
	}
	my->valid = TRUE;

	return TRUE;
}
static int conn_close(Value *vret, Value *v, Engine *e, Node *node)
{
	Mysql *my = VALUE_BUF(*v);

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
static int conn_tostr(Value *vret, Value *v, Engine *e, Node *node)
{
	Mysql *my = VALUE_BUF(*v);
	int host_len = (my->valid ? strlen(my->host) : 0);
	int user_len = (my->valid ? strlen(my->user) : 0);
	char *cbuf = so->Value_new_str_n(vret, so->cls_str, host_len + user_len + 42);

	if (my->valid) {
		sprintf(cbuf, "MySQL(host=%s, user=%s, pass=***, port=%d)", my->host, my->user, my->port);
	} else {
		sprintf(cbuf, "MySQL(closed)");
	}
	so->Value_str_setsize(vret, strlen(cbuf));

	return TRUE;
}

static int conn_select_db(Value *vret, Value *v, Engine *e, Node *node)
{
	Mysql *my = VALUE_BUF(*v);

	if (conn_check_open(my, e, node)) {
		char *db = so->Str_dup_p(so->Value_str(&v[1]), NULL);
		if (mysql_select_db(&my->conn, db) != 0) {
			mysql_fox_error(&my->conn, e);
			so->add_trace(e, NULL, node, 0);
			free(db);
			return FALSE;
		}
		free(db);
	} else {
		return FALSE;
	}
	return TRUE;
}
static int conn_set_timezone(Value *vret, Value *v, Engine *e, Node *node)
{
	Mysql *my = VALUE_BUF(*v);
	TimeZone *tz = so->Value_to_tz(e, &v[1], 0);
	if (tz == NULL) {
		so->add_trace(e, NULL, node, 0);
		return FALSE;
	}
	my->tz = tz;

	return TRUE;
}

static int set_mysql_bind(MYSQL_BIND *mybind, Value *v, Mem *mem, Engine *e)
{
	const Node *type = so->Value_type(v);

	if (type == so->cls_int) {
		int err = FALSE;
		int64_t i64 = so->Value_int(v, &err);
		if (err) {
			so->throw_error(e, so->mod_lang, "ValueError", "Integer out of range (-2^63 - 2^63-1)");
			return FALSE;
		}
		if (i64 >= LONG_MIN && i64 <= LONG_MAX) {
			int32_t *p32 = so->Mem_get(mem, sizeof(int32_t));
			*p32 = (int32_t)i64;

			mybind->buffer_type = MYSQL_TYPE_LONG;
			mybind->buffer = p32;
			mybind->buffer_length = sizeof(int32_t);
		} else {
			int64_t *p64 = so->Mem_get(mem, sizeof(int32_t));
			*p64 = i64;

			mybind->buffer_type = MYSQL_TYPE_LONGLONG;
			mybind->buffer = p64;
			mybind->buffer_length = sizeof(int64_t);
		}
	} else if (type == so->cls_float) {
		double *pd = so->Mem_get(mem, sizeof(double));
		*pd = v->b.d;

		mybind->buffer_type = MYSQL_TYPE_DOUBLE;
		mybind->buffer = pd;
		mybind->buffer_length = sizeof(double);
	} else if (type == so->cls_frac) {
		char *cbuf;
		int clen;
		char *p = so->Value_frac(v);
		if (p == NULL) {
			so->throw_error(e, so->mod_lang, "ValueError", "Frac value cannot representable (c.e. 0.3333333..)");
			return FALSE;
		}
		clen = strlen(p);
		cbuf = so->Mem_get(mem, clen + 1);
		strcpy(cbuf, p);
		free(p);

		mybind->buffer_type = MYSQL_TYPE_STRING;
		mybind->buffer = cbuf;
		mybind->buffer_length = clen;
	} else if (type == so->cls_str || type == so->cls_bytes) {
		Str s = so->Value_str(v);
		char *cbuf = so->Mem_get(mem, s.size);
		memcpy(cbuf, s.p, s.size);

		mybind->buffer_type = (type == so->cls_str ? MYSQL_TYPE_STRING : MYSQL_TYPE_BLOB);
		mybind->buffer = cbuf;
		mybind->buffer_length = s.size;
	} else if (type == so->cls_time) {
		MYSQL_TIME *tm = so->Mem_get(mem, sizeof(MYSQL_TIME));
		DateTime *d = VALUE_BUF(*v);
		tm->year = d->cal.year;
		tm->month = d->cal.month;
		tm->day = d->cal.day_of_month;
		tm->hour = d->cal.hour;
		tm->minute = d->cal.minute;
		tm->second = d->cal.second;

		mybind->buffer_type = MYSQL_TYPE_DATETIME;
		mybind->buffer = tm;
		mybind->buffer_length = sizeof(MYSQL_TIME);
	} else if (type == so->cls_bool) {
		int8_t *p8 = so->Mem_get(mem, 1);
		*p8 = (VALUE_BOOL(*v) ? 1 : 0);

		mybind->buffer_type = MYSQL_TYPE_TINY;
		mybind->buffer = p8;
		mybind->buffer_length = sizeof(int8_t);
	} else if (type == so->cls_null) {
		mybind->buffer_type = MYSQL_TYPE_NULL;
	} else {
		const Str *t = type->name;
		so->throw_error(e, so->mod_lang, "TypeError", "Bool, Int, Float, Str, Bytes or Null required but %.*s.", t->size, t->p);
		return FALSE;
	}
	return TRUE;
}
static MYSQL_BIND *create_bind_param(Value *argv, int argc, int n_param, Mem *mem, Engine *e)
{
	int i;
	MYSQL_BIND *mybind = so->Mem_get(mem, sizeof(MYSQL_BIND) * n_param);
	memset(mybind, 0, sizeof(MYSQL_BIND) * n_param);

	for (i = 0; i < argc; i++) {
		if (!set_mysql_bind(&mybind[i], &argv[i], mem, e)) {
			return NULL;
		}
	}
	return mybind;
}

static int init_result_bind(MysqlCursor *cur, MYSQL_STMT *stmt, Mem *mem, Engine *e)
{
	MYSQL_RES *res = mysql_stmt_result_metadata(stmt);
	MYSQL_BIND *mybind;
	int i;

	if (res == NULL) {
		return TRUE;
	}
	cur->n_field = mysql_num_fields(res);
	cur->fields = mysql_fetch_fields(res);
	mybind = so->Mem_get(mem, sizeof(MYSQL_BIND) * cur->n_field);
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
			b->length = so->Mem_get(mem, sizeof(unsigned int));
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
		b->buffer = so->Mem_get(mem, length);
		b->buffer_length = length;
		b->is_null = so->Mem_get(mem, sizeof(my_bool));
		b->error = so->Mem_get(mem, sizeof(my_bool));
	}
	mysql_stmt_bind_result(stmt, mybind);
	cur->result = mybind;

	return TRUE;
}
static int conn_query(Value *vret, Value *v, Engine *e, Node *node)
{
	int type = FUNC_INT(node);
	Mysql *my = VALUE_BUF(*v);
	Mem mem;
	int mem_init = FALSE;
	MYSQL_STMT *stmt = NULL;
	int n_param;
	Str sql;
	Value *argv;
	int argc;

	if (!conn_check_open(my, e, node)) {
		return FALSE;
	}
	stmt = mysql_stmt_init(&my->conn);
	if (stmt == NULL) {
		mysql_fox_error(&my->conn, e);
		goto ERROR_END;
	}
	sql = so->Value_str(&v[1]);
	if (mysql_stmt_prepare(stmt, sql.p, sql.size) != 0) {
		mysql_stmt_fox_error(stmt, e);
		mysql_stmt_close(stmt);
		goto ERROR_END;
	}

	argv = v + 2;
	argc = (e->stk_top - v) - 2;
	if (argc == 1 && so->Value_type(&argv[0]) == so->cls_array) {
		RefArray *ra = VALUE_BUF(argv[0]);
		argv = ra->p;
		argc = ra->size;
	}

	n_param = mysql_stmt_param_count(stmt);
	if (n_param != argc) {
		so->throw_error(e, so->mod_lang, "ArgumentError", "%d additional parameters excepted (%d given)", n_param, argc);
		so->add_trace(e, NULL, node, 0);
		return FALSE;
	}

	if (n_param > 0) {
		MYSQL_BIND *mybind;

		so->Mem_init(&mem, 512);
		mem_init = TRUE;
		mybind = create_bind_param(argv, argc, n_param, &mem, e);
		if (mybind == NULL) {
			mysql_stmt_close(stmt);
			goto ERROR_END;
		}
		mysql_stmt_bind_param(stmt, mybind);
	}

	if (mysql_stmt_execute(stmt) != 0) {
		mysql_stmt_fox_error(stmt, e);
		goto ERROR_END;
	}

	if (type == QUERY_EXEC) {
		so->Value_setint(vret, so->cls_int, mysql_stmt_affected_rows(stmt));
		mysql_stmt_close(stmt);
		if (mem_init) {
			so->Mem_close(&mem);
		}
	} else if (type == QUERY_SINGLE) {
		MysqlCursor cur;

		memset(&cur, 0, sizeof(cur));
		if (!mem_init) {
			so->Mem_init(&mem, 512);
			mem_init = TRUE;
		}
		cur.tz = my->tz;
		if (!init_result_bind(&cur, stmt, &mem, e)) {
			mysql_stmt_close(stmt);
			goto ERROR_END;
		}
		if (cur.result != NULL) {
			switch (mysql_stmt_fetch(stmt)) {
			case 0:
			case MYSQL_DATA_TRUNCATED:
				mysql_to_fox_value(vret, &cur.result[0], &cur, 0);
				break;
			default:
				break;
			}
		}
		mysql_stmt_close(stmt);
		so->Mem_close(&mem);
	} else {
		MysqlCursor *cur = so->Value_new_ptr(vret, cls_cursor, sizeof(MysqlCursor), NULL);

		if (!mem_init) {
			so->Mem_init(&mem, 512);
			mem_init = TRUE;
		}
		cur->tz = my->tz;
		if (!init_result_bind(cur, stmt, &mem, e)) {
			mysql_stmt_close(stmt);
			goto ERROR_END;
		}
		cur->stmt = stmt;
		cur->mem = mem;

		cur->connect = *v;
		VALUE_INC(*v);
		cur->valid = TRUE;
		cur->is_map = (type == QUERY_MAP);
	}

	return TRUE;

ERROR_END:
	if (mem_init) {
		so->Mem_close(&mem);
	}
	so->add_trace(e, NULL, node, 0);
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int cursor_close(Value *vret, Value *v, Engine *e, Node *node)
{
	MysqlCursor *cur = VALUE_BUF(*v);

	if (cur->valid) {
		mysql_stmt_close(cur->stmt);
		so->Mem_close(&cur->mem);
		cur->valid = FALSE;
	}
	so->Value_dec(&cur->connect, e);
	cur->connect = so->Value_NULL;

	return TRUE;
}

static void mysql_to_fox_value(Value *v, MYSQL_BIND *b, MysqlCursor *cur, int n)
{
	if (*b->is_null) {
		*v = so->Value_NULL;
		return;
	}

	switch (b->buffer_type) {
	case MYSQL_TYPE_TINY: {
		signed char *p = b->buffer;
		so->Value_setint(v, so->cls_int, *p);
		break;
	}
	case MYSQL_TYPE_SHORT: {
		short *p = b->buffer;
		so->Value_setint(v, so->cls_int, *p);
		break;
	}
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_LONG: {
		int *p = b->buffer;
		so->Value_setint(v, so->cls_int, *p);
		break;
	}
	case MYSQL_TYPE_LONGLONG: {
		int64_t *p = b->buffer;
		so->Value_setint(v, so->cls_int, *p);
		break;
	}
	case MYSQL_TYPE_FLOAT: {
		float *p = b->buffer;
		so->Value_setfloat(v, (double)*p);
		break;
	}
	case MYSQL_TYPE_DOUBLE: {
		double *p = b->buffer;
		double d = *p;
		so->Value_setfloat(v, d);
		break;
	}
	case MYSQL_TYPE_NEWDECIMAL:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB: {
		Str s;
		int length = *b->length;

		if (length > b->buffer_length) {
			while (length >= b->buffer_length) {
				b->buffer_length *= 2;
			}
			b->buffer = so->Mem_get(&cur->mem, b->buffer_length);
			mysql_stmt_fetch_column(cur->stmt, b, n, 0);
			length = *b->length;
			fprintf(stderr, "%p\n", b->buffer);
		}
		s = so->Str_new(b->buffer, length);
		if (b->buffer_type == MYSQL_TYPE_NEWDECIMAL) {
			char *cbuf = b->buffer;
			cbuf[length] = '\0';
			so->Value_setfrac(v, cbuf);
		} else if (cur->fields[n].charsetnr == 63) {
			so->Value_new_str(v, so->cls_bytes, s);
		} else {
//			fprintf(stderr, "%s\n", get_charset_name(cur->fields[n].charsetnr));
			so->Value_setstr_safe(v, s, FALSE);
		}
		break;
	}
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_TIMESTAMP: {
		MYSQL_TIME *tm = b->buffer;
		DateTime d;

		d.cal.year = tm->year;
		d.cal.month = tm->month;
		d.cal.day_of_month = tm->day;
		d.cal.hour = tm->hour;
		d.cal.minute = tm->minute;
		d.cal.second = tm->second;
		d.cal.millisec = 0;
		d.tz = cur->tz;
		so->adjust_timezone(&d);

		so->Value_new_ptr(v, so->cls_time, sizeof(d), &d);
		break;
	}
	case MYSQL_TYPE_TIME: {
		MYSQL_TIME *tm = b->buffer;
		char cbuf[32];
		sprintf(cbuf, "%s%02d:%02d%02d", (tm->neg ? "-" : ""), tm->hour, tm->minute, tm->second);
		so->Value_new_str(v, so->cls_str, so->Str_new_p(cbuf));
		break;
	}
	default:
		*v = so->Value_NULL;
		break;
	}
}
static int cursor_columns(Value *vret, Value *v, Engine *e, Node *node)
{
	MysqlCursor *cur = VALUE_BUF(*v);
	int i;

	int num = cur->n_field;
	RefArray *r = so->array_init_sized(vret, num);

	if (cur->result != NULL) {
		for (i = 0; i < num; i++) {
			const char *col_p = cur->fields[i].name;
			so->Value_setstr_safe(&r->p[i], so->Str_new_p(col_p), FALSE);
		}
	}

	return TRUE;
}
static int cursor_next(Value *vret, Value *v, Engine *e, Node *node)
{
	MysqlCursor *cur = VALUE_BUF(*v);
	int num = cur->n_field;

	if (cur->result == NULL) {
		so->throw_stopiter(e);
		so->add_trace(e, NULL, node, 0);
		return FALSE;
	}

	switch (mysql_stmt_fetch(cur->stmt)) {
	case 0:
	case MYSQL_DATA_TRUNCATED:
		break;
	case 1:
		mysql_stmt_fox_error(cur->stmt, e);
		so->add_trace(e, NULL, node, 0);
		return FALSE;
	case MYSQL_NO_DATA:
	default:
		so->throw_stopiter(e);
		so->add_trace(e, NULL, node, 0);
		return FALSE;
	}

	if (cur->is_map) {
		int i;
		so->map_init_sized(vret, num);

		for (i = 0; i < num; i++) {
			Value key, val;
			const char *key_p = cur->fields[i].name;

			so->Value_setstr_safe(&key, so->Str_new_p(key_p), FALSE);
			mysql_to_fox_value(&val, &cur->result[i], cur, i);

			so->map_add_elem(e, vret, &key, &val, TRUE, FALSE);
		}
	} else {
		int i;
		RefArray *r = so->array_init_sized(vret, num);

		for (i = 0; i < num; i++) {
			mysql_to_fox_value(&r->p[i], &cur->result[i], cur, i);
		}
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(Module *m)
{
	Node *root = &m->root;
	Node *cls;
	Node *n;

	cls_mysql = so->define_identifier(m, root, "MySQL", NODE_CLASS, 0);
	cls_cursor = so->define_identifier(m, root, "MySQLCursor", NODE_CLASS, 0);


	cls = cls_mysql;
	n = so->define_identifier_p(m, cls, so->str_new, NODE_NEW_N, 0);
	so->define_native_func_a(n, conn_new, 4, 5, NULL, so->cls_str, so->cls_str, so->cls_str, so->cls_str, so->cls_int);

	n = so->define_identifier_p(m, cls, so->str_tostr, NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_tostr, 0, 2, NULL, so->cls_str, so->cls_locale);
	n = so->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_close, 0, 0, NULL);
	n = so->define_identifier(m, cls, "select_db", NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_select_db, 1, 1, NULL, so->cls_str);
	n = so->define_identifier(m, cls, "timezone=", NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_set_timezone, 1, 1, NULL, NULL);
	n = so->define_identifier(m, cls, "exec", NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_EXEC, so->cls_str);
	n = so->define_identifier(m, cls, "query", NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_ARRAY, so->cls_str);
	n = so->define_identifier(m, cls, "query_map", NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_MAP, so->cls_str);
	n = so->define_identifier(m, cls, "single", NODE_FUNC_N, 0);
	so->define_native_func_a(n, conn_query, 1, -1, (void*) QUERY_SINGLE, so->cls_str);
	so->extends_method(cls, so->cls_obj);


	cls = cls_cursor;

	n = so->define_identifier_p(m, cls, so->str_dispose, NODE_FUNC_N, 0);
	so->define_native_func_a(n, cursor_close, 0, 0, NULL);
	n = so->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
	so->define_native_func_a(n, cursor_next, 0, 0, NULL);
	n = so->define_identifier(m, cls, "columns", NODE_FUNC_N, NODEOPT_PROPERTY);
	so->define_native_func_a(n, cursor_columns, 0, 0, NULL);
	so->extends_method(cls, so->cls_iterator);


	cls = so->define_identifier(m, root, "MySQLError", NODE_CLASS, 0);
	cls->u.c.n_memb = 2;
	so->extends_method(cls, so->cls_error);
}

void define_module(Module *m, StockObject *s)
{
	so = s;
	define_class(m);
	mod_mysql = m;
}

const char *module_version(StockObject *s)
{
	static char *buf = NULL;
	if (buf == NULL) {
		buf = malloc(256);
		sprintf(buf, "Build at\t" __DATE__ "\nMySQL\t%s\n", mysql_get_client_info());
	}
	return buf;
}
