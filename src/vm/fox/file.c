#include "fox_vm.h"
#include <string.h>

/*
 * in  : /hoge/aaa/bbb.txt
 * out : /hoge/aaa/
 */
Str base_dir_with_sep(const char *path_p, int path_size)
{
    Str ret;
    const char *p;

    if (path_size < 0) {
        path_size = strlen(path_p);
    }

    ret.p = path_p;
    ret.size = path_size;

    p = &path_p[ret.size];

    while (p > path_p) {
        if (p[-1] == SEP_C) {
            break;
        }
        p--;
    }

    if (p > path_p) {
        ret.size = p - path_p;
    } else {
        ret.p = "." SEP_S;
        ret.size = 2;
    }

    return ret;
}
/*
 * in  : /hoge/aaa/bbb.txt
 * out : bbb.txt
 */
Str file_name_from_path(const char *path_p, int path_size)
{
    Str ret;
    const char *p;

    if (path_size < 0) {
        path_size = strlen(path_p);
    }

    ret.p = path_p;
    ret.size = path_size;

    p = &path_p[ret.size];

    while (p > path_p) {
        if (p[-1] == SEP_C) {
            break;
        }
        p--;
    }

    if (p > path_p) {
        ret.p = p;
        ret.size = &path_p[ret.size] - p;
    } else {
        ret.p = path_p;
        ret.size = path_size;
    }
    return ret;
}

/**
 * パスの途中の /../ や /./ を正規化
 * NULバイトを通すので、ファイルを開くときにチェックが必要
 */
int make_path_regularize(char *path, int size)
{
    enum {
        MAX_DIRS = 256,
    };
    int src = 0, dst = 0;
    int dir_pos = 0;
    int dir[MAX_DIRS];

#ifdef WIN32
    // ネットワークパス(\\MACHINE\)の先頭を除去しないよう
    if (size >= 2) {
        if ((path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/')) {
            path[0] = '\\';
            path[1] = '\\';
            src = 2;
            dst = 2;
            // \\machine -> \\MACHINE
            while (src < size && path[src] != '/' && path[src] != '\\') {
                path[dst++] = toupper(path[src++]);
            }
        } else if (path[1] == ':') {
            path[0] = toupper(path[0]);
            src = 3;
            dst = 3;
        }
    }
#endif

    while (src < size) {
#ifdef WIN32
        if (path[src] == '/' || path[src] == '\\') {
#else
        if (path[src] == '/') {
#endif
#ifdef WIN32
            if (path[src+1] == '.' && path[src + 2] == '.' && (path[src + 3] == '/' || path[src + 3] == '\\' || src + 3 == size)) {
#else
            if (path[src+1] == '.' && path[src + 2] == '.' && (path[src + 3] == '/' || src + 3 == size)) {
#endif
                if (dir_pos > 0) {
                    dir_pos--;
                    dst = dir[dir_pos];
                }
                if (path[src + 3] == '\0') {
                    break;
                }
                src += 3;
#ifdef WIN32
            } else if (path[src + 1] == '.' && (path[src + 2] == '/' || path[src + 2] == '\\' || src + 2 == size)) {
#else
            } else if (path[src + 1] == '.' && (path[src + 2] == '/' || src + 2 == size)) {
#endif
                if (src + 2 == size) {
                    break;
                }
                src += 2;
#ifdef WIN32
            } else if (path[src + 1] == '/' || path[src + 1] == '\\') {
#else
            } else if (path[src + 1] == '/') {
#endif
                src++;
            } else {
                if (dir_pos < MAX_DIRS - 1) {
                    dir[dir_pos] = dst;
                    dir_pos++;
                }
#ifdef WIN32
                path[dst] = (path[src] == '/' ? '\\' : path[src]);
#else
                path[dst] = path[src];
#endif
                src++;
                dst++;
            }
        } else {
            path[dst] = path[src];
            dst++;
            src++;
        }
    }
    path[dst] = '\0';

    return dst;
}

/**
 * 正規化済みのファイルパス(.. などを解釈)を返す
 * NULバイトを通す
 */
char *path_normalize(Str *ret, RefStr *base_dir, const char *path_p, int path_size, Mem *mem)
{
    char *ptr;
    int size, ret_size;

    if (path_size < 0) {
        path_size = strlen(path_p);
    }

    if (base_dir->size == 0 || is_absolute_path(path_p, path_size)) {
        ptr = str_dup_p(path_p, path_size, mem);
        size = path_size;
#ifdef WIN32
    } else if (path_size > 0 && (path_p[0] == '\\' || path_p[0] == '/')) {
        // ドライブからの相対パス
        // pwd=C:/test, path=/hoge -> C:/hoge
        int i = 2;
        while (i < base_dir->size && (base_dir->c[i] != '\\' && base_dir->c[i] != '/')) {
            i++;
        }
        size = i;
        ptr = Mem_get(mem, size + path_size);
        memcpy(ptr, base_dir->c, size);
        memcpy(ptr + size, path_p, path_size);
        size += path_size;
        ptr[size] = '\0';
#endif
    } else {
        ptr = Mem_get(mem, base_dir->size + path_size + 2);
        memcpy(ptr, base_dir->c, base_dir->size);
        size = base_dir->size;

        // base_dirの最後が/で終わっていない場合、/を補う
        if (base_dir->c[base_dir->size - 1] != SEP_C) {
            ptr[size++] = SEP_C;
        }
        memcpy(ptr + size, path_p, path_size);
        size += path_size;
        ptr[size] = '\0';
    }

    ret_size = make_path_regularize(ptr, size);
    if (ret != NULL) {
        ret->p = ptr;
        ret->size = ret_size;
    }
    return ptr;
}

Str get_name_part_from_path(Str path)
{
    const char *end = &path.p[path.size];
    const char *p = end;
    Str ret;

    while (p > path.p) {
        if (p[-1] == SEP_C) {
            break;
        }
        p--;
    }

    ret.p = p;
    while (p < end) {
        if (*p == '.') {
            break;
        }
        p++;
    }
    ret.size = p - ret.p;
    return ret;
}

