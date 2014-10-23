#include "c_posix_osx.c"
#include "config.h"


const char *get_fox_home()
{
#ifdef FOX_HOME
    return FOX_HOME;
#elif 0
    enum {
        FOX_PATH_MAX = 512,
    };
    // FreeBSD
    int mib[4] = {
        CTL_KERN,
        KERN_PROC,
        KERN_PROC_PATHNAME,
        -1
    };
    char *path = Mem_get(&st_mem, FOX_PATH_MAX);
    size_t cb = FOX_PATH_MAX;
    sysctl(mib, 4, path, &cb, NULL, 0);

    return path;
#else
    enum {
        FOX_PATH_MAX = 512,
    };
    char *path = Mem_get(&fg->st_mem, FOX_PATH_MAX);
    char *p;
    int n = 2;
    int len;

    memset(path, 0, FOX_PATH_MAX);
    if (readlink("/proc/self/exe", path, FOX_PATH_MAX - 1) != -1) {
    } else if (readlink("/proc/curproc/file", path, FOX_PATH_MAX - 1) != -1) {
    } else {
        return "./";
    }
    len = strlen(path);
    len = make_path_regularize(path, len);

    p = path + len;
    while (p > path) {
        p--;
        if (*p == '/') {
            *p = '\0';
            n--;
            if (n <= 0) {
                break;
            }
        }
    }
    return path;
#endif
}

int64_t get_now_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}
