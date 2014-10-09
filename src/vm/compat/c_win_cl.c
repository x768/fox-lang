#include "fox_vm.h"
#include <windows.h>
#include "c_win.c"
#include "c_win_cl_cgi.c"
#include "c_win_cl_gui.c"

#include <windows.h>

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

