#ifndef _COMPAT_H_
#define _COMPAT_H_

#ifdef WIN32
#define _WIN32_WINNT 0x0501
#endif

#include <stdint.h>

#ifdef __MINGW32__
#define FMT_INT64_PREFIX "I64"
#else
#define FMT_INT64_PREFIX "ll"
#endif

#ifdef WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <wchar.h>

#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_CREAT	   0100	/* not fcntl */
#define O_TRUNC	  01000	/* not fcntl */
#define O_APPEND	  02000

#define DT_REG 1
#define DT_DIR 4

#ifndef SEP_C
#define SEP_C '\\'
#endif

#ifndef SEP_S
#define SEP_S "\\"
#endif

struct dirent {
    unsigned int d_type;
    char d_name[1024];
};
typedef struct {
    void *hDir;
    int first;
    struct dirent ent;
    char wfd[0];
} DIR;

typedef intptr_t FileHandle;

FileHandle open_fox(const char *fname, int mode, int dummy);
int close_fox(FileHandle fd);
int read_fox(FileHandle fd, char *buf, int size);
int write_fox(FileHandle fd, const char *buf, int size);
int64_t seek_fox(FileHandle fd, int64_t offset, int whence);

char *utf16to8(const wchar_t *src);
wchar_t *cstr_to_utf16(const char *src, int src_size);
wchar_t *filename_to_utf16(const char *src, const wchar_t *opt);

DIR *opendir_fox(const char *dname);
struct dirent *readdir_fox(DIR *d);
void closedir_fox(DIR *d);

#else  /* WIN32 */

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef SEP_C
#define SEP_C '/'
#endif

#ifndef SEP_S
#define SEP_S "/"
#endif

typedef int FileHandle;

#define open_fox open
#define close_fox close
#define read_fox read
#define write_fox write
#define seek_fox lseek

#define opendir_fox opendir
#define readdir_fox readdir
#define closedir_fox closedir

#endif  /* WIN32 */

int64_t get_file_size(FileHandle fh);


#endif /* _COMPAT_H_ */
