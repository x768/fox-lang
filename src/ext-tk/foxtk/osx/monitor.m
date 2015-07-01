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

int native_filemonitor_new(Ref *r, const char *path)
{
    return TRUE;
}
void native_filemonitor_remove(Ref *r)
{
}

void native_object_remove_all()
{
}
