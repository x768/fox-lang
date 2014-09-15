#include "gui_compat.h"
#include "gui.h"
#include <stdio.h>
#include <stdlib.h>


void gui_initialize()
{
}
void gui_dispose()
{
}

char *get_toolkit_version()
{
	char *p = malloc(64);
	sprintf(p, "Cocoa");
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
