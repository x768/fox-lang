#include "fox_vm.h"
#include <windows.h>
#include "c_win.c"
#include "c_win_cl_gui.c"


void init_stdio()
{
    Ref *r = ref_new(fv->cls_nullio);
    fg->v_cio = vp_Value(r);
    init_stream_ref(r, STREAM_READ | STREAM_WRITE);
}
void show_error_message(const char *msg, int msg_size, int warn)
{
    wchar_t *wtmp = cstr_to_utf16(msg, msg_size);
    MessageBoxW(NULL, wtmp, L"fox", (warn ? MB_OK|MB_ICONWARNING : MB_OK|MB_ICONERROR));
    free(wtmp);
}
static LONG CALLBACK exception_filter(EXCEPTION_POINTERS *e)
{
    show_error_message("UnhandledException occurred\n", -1, FALSE);
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

