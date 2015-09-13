#define NOT_DEFINE_BOOL
#include "gui_compat.h"
#include "m_gui.h"



int native_timer_new(int millisec, Ref *r)
{
    return 0;
}
void native_timer_remove(Ref *r)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

int native_dirmonitor_new(Ref *r, const char *path)
{
    return TRUE;
}
void native_dirmonitor_remove(Ref *r)
{
}
