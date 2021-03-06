#ifndef _COMPAT_VM_H_
#define _COMPAT_VM_H_

#include "compat.h"

enum {
    EXISTS_NONE = 0,
    EXISTS_FILE = 1,
    EXISTS_DIR = 2,
};


#ifdef WIN32

#define RTLD_LAZY   0x00001


#ifdef STDIN_FILENO
#undef STDIN_FILENO
#endif
#ifdef STDOUT_FILENO
#undef STDOUT_FILENO
#endif
#ifdef STDERR_FILENO
#undef STDERR_FILENO
#endif

extern FileHandle STDIN_FILENO;
extern FileHandle STDOUT_FILENO;
extern FileHandle STDERR_FILENO;


int mkdir_fox(const char *dname, int dummy);
int remove_fox(const char *fname);
int rename_fox(const char *fname, const char *tname);

void *dlopen_fox(const char *fname, int dummy);
void *dlsym_fox(void *handle, const char *name);
const char *dlerror_fox(void);

int win_read_console(FileHandle fd, char *dst, int dst_size);
int win_write_console(FileHandle fd, const char *str, int size);

#else  /* WIN32 */

#define mkdir_fox mkdir
#define remove_fox remove
#define rename_fox rename

#define dlopen_fox dlopen
#define dlsym_fox dlsym
#define dlerror_fox dlerror

#endif  /* WIN32 */

int get_file_mtime(int64_t *tm, const char *fname);
int exists_file(const char *file);
int is_root_dir(const char *path_p, int path_size);
int is_absolute_path(const char *path_p, int path_size);
Str get_root_name(const char *path_p, int path_size);
const char *get_fox_home(void);
void native_sleep(int ms);
const char *get_default_locale(void);
void get_local_timezone_name(char *buf, int max);
int64_t get_now_time(void);
void get_random(void *buf, int len);

void init_stdio(void);
char *get_current_directory(void);
int set_current_directory(const char *path);
void show_error_message(const char *msg, int msg_size, int warn);


#endif /* _COMPAT_VM_H_ */
