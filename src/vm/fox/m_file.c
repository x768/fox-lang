#include "fox_vm.h"
#include <errno.h>


enum {
    FILEPATH_BASEDIR,

    FILEPATH_PATH,
    FILEPATH_FILENAME,
    FILEPATH_BASENAME,
    FILEPATH_SUFFIX,

    FILEPATH_PATH_BYTES,
    FILEPATH_FILENAME_BYTES,
    FILEPATH_BASENAME_BYTES,
    FILEPATH_SUFFIX_BYTES,
};
enum {
    DIRITER_BOTH,
    DIRITER_DIRS,
    DIRITER_FILES,
    DIRITER_GLOB,
};

enum {
    DIRGLOB_MAX_STACK = 256,
};

typedef struct {
    DIR *d;
    char *cur;
    const char *rest;
} DirGlob;
typedef struct {
    RefHeader rh;

    Value vfile;
    int type;

    DIR *dir;
    DirGlob *dir_top;
    DirGlob *dir_stk;
} RefDirIter;

static RefNode *cls_diriter;


/**
 * vが文字列ならpath_regularizeした結果を返す
 * vがFileならf.path()をdupした結果を返す
 *
 * mallocで確保したバッファを返す
 * NULを含む場合はエラー
 */
char *file_value_to_path(Str *ret, Value v, int argn)
{
    RefNode *type = Value_type(v);
    Str path;
    char *ptr;

    if (type == fs->cls_str) {
        int len;
        RefStr *rs = Value_vp(v);
        ptr = path_normalize(&path, fv->cur_dir, rs->c, rs->size, NULL);

        // NULを含む場合エラー
        if (str_has0(path.p, path.size)) {
            throw_errorf(fs->mod_file, "FileOpenError", "File path contains '\\0'");
            return NULL;
        }
        len = strlen(ptr) - 1;
        // 末尾のセパレータを除去
        if (len >= 0 && ptr[len] == SEP_C) {
            ptr[len] = '\0';
        }
    } else if (type == fs->cls_file) {
        RefStr *rs = Value_vp(v);
        ptr = str_dup_p(rs->c, rs->size, NULL);
        path = Str_new(ptr, rs->size);
    } else {
        throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_file, type, argn + 1);
        return NULL;
    }
    if (ret != NULL) {
        *ret = path;
    }
    return ptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// class File

/**
 * 内部構造はRef型のStrと同じ
 */
static int file_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *r1 = Value_vp(v[1]);
    char *path;
    Str path_s;

    path = path_normalize(&path_s, fv->cur_dir, r1->c, r1->size, NULL);
    if (path_s.size > 0 && path_s.p[path_s.size - 1] == SEP_C) {
        path_s.size--;
    }
    // NULを含む場合エラー
    if (str_has0(path_s.p, path_s.size)) {
        throw_errorf(fs->mod_file, "FileOpenError", "File path contains '\\0'");
        return FALSE;
    }

    *vret = cstr_Value(fs->cls_file, path_s.p, path_s.size);
    free(path);

    return TRUE;
}

/**
 * 相対パスに変換
 * パス区切り文字はWindowsでも/に変換
 * 同じディレクトリの場合は、./から始まる
 */
static int file_relative(Value *vret, Value *v, RefNode *node)
{
    RefStr *path = Value_vp(*v);
    Str base;
    Str relpath;
    char *path_p = NULL;
    int idx = 0, count = 0;
    Value tmp = VALUE_NULL;
    RefStr *rs_tmp;
    char *dst;
    int i;

    if (fg->stk_top > v + 1) {
        // 引数のディレクトリを基準にする
        path_p = file_value_to_path(&base, v[1], 0);
        base = Str_new(path_p, -1);
    } else {
        // ソースファイルのあるディレクトリを基準にする
        base = Str_new(fv->cur_dir->c, fv->cur_dir->size);
        if (base.size > 0 && base.p[base.size - 1] == SEP_C) {
            base.size--;
        }
    }
#ifdef WIN32
    // Windowsでドライブが異なる場合、エラーにする
    // TODO UNC対応
    if (toupper(path->c[0]) != toupper(base.p[0])) {
        throw_errorf(fs->mod_lang, "ValueError", "Cannot resolve relative path");
        return FALSE;
    }
#endif

    // 一致しない所までidxを進める
    while (idx < base.size && idx < path->size) {
        if (base.p[idx] != path->c[idx]) {
            break;
        }
        idx++;
    }
    // baseを遡る回数
    for (i = idx; i < base.size; i++) {
        if (base.p[i] != SEP_C) {
            // /で区切られた区間を数える(/が出現しなくても1になる場合がある)
            if (i == idx || base.p[i - 1] == SEP_C) {
                count++;
            }
        }
    }

    relpath = Str_new(path->c + idx, path->size - idx);
    if (relpath.size > 0 && relpath.p[0] == SEP_C) {
        // 先頭の1文字を削除
        relpath.p++;
        relpath.size--;
    }
    if (count == 0) {
        // "./path"
        rs_tmp = refstr_new_n(fs->cls_str, relpath.size + 2);
        tmp = vp_Value(rs_tmp);
        dst = rs_tmp->c;
        memcpy(dst, "./", 2);
        dst += 2;
    } else {
        // "../../path"
        rs_tmp = refstr_new_n(fs->cls_str, relpath.size + count * 3);
        tmp = vp_Value(rs_tmp);
        dst = rs_tmp->c;
        for (i = 0; i < count; i++) {
            memcpy(dst, "../", 3);
            dst += 3;
        }
    }
    memcpy(dst, relpath.p, relpath.size);

#ifdef WIN32
    for (i = 0; i < relpath.size; i++) {
        if (dst[i] == SEP_C) {
            dst[i] = '/';
        }
    }
#endif
    if (invalid_utf8_pos(relpath.p, relpath.size) >= 0) {
        *vret = cstr_Value_conv(rs_tmp->c, rs_tmp->size, NULL);
        Value_dec(tmp);
    } else {
        *vret = tmp;
    }
    free(path_p);

    return TRUE;
}
static int file_touri(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(*v);
    StrBuf buf;
    int i;
    StrBuf_init(&buf, src->size);

#ifdef WIN32
    if (src->size >= 2 && src->c[0] == '\\' && src->c[1] == '\\') {
        StrBuf_add(&buf, "file:", 5);
    } else {
        StrBuf_add(&buf, "file:///", 8);
    }
#else
    StrBuf_add(&buf, "file://", 7);
#endif
    for (i = 0; i < src->size; i++) {
        int ch = src->c[i];

        if ((ch & 0x80) == 0 && (isalnumu_fox(ch) || ch == '.' || ch == '-')) {
            StrBuf_add_c(&buf, ch);
        } else if (ch == SEP_C) {
            StrBuf_add_c(&buf, '/');
#ifdef WIN32
        } else if (ch == ':' || ch == '/') {
            StrBuf_add_c(&buf, ch);
#endif
        } else {
            StrBuf_add_c(&buf, '%');
            StrBuf_add_c(&buf, hex2uchar((ch >> 4) & 0xF));
            StrBuf_add_c(&buf, hex2uchar(ch & 0xF));
        }
    }

    *vret = cstr_Value(fs->cls_uri, buf.p, buf.size);
    StrBuf_close(&buf);

    return TRUE;
}

static int file_in(Value *vret, Value *v, RefNode *node)
{
    RefStr *r1 = Value_vp(*v);
    RefStr *r2 = Value_vp(v[1]);
    int ret = FALSE;

    if (r1->size > r2->size) {
    } else if (r1->size == r2->size) {
        if (memcmp(r1->c, r2->c, r1->size) == 0) {
            ret = TRUE;
        }
    } else {
        if (memcmp(r1->c, r2->c, r1->size) == 0 && r2->c[r1->size] == SEP_C) {
            ret = TRUE;
        }
    }
    *vret = bool_Value(ret);

    return TRUE;
}
/**
 * File('dir') / 'file.txt'
 */
static int file_cat(Value *vret, Value *v, RefNode *node)
{
    RefStr *base_s = Value_vp(*v);
    RefStr *path_s = Value_vp(v[1]);
    RefStr *rs;
    Value tmp;

    if (base_s->c[base_s->size - 1] == SEP_C) {
        tmp = printf_Value("%r%r", base_s, path_s);
        rs = Value_vp(tmp);
    } else {
        tmp = printf_Value("%r" SEP_S "%r", base_s, path_s);
        rs = Value_vp(tmp);
    }
    *vret = tmp;
    rs->size = make_path_regularize(rs->c, rs->size);
    rs->rh.type = fs->cls_file;

    return TRUE;
}
static int file_path(Value *vret, Value *v, RefNode *node)
{
    int type = FUNC_INT(node);
    Str s = Value_str(*v);
    Str path;
    int i;

    switch (type) {
    case FILEPATH_BASEDIR: // basedir:File
        path = base_dir_with_sep(s.p, s.size);
        if (path.p[0] != '.') {
            // 末尾の / を除去
            path.size--;
            *vret = cstr_Value(fs->cls_file, path.p, path.size);
            return TRUE;
        }
        break;
    case FILEPATH_PATH:          // path:Str
    case FILEPATH_PATH_BYTES:    // path_bytes:Bytes
        if (is_root_dir(s)) {
            path = get_root_name(s);
        } else {
            path = s;
        }
        break;
    case FILEPATH_FILENAME:       // filename:Str
    case FILEPATH_FILENAME_BYTES: // filename_bytes:Bytes
        path = file_name_from_path(s);
        break;
    case FILEPATH_BASENAME:       // basename:Str
    case FILEPATH_BASENAME_BYTES: // basename_bytes:Bytes
        path = file_name_from_path(s);
        // 最後の.以降を削除
        for (i = path.size - 1; i >= 0; i--) {
            if (path.p[i] == '.') {
                path.size = i;
                break;
            }
        }
        break;
    case FILEPATH_SUFFIX:       // suffix:Str
    case FILEPATH_SUFFIX_BYTES: // suffix_bytes:Bytes
        path = s;
        // 最後の.以降
        for (i = path.size - 1; i >= 0; i--) {
            if (path.p[i] == '.') {
                i++;
                path.size -= i;
                path.p += i;
                break;
            }
        }
        break;
    default:
        return TRUE;
    }
    if (type < FILEPATH_PATH_BYTES) {
        *vret = cstr_Value_conv(path.p, path.size, NULL);
    } else {
        *vret = cstr_Value(fs->cls_bytes, path.p, path.size);
    }

    return TRUE;
}
static int file_size(Value *vret, Value *v, RefNode *node)
{
    RefStr *path = Value_vp(*v);
    int fd = open_fox(path->c, O_RDONLY, DEFAULT_PERMISSION);
    int64_t size = -1;

    if (fd != -1) {
        size = get_file_size(fd);
        close_fox(fd);
    }
    if (size < 0) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path->c, path->size));
        return FALSE;
    }
    *vret = int64_Value(size);
    return TRUE;
}
static int file_exists(Value *vret, Value *v, RefNode *node)
{
    RefStr *path = Value_vp(*v);
    int flags = FUNC_INT(node);
    int ret = FALSE;
    int type = exists_file(path->c);
    if ((type & flags) != 0) {
        ret = TRUE;
    }
    *vret = bool_Value(ret);

    return TRUE;
}
static int file_mtime(Value *vret, Value *v, RefNode *node)
{
    RefStr *path = Value_vp(*v);
    int64_t mtime;

    if (get_file_mtime(&mtime, path->c)) {
        RefInt64 *tm = buf_new(fs->cls_timestamp, sizeof(RefInt64));
        tm->u.i = mtime;
        *vret = vp_Value(tm);
        return TRUE;
    }
    throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path->c, path->size));
    return FALSE;
}

// ディレクトリリスト
static int file_iter(Value *vret, Value *v, RefNode *node)
{
    RefStr *path = Value_vp(*v);
    DIR *d;
    RefDirIter *r;

    errno = 0;
    d = opendir_fox(path->c);

    if (d == NULL) {
        goto FINALLY;
    }
    r = buf_new(cls_diriter, sizeof(RefDirIter));
    *vret = vp_Value(r);
    r->vfile = Value_cp(*v);
    r->dir = d;
    r->dir_top = NULL;
    r->dir_stk = NULL;
    r->type = FUNC_INT(node);

    return TRUE;

FINALLY:
    throw_errorf(fs->mod_file, "DirOpenError", "Cannot open directory %q", path->c);
    return FALSE;
}
static int file_marshal_read(Value *vret, Value *v, RefNode *node)
{
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint32_t size;
    int rd_size;
    RefStr *rs;

    if (!stream_read_uint32(&size, r)) {
        return FALSE;
    }
    if (size > 0xffffff) {
        throw_errorf(fs->mod_lang, "ValueError", "Invalid size number");
        return FALSE;
    }
    if (size > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }

    rs = refstr_new_n(fs->cls_file, size);
    *vret = vp_Value(rs);
    rd_size = size;
    if (!stream_read_data(r, NULL, rs->c, &rd_size, FALSE, TRUE)) {
        return FALSE;
    }
    rs->c[size] = '\0';

    return TRUE;
}
static int file_marshal_write(Value *vret, Value *v, RefNode *node)
{
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    RefStr *rs = Value_vp(*v);

    if (!stream_write_uint32(rs->size, w)) {
        return FALSE;
    }
    if (!stream_write_data(w, rs->c, rs->size)) {
        return FALSE;
    }

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////

// .と..を除外した列挙
static struct dirent *readdir_except_self(DIR *d)
{
    struct dirent *dh;
    while ((dh = readdir_fox(d)) != NULL) {
        const char *name = dh->d_name;
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            return dh;
        }
    }
    return NULL;
}
static int file_glob_dir(RefDirIter *r, char *cur, const char *rest)
{
    DIR *d;
    errno = 0;
    d = opendir_fox(cur);
    if (d == NULL) {
        goto FINALLY;
    }
    r->dir_top->d = d;
    r->dir_top->cur = cur;
    r->dir_top->rest = rest;
    return TRUE;

FINALLY:
    throw_errorf(fs->mod_file, "DirOpenError", "Cannot open directory %q", cur);
    return FALSE;
}
static int match_glob_sub(const char *fname, const char *fend, const char *rest, const char *end)
{
    {
        const char *p;
        for (p = rest; p < end; p++) {
            if (*p != '*') {
                break;
            }
        }
        // 全て"*"
        if (p == end) {
            return TRUE;
        }
    }
    while (rest < end) {
        if (*rest == '*') {
            rest++;
            if (rest == end) {
                return TRUE;
            }
            while (fname < fend) {
                if (match_glob_sub(fname, fend, rest, end)) {
                    return TRUE;
                }
                utf8_next(&fname, fend);
            }
            return FALSE;
        } else if (*rest == '?') {
            // 1文字(コードポイント)食べる
            rest++;
            if (fname >= fend) {
                return FALSE;
            }
            utf8_next(&fname, fend);
        } else {
#ifndef WIN32
            if (*rest == '\\') {
                rest++;
                if (fname >= fend) {
                    return rest == end;
                }
            }
#endif
            if (fname >= fend) {
                return FALSE;
            }
            if (*fname != *rest) {
                return FALSE;
            }
            rest++;
            fname++;
        }
    }
    return fname == fend;
}
#if 0
static int match_double_aster(struct dirent *dh, const char **prest)
{
    const char *rest = *prest;
    if (rest[0] == '*' && rest[1] == '*') {
#ifdef WIN32
        if (rest[2] == '/' || rest[2] == '\\') {
            *prest = rest + 3;
            return TRUE;
        }
#else
        if (rest[2] == '/') {
            *prest = rest + 3;
            return TRUE;
        }
#endif
    }
    return FALSE;
}
#endif

static int match_glob_name(struct dirent *dh, const char **prest)
{
    const char *rest = *prest;
    const char *end = rest;

    while (*end != '\0') {
#ifdef WIN32
        if (*end == '/' || *end == '\\') {
            break;
        }
#else
        if (*end == '/') {
            break;
        }
#endif
        end++;
    }

    if (*end != '\0') {
        if (dh->d_type != DT_DIR) {
            return FALSE;
        }
        if (match_glob_sub(dh->d_name, dh->d_name + strlen(dh->d_name), rest, end)) {
            *prest = end + 1;
            return TRUE;
        }
    } else {
        if (match_glob_sub(dh->d_name, dh->d_name + strlen(dh->d_name), rest, end)) {
            *prest = end;
            return TRUE;
        }
    }
    return FALSE;
}

static int diriter_next(Value *vret, Value *v, RefNode *node)
{
    RefDirIter *r = Value_vp(*v);
    int type = r->type;
    struct dirent *dh;

    if (type == DIRITER_GLOB) {
        DIR *d;
        const char *rest;
        if (r->dir_stk == NULL) {
            throw_stopiter();
            return FALSE;
        }
        if (r->dir_top < r->dir_stk) {
            throw_stopiter();
            return FALSE;
        }

        d = r->dir_top->d;
        rest = r->dir_top->rest;
        for (;;) {
            for (;;) {
                dh = readdir_except_self(d);
                if (dh != NULL) {
                    break;
                }
                free(r->dir_top->cur);
                closedir_fox(r->dir_top->d);
                r->dir_top--;
                if (r->dir_top < r->dir_stk) {
                    break;
                }
                d = r->dir_top->d;
                rest = r->dir_top->rest;
            }
            if (dh == NULL) {
                break;
            }

            if (match_glob_name(dh, &rest)) {
                if (rest[0] != '\0') {
                    // 下の階層へ
                    if (r->dir_top < r->dir_stk + DIRGLOB_MAX_STACK - 1) {
                        char *path2 = str_printf("%s" SEP_S "%s", r->dir_top->cur, dh->d_name);
                        r->dir_top++;
                        if (!file_glob_dir(r, path2, rest)) {
                            return FALSE;
                        }
                        d = r->dir_top->d;
                        rest = r->dir_top->rest;
                    } else {
                        throw_errorf(fs->mod_file, "DirOpenError", "Directory too depth (max:%d)", DIRGLOB_MAX_STACK);
                        return FALSE;
                    }
                } else {
                    break;
                }
            }
        }
        if (dh != NULL) {
            const char *path = r->dir_top->cur;
            Value vf = printf_Value("%s" SEP_S "%s", path, dh->d_name);
            Value_ref_header(vf)->type = fs->cls_file;
            *vret = vf;
            return TRUE;
        }
    } else {
        DIR *d = r->dir;
        if (d == NULL) {
            throw_stopiter();
            return FALSE;
        }
        while ((dh = readdir_except_self(d)) != NULL) {
            if (type == DIRITER_DIRS) {
                // ディレクトリのみを列挙
                if (dh->d_type != DT_DIR) {
                    continue;
                }
                break;
            } else if (type == DIRITER_FILES) {
                // ファイルのみを列挙
                if (dh->d_type != DT_REG) {
                    continue;
                }
                break;
            } else {
                break;
            }
        }

        if (dh != NULL) {
            RefStr *path = Value_vp(r->vfile);
            Value vf = printf_Value("%r" SEP_S "%s", path, dh->d_name);
            Value_ref_header(vf)->type = fs->cls_file;
            *vret = vf;
            return TRUE;
        }
    }

    throw_stopiter();
    return FALSE;
}
static int diriter_close(Value *vret, Value *v, RefNode *node)
{
    RefDirIter *r = Value_vp(*v);

    if (r->dir_stk != NULL) {
        while (r->dir_top >= r->dir_stk) {
            free(r->dir_top->cur);
            closedir_fox(r->dir_top->d);
            r->dir_top--;
        }
        free(r->dir_stk);
        r->dir_stk = NULL;
    } else if (r->dir != NULL) {
        closedir_fox(r->dir);
        r->dir = NULL;
    }
    Value_dec(r->vfile);
    r->vfile = VALUE_NULL;

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// class FileIO

static int fileio_new(Value *vret, Value *v, RefNode *node)
{
    int fd;
    RefFileHandle *fh;
    int mode = O_RDONLY;

    Ref *r = ref_new(fv->cls_fileio);
    Value v1 = v[1];

    Str path_s;
    char *path = file_value_to_path(&path_s, v1, 1);

    *vret = vp_Value(r);

    if (path == NULL) {
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        RefStr *m = Value_vp(v[2]);
        if (strcmp(m->c, "r") == 0) {
            //
        } else if (strcmp(m->c, "w") == 0) {
            mode = O_CREAT|O_WRONLY|O_TRUNC;
        } else if (strcmp(m->c, "a") == 0) {
            mode = O_CREAT|O_WRONLY|O_APPEND;
        } else {
            throw_errorf(fs->mod_lang, "ValueError", "Unknown open mode %Q", m);
            free(path);
            return FALSE;
        }
    }

    fd = open_fox(path, mode, DEFAULT_PERMISSION);

    if (fd == -1) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
        free(path);
        return FALSE;
    }

    fh = buf_new(NULL, sizeof(RefFileHandle));
    r->v[INDEX_FILEIO_HANDLE] = vp_Value(fh);
    if (mode == O_RDONLY) {
        init_stream_ref(r, STREAM_READ);
        fh->fd_read = fd;
        fh->fd_write = -1;
    } else {
        init_stream_ref(r, STREAM_WRITE);
        fh->fd_read = -1;
        fh->fd_write = fd;
    }

    free(path);

    return TRUE;
}
static int fileio_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);

    if (fh != NULL) {
        int fd_read = fh->fd_read;

        if (fh->fd_read != -1) {
            close_fox(fh->fd_read);
            fh->fd_read = -1;
        }
        if (fh->fd_write != -1) {
            if (fh->fd_write != fd_read) {
                close_fox(fh->fd_write);
            }
            fh->fd_write = -1;
        }
    }
    return TRUE;
}
static int fileio_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);
    RefBytesIO *mb = Value_vp(v[1]);
    int size = Value_int64(v[2], NULL);
    int rd;

    if (fh->fd_read == -1) {
        throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    }
#ifdef WIN32
    if (fh->fd_write == STDIN_FILENO && fv->console_read) {
        rd = win_read_console(fh->fd_read, mb->buf.p + mb->buf.size, size);
    } else {
        rd = read_fox(fh->fd_read, mb->buf.p + mb->buf.size, size);
    }
#else
    rd = read_fox(fh->fd_read, mb->buf.p + mb->buf.size, size);
#endif
    mb->buf.size += rd;

    return TRUE;
}
/*
 * 基本的にエラーが発生しても例外をださない
 */
static int fileio_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);
    RefBytesIO *mb = Value_vp(v[1]);
    int wrote_size = 0;

    if (fh->fd_write != -1) {
#ifdef WIN32
        if (fh->fd_write == STDOUT_FILENO && fv->console_write) {
            wrote_size = win_write_console(fh->fd_write, mb->buf.p, mb->buf.size);
        } else {
            wrote_size = write_fox(fh->fd_write, mb->buf.p, mb->buf.size);
        }
#else
        wrote_size = write_fox(fh->fd_write, mb->buf.p, mb->buf.size);
#endif
    }
    *vret = int32_Value(wrote_size);

    return TRUE;
}
static int fileio_empty(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);

    if (fh->fd_read != -1 || fh->fd_write != -1) {
        *vret = VALUE_FALSE;
    } else {
        *vret = VALUE_TRUE;
    }

    return TRUE;
}

static int fileio_seek(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);

    if (fh->fd_read != -1) {
        int64_t offset = Value_int64(v[1], NULL);
        int64_t result = seek_fox(fh->fd_read, offset, 0);

        if (result == -1) {
            throw_errorf(fs->mod_io, "ReadError", "seek operation failed");
            return FALSE;
        }
        *vret = int64_Value(result);
    } else {
        throw_errorf(fs->mod_io, "ReadError", "Not opened for read");
        return FALSE;
    }

    return TRUE;
}
static int fileio_size(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);

    if (fh->fd_read != -1) {
        int64_t result = get_file_size(fh->fd_read);

        if (result == -1) {
            throw_errorf(fs->mod_io, "ReadError", "get_file_size failed");
            return FALSE;
        }
        *vret = int64_Value(result);
    } else {
        throw_errorf(fs->mod_io, "ReadError", "Not opened for read");
        return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

// func
static int file_fopen(Value *vret, Value *v, RefNode *node)
{
    Value tmp = VALUE_NULL;
    Ref *ref;

    if (!fileio_new(&tmp, v, node)) {
        Value_dec(tmp);
        return FALSE;
    }
    ref = ref_new(fs->cls_textio);
    *vret = vp_Value(ref);
    ref->v[INDEX_TEXTIO_STREAM] = tmp;
    ref->v[INDEX_TEXTIO_TEXTIO] = vp_Value(fv->ref_textio_utf8);

    return TRUE;
}

static int make_directory(const char *path)
{
    int result = TRUE;
    char *cp = str_dup_p(path, -1, NULL);
    char *cp_end = &cp[strlen(cp)];
    char *path_ptr = cp_end - 1;
    int type;

    // 既に存在している
    type = exists_file(cp);
    if (type != EXISTS_NONE) {
        free(cp);
        return type == EXISTS_DIR;
    }

    // ディレクトリが存在する階層を探す
    while (path_ptr > cp) {
        if (*path_ptr == SEP_C) {
            *path_ptr = '\0';
            type = exists_file(cp);
            if (type == EXISTS_DIR) {
                break;
            } else if (type == EXISTS_FILE) {
                free(cp);
                return FALSE;
            }
        }
        path_ptr--;
    }

    // ディレクトリを作成する
    while (path_ptr < cp_end) {
        if (*path_ptr == '\0') {
            *path_ptr = SEP_C;
            if (mkdir_fox(cp, DEFAULT_PERMISSION_DIR) != 0) {
                result = FALSE;
                break;
            }
        }
        path_ptr++;
    }

    free(cp);
    return result;
}
static int file_mkdir(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    int error_num = 0;
    int ignore = FALSE;
    int already_exists = FALSE;

    Str path_s;
    char *path = file_value_to_path(&path_s, v1, 1);
    int result;

    if (path == NULL) {
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        ignore = Value_bool(v[2]);
    }

    errno = 0;
    // ignore == FALSEで、既に存在する場合は失敗とする
    if (ignore || exists_file(path) == EXISTS_NONE) {
        result = make_directory(path);
        error_num = errno;
    } else {
        result = FALSE;
        already_exists = TRUE;
    }

    if (!result) {
        if (already_exists) {
            throw_errorf(fs->mod_io, "WriteError", "Directory already exists %q", path);
        } else if (error_num != 0) {
            throw_errorf(fs->mod_io, "WriteError", "%s (%q)", strerror(error_num), path);
        } else {
            throw_errorf(fs->mod_io, "WriteError", "Fault to make directory %q", path);
        }
    }
    free(path);

    return result;
}
static int file_unlink(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    int error_num = 0;
    int ignore = FALSE;

    Str path_s;
    char *path = file_value_to_path(&path_s, v1, 1);
    int result;

    if (path == NULL) {
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        ignore = Value_bool(v[2]);
    }

    errno = 0;
    // ignore == TRUEの場合、存在しない場合は無視する
    if (exists_file(path) != EXISTS_NONE) {
        result = (remove_fox(path) == 0);
        error_num = errno;
        if (!result) {
            if (error_num != 0) {
                throw_errorf(fs->mod_io, "WriteError", "%s (%q)", strerror(error_num), path);
            } else {
                throw_errorf(fs->mod_io, "WriteError", "No such file or directory %q", path);
            }
        }
    } else if (!ignore) {
        throw_errorf(fs->mod_io, "WriteError", "No such file or directory %q", path);
        result = FALSE;
    } else {
        return TRUE;
    }

    free(path);

    return result;
}
static int file_rename(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    Value v2 = v[2];
    int error_num = 0;

    Str path1_s;
    Str path2_s;
    char *path1 = NULL;
    char *path2 = NULL;
    int result = TRUE;

    path1 = file_value_to_path(&path1_s, v1, 1);
    if (path1 == NULL) {
        goto FINALLY;
    }
    path2 = file_value_to_path(&path2_s, v2, 1);
    if (path2 == NULL) {
        goto FINALLY;
    }

    errno = 0;
    result = (rename_fox(path1, path2) == 0);
    error_num = errno;

    if (!result) {
        if (error_num != 0) {
            throw_errorf(fs->mod_io, "WriteError", "%s (%q to %q)", strerror(error_num), path1, path2);
        } else {
            throw_errorf(fs->mod_io, "WriteError", "Rename fault (%q to %q)", path1, path2);
        }
    }

FINALLY:
    free(path1);
    free(path2);

    return result;
}
static int file_copy(Value *vret, Value *v, RefNode *node)
{
    enum {
        COPY_BUF_SIZE = 32 * 1024,
    };
    Value v1 = v[1];
    Value v2 = v[2];

    Str path1_s;
    Str path2_s;
    char *path1 = NULL;
    char *path2 = NULL;
    int result = TRUE;
    int overwrite = FALSE;

    path1 = file_value_to_path(&path1_s, v1, 1);
    if (path1 == NULL) {
        goto FINALLY;
    }
    path2 = file_value_to_path(&path2_s, v2, 1);
    if (path2 == NULL) {
        goto FINALLY;
    }

    if (fg->stk_top > v + 3) {
        overwrite = Value_bool(v[3]);
    }
    if (exists_file(path1) != EXISTS_FILE) {
        throw_errorf(fs->mod_io, "ReadError", "Cannot read file %q", path1);
        result = FALSE;
    } else if (!overwrite && exists_file(path2) != EXISTS_NONE) {
        // overwriteが指定されていない場合、コピー先にファイルが存在したらエラー
        throw_errorf(fs->mod_io, "WriteError", "Already exists file %q", path2);
        result = FALSE;
    } else {
        int f1 = -1, f2 = -1;

        if ((f1 = open_fox(path1, O_RDONLY, DEFAULT_PERMISSION)) == -1) {
            throw_errorf(fs->mod_io, "ReadError", "Cannot read file %q", path1);
            result = FALSE;
        } else if ((f2 = open_fox(path2, O_CREAT|O_WRONLY|O_TRUNC, DEFAULT_PERMISSION)) == -1) {
            throw_errorf(fs->mod_io, "WriteError", "Fault to copy file %q", path2);
            result = FALSE;
        } else {
            char *buf = malloc(COPY_BUF_SIZE);
            int rd;
            while ((rd = read_fox(f1, buf, COPY_BUF_SIZE)) > 0) {
                write_fox(f2, buf, rd);
            }
            free(buf);
        }
        if (f1 != -1) {
            close_fox(f1);
        }
        if (f2 != -1) {
            close_fox(f2);
        }
    }

FINALLY:
    free(path1);
    free(path2);

    return result;
}
static int read_file_sub(char *buf, int *psize, const char *path)
{
    int size = *psize;
    int fh = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
    if (fh == -1) {
        return FALSE;
    }
    size = read_fox(fh, buf, size);
    if (size == -1) {
        return FALSE;
    }
    *psize = size;

    return TRUE;
}
static int file_readfile(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    int64_t size64;
    Str path_s;
    char *path = file_value_to_path(&path_s, v1, 1);
    int fd;

    if (path == NULL) {
        return FALSE;
    }

    fd = open_fox(path, O_RDONLY, DEFAULT_PERMISSION);
    size64 = get_file_size(fd);
    if (fd != -1) {
        close_fox(fd);
    }
    if (size64 == -1) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
        goto ERROR_END;
    } else if (size64 > fs->max_alloc) {
        throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        goto ERROR_END;
    }
    if (fg->stk_top > v + 2) {
        // 文字コード変換を行う
        int size = size64;
        Str ret_s;
        char *buf = malloc(size);
        if (!read_file_sub(buf, &size, path)) {
            throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
            free(buf);
            goto ERROR_END;
        }
        ret_s = Str_new(buf, size);
        if (!convert_bin_to_str(vret, &ret_s, 1)) {
            return FALSE;
        }
        free(buf);
    } else {
        // そのまま読み込む
        int size = size64;
        RefStr *buf = refstr_new_n(fs->cls_bytes, size);
        *vret = vp_Value(buf);
        if (!read_file_sub(buf->c, &size, path)) {
            throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
            free(buf);
            goto ERROR_END;
        }
        buf->size = size;
    }
    free(path);

    return TRUE;

ERROR_END:
    free(path);
    return FALSE;
}

static int file_write_stream_sub(const char *path, Value v1, int append)
{
    char *dbuf;    // 出力先バッファ

    int fd = open_fox(path, (append ? O_CREAT|O_WRONLY|O_APPEND : O_CREAT|O_WRONLY|O_TRUNC), DEFAULT_PERMISSION);
    if (fd == -1) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(path, -1));
        return FALSE;
    }

    dbuf = malloc(BUFFER_SIZE);

    for (;;) {
        int read_size = BUFFER_SIZE;

        // r1から読んで、直接dbufのバッファに入れる
        if (!stream_read_data(v1, NULL, dbuf, &read_size, FALSE, FALSE)) {
            return FALSE;
        }
        // r1が終端に達した
        if (read_size <= 0) {
            break;
        }
        write_fox(fd, dbuf, read_size);
    }
    close_fox(fd);

    return TRUE;
}
static int write_to_file(const char *path, const char *write_p, int write_size, int append)
{
    int fd = open_fox(path, (append ? O_CREAT|O_WRONLY|O_APPEND : O_CREAT|O_WRONLY|O_TRUNC), DEFAULT_PERMISSION);
    int ret;

    if (fd == -1) {
        return FALSE;
    }
    ret = (write_fox(fd, write_p, write_size) == write_size);
    close_fox(fd);

    return ret;
}
static int file_writefile(Value *vret, Value *v, RefNode *node)
{
    Value v1 = v[1];
    Value v2 = v[2];
    StrBuf sbuf;
    RefNode *type = Value_type(v2);
    const char *write_p = NULL;
    int write_size = 0;

    Str path_s;
    char *path = file_value_to_path(&path_s, v1, 1);

    if (path == NULL) {
        return FALSE;
    }
    sbuf.p = NULL;

    if (fg->stk_top > v + 3) {
        // 引数が2つ以上ある場合は文字列
        if (type == fs->cls_str) {
            RefCharset *cs = Value_vp(v[3]);
            RefStr *rs;
            int alt = FALSE;

            if (fg->stk_top > v + 3 && Value_bool(v[4])) {
                alt = TRUE;
            }
            rs = Value_vp(v2);
            if (cs != fs->cs_utf8) {
                StrBuf_init(&sbuf, rs->size);
                if (!convert_str_to_bin_sub(&sbuf, rs->c, rs->size, cs, alt ? "?" : NULL)) {
                    StrBuf_close(&sbuf);
                    return FALSE;
                }
                write_p = sbuf.p;
                write_size = sbuf.size;
            } else {
                write_p = rs->c;
                write_size = rs->size;
            }
        } else {
            throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, fs->cls_str, type, 2);
            return FALSE;
        }
    } else {
        if (type == fs->cls_bytes) {
            RefStr *rs = Value_vp(v2);
            write_p = rs->c;
            write_size = rs->size;
        } else if (type == fs->cls_bytesio) {
            RefBytesIO *bio = Value_vp(v2);
            write_p = bio->buf.p;
            write_size = bio->buf.size;
        } else if (is_subclass(type, fs->cls_streamio)) {
            if (!file_write_stream_sub(path, v2, FUNC_INT(node))) {
                return FALSE;
            }
            return TRUE;
        } else {
            throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_bytes, fs->cls_streamio, type, 2);
            return FALSE;
        }
    }
    if (!write_to_file(path, write_p, write_size, FUNC_INT(node))) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, path_s);
        return FALSE;
    }

    if (sbuf.p != NULL) {
        StrBuf_close(&sbuf);
    }
    free(path);

    return TRUE;
}

static int file_getcwd(Value *vret, Value *v, RefNode *node)
{
    char *path = get_current_directory();
    *vret = cstr_Value(fs->cls_file, path, -1);
    free(path);
    return TRUE;
}
static int file_chdir(Value *vret, Value *v, RefNode *node)
{
    char *path = file_value_to_path(NULL, v[1], 0);

    if (path == NULL) {
        return FALSE;
    }
    if (!set_current_directory(path)) {
        throw_errorf(fs->mod_file, "DirOpenError", "Change directory failed %q", path);
        free(path);
        return FALSE;
    }
    free(path);

    path = get_current_directory();
    Value_dec(vp_Value(fv->cur_dir));
    fv->cur_dir = Value_vp(cstr_Value(fs->cls_file, path, -1));
    free(path);

    return TRUE;
}
// def dir(f) => File(f).iterator()
static int file_dir(Value *vret, Value *v, RefNode *node)
{
    if (Value_type(v[1]) == fs->cls_file) {
        *v = v[1];
    } else {
        Value vf;
        if (!file_new(&vf, v, node)) {
            return FALSE;
        }
        *v = vf;
        Value_dec(v[1]);
    }
    fg->stk_top = fg->stk_base + 1;

    if (!file_iter(vret, v, node)) {
        return FALSE;
    }
    return TRUE;
}

static char *split_glob_path(char *path)
{
    char *p = path;
    char *last = p;

    while (*p != '\0') {
        if (*p == '/') {
            if (p[1] != '\0') {
                last = p;
            }
        }
#ifdef WIN32
        if (*p == '\\') {
            if (p[1] != '\0') {
                last = p;
            }
        } else {
            if (*p == '*' || *p == '?') {
                break;
            }
        }
#else
        if (*p == '\\') {
            p++;
            if (*p == '\0') {
                break;
            }
            if (*p == '/') {
                if (p[1] != '\0') {
                    last = p;
                }
            }
        } else {
            if (*p == '*' || *p == '?') {
                break;
            }
        }
#endif
        p++;
    }
    if (*last != '\0') {
        *last++ = '\0';
    }
    return last;
}

static int file_glob(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs_path = Value_vp(v[1]);
    RefDirIter *r;
    char *path;
    char *last;
    Str path_s;

    path = path_normalize(&path_s, fv->cur_dir, rs_path->c, rs_path->size, NULL);
    if (str_has0(path_s.p, path_s.size)) {
        throw_errorf(fs->mod_file, "FileOpenError", "Path contains '\\0'");
        return FALSE;
    }

    last = split_glob_path(path);

    r = buf_new(cls_diriter, sizeof(RefDirIter));
    *vret = vp_Value(r);

    r->vfile = Value_cp(v[1]);
    r->dir_stk = malloc(sizeof(DirGlob) * DIRGLOB_MAX_STACK);
    memset(r->dir_stk, 0, sizeof(sizeof(DirGlob) * DIRGLOB_MAX_STACK));
    r->dir_top = r->dir_stk;
    r->type = DIRITER_GLOB;

    if (!file_glob_dir(r, path, last)) {
        return FALSE;
    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

static void define_file_const(RefNode *m)
{
    RefNode *n;

    n = define_identifier(m, m, "FOX_HOME", NODE_CONST, 0);
    n->u.k.val = Value_cp(vp_Value(fs->fox_home));
}
static void define_file_func(RefNode *m)
{
    RefNode *n;

    // TextIO(FileIO(), Charset.UTF8)の省略記法
    n = define_identifier(m, m, "fopen", NODE_FUNC_N, 0);
    define_native_func_a(n, file_fopen, 1, 2, NULL, NULL, fs->cls_str);

    // ファイル操作系
    n = define_identifier(m, m, "mkdir", NODE_FUNC_N, 0);
    define_native_func_a(n, file_mkdir, 1, 2, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "unlink", NODE_FUNC_N, 0);
    define_native_func_a(n, file_unlink, 1, 2, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "mvfile", NODE_FUNC_N, 0);
    define_native_func_a(n, file_rename, 2, 2, NULL, NULL, NULL);

    n = define_identifier(m, m, "cpfile", NODE_FUNC_N, 0);
    define_native_func_a(n, file_copy, 2, 3, NULL, NULL, NULL, fs->cls_bool);

    n = define_identifier(m, m, "readfile", NODE_FUNC_N, 0);
    define_native_func_a(n, file_readfile, 1, 3, NULL, NULL, fs->cls_charset, fs->cls_bool);

    n = define_identifier(m, m, "writefile", NODE_FUNC_N, 0);
    define_native_func_a(n, file_writefile, 2, 4, (void*) FALSE, NULL, NULL, fs->cls_charset, fs->cls_bool);

    n = define_identifier(m, m, "appendfile", NODE_FUNC_N, 0);
    define_native_func_a(n, file_writefile, 2, 4, (void*) TRUE, NULL, NULL, fs->cls_charset, fs->cls_bool);

    // ディレクトリ系
    n = define_identifier(m, m, "getcwd", NODE_FUNC_N, 0);
    define_native_func_a(n, file_getcwd, 0, 0, NULL);

    n = define_identifier(m, m, "chdir", NODE_FUNC_N, 0);
    define_native_func_a(n, file_chdir, 1, 1, NULL, NULL);

    n = define_identifier(m, m, "dir", NODE_FUNC_N, 0);
    define_native_func_a(n, file_dir, 1, 1, NULL, NULL);

    n = define_identifier(m, m, "glob", NODE_FUNC_N, 0);
    define_native_func_a(n, file_glob, 1, 1, NULL, fs->cls_str);
}
static void define_file_class(RefNode *m)
{
    RefNode *cls, *cls2;
    RefNode *n;

    // File
    cls = fs->cls_file;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, file_new, 1, 1, NULL, fs->cls_sequence);
    n = define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    define_native_func_a(n, file_path, 0, 2, (void*) FILEPATH_PATH, fs->cls_str, fs->cls_locale);
    n = define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    define_native_func_a(n, file_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    define_native_func_a(n, sequence_cmp, 1, 1, NULL, fs->cls_file);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    define_native_func_a(n, file_in, 1, 1, NULL, fs->cls_file);
    n = define_identifier_p(m, cls, fs->symbol_stock[T_DIV], NODE_FUNC_N, 0);
    define_native_func_a(n, file_cat, 1, 1, NULL, fs->cls_sequence);
    n = define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    define_native_func_a(n, file_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);
    n = define_identifier(m, cls, "path", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_PATH);
    n = define_identifier(m, cls, "filename", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_FILENAME);
    n = define_identifier(m, cls, "basename", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_BASENAME);
    n = define_identifier(m, cls, "path_bytes", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_PATH_BYTES);
    n = define_identifier(m, cls, "filename_bytes", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_FILENAME_BYTES);
    n = define_identifier(m, cls, "basename_bytes", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_BASENAME_BYTES);
    n = define_identifier(m, cls, "basedir", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_BASEDIR);
    n = define_identifier(m, cls, "suffix", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_path, 0, 0, (void*) FILEPATH_SUFFIX);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_size, 0, 0, NULL);
    n = define_identifier(m, cls, "exists", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_exists, 0, 0, (void*) (EXISTS_FILE | EXISTS_DIR));
    n = define_identifier(m, cls, "is_file", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_exists, 0, 0, (void*) EXISTS_FILE);
    n = define_identifier(m, cls, "is_dir", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_exists, 0, 0, (void*) EXISTS_DIR);
    n = define_identifier(m, cls, "mtime", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_mtime, 0, 0, NULL);
    n = define_identifier(m, cls, "list", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_iter, 0, 0, (void*)DIRITER_BOTH);
    n = define_identifier(m, cls, "dirs", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_iter, 0, 0, (void*)DIRITER_DIRS);
    n = define_identifier(m, cls, "files", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, file_iter, 0, 0, (void*)DIRITER_FILES);
    n = define_identifier(m, cls, "relative", NODE_FUNC_N, 0);
    define_native_func_a(n, file_relative, 0, 1, NULL, NULL);
    n = define_identifier(m, cls, "to_uri", NODE_FUNC_N, 0);
    define_native_func_a(n, file_touri, 0, 0, NULL, NULL);
    extends_method(cls, fs->cls_obj);


    // DirIterator
    cls = cls_diriter;
    n = define_identifier_p(m, cls, fs->str_next, NODE_FUNC_N, 0);
    define_native_func_a(n, diriter_next, 0, 0, NULL);
    n = define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    define_native_func_a(n, diriter_close, 0, 0, NULL);
    extends_method(cls, fs->cls_iterator);


    // FileIO
    cls = fv->cls_fileio;
    n = define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    define_native_func_a(n, fileio_new, 1, 2, fv->cls_fileio, NULL, fs->cls_str);

    n = define_identifier(m, cls, "_close", NODE_FUNC_N, 0);
    define_native_func_a(n, fileio_close, 0, 0, NULL);
    n = define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, fileio_empty, 0, 0, NULL);

    n = define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    define_native_func_a(n, fileio_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    define_native_func_a(n, fileio_write, 1, 1, NULL, fs->cls_bytesio);
    n = define_identifier(m, cls, "_seek", NODE_FUNC_N, 0);
    define_native_func_a(n, fileio_seek, 1, 1, NULL, fs->cls_int);
    n = define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    define_native_func_a(n, fileio_size, 0, 0, NULL);
    cls->u.c.n_memb = INDEX_FILEIO_NUM;
    extends_method(cls, fs->cls_streamio);


    cls2 = Hash_get(&fs->mod_io->u.m.h, "IOError", -1);
    cls = define_identifier(m, m, "FileOpenError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);

    cls = define_identifier(m, m, "DirOpenError", NODE_CLASS, 0);
    define_error_class(cls, cls2, m);
}


void init_file_module_stubs()
{
    RefNode *m = Module_new_sys("io.file");

    redefine_identifier(m, m, "File", NODE_CLASS, NODEOPT_STRCLASS, fs->cls_file);

    fv->cls_fileio = define_identifier(m, m, "FileIO", NODE_CLASS, 0);
    cls_diriter = define_identifier(m, m, "DirIterator", NODE_CLASS, 0);

    fs->mod_file = m;
}
void init_file_module_1()
{
    RefNode *m = fs->mod_file;

    define_file_class(m);
    define_file_func(m);
    define_file_const(m);

    m->u.m.loaded = TRUE;
}
