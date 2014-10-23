#define DEFINE_GLOBALS
#include "m_process.h"
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#endif


static RefNode *cls_process;
static RefNode *cls_pipeio;


static char **value_to_argv(char *path, Value v)
{
    RefArray *ra = Value_vp(v);
    char **argv = NULL;
    int i;

    argv = malloc(sizeof(char*) * (ra->size + 1));
    for (i = 0; i < ra->size; i++) {
        RefNode *type = fs->Value_type(ra->p[i]);
        if (type == fs->cls_str) {
            RefStr *rs = Value_vp(ra->p[i]);
            argv[i] = rs->c;
        } else {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Str required but %n", type);
            free(argv);
            return NULL;
        }
    }
    argv[i] = NULL;

    return argv;
}

static int process_new(Value *vret, Value *v, RefNode *node)
{
    int create_pipe = FUNC_INT(node);
    Str path_s;
    char *path = fs->file_value_to_path(&path_s, v[1], 0);
    char **argv = NULL;
    int flags;
    Ref *r;
    RefProcessHandle *ph;

    if (path == NULL) {
        goto ERROR_END;
    }
    if (fg->stk_top > v + 3) {
        Str mode_s = fs->Value_str(v[3]);
        if (Str_eq_p(mode_s, "r")) {
            flags = STREAM_READ;
        } else if (Str_eq_p(mode_s, "w")) {
            flags = STREAM_WRITE;
        } else if (Str_eq_p(mode_s, "rw")) {
            flags = STREAM_READ|STREAM_WRITE;
        } else {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Unknown mode %Q", mode_s);
            goto ERROR_END;
        }
    } else {
        flags = STREAM_READ;
    }

    r = fs->ref_new(create_pipe ? cls_pipeio : cls_process);
    *vret = vp_Value(r);
    if (create_pipe) {
        fs->init_stream_ref(r, flags);
    }

    argv = value_to_argv(path, v[2]);
    ph = fs->buf_new(NULL, sizeof(RefProcessHandle));
    r->v[INDEX_P_HANDLE] = vp_Value(ph);

    if (!process_new_sub(ph, create_pipe, path, argv, flags)) {
        fs->throw_errorf(mod_process, "ProcessError", "Cannot create process %q", path);
        goto ERROR_END;
    }
    free(path);
    free(argv);

    return TRUE;

ERROR_END:
    free(path);
    free(argv);
    return FALSE;
}

static int process_wait(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefProcessHandle *ph = Value_vp(r->v[INDEX_P_HANDLE]);
    return process_wait_sub(ph);
}
static int process_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefProcessHandle *ph = Value_vp(r->v[INDEX_P_HANDLE]);
    process_close_sub(ph);
    return TRUE;
}
static int process_status(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefProcessHandle *ph = Value_vp(r->v[INDEX_P_HANDLE]);
    int return_exit_code = FUNC_INT(node);

    if (ph->valid) {
        if (is_process_terminated(ph)) {
            ph->valid = FALSE;
        }
    }
    if (return_exit_code) {
        *vret = int32_Value(ph->exit_status);
    } else {
        *vret = Value_bool(ph->valid);
    }
    return TRUE;
}
static int pipeio_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefProcessHandle *ph = Value_vp(r->v[INDEX_P_HANDLE]);
    RefBytesIO *mb = Value_vp(v[1]);
    int64_t max_size = fs->Value_int64(v[2], NULL);
    char *dst_buf;

    if (!ph->valid || ph->fd_in == -1) {
        return TRUE;
    }
    for (;;) {
        int read_size;

        dst_buf = mb->buf.p;
        read_size = read_pipe(ph, dst_buf, max_size);
        if (read_size < 0) {
            // 終端
            mb->buf.size = 0;
            ph->valid = VALUE_FALSE;

            if (ph->fd_in != -1) {
                close_fox(ph->fd_in);
                if (ph->fd_in == ph->fd_out) {
                    ph->fd_out = -1;
                }
                ph->fd_in = -1;
            }
            return TRUE;
        }
        if (read_size > 0) {
            mb->buf.size = read_size;
            break;
        }
    }
    return TRUE;
}
static int pipeio_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefProcessHandle *ph = Value_vp(r->v[INDEX_P_HANDLE]);
    RefBytesIO *mb = Value_vp(v[1]);

    if (!ph->valid || ph->fd_out) {
        return TRUE;
    }
    write_pipe(ph, mb->buf.p, mb->buf.size);
    return TRUE;
}
static int pipeio_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefProcessHandle *ph = Value_vp(r->v[INDEX_P_HANDLE]);

    pipeio_close_sub(ph);

    if (ph->fd_in != -1) {
        close_fox(ph->fd_in);
        ph->fd_in = -1;
    }
    if (ph->fd_out != -1) {
        close_fox(ph->fd_out);
        ph->fd_out = -1;
    }

    return TRUE;
}

// TextIO(PipeIO(...), "ローカルCP")
static int process_popen(Value *vret, Value *v, RefNode *node)
{
    RefNode *fn_textio_new = fs->Hash_get_p(&fs->cls_textio->u.c.h, fs->str_new);
    RefNode *fn_pipeio_new = fs->Hash_get_p(&cls_pipeio->u.c.h, fs->str_new);

    if (!fs->call_function(fn_pipeio_new, 3)) {
        return FALSE;
    }

    fg->stk_top[0] = fg->stk_top[-1];
    fg->stk_top[-1] = VALUE_NULL;
    fg->stk_top++;
    fs->Value_push("rb", get_local_codepage(), TRUE);
    if (!fs->call_function(fn_textio_new, 3)) {
        return FALSE;
    }
    *vret = fg->stk_top[-1];
    fg->stk_top--;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "popen", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, process_popen, 3, 3, NULL, NULL, fs->cls_list, fs->cls_str);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_process = fs->define_identifier(m, m, "Process", NODE_CLASS, 0);
    cls = cls_process;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, process_new, 2, 2, (void*)FALSE, NULL, fs->cls_list);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, process_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "kill", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, process_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "wait", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, process_wait, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "alive", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, process_status, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "exit_status", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, process_status, 0, 0, (void*)TRUE);

    cls->u.c.n_memb = INDEX_P_NUM;
    fs->extends_method(cls, fs->cls_obj);


    cls_pipeio = fs->define_identifier(m, m, "PipeIO", NODE_CLASS, 0);
    cls = cls_pipeio;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, process_new, 3, 3, (void*)TRUE, NULL, fs->cls_list, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, pipeio_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, pipeio_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, pipeio_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, pipeio_write, 1, 1, NULL, fs->cls_bytesio);

    n = fs->define_identifier(m, cls, "wait", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, process_wait, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "alive", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, process_status, 0, 0, (void*)FALSE);
    n = fs->define_identifier(m, cls, "exit_status", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, process_status, 0, 0, (void*)TRUE);

    cls->u.c.n_memb = INDEX_P_NUM;
    fs->extends_method(cls, fs->cls_streamio);


    cls = fs->define_identifier(m, m, "ProcessError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_process = m;

    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\n";
}
