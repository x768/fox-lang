#include "gui_compat.h"
#include "gui.h"
#include <stdlib.h>

void gui_initialize()
{
	int argc = 1;
	char *argv_v[] = {(char*)"fox", NULL};
	char **argv = argv_v;
	gtk_init(&argc, &argv);
}
void gui_dispose()
{
}
char *get_toolkit_version()
{
	char *p = malloc(64);
	sprintf(p, "GTK+ ver %u.%u.%u", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
	return p;
}

void *Value_handle(Value v)
{
	return Value_ptr(v);
}
Value handle_Value(void *handle)
{
	return ptr_Value(handle);
}
