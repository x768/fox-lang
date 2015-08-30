#include "fox_vm.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>


int get_file_mtime(int64_t *tm, const char *fname)
{
    struct stat st;

    if (stat(fname, &st) == -1) {
        return FALSE;
    }
#if defined __USE_MISC || defined __USE_XOPEN2K8
    *tm = (int64_t)st.st_mtim.tv_sec * 1000 + (st.st_mtim.tv_nsec / 1000000);
#else
    *tm = (int64_t)st.st_mtimespec.tv_sec * 1000 + (st.st_mtimespec.tv_nsec / 1000000);
#endif

    return TRUE;
}

int exists_file(const char *file)
{
    struct stat st;

    if (stat(file, &st) == -1) {
        return EXISTS_NONE;
    }
    if (S_ISDIR(st.st_mode)) {
        return EXISTS_DIR;
    } else {
        return EXISTS_FILE;
    }
}

int is_root_dir(const char *path_p, int path_size)
{
    if (path_size < 0) {
        path_size = strlen(path_p);
    }
    if (path_size == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}
int is_absolute_path(const char *path_p, int path_size)
{
    if (path_size < 0) {
        path_size = strlen(path_p);
    }
    if (path_size > 0 && path_p[0] == '/') {
        return TRUE;
    } else {
        return FALSE;
    }
}

// 通常、ディレクトリ名は最後の/または\を省略して表現するが、
// rootディレクトリは/またはC:\で表すため、表示するときだけ変換する
Str get_root_name(const char *path_p, int path_size)
{
    if (path_size < 0) {
        path_size = strlen(path_p);
    }
    if (path_size == 0) {
        return Str_new("/", 1);
    } else {
        return Str_new(path_p, path_size);
    }
}

void get_random(void *buf, int len)
{
    int fd = open("/dev/urandom", O_RDONLY, DEFAULT_PERMISSION); // for Unix like system
    if (fd != -1) {
        read(fd, buf, len);
        close(fd);
    }
}
void native_sleep(int ms)
{
    struct timespec tv;
    tv.tv_sec = ms / 1000;
    tv.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&tv, NULL);
}

char *get_current_directory(void)
{
    return getcwd(NULL, 0);
}
int set_current_directory(const char *path)
{
    return chdir(path) == 0;
}

////////////////////////////////////////////////////////////////////////

static void init_env(char **envp)
{
    int i;

    Hash_init(&fs->envs, &fg->st_mem, 64);
    for (i = 0; envp[i] != NULL; i++) {
        const char *src = envp[i];
        const char *p = src;

        while (isalnum(*p) || *p == '_') {
            p++;
        }
        if (*p == '=') {
            char *val = str_dup_p(p + 1, -1, &fg->st_mem);
            Hash_add_p(&fs->envs, &fg->st_mem, intern(src, p - src), val);
        }
    }
}

void init_stdio()
{
    RefFileHandle *fh;
    Ref *r = ref_new(fv->cls_fileio);
    fg->v_cio = vp_Value(r);

    init_stream_ref(r, STREAM_READ|STREAM_WRITE);
    fh = buf_new(NULL, sizeof(RefFileHandle));
    r->v[INDEX_FILEIO_HANDLE] = vp_Value(fh);
    fh->fd_read = STDIN_FILENO;
    fh->fd_write = STDOUT_FILENO;
}
