#include "fox.h"
#include <windows.h>

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
}
