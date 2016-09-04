#include "fox_vm.h"
#include <windows.h>
#include "c_win.c"


void init_stdio()
{
    RefFileHandle *fh;
    Ref *r = ref_new(fv->cls_fileio);
    fg->v_cio = vp_Value(r);

    init_stream_ref(r, STREAM_READ|STREAM_WRITE);

    STDIN_FILENO = (FileHandle)GetStdHandle(STD_INPUT_HANDLE);
    STDOUT_FILENO = (FileHandle)GetStdHandle(STD_OUTPUT_HANDLE);
    STDERR_FILENO = (FileHandle)GetStdHandle(STD_ERROR_HANDLE);

    fh = buf_new(NULL, sizeof(RefFileHandle));
    r->v[INDEX_FILEIO_HANDLE] = vp_Value(fh);
    fh->fd_read = STDIN_FILENO;
    fh->fd_write = STDOUT_FILENO;

    fv->console_read = FALSE;
    fv->console_write = FALSE;
    fv->console_error = FALSE;
}

/**
 * OSで設定されている地域と言語を取得
 */
const char *get_default_locale(void)
{
    return "(neutral)";
}
void get_local_timezone_name(char *buf, int max)
{
    strcpy(buf, "Etc/UTC");
}

// UTF-8 -> ISO-8859-1
// コード範囲外の値は、下8ビッドだけ取る
static const char *utf8_to_8bit(const char *src)
{
    if (src != NULL) {
        int len = strlen(src);
        const char *p = src;
        const char *end = p + len;

        char *c_buf = Mem_get(&fg->st_mem, len + 1);
        int i = 0;

        while (p < end) {
            int ch = utf8_codepoint_at(p);
            c_buf[i++] = ch & 0xFF;
            utf8_next(&p, end);
        }
        c_buf[i] = '\0';
        return c_buf;
    } else {
        return NULL;
    }
}

int main()
{
    const char *argv[3];

    init_fox_vm(RUNNING_MODE_CGI);
    init_env();

    argv[0] = "fox.cgi";
    argv[1] = utf8_to_8bit(Hash_get(&fs->envs, "PATH_TRANSLATED", -1));
    argv[2] = NULL;

    main_fox(argv[1] != NULL ? 2 : 1, argv);

    return 0;
}

