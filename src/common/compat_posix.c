#include "fox.h"
#include <sys/stat.h>


int64_t get_file_size(FileHandle fh)
{
    if (fh != -1) {
#if defined(_LARGEFILE64_SOURCE) && _LFS64_LARGEFILE-0 && !defined(__APPLE__)
        struct stat64 st;
        if (fstat64(fh, &st) == -1) {
            return -1;
        }
#else
        struct stat st;
        if (fstat(fh, &st) == -1) {
            return -1;
        }
#endif
        return st.st_size;
    } else {
        return -1;
    }
}
