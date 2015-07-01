#include "fox_vm.h"
#include <windows.h>
#include "c_win.c"
#include "c_win_cl_gui.c"



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

    fv->console_read = (GetFileType((HANDLE)STDIN_FILENO) == FILE_TYPE_CHAR);
    fv->console_write = (GetFileType((HANDLE)STDOUT_FILENO) == FILE_TYPE_CHAR);
    fv->console_error = (GetFileType((HANDLE)STDERR_FILENO) == FILE_TYPE_CHAR);
}

static void write_errmsg(const char *str)
{
    write_fox(STDERR_FILENO, str, strlen(str));
}
static LONG CALLBACK exception_filter(EXCEPTION_POINTERS *e)
{
    write_errmsg("UnhandledException occurred\n");
    return EXCEPTION_EXECUTE_HANDLER;
}

int main()
{
    int argc;
    const char **argv;

    SetUnhandledExceptionFilter(exception_filter);

    init_fox_vm();
    init_env();
    argv = get_cmd_args(&argc);
    return main_fox(argc, argv);
}

