#include "fox.h"
#include "m_number.h"
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>


enum {
    INDEX_SQLITE_CONN,
    INDEX_SQLITE_FUNC,
    INDEX_SQLITE_NUM,
};
enum {
    SQLITE_ERROR_USER = 201,
};

typedef struct RefCursor {
    RefHeader rh;

    Value connect;
    sqlite3_stmt *stmt;
    int is_map;
} RefCursor;

static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_sqlite;
static RefNode *cls_sqlite;
static RefNode *cls_cursor;

//////////////////////////////////////////////////////////////////////////////////////////

static int sqlite_bind(sqlite3_stmt *stmt, int num, Value v)
{
    const RefNode *type = fs->Value_type(v);
    int ret = SQLITE_OK;

    if (type == fs->cls_int) {
        int err = FALSE;
        int64_t i64 = fs->Value_int64(v, &err);
        if (err) {
            fs->throw_errorf(mod_sqlite, "SQLiteError", "'INTEGER' out of range (-2^63 - 2^63-1)");
            return FALSE;
        }
        ret = sqlite3_bind_int64(stmt, num, i64);
    } else if (type == fs->cls_float) {
        double dval = Value_float2(v);
        ret = sqlite3_bind_double(stmt, num, dval);
    } else if (type == fs->cls_str) {
        RefStr *rs = Value_vp(v);
        ret = sqlite3_bind_text(stmt, num, rs->c, rs->size, SQLITE_TRANSIENT);
    } else if (type == fs->cls_bytes) {
        RefStr *rs = Value_vp(v);
        ret = sqlite3_bind_blob(stmt, num, rs->c, rs->size, SQLITE_TRANSIENT);
    } else if (type == fs->cls_null) {
        ret = sqlite3_bind_null(stmt, num);
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Int, Float, Str, Bytes or Null required but %n", type);
        return FALSE;
    }
    if (ret == SQLITE_RANGE) {
        fs->throw_errorf(mod_sqlite, "SQLiteError", "Index out of range (%d)", num);
        return FALSE;
    }
    return TRUE;
}

static Value cursor_get_sub(sqlite3_stmt *stmt, int i)
{
    switch (sqlite3_column_type(stmt, i)) {
    case SQLITE_INTEGER:
        return fs->int64_Value(sqlite3_column_int64(stmt, i));
    case SQLITE_FLOAT:
        return fs->float_Value(fs->cls_float, sqlite3_column_double(stmt, i));
    case SQLITE_TEXT: {
        const char *p = (const char*)sqlite3_column_text(stmt, i);
        int len = sqlite3_column_bytes(stmt, i);
        return fs->cstr_Value_conv(p, len, NULL);
    }
    case SQLITE_BLOB: {
        const char *p = (const char*)sqlite3_column_blob(stmt, i);
        int len = sqlite3_column_bytes(stmt, i);
        return fs->cstr_Value(fs->cls_bytes, p, len);
    }
    default:
        break;
    }
    return VALUE_NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int conn_new(Value *vret, Value *v, RefNode *node)
{
    int flag = SQLITE_OPEN_READONLY;
    sqlite3 *conn = NULL;
    int result = SQLITE_OK;
    Ref *r = fs->ref_new(cls_sqlite);

    Str path_s;
    char *path = fs->file_value_to_path(&path_s, v[1], 0);

    *vret = vp_Value(r);

    if (path == NULL) {
        return FALSE;
    }

    if (fg->stk_top > v + 2) {
        RefStr *m = Value_vp(v[2]);
        if (str_eq(m->c, m->size, "r", -1)) {
        } else if (str_eq(m->c, m->size, "w", -1)) {
            flag = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        } else {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown open mode %q", m->c);
            free(path);
            return FALSE;
        }
    }

    result = sqlite3_open_v2(path, &conn, flag, NULL);
    if (result != SQLITE_OK || conn == NULL) {
        fs->throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
        free(path);
        return FALSE;
    }
    r->v[INDEX_SQLITE_CONN] = ptr_Value(conn);
    r->v[INDEX_SQLITE_FUNC] = vp_Value(fs->refarray_new(0));
    free(path);

    return TRUE;
}
static int conn_memory(Value *vret, Value *v, RefNode *node)
{
    sqlite3 *conn = NULL;
    Ref *r = fs->ref_new(cls_sqlite);
    int result = sqlite3_open(":memory:", &conn);

    *vret = vp_Value(r);

    if (result != SQLITE_OK || conn == NULL) {
        fs->throw_errorf(mod_sqlite, "SQLiteError", "Cannot connect sqlite (memory)");
        return FALSE;
    }
    r->v[INDEX_SQLITE_CONN] = ptr_Value(conn);
    r->v[INDEX_SQLITE_FUNC] = vp_Value(fs->refarray_new(0));

    return TRUE;
}
static int conn_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    sqlite3 *conn = Value_ptr(r->v[INDEX_SQLITE_CONN]);

    if (conn != NULL) {
        sqlite3_close(conn);
        r->v[INDEX_SQLITE_CONN] = VALUE_NULL;
    }

    return TRUE;
}

static int conn_prepare_sub(sqlite3_stmt **stmt, sqlite3 *conn, Value *v)
{
    Value *v1 = v + 1;
    int argc = fg->stk_top - v1;
    RefStr *sql;
    int result;

    if (conn == NULL) {
        fs->throw_errorf(mod_sqlite, "SQLiteError", "Connection is not opened");
        return FALSE;
    }

    sql = Value_vp(*v1);
    result = sqlite3_prepare_v2(conn, sql->c, -1, stmt, NULL);
    if (result != SQLITE_OK){
        if (result != SQLITE_ERROR_USER) {
            fs->throw_errorf(mod_sqlite, "SQLiteError", "%s", sqlite3_errmsg(conn));
        }
        return FALSE;
    }
    if (*stmt == NULL) {
        fs->throw_errorf(mod_sqlite, "SQLiteError", "SQL string is empty");
        return FALSE;
    }

    if (argc > 1) {
        int i;
        if (argc == 2) {
            const RefNode *type = fs->Value_type(v[2]);
            if (type == fs->cls_list) {
                RefArray *ra = Value_vp(v[2]);
                for (i = 0; i < ra->size; i++) {
                    if (!sqlite_bind(*stmt, i + 1, ra->p[i])) {
                        return FALSE;
                    }
                }
                return TRUE;
            } else if (type == fs->cls_map) {
                RefMap *rm = Value_vp(v[2]);
                for (i = 0; i < rm->entry_num; i++) {
                    HashValueEntry *ep = rm->entry[i];
                    for (; ep != NULL; ep = ep->next) {
                        RefStr *rs;
                        int idx;
                        if (fs->Value_type(ep->key) != fs->cls_str) {
                            fs->throw_errorf(fs->mod_lang, "TypeError", "Map {Str:*, Str:* ...} required");
                            return FALSE;
                        }
                        rs = Value_vp(ep->key);
                        if (rs->size > 0 && !str_has0(rs->c, rs->size)) {
                            // 先頭が記号で始まらない場合は、:を補う
                            char ch = rs->c[0];
                            if (isdigit_fox(ch) || isupper_fox(ch) || islower_fox(ch)) {
                                char *ptr = malloc(rs->size + 2);
                                sprintf(ptr, ":%.*s", rs->size, rs->c);
                                idx = sqlite3_bind_parameter_index(*stmt, ptr);
                                free(ptr);
                            } else {
                                idx = sqlite3_bind_parameter_index(*stmt, rs->c);
                            }
                        } else {
                            idx = 0;
                        }
                        if (idx == 0) {
                            fs->throw_errorf(mod_sqlite, "SQLiteError", "No parameters names %r", rs);
                            return FALSE;
                        }
                        if (!sqlite_bind(*stmt, idx, ep->val)) {
                            return FALSE;
                        }
                    }
                }
                return TRUE;
            }
        }
        for (i = 1; i < argc; i++) {
            if (!sqlite_bind(*stmt, i, v1[i])) {
                return FALSE;
            }
        }
    }

    return TRUE;
}
static int conn_exec(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    sqlite3 *conn = Value_ptr(r->v[INDEX_SQLITE_CONN]);
    int result;
    int count;

    sqlite3_stmt *stmt = NULL;
    if (!conn_prepare_sub(&stmt, conn, v)) {
        return FALSE;
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE && result != SQLITE_ROW) {
        // ???
        if (result != SQLITE_ERROR_USER) {
            fs->throw_errorf(mod_sqlite, "SQLiteError", "%s", sqlite3_errmsg(conn));
            return FALSE;
        }
        //return FALSE;
    }
    count = sqlite3_changes(conn);
    sqlite3_finalize(stmt);

    *vret = int32_Value(count);

    return TRUE;
}

static int conn_query(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    sqlite3 *conn = Value_ptr(r->v[INDEX_SQLITE_CONN]);

    sqlite3_stmt *stmt = NULL;
    if (!conn_prepare_sub(&stmt, conn, v)) {
        return FALSE;
    }

    {
        RefCursor *rc = fs->buf_new(cls_cursor, sizeof(RefCursor));
        *vret = vp_Value(rc);
        rc->connect = fs->Value_cp(*v);
        rc->stmt = stmt;
        rc->is_map = FUNC_INT(node);
    }

    return TRUE;
}
/*
 * 最初の行の最初の列だけ取得
 */
static int conn_single(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    sqlite3 *conn = Value_ptr(r->v[INDEX_SQLITE_CONN]);

    sqlite3_stmt *stmt = NULL;
    if (!conn_prepare_sub(&stmt, conn, v)) {
        return FALSE;
    }

    if (stmt != NULL) {
        int result = sqlite3_step(stmt);
        if (result == SQLITE_ROW) {
            *vret = cursor_get_sub(stmt, 0);
            sqlite3_finalize(stmt);
        } else if (result == SQLITE_ERROR_USER) {
            fs->throw_errorf(mod_sqlite, "SQLiteError", "%s", sqlite3_errmsg(conn));
            return FALSE;
        } else {
            sqlite3_finalize(stmt);
        }
    }

    return TRUE;
}

static void sqlite_callback_args(int argc, sqlite3_value **argv)
{
    int i;

    for (i = 0; i < argc; i++) {
        sqlite3_value *sv = argv[i];
        Value *vp = fg->stk_top;

        switch (sqlite3_value_type(sv)) {
        case SQLITE_INTEGER:
            *vp = fs->int64_Value(sqlite3_value_int64(sv));
            break;
        case SQLITE_FLOAT:
            *vp = fs->float_Value(fs->cls_float, sqlite3_value_double(sv));
            break;
        case SQLITE_TEXT: {
            const char *p = (const char*)sqlite3_value_text(sv);
            int len = sqlite3_value_bytes(sv);
            *vp = fs->cstr_Value_conv(p, len, NULL);
            break;
        }
        case SQLITE_BLOB: {
            const char *p = (const char*)sqlite3_value_blob(sv);
            int len = sqlite3_value_bytes(sv);
            *vp = fs->cstr_Value(fs->cls_bytes, p, len);
            break;
        }
        default:
            *vp = VALUE_NULL;
            break;
        }
        fg->stk_top++;
    }
}
static int sqlite_callback_return(Value v, sqlite3_context *ctx)
{
    const RefNode *r_type = fs->Value_type(v);
    if (r_type == fs->cls_int) {
        int err = FALSE;
        int64_t i64 = fs->Value_int64(v, &err);
        if (err) {
            fs->throw_errorf(mod_sqlite, "SQLiteError", "'INTEGER' out of range (-2^63 - 2^63-1)");
            return FALSE;
        }
        sqlite3_result_int64(ctx, i64);
    } else if (r_type == fs->cls_bool) {
        int i32 = Value_bool(v);
        sqlite3_result_int(ctx, i32);
    } else if (r_type == fs->cls_float) {
        double dval = Value_float2(v);
        sqlite3_result_double(ctx, dval);
    } else if (r_type == fs->cls_str) {
        RefStr *s = Value_vp(v);
        sqlite3_result_text(ctx, s->c, s->size, SQLITE_TRANSIENT);
    } else if (r_type == fs->cls_bytes) {
        RefStr *s = Value_vp(v);
        sqlite3_result_blob(ctx, s->c, s->size, SQLITE_TRANSIENT);
    } else if (r_type == fs->cls_null) {
        sqlite3_result_null(ctx);
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Bool, Int, Float, Str, Bytes or Null required but %n", r_type);
        return FALSE;
    }
    return TRUE;
}

static void sqlite_callback_func(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
    Value *fn = (Value*)sqlite3_user_data(ctx);

    fs->Value_push("v", *fn);
    // 引数の設定
    sqlite_callback_args(argc, argv);

    if (!fs->call_function_obj(argc)) {
        sqlite3_result_error_code(ctx, SQLITE_ERROR_USER);
        return;
    }

    // 戻り値の設定
    if (!sqlite_callback_return(fg->stk_top[-1], ctx)) {
        sqlite3_result_error_code(ctx, SQLITE_ERROR_USER);
        return;
    }
    fs->Value_pop();

    return;
}
static int sqlite_aggrigate_new(Value *v, RefNode *klass)
{
    fs->Value_push("p", fs->cls_class, klass);
    if (!fs->call_member_func(fs->str_new, 0, TRUE)) {
        return FALSE;
    }
    *v = *fg->stk_top;
    fg->stk_top--;

    return TRUE;
}
static void sqlite_callback_step(sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
    RefNode *klass = (RefNode*)sqlite3_user_data(ctx);
    Value *v = sqlite3_aggregate_context(ctx, sizeof(Value));

    if (*v == VALUE_NULL) {
        if (!sqlite_aggrigate_new(v, klass)) {
            goto ERROR_END;
        }
    }

    fs->Value_push("v", v);
    sqlite_callback_args(argc, argv);

    if (!fs->call_member_func(fs->intern("step", -1), argc, TRUE)) {
        goto ERROR_END;
    }
    fs->Value_pop();
    return;
ERROR_END:
    sqlite3_result_error_code(ctx, SQLITE_ERROR_USER);
    return;
}
static void sqlite_callback_final(sqlite3_context *ctx)
{
    RefNode *klass = (RefNode*)sqlite3_user_data(ctx);
    Value *v = sqlite3_aggregate_context(ctx, sizeof(Value));

    if (*v == VALUE_NULL) {
        if (!sqlite_aggrigate_new(v, klass)) {
            goto ERROR_END;
        }
    }

    fs->Value_push("v", v);
    if (!fs->call_member_func(fs->intern("final", -1), 0, TRUE)) {
        goto ERROR_END;
    }
    // 戻り値の設定
    if (!sqlite_callback_return(fg->stk_top[-1], ctx)) {
        goto ERROR_END;
    }
    fs->Value_pop();

    fs->Value_dec(*v);
    *v = VALUE_NULL;

    return;

ERROR_END:
    if (fg->error == VALUE_NULL) {
        sqlite3_result_error_code(ctx, SQLITE_ERROR_USER);
    }
    return;
}

static Value *conn_create_function_reg(Ref *r, Value v)
{
    RefArray *ra = Value_vp(r->v[INDEX_SQLITE_FUNC]);
    Value *vp = fs->refarray_push(ra);
    *vp = fs->Value_cp(v);
    return vp;
}

/**
 * Function f : スカラ関数
 * Class c : 集約関数
 * c.new(), c#step(..), c#result() を実装する
 */
static int conn_create_function(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    sqlite3 *conn = Value_ptr(r->v[INDEX_SQLITE_CONN]);
    RefStr *name = Value_vp(v[1]);
    Value v2 = v[2];
    const RefNode *v2_type = fs->Value_type(v2);

    if (v2_type == fs->cls_fn) {
        Value *vf = conn_create_function_reg(r, v2);
        sqlite3_create_function_v2(conn, name->c, -1, SQLITE_UTF8, vf, sqlite_callback_func, NULL, NULL, NULL);
    } else if (v2_type == fs->cls_class) {
        RefNode *klass = Value_vp(v2);
        sqlite3_create_function_v2(conn, name->c, -1, SQLITE_UTF8, klass, NULL, sqlite_callback_step, sqlite_callback_final, NULL);
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_fn, fs->cls_class, v2_type, 2);
        return FALSE;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int cursor_close(Value *vret, Value *v, RefNode *node)
{
    RefCursor *rc = Value_vp(*v);

    if (rc->stmt != NULL) {
        sqlite3_finalize(rc->stmt);
        rc->stmt = NULL;
    }
    fs->Value_dec(rc->connect);
    rc->connect = VALUE_NULL;

    return TRUE;
}

static int cursor_bind(Value *vret, Value *v, RefNode *node)
{
    RefCursor *rc = Value_vp(*v);

    Value v1 = v[1];
    Value v2 = v[2];

    int idx;
    const RefNode *type = fs->Value_type(v1);
    if (type == fs->cls_int) {
        idx = fs->Value_int64(v1, NULL);
    } else if (type == fs->cls_str) {
        RefStr *rs = Value_vp(v1);
        if (!str_has0(rs->c, rs->size)) {
            idx = sqlite3_bind_parameter_index(rc->stmt, rs->c);
        } else {
            idx = 0;
        }
        if (idx == 0) {
            fs->throw_errorf(mod_sqlite, "SQLiteError", "No parameters names %r", rs);
            return FALSE;
        }
    } else {
        fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_int, fs->cls_str, type, 1);
        return FALSE;
    }
    if (!sqlite_bind(rc->stmt, idx, v2)) {
        return FALSE;
    }

    return TRUE;
}

static int cursor_get(Value *vret, RefCursor *rc)
{
    int i;
    int num = sqlite3_column_count(rc->stmt);

    if (rc->is_map) {
        RefMap *rm = fs->refmap_new(num);
        *vret = vp_Value(rm);

        for (i = 0; i < num; i++) {
            const char *key_p = sqlite3_column_name(rc->stmt, i);
            Value key = fs->cstr_Value_conv(key_p, -1, NULL);
            HashValueEntry *ve = fs->refmap_add(rm, key, TRUE, FALSE);

            if (ve != NULL) {
                ve->val = cursor_get_sub(rc->stmt, i);
            }
            fs->Value_dec(key);
        }
    } else {
        RefArray *ra = fs->refarray_new(num);
        *vret = vp_Value(ra);
        for (i = 0; i < num; i++) {
            ra->p[i] = cursor_get_sub(rc->stmt, i);
        }
    }

    return TRUE;
}
static int cursor_next(Value *vret, Value *v, RefNode *node)
{
    RefCursor *rc = Value_vp(*v);
    int result = sqlite3_step(rc->stmt);

    if (result == SQLITE_DONE) {
        fs->throw_stopiter();
        return FALSE;
    } else if (result == SQLITE_ROW) {
        if (!cursor_get(vret, rc)) {
            return FALSE;
        }
    } else {
        fs->throw_errorf(mod_sqlite, "SQLiteError", "sqlite3_step error (%d)", result);
        return FALSE;
    }

    return TRUE;
}
static int cursor_columns(Value *vret, Value *v, RefNode *node)
{
    int i;
    RefCursor *rc = Value_vp(*v);
    int num = sqlite3_column_count(rc->stmt);
    RefArray *r = fs->refarray_new(num);
    *vret = vp_Value(r);

    for (i = 0; i < num; i++) {
        const char *col_p = sqlite3_column_name(rc->stmt, i);
        r->p[i] = fs->cstr_Value(fs->cls_str, col_p, -1);
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void define_const(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "SQLITE_INT_MIN", NODE_CONST, 0);
    n->u.k.val = fs->int64_Value(INT64_MIN);

    n = fs->define_identifier(m, m, "SQLITE_INT_MAX", NODE_CONST, 0);
    n->u.k.val = fs->int64_Value(INT64_MAX);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;
    cls_sqlite = fs->define_identifier(m, m, "SQLite", NODE_CLASS, 0);
    cls_cursor = fs->define_identifier(m, m, "SQLiteCursor", NODE_CLASS, 0);

    cls = cls_sqlite;
    cls->u.c.n_memb = INDEX_SQLITE_NUM;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, conn_new, 1, 2, NULL, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "memory", NODE_NEW_N, 0);
    fs->define_native_func_a(n, conn_memory, 0, 0, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conn_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conn_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "exec", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conn_exec, 1, -1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "query", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conn_query, 1, -1, (void*) FALSE, fs->cls_str);
    n = fs->define_identifier(m, cls, "query_map", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conn_query, 1, -1, (void*) TRUE, fs->cls_str);
    n = fs->define_identifier(m, cls, "single", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conn_single, 1, -1, NULL, fs->cls_str);
    n = fs->define_identifier(m, cls, "create_function", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, conn_create_function, 2, 2, NULL, fs->cls_str, NULL);

    fs->extends_method(cls, fs->cls_obj);


    cls = cls_cursor;
    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cursor_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "bind", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cursor_bind, 2, 2, NULL, NULL, NULL);
    n = fs->define_identifier(m, cls, "next", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cursor_next, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "columns", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cursor_columns, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_iterator);



    cls = fs->define_identifier(m, m, "SQLiteError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_sqlite = m;

    define_const(m);
    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\nSQLite\t" SQLITE_VERSION "\n";
}
